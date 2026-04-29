#include "vendor_hid_protocol.h"

#include <subsys/usb_connect.h>

#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usbd_hid.h>

LOG_MODULE_DECLARE(usb_connect, CONFIG_USB_CONNECT_LOG_LEVEL);

#define YKB_PROTOCOL_MAX_PACKET_SIZE                                           \
    CONFIG_USB_CONNECT_MAX_VENDOR_OUT_REPORT_SIZE
#include <lib/ykb_protocol.h>

// No need for a parallel streams here, just request -> response
// So just one rx slot and one tx slot

static uint8_t rx_buffer[sizeof(vendor_hid_proto_packet_t)] = {0};

static ykb_protocol_rx_state_t rx = {
    .buffer = rx_buffer,
    .capacity = sizeof(rx_buffer),
};

static const struct device *hid_vendor_dev =
    DEVICE_DT_GET(DT_NODELABEL(hid_vendor));

static struct k_work response_work;
static enum response_type current_response;
static bool busy = false;

static void response_work_handler(struct k_work *work) {
    uint16_t values[TOTAL_KEY_COUNT];
    uint8_t resp;
    uint8_t *data;
    uint16_t len;
    switch (current_response) {
    case RESPONSE_GET_FEATURES: {
        device_features *features = vendor_hid_protocol_get_features();
        if (!features) {
            LOG_ERR("features null");
            goto response_error;
        }
        data = (uint8_t *)features;
        len = sizeof(device_features);
        break;
    }
    case RESPONSE_GET_VALUES: {
        vendor_hid_protocol_get_values(values, TOTAL_KEY_COUNT);
        data = (uint8_t *)values;
        len = sizeof(values);
        break;
    }
    case RESPONSE_GET_SETTINGS: {
        kb_settings_t *settings = vendor_hid_protocol_get_settings();
        if (!settings) {
            LOG_ERR("settigns nil");
            goto response_error;
        }
        data = (uint8_t *)settings;
        len = sizeof(kb_settings_t);
        break;
    }
    case RESPONSE_SET_SETTINGS_OK: {
        resp = RESPONSE_SET_SETTINGS_OK;
        data = &resp;
        len = sizeof(resp);
        break;
    }
    case RESPONSE_ERROR: {
    response_error:
        resp = RESPONSE_ERROR;
        data = &resp;
        len = sizeof(resp);
        break;
    }
    default: {
        LOG_ERR("Unknown response %d", current_response);
        return;
    }
    }

    ykb_protocol_tx_state_t tx;
    ykb_protocol_tx_init(&tx, data, len, 0, YKB_PROTOCOL_TYPE_DATA);

    ykb_protocol_packet_t packet;
    while (ykb_protocol_tx_has_more(&tx)) {
        if (!ykb_protocol_tx_build_packet(&tx, &packet)) {
            LOG_ERR("ykb_protocol_tx_build_packet failed");
            break;
        }
        int err = hid_device_submit_report(
            hid_vendor_dev,
            ykb_protocol_payload_len_for_index(
                tx.total_len, packet.header.packet_idx, tx.packet_count),
            (uint8_t *)&packet);
        if (err) {
            LOG_ERR("hid_device_submit_report (vendor): %d", err);
        }
    }
    ykb_protocol_rx_reset(&rx);
    busy = false;
}

int vendor_hid_protocol_parse(const uint8_t *const data, size_t len) {
    if (busy) {
        LOG_ERR("Response in progress, skipping packet");
        return -1;
    }

    if (len < sizeof(ykb_protocol_header_t)) {
        LOG_ERR("len < sizeof(ykb_protocol_header_t)");
        return -2;
    }

    ykb_protocol_rx_result_t res =
        ykb_protocol_rx_push_packet(&rx, (ykb_protocol_packet_t *)data);
    if (res == YKB_PROTOCOL_RX_RESULT_COMPLETE) {
        busy = true;
        vendor_hid_proto_packet_t pack;
        memcpy(&pack, rx_buffer, rx.total_len);
        enum request_type req = pack.header.type;
        int err;
        switch (req) {
        case REQUEST_GET_FEATURES:
            current_response = RESPONSE_GET_FEATURES;
            k_work_submit(&response_work);
            break;
        case REQUEST_GET_VALUES:
            current_response = RESPONSE_GET_VALUES;
            k_work_submit(&response_work);
            break;
        case REQUEST_GET_SETTINGS:
            current_response = RESPONSE_GET_SETTINGS;
            k_work_submit(&response_work);
        case REQUEST_SET_SETTINGS:
            err = vendor_hid_protocol_set_settings((kb_settings_t *)rx_buffer);
            if (err) {
                current_response = RESPONSE_ERROR;
                k_work_submit(&response_work);
                break;
            }
            current_response = RESPONSE_SET_SETTINGS_OK;
            k_work_submit(&response_work);
            break;
        default:
            LOG_ERR("Unknown request type %d", req);
            return -3;
        }
    }

    if (res < 0) {
        LOG_ERR("ykb_protocol_rx_push_packet: %d", res);
        return res;
    }

    return 0;
}

int vendor_hid_protocol_init(void) {
    ykb_protocol_rx_init(&rx, rx_buffer, sizeof(rx_buffer), false, NULL, 0);

    k_work_init(&response_work, response_work_handler);

    return 0;
}
