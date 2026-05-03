#include <lib/vendor_hid_protocol.h>

#include <subsys/kb_handler.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(vendor_hid_protocol, LOG_LEVEL_INF);

static FEATURES_DEFINE(features);
static kb_settings_t settings_snap;

static device_features *vendor_hid_protocol_get_features(void) {
    return &features;
}

static void vendor_hid_protocol_get_values(uint16_t *values, uint16_t count) {
    kb_handler_get_values(values, count);
}

static kb_settings_t *vendor_hid_protocol_get_settings(void) {
    int err = kb_settings_get(&settings_snap);
    if (err) {
        LOG_ERR("kb_settings_get: %d", err);
        return NULL;
    }

    return &settings_snap;
}

static int vendor_hid_protocol_set_settings(const kb_settings_t *settings) {
    int err = kb_settings_apply(settings);
    if (err) {
        LOG_ERR("kb_settings_apply: %d", err);
    }

    return err;
}

static void response_work_handler(struct k_work *work) {
    vendor_hid_protocol_ctx_t *ctx =
        CONTAINER_OF(work, vendor_hid_protocol_ctx_t, response_work);
    uint16_t values[TOTAL_KEY_COUNT];
    uint8_t response_code;
    uint8_t *data;
    uint16_t len;

    switch (ctx->current_response) {
    case RESPONSE_GET_FEATURES: {
        device_features *feature_data = vendor_hid_protocol_get_features();
        if (!feature_data) {
            response_code = RESPONSE_ERROR;
            data = &response_code;
            len = sizeof(response_code);
            break;
        }
        data = (uint8_t *)feature_data;
        len = sizeof(*feature_data);
        break;
    }
    case RESPONSE_GET_VALUES:
        vendor_hid_protocol_get_values(values, TOTAL_KEY_COUNT);
        data = (uint8_t *)values;
        len = sizeof(values);
        break;

    case RESPONSE_GET_SETTINGS: {
        kb_settings_t *settings = vendor_hid_protocol_get_settings();
        if (!settings) {
            response_code = RESPONSE_ERROR;
            data = &response_code;
            len = sizeof(response_code);
            break;
        }
        data = (uint8_t *)settings;
        len = sizeof(*settings);
        break;
    }
    case RESPONSE_SET_SETTINGS_OK:
        response_code = RESPONSE_SET_SETTINGS_OK;
        data = &response_code;
        len = sizeof(response_code);
        break;

    case RESPONSE_ERROR:
    default:
        response_code = RESPONSE_ERROR;
        data = &response_code;
        len = sizeof(response_code);
        break;
    }

    ykb_protocol_tx_state_t tx;
    ykb_protocol_tx_init(&tx, data, len, 0, YKB_PROTOCOL_TYPE_DATA);

    ykb_protocol_packet_t packet;
    while (ykb_protocol_tx_has_more(&tx)) {
        if (!ykb_protocol_tx_build_packet(&tx, &packet)) {
            LOG_ERR("ykb_protocol_tx_build_packet failed");
            break;
        }

        uint16_t payload_len = ykb_protocol_payload_len_for_index(
            tx.total_len, packet.header.packet_idx, tx.packet_count);
        size_t packet_len = sizeof(packet.header) + payload_len;

        int err = ctx->send_packet((const uint8_t *)&packet, packet_len,
                                   ctx->user_data);
        if (err) {
            LOG_ERR("send_packet failed: %d", err);
            break;
        }
    }

    ykb_protocol_rx_reset(&ctx->rx);
    ctx->busy = false;
}

int vendor_hid_protocol_init(vendor_hid_protocol_ctx_t *ctx,
                             vendor_hid_send_packet_cb_t send_packet,
                             void *user_data) {
    if (!ctx || !send_packet) {
        return -EINVAL;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->send_packet = send_packet;
    ctx->user_data = user_data;

    ykb_protocol_rx_init(&ctx->rx, ctx->rx_buffer, sizeof(ctx->rx_buffer),
                         false, NULL, 0);
    k_work_init(&ctx->response_work, response_work_handler);

    return 0;
}

int vendor_hid_protocol_parse(vendor_hid_protocol_ctx_t *ctx,
                              const uint8_t *data, size_t len) {
    if (!ctx || !data) {
        return -EINVAL;
    }

    if (ctx->busy) {
        LOG_WRN("Response in progress, skipping packet");
        return -EBUSY;
    }

    if (len < sizeof(ykb_protocol_header_t)) {
        LOG_ERR("Packet too short: %u", (unsigned)len);
        return -EMSGSIZE;
    }

    ykb_protocol_rx_result_t res =
        ykb_protocol_rx_push_packet(&ctx->rx, (const ykb_protocol_packet_t *)data);
    if (res < 0) {
        LOG_ERR("ykb_protocol_rx_push_packet: %d", res);
        return (int)res;
    }

    if (res != YKB_PROTOCOL_RX_RESULT_COMPLETE) {
        return 0;
    }

    ctx->busy = true;

    const vendor_hid_proto_packet_t *request =
        (const vendor_hid_proto_packet_t *)ctx->rx_buffer;

    switch ((enum request_type)request->header.type) {
    case REQUEST_GET_FEATURES:
        ctx->current_response = RESPONSE_GET_FEATURES;
        break;

    case REQUEST_GET_VALUES:
        ctx->current_response = RESPONSE_GET_VALUES;
        break;

    case REQUEST_GET_SETTINGS:
        ctx->current_response = RESPONSE_GET_SETTINGS;
        break;

    case REQUEST_SET_SETTINGS: {
        int err = vendor_hid_protocol_set_settings(
            (const kb_settings_t *)request->data);
        ctx->current_response =
            (err == 0) ? RESPONSE_SET_SETTINGS_OK : RESPONSE_ERROR;
        break;
    }

    default:
        LOG_ERR("Unknown request type %u", request->header.type);
        ctx->current_response = RESPONSE_ERROR;
        break;
    }

    k_work_submit(&ctx->response_work);

    return 0;
}
