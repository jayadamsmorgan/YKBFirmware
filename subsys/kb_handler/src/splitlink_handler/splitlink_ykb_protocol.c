#include "splitlink_handler.h"

#include <lib/ykb_protocol.h>

#include <drivers/splitlink.h>

#include <zephyr/logging/log.h>

#if CONFIG_KB_HANDLER_SPLITLINK_MASTER
LOG_MODULE_DECLARE(kb_handler_sm);
#endif // CONFIG_KB_HANDLER_SPLITLINK_MASTER
#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
LOG_MODULE_DECALRE(kb_handler_ss);
#endif // CONFIG_KB_HANDLER_SPLITLINK_SLAVE

#ifndef CONFIG_KB_HANDLER_SL_RX_BUFFER_LENGTH
#define CONFIG_KB_HANDLER_SL_RX_BUFFER_LENGTH 256
#endif // CONFIG_KB_HANDLER_SL_RX_BUFFER_LENGTH

#if CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK

#ifndef CONFIG_KB_HANDLER_SL_RX_BITMAP_LENGTH
#define CONFIG_KB_HANDLER_SL_RX_BITMAP_LENGTH 32
#endif // CONFIG_KB_HANDLER_SL_RX_BITMAP_LENGTH

static uint8_t rx_bitmap[CONFIG_KB_HANDLER_SL_RX_BITMAP_LENGTH] = {0};

#endif // CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK

#ifndef CONFIG_KB_HANDLER_SL_TX_BUFFER_LENGTH
#define CONFIG_KB_HANDLER_SL_TX_BUFFER_LENGTH 256
#endif // CONFIG_KB_HANDLER_SL_TX_BUFFER_LENGTH

#ifndef CONFIG_KB_HANDLER_SL_RX_SLOT_COUNT
#define CONFIG_KB_HANDLER_SL_RX_SLOT_COUNT 2
#endif // CONFIG_KB_HANDLER_SL_RX_SLOT_COUNT

#ifndef CONFIG_KB_HANDLER_SL_TX_SLOT_COUNT
#define CONFIG_KB_HANDLER_SL_TX_SLOT_COUNT 2
#endif // CONFIG_KB_HANDLER_SL_TX_SLOT_COUNT

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
    uint8_t data[CONFIG_KB_HANDLER_SL_RX_BUFFER_LENGTH];
#if CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
    uint8_t bitmap[RX_BITMAP_SIZE];
#endif // CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
    ykb_protocol_rx_state_t rx;
};

struct tx_slot {
    struct k_work work;
    enum tx_slot_state state;
    uint8_t data[CONFIG_KB_HANDLER_SL_TX_BUFFER_LENGTH];
    ykb_protocol_tx_state_t tx;
    uint8_t id;
};

struct rx_slot rx_slots[CONFIG_KB_HANDLER_SL_RX_SLOT_COUNT];
struct tx_slot tx_slots[CONFIG_KB_HANDLER_SL_TX_SLOT_COUNT];

const struct device *splitlink_dev = NULL;

static inline void rx_init(struct rx_slot *slot) {
#if CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
    ykb_protocol_rx_init(&slot->rx, slot->data, sizeof(slot->data) true,
                         slot->bitmap, sizeof(slot->bitmap));
#else
    ykb_protocol_rx_init(&slot->rx, slot->data, sizeof(slot->data), false, NULL,
                         0);
#endif // CONFIG_KB_HANDLER_SL_OUT_OF_ORDER_TRACK
}

#define DATA_VALUES 1U
#define DATA_SETTINGS 2U
#define DATA_LUMISCRIPT 3U

void rx_slot_work_handler(struct k_work *work) {
    struct rx_slot *slot = CONTAINER_OF(work, struct rx_slot, work);
    slot->state = RX_SLOT_PROCESSING;

    uint8_t data_type = slot->data[0];
    if (data_type == DATA_VALUES) {
        // We got values probably from slave device
        uint16_t count = (slot->data[1] << 8) | slot->data[2];
        uint16_t *values = (uint16_t *)&slot->data[3];
        uint16_t expected_bytes = count * 2 + 3;
        if (expected_bytes > slot->rx.total_len) {
            LOG_ERR("Values packet malformed expected %d got %d bytes",
                    expected_bytes, slot->rx.total_len);
        } else {
            splitlink_handler_values_received(values, count);
        }
    } else if (data_type == DATA_SETTINGS) {
        // We got settings update from master device
        // TODO
    } else if (data_type == DATA_LUMISCRIPT) {
        // Got lumiscript storage update from master device
        // TODO
    } else {
        LOG_ERR("Unknown data packet with data_type byte %d", data_type);
    }

    slot->state = RX_SLOT_EMPTY;
}

void tx_slot_work_handler(struct k_work *work) {
    struct tx_slot *slot = CONTAINER_OF(work, struct tx_slot, work);

    slot->state = TX_SLOT_TRANSCEIVING;

    ykb_protocol_packet_t packet;
    int err;
    ykb_protocol_tx_init(&slot->tx, slot->data, slot->tx.total_len, slot->id,
                         YKB_PROTOCOL_TYPE_DATA);
    while (ykb_protocol_tx_has_more(&slot->tx)) {
        if (!ykb_protocol_tx_build_packet(&slot->tx, &packet)) {
            LOG_ERR("ykb_protocol_tx_build_packet failed");
            break;
        }
        err = splitlink_send(splitlink_dev, (uint8_t *)&packet, sizeof(packet));
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

    if (data_len > sizeof(ykb_protocol_packet_t)) {
        LOG_ERR("packet data_len > sizeof(ykb_protocol_packet_t), skipping");
        return;
    }

    ykb_protocol_packet_t packet;
    memcpy(&packet, data, data_len);
    uint8_t id = packet.header.transfer_id;
    if (id >= ARRAY_SIZE(rx_slots)) {
        LOG_ERR("packet transfer id is out of bounds, skipping");
        return;
    }

    ykb_protocol_rx_result_t res;

    struct rx_slot *slot = &rx_slots[id];
    switch (slot->state) {
    case RX_SLOT_EMPTY:
        rx_init(slot);
        res = ykb_protocol_rx_begin_transfer(&slot->rx, &packet);
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
        return;
    }

    if (res == YKB_PROTOCOL_RX_RESULT_COMPLETE) {
        slot->state = RX_SLOT_READY;
        k_work_submit(&slot->work);
    }
}

static inline struct tx_slot *free_tx_slot(void) {
    struct tx_slot *slot = NULL;
    for (size_t i = 0; i < CONFIG_KB_HANDLER_SL_TX_SLOT_COUNT; ++i) {
        if (tx_slots[i].state == TX_SLOT_EMPTY) {
            slot = &tx_slots[i];
            break;
        }
    }
    return slot;
}

void splitlink_handler_send_values(uint16_t *values, uint16_t count) {
    struct tx_slot *slot = free_tx_slot();
    if (!slot) {
        LOG_ERR("No available tx slots at the moment, dropping value packet");
        return;
    }
    if (count * sizeof(uint16_t) > sizeof(slot->data)) {
        LOG_ERR("TX slot overflow (got %d, max %d)", count * sizeof(uint16_t),
                sizeof(slot->data));
        return;
    }

    slot->tx.total_len = count;
    memcpy(slot->data, values, count * sizeof(uint16_t));
    k_work_submit(&slot->work);
}

void splitlink_handler_send_settings(kb_settings_t *settings) {
    struct tx_slot *slot = free_tx_slot();
    if (!slot) {
        LOG_ERR(
            "No available tx slots at the moment, dropping settings packet");
        return;
    }
    // TODO
}

__weak void splitlink_handler_values_received(uint16_t *values,
                                              uint16_t count) {}

__weak void splitlink_handler_settings_received(kb_settings_t *settings) {}

SPLITLINK_CB_DEFINE(kb_handler_splitlink_ykb_protocol) = {
    .on_receive_cb = on_receive_cb,
};

int splitlink_handler_init(void) {
    for (size_t i = 0; i < CONFIG_KB_HANDLER_SL_RX_SLOT_COUNT; ++i) {
        memset(rx_slots[i].data, 0, sizeof(rx_slots[i].data));
        rx_slots[i].state = RX_SLOT_EMPTY;
        k_work_init(&rx_slots[i].work, rx_slot_work_handler);
    }
    for (size_t i = 0; i < CONFIG_KB_HANDLER_SL_TX_SLOT_COUNT; ++i) {
        memset(tx_slots[i].data, 0, sizeof(tx_slots[i].data));
        tx_slots[i].state = TX_SLOT_EMPTY;
        k_work_init(&tx_slots[i].work, tx_slot_work_handler);
        tx_slots[i].id = i;
    }

    return 0;
}
