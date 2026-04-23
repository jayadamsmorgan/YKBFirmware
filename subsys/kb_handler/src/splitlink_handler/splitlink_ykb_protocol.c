#include "splitlink_handler.h"

#include <drivers/splitlink.h>

#include <subsys/zephyr_user_helpers.h>

#define YKB_PROTOCOL_MAX_PACKET_SIZE SPLITLINK_MAX_PACKET_LENGTH
#include <lib/ykb_protocol.h>

#include <zephyr/logging/log.h>

#include <stdatomic.h>

LOG_MODULE_DECLARE(kb_handler);

enum rx_slot_state {
    RX_SLOT_EMPTY,
    RX_SLOT_RECEIVING,
    RX_SLOT_READY,
    RX_SLOT_PROCESSING,
};

enum tx_slot_state {
    TX_SLOT_EMPTY,
    TX_SLOT_TRANSCEIVING,
};

struct rx_slot {
    struct k_work work;
    enum rx_slot_state state;
    uint8_t *data;
    uint16_t max_data_length;
#if CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
    uint8_t bitmap[CONFIG_KB_HANDLER_SL_BITMAP_LENGTH];
#endif // CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
    ykb_protocol_rx_state_t rx;
    uint8_t id;
};

struct tx_slot {
    struct k_work work;
    enum tx_slot_state state;
    uint8_t *data;
    uint16_t max_data_length;
    ykb_protocol_tx_state_t tx;
    uint8_t id;
};

#define ATOMIC_STORE(var, val)                                                 \
    atomic_store_explicit(var, val, memory_order_relaxed)
#define ATOMIC_LOAD(var) atomic_load_explicit(var, memory_order_relaxed)
static atomic_bool connected;

#define VALUES_SLOT_ID 1U
#define SETTINGS_SLOT_ID 2U

#define TX_SLOT(NAME, DATA_SIZE, ID)                                           \
    static uint8_t NAME##_tx_slot_data[DATA_SIZE] = {0};                       \
    static struct tx_slot NAME##_tx_slot = {                                   \
        .data = NAME##_tx_slot_data,                                           \
        .max_data_length = sizeof(NAME##_tx_slot_data),                        \
        .state = TX_SLOT_EMPTY,                                                \
        .id = ID,                                                              \
    }

#define RX_SLOT(NAME, DATA_SIZE, ID)                                           \
    static uint8_t NAME##_rx_slot_data[DATA_SIZE] = {0};                       \
    static struct rx_slot NAME##_rx_slot = {                                   \
        .data = NAME##_rx_slot_data,                                           \
        .max_data_length = sizeof(NAME##_rx_slot_data),                        \
        .state = RX_SLOT_EMPTY,                                                \
        .id = ID,                                                              \
    }

#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
TX_SLOT(values, sizeof(uint16_t) * CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE,
        VALUES_SLOT_ID);
RX_SLOT(settings, sizeof(kb_settings_t), SETTINGS_SLOT_ID);
#endif // CONFIG_KB_HANDLER_SPLITLINK_SLAVE

#if CONFIG_KB_HANDLER_SPLITLINK_MASTER
RX_SLOT(values, sizeof(uint16_t) * CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE,
        VALUES_SLOT_ID);
TX_SLOT(settings, sizeof(kb_settings_t), SETTINGS_SLOT_ID);
#endif // CONFIG_KB_HANDLER_SPLITLINK_MASTER

#if !Z_USER_HAS_PROP(kb_handler_splitlink)
#error                                                                         \
    "KB Handler Splitlink requires kb-handler-splitlink to be present in zephyr,user"
#endif // Z_USER_HAS_PROP(kb_handler_splitlink)

const struct device *splitlink_dev = Z_USER_DEV(kb_handler_splitlink);

static inline void rx_init(struct rx_slot *slot) {
#if CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
    ykb_protocol_rx_init(&slot->rx, slot->data, slot->max_data_length, true,
                         slot->bitmap, sizeof(slot->bitmap));
#else
    ykb_protocol_rx_init(&slot->rx, slot->data, slot->max_data_length, false,
                         NULL, 0);
#endif // CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
}

void rx_slot_work_handler(struct k_work *work) {
    struct rx_slot *slot = CONTAINER_OF(work, struct rx_slot, work);
    slot->state = RX_SLOT_PROCESSING;

#if CONFIG_KB_HANDLER_SPLITLINK_MASTER
    if (slot->id == VALUES_SLOT_ID) {
        uint16_t values[CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE];
        memcpy(values, slot->data, slot->rx.total_len);
        uint16_t total_len = slot->rx.total_len;
        ykb_protocol_rx_reset(&slot->rx);
        slot->state = RX_SLOT_EMPTY;
        splitlink_handler_values_received(values, total_len / sizeof(uint16_t));
        return;
    }
#endif // CONFIG_KB_HANDLER_SPLITLINK_MASTER
#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
    if (slot->id == SETTINGS_SLOT_ID) {
        kb_settings_t settings;
        memcpy(&settings, slot->data, slot->rx.total_len);
        ykb_protocol_rx_reset(&slot->rx);
        slot->state = RX_SLOT_EMPTY;
        splitlink_handler_settings_received(&settings);
        return;
    }
#endif // CONFIG_KB_HANDLER_SPLITLINK_SLAVE

    // Just in case
    ykb_protocol_rx_reset(&slot->rx);
    slot->state = RX_SLOT_EMPTY;
}

void tx_slot_work_handler(struct k_work *work) {
    struct tx_slot *slot = CONTAINER_OF(work, struct tx_slot, work);

    ykb_protocol_packet_t packet;
    int err;
    ykb_protocol_tx_init(&slot->tx, slot->data, slot->tx.total_len, slot->id,
                         YKB_PROTOCOL_TYPE_DATA);
    while (ykb_protocol_tx_has_more(&slot->tx)) {
        if (!ykb_protocol_tx_build_packet(&slot->tx, &packet)) {
            LOG_ERR("ykb_protocol_tx_build_packet failed");
            break;
        }
        err =
            splitlink_send(splitlink_dev, (uint8_t *)&packet,
                           YKB_PROTOCOL_HEADER_SIZE +
                               ykb_protocol_payload_len_for_index(
                                   slot->tx.total_len, packet.header.packet_idx,
                                   slot->tx.packet_count));
        if (err) {
            LOG_ERR("splitlink_send: %d", err);
            break;
        }
    }

    slot->state = TX_SLOT_EMPTY;
}

static void on_receive_cb(const struct device *dev, uint8_t *data,
                          size_t data_len) {
    if (!data || data_len == 0) {
        return;
    }

    if (data_len < YKB_PROTOCOL_HEADER_SIZE ||
        data_len > sizeof(ykb_protocol_packet_t)) {
        LOG_ERR("invalid packet length %d", data_len);
        return;
    }

    ykb_protocol_packet_t packet = {0};
    memcpy(&packet, data, data_len);
    uint8_t id = packet.header.transfer_id;

    struct rx_slot *slot;

    switch (id) {
    case VALUES_SLOT_ID:
#if CONFIG_KB_HANDLER_SPLITLINK_MASTER
        slot = &values_rx_slot;
        break;
#else
        LOG_ERR("Values RX is not supported on splitlink slave");
        return;
#endif // CONFIG_KB_HANDLER_SPLITLINK_MASTER
    case SETTINGS_SLOT_ID:
#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
        slot = &settings_rx_slot;
        break;
#else
        LOG_ERR("Settings RX is not supported on splitlink master");
        return;
#endif // CONFIG_KB_HANDLER_SPLITLINK_SLAVE
    default:
        LOG_ERR("Packet for unknown slot id %d", id);
        return;
    }

    ykb_protocol_rx_result_t res;

    switch (slot->state) {
    case RX_SLOT_EMPTY:
        rx_init(slot);
        slot->state = RX_SLOT_RECEIVING;
        res = ykb_protocol_rx_push_packet(&slot->rx, &packet);
        break;
    case RX_SLOT_RECEIVING:
        res = ykb_protocol_rx_push_packet(&slot->rx, &packet);
        break;
    default:
        LOG_ERR("Slot %d is in progress, skipping packet", id);
        return;
    }

    if (res < 0) {
        LOG_ERR("packet transfer: %d", res);
        ykb_protocol_rx_reset(&slot->rx);
        slot->state = RX_SLOT_EMPTY;
        return;
    }

    if (res == YKB_PROTOCOL_RX_RESULT_COMPLETE) {
        slot->state = RX_SLOT_READY;
        k_work_submit(&slot->work);
    }
}

#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
void splitlink_handler_send_values(uint16_t *values, uint16_t count) {
    if (!values || count == 0) {
        LOG_ERR("splitlink_handler_send_values: values null or count 0");
        return;
    }
    bool con = ATOMIC_LOAD(&connected);
    if (!con) {
        return;
    }
    struct tx_slot *slot = &values_tx_slot;
    if (slot->state == TX_SLOT_TRANSCEIVING) {
        LOG_ERR("Slot %d already in progreess, skipping", slot->id);
        return;
    }
    uint16_t size_bytes = count * sizeof(uint16_t);
    if (size_bytes > slot->max_data_length) {
        LOG_ERR("TX slot %d overflow (got %d, max %d)", slot->id, size_bytes,
                slot->max_data_length);
        return;
    }

    slot->tx.total_len = size_bytes;
    memcpy(slot->data, values, size_bytes);
    slot->state = TX_SLOT_TRANSCEIVING;
    k_work_submit(&slot->work);
}
#endif // CONFIG_KB_HANDLER_SPLITLINK_SLAVE

#if CONFIG_KB_HANDLER_SPLITLINK_MASTER
void splitlink_handler_send_settings(kb_settings_t *settings) {
    if (!settings) {
        LOG_ERR("splitlink_handler_send_settings: settings null");
        return;
    }
    bool con = ATOMIC_LOAD(&connected);
    if (!con) {
        return;
    }
    struct tx_slot *slot = &settings_tx_slot;
    if (slot->state == TX_SLOT_TRANSCEIVING) {
        LOG_ERR("Slot %d already in progress, skipping", slot->id);
        return;
    }

    slot->tx.total_len = slot->max_data_length;
    memcpy(slot->data, settings, sizeof(kb_settings_t));
    slot->state = TX_SLOT_TRANSCEIVING;
    k_work_submit(&slot->work);
}
#endif // CONFIG_KB_HANDLER_SPLITLINK_MASTER

#if CONFIG_KB_HANDLER_SPLITLINK_MASTER
__weak void splitlink_handler_values_received(uint16_t *values,
                                              uint16_t count) {}
#endif // CONFIG_KB_HANDLER_SPLITLINK_MASTER
#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
__weak void splitlink_handler_settings_received(kb_settings_t *settings) {}
#endif // CONFIG_KB_HANDLER_SPLITLINK_SLAVE
__weak void splitlink_handler_on_connect() {}
__weak void splitlink_handler_on_disconnect() {}

int splitlink_handler_init() {

    ATOMIC_STORE(&connected, false);

    if (!device_is_ready(splitlink_dev)) {
        LOG_ERR("Splitlink device is not ready");
        return -ENODEV;
    }

#if CONFIG_KB_HANDLER_SPLITLINK_MASTER
    k_work_init(&values_rx_slot.work, rx_slot_work_handler);
    k_work_init(&settings_tx_slot.work, tx_slot_work_handler);
#endif // CONFIG_KB_HANDLER_SPLITLINK_MASTER

#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
    k_work_init(&settings_rx_slot.work, rx_slot_work_handler);
    k_work_init(&values_tx_slot.work, tx_slot_work_handler);
#endif // CONFIG_KB_HANDLER_SPLITLINK_SLAVE

    return 0;
}

static void on_connect(const struct device *dev) {
    if (dev->data == splitlink_dev->data) {
        ATOMIC_STORE(&connected, true);
        splitlink_handler_on_connect();
    }
}

static void on_disconnect(const struct device *dev) {
    if (dev->data == splitlink_dev->data) {
        ATOMIC_STORE(&connected, false);
        splitlink_handler_on_disconnect();
    }
}

SPLITLINK_CB_DEFINE(kb_handler_splitlink_ykb_protocol) = {
    .on_receive_cb = on_receive_cb,
    .connect_cb = on_connect,
    .disconnect_cb = on_disconnect,
};
