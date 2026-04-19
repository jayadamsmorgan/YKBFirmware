#ifndef YKB_PROTOCOL_H
#define YKB_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// NOTE:
// Currently the fields are sent in native-endian, be careful

#define YKB_PROTOCOL_VERSION 1U

#ifndef YKB_PROTOCOL_MAX_PACKET_SIZE
#define YKB_PROTOCOL_MAX_PACKET_SIZE 64U
#endif

#ifdef __GNUC__
#define YKB_PACKED __attribute__((__packed__))
#elif defined(_MSC_VER)
#define YKB_PACKED
#pragma pack(push, 1)
#else
#define YKB_PACKED
#endif

#ifndef YKB_PROTOCOL_STATIC
#define YKB_PROTOCOL_STATIC static inline
#endif

// type_flags layout:
// bits 0..1 : type
// bit  2    : START
// bit  3    : END
// bit  4    : ACK_REQ
// bit  5    : RESERVED
// bit  6    : RESERVED
// bit  7    : RESERVED
#define YKB_PROTOCOL_TYPE_MASK 0x03u
#define YKB_PROTOCOL_FLAG_START 0x04u
#define YKB_PROTOCOL_FLAG_END 0x08u
#define YKB_PROTOCOL_FLAG_ACK_REQ 0x10u

typedef enum {
    YKB_PROTOCOL_TYPE_DATA = 0u,
    YKB_PROTOCOL_TYPE_ACK = 1u,
    YKB_PROTOCOL_TYPE_ABORT = 2u,
} ykb_protocol_type_t;

typedef enum {
    YKB_PROTOCOL_RX_RESULT_ACCEPTED = 0,
    YKB_PROTOCOL_RX_RESULT_COMPLETE,
    YKB_PROTOCOL_RX_RESULT_DUPLICATE,

    YKB_PROTOCOL_RX_ERROR_NULL = -1,
    YKB_PROTOCOL_RX_ERROR_BAD_VERSION = -2,
    YKB_PROTOCOL_RX_ERROR_BAD_TYPE = -3,
    YKB_PROTOCOL_RX_ERROR_BAD_PACKET_COUNT = -4,
    YKB_PROTOCOL_RX_ERROR_BAD_PACKET_INDEX = -5,
    YKB_PROTOCOL_RX_ERROR_BAD_TOTAL_LEN = -6,
    YKB_PROTOCOL_RX_ERROR_PACKET_CRC = -7,
    YKB_PROTOCOL_RX_ERROR_TRANSFER_MISMATCH = -8,
    YKB_PROTOCOL_RX_ERROR_BUFFER_TOO_SMALL = -9,
    YKB_PROTOCOL_RX_ERROR_OUT_OF_ORDER = -10,
    YKB_PROTOCOL_RX_ERROR_MISSING_BITMAP_REQUIRED = -11,
} ykb_protocol_rx_result_t;

typedef struct YKB_PACKED {
    uint16_t crc;
    uint8_t version;
    uint8_t type_flags;
    uint8_t transfer_id;
    uint16_t packet_idx;
    uint16_t packet_count;
    uint16_t total_len;
} ykb_protocol_header_t;

#define YKB_PROTOCOL_HEADER_SIZE ((uint16_t)sizeof(ykb_protocol_header_t))
#define YKB_PROTOCOL_MAX_PAYLOAD_SIZE                                          \
    (YKB_PROTOCOL_MAX_PACKET_SIZE - YKB_PROTOCOL_HEADER_SIZE)

#ifndef BUILD_ASSERT
#define BUILD_ASSERT(EXPR, MSG...) _Static_assert((EXPR), "" MSG)
#endif // BUILD_ASSERT

BUILD_ASSERT(YKB_PROTOCOL_MAX_PACKET_SIZE > sizeof(ykb_protocol_header_t),
             "YKB_PROTOCOL_MAX_PACKET_SIZE must be greater than "
             "sizeof(ykb_protocol_header_t).");

typedef struct YKB_PACKED {
    ykb_protocol_header_t header;
    uint8_t payload[YKB_PROTOCOL_MAX_PAYLOAD_SIZE];
} ykb_protocol_packet_t;

typedef struct {
    const uint8_t *data;
    uint16_t total_len;
    uint16_t offset;
    uint8_t transfer_id;
    uint16_t packet_count;
    uint16_t next_packet_idx;
    uint8_t type_flags_base;
} ykb_protocol_tx_state_t;

typedef struct {
    uint8_t *buffer;
    uint16_t capacity;

    bool allow_out_of_order;
    bool active;
    bool complete;

    uint8_t transfer_id;
    uint16_t total_len;
    uint16_t received_bytes;

    uint16_t packet_count;
    uint16_t received_count;
    uint16_t next_expected_packet_idx;

    uint8_t type_flags_base;

    uint8_t *received_bitmap;
    size_t received_bitmap_size;
} ykb_protocol_rx_state_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

YKB_PROTOCOL_STATIC uint16_t ykb_protocol_crc16_update(uint16_t crc,
                                                       const uint8_t *data,
                                                       size_t length) {
    const uint16_t polynomial = 0xA001u;

    if (data == NULL || length == 0u) {
        return crc;
    }

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1u) ^ polynomial);
            } else {
                crc = (uint16_t)(crc >> 1u);
            }
        }
    }

    return crc;
}

YKB_PROTOCOL_STATIC uint16_t ykb_protocol_crc16(const uint8_t *data,
                                                size_t length) {
    return ykb_protocol_crc16_update(0xFFFFu, data, length);
}

YKB_PROTOCOL_STATIC uint8_t ykb_protocol_get_type(uint8_t type_flags) {
    return (uint8_t)(type_flags & YKB_PROTOCOL_TYPE_MASK);
}

YKB_PROTOCOL_STATIC bool ykb_protocol_has_flag(uint8_t type_flags,
                                               uint8_t flag) {
    return (type_flags & flag) != 0u;
}

YKB_PROTOCOL_STATIC uint16_t
ykb_protocol_calc_packet_count(uint16_t total_len) {
    if (total_len == 0u) {
        return 1u;
    }

    return (uint16_t)((total_len + YKB_PROTOCOL_MAX_PAYLOAD_SIZE - 1u) /
                      YKB_PROTOCOL_MAX_PAYLOAD_SIZE);
}

YKB_PROTOCOL_STATIC size_t
ykb_protocol_bitmap_size_bytes(uint16_t packet_count) {
    return (size_t)((packet_count + 7u) / 8u);
}

YKB_PROTOCOL_STATIC bool ykb_protocol_bitmap_test(const uint8_t *bitmap,
                                                  uint16_t idx) {
    return (bitmap[idx / 8u] & (uint8_t)(1u << (idx % 8u))) != 0u;
}

YKB_PROTOCOL_STATIC void ykb_protocol_bitmap_set(uint8_t *bitmap,
                                                 uint16_t idx) {
    bitmap[idx / 8u] |= (uint8_t)(1u << (idx % 8u));
}

YKB_PROTOCOL_STATIC void ykb_protocol_bitmap_clear_all(uint8_t *bitmap,
                                                       size_t bitmap_size) {
    if (bitmap != NULL && bitmap_size > 0u) {
        memset(bitmap, 0, bitmap_size);
    }
}

YKB_PROTOCOL_STATIC uint16_t ykb_protocol_payload_len_for_index(
    uint16_t total_len, uint16_t packet_idx, uint16_t packet_count) {
    uint16_t used;

    if (packet_count == 0u || packet_idx >= packet_count) {
        return 0u;
    }

    if (total_len == 0u) {
        return 0u;
    }

    if (packet_count == 1u) {
        return total_len;
    }

    if (packet_idx < (uint16_t)(packet_count - 1u)) {
        return (uint16_t)YKB_PROTOCOL_MAX_PAYLOAD_SIZE;
    }

    used = (uint16_t)((packet_count - 1u) * YKB_PROTOCOL_MAX_PAYLOAD_SIZE);
    return (uint16_t)(total_len - used);
}

YKB_PROTOCOL_STATIC bool
ykb_protocol_is_header_valid(const ykb_protocol_header_t *h) {
    if (h == NULL) {
        return false;
    }

    if (h->version != YKB_PROTOCOL_VERSION) {
        return false;
    }

    if (ykb_protocol_get_type(h->type_flags) > YKB_PROTOCOL_TYPE_ABORT) {
        return false;
    }

    if (h->packet_count == 0u) {
        return false;
    }

    if (h->packet_idx >= h->packet_count) {
        return false;
    }

    if (h->total_len == 0u) {
        return h->packet_count == 1u && h->packet_idx == 0u;
    }

    return ykb_protocol_calc_packet_count(h->total_len) == h->packet_count;
}

YKB_PROTOCOL_STATIC uint16_t
ykb_protocol_compute_packet_crc(const ykb_protocol_packet_t *packet) {
    ykb_protocol_header_t hdr;
    uint16_t payload_len;
    uint16_t crc;

    if (packet == NULL) {
        return 0u;
    }

    hdr = packet->header;
    hdr.crc = 0u;

    payload_len = ykb_protocol_payload_len_for_index(
        hdr.total_len, hdr.packet_idx, hdr.packet_count);

    crc = 0xFFFFu;
    crc = ykb_protocol_crc16_update(crc, (const uint8_t *)&hdr, sizeof(hdr));
    crc = ykb_protocol_crc16_update(crc, packet->payload, payload_len);

    return crc;
}

YKB_PROTOCOL_STATIC void ykb_protocol_tx_init(ykb_protocol_tx_state_t *tx,
                                              const void *data,
                                              uint16_t total_len,
                                              uint8_t transfer_id,
                                              uint8_t type_flags_base) {
    if (tx == NULL) {
        return;
    }

    tx->data = (const uint8_t *)data;
    tx->total_len = total_len;
    tx->offset = 0u;
    tx->transfer_id = transfer_id;
    tx->packet_count = ykb_protocol_calc_packet_count(total_len);
    tx->next_packet_idx = 0u;
    tx->type_flags_base =
        (uint8_t)(type_flags_base &
                  (uint8_t)~(YKB_PROTOCOL_FLAG_START | YKB_PROTOCOL_FLAG_END));
}

YKB_PROTOCOL_STATIC bool
ykb_protocol_tx_has_more(const ykb_protocol_tx_state_t *tx) {
    return (tx != NULL) && (tx->next_packet_idx < tx->packet_count);
}

YKB_PROTOCOL_STATIC bool
ykb_protocol_tx_build_packet(ykb_protocol_tx_state_t *tx,
                             ykb_protocol_packet_t *out_packet) {
    uint16_t idx;
    uint16_t payload_len;
    uint8_t type_flags;

    if (tx == NULL || out_packet == NULL) {
        return false;
    }

    if (!ykb_protocol_tx_has_more(tx)) {
        return false;
    }

    memset(out_packet, 0, sizeof(*out_packet));

    idx = tx->next_packet_idx;
    payload_len = ykb_protocol_payload_len_for_index(tx->total_len, idx,
                                                     tx->packet_count);

    type_flags = tx->type_flags_base;
    if (idx == 0u) {
        type_flags |= YKB_PROTOCOL_FLAG_START;
    }
    if (idx == (uint16_t)(tx->packet_count - 1u)) {
        type_flags |= YKB_PROTOCOL_FLAG_END;
    }

    out_packet->header.crc = 0u;
    out_packet->header.version = YKB_PROTOCOL_VERSION;
    out_packet->header.type_flags = type_flags;
    out_packet->header.transfer_id = tx->transfer_id;
    out_packet->header.packet_idx = idx;
    out_packet->header.packet_count = tx->packet_count;
    out_packet->header.total_len = tx->total_len;

    if (payload_len > 0u && tx->data != NULL) {
        memcpy(out_packet->payload, &tx->data[tx->offset], payload_len);
        tx->offset = (uint16_t)(tx->offset + payload_len);
    }

    out_packet->header.crc = ykb_protocol_compute_packet_crc(out_packet);
    tx->next_packet_idx++;

    return true;
}

YKB_PROTOCOL_STATIC void ykb_protocol_rx_reset(ykb_protocol_rx_state_t *rx) {
    if (rx == NULL) {
        return;
    }

    rx->active = false;
    rx->complete = false;
    rx->transfer_id = 0u;
    rx->total_len = 0u;
    rx->received_bytes = 0u;
    rx->packet_count = 0u;
    rx->received_count = 0u;
    rx->next_expected_packet_idx = 0u;
    rx->type_flags_base = 0u;

    ykb_protocol_bitmap_clear_all(rx->received_bitmap,
                                  rx->received_bitmap_size);
}

YKB_PROTOCOL_STATIC void ykb_protocol_rx_init(ykb_protocol_rx_state_t *rx,
                                              void *buffer, uint16_t capacity,
                                              bool allow_out_of_order,
                                              uint8_t *received_bitmap,
                                              size_t received_bitmap_size) {
    if (rx == NULL) {
        return;
    }

    rx->buffer = (uint8_t *)buffer;
    rx->capacity = capacity;
    rx->allow_out_of_order = allow_out_of_order;
    rx->received_bitmap = received_bitmap;
    rx->received_bitmap_size = received_bitmap_size;

    ykb_protocol_rx_reset(rx);
}

YKB_PROTOCOL_STATIC ykb_protocol_rx_result_t ykb_protocol_rx_begin_transfer(
    ykb_protocol_rx_state_t *rx, const ykb_protocol_packet_t *packet) {
    size_t needed_bitmap_size;

    if (rx == NULL || packet == NULL) {
        return YKB_PROTOCOL_RX_ERROR_NULL;
    }

    if (packet->header.total_len > rx->capacity) {
        return YKB_PROTOCOL_RX_ERROR_BUFFER_TOO_SMALL;
    }

    if (rx->allow_out_of_order) {
        needed_bitmap_size =
            ykb_protocol_bitmap_size_bytes(packet->header.packet_count);
        if (rx->received_bitmap == NULL ||
            rx->received_bitmap_size < needed_bitmap_size) {
            return YKB_PROTOCOL_RX_ERROR_MISSING_BITMAP_REQUIRED;
        }
        ykb_protocol_bitmap_clear_all(rx->received_bitmap,
                                      rx->received_bitmap_size);
    }

    rx->active = true;
    rx->complete = false;
    rx->transfer_id = packet->header.transfer_id;
    rx->total_len = packet->header.total_len;
    rx->received_bytes = 0u;
    rx->packet_count = packet->header.packet_count;
    rx->received_count = 0u;
    rx->next_expected_packet_idx = 0u;
    rx->type_flags_base =
        (uint8_t)(packet->header.type_flags &
                  (uint8_t)~(YKB_PROTOCOL_FLAG_START | YKB_PROTOCOL_FLAG_END));

    return YKB_PROTOCOL_RX_RESULT_ACCEPTED;
}

YKB_PROTOCOL_STATIC bool
ykb_protocol_rx_is_complete(const ykb_protocol_rx_state_t *rx) {
    return (rx != NULL) ? rx->complete : false;
}

YKB_PROTOCOL_STATIC ykb_protocol_rx_result_t ykb_protocol_rx_push_packet(
    ykb_protocol_rx_state_t *rx, const ykb_protocol_packet_t *packet) {
    uint16_t payload_len;
    uint16_t crc;
    uint16_t offset;
    bool already_received = false;
    uint8_t packet_type_flags_base;

    if (rx == NULL || packet == NULL) {
        return YKB_PROTOCOL_RX_ERROR_NULL;
    }

    if (!ykb_protocol_is_header_valid(&packet->header)) {
        if (packet->header.version != YKB_PROTOCOL_VERSION) {
            return YKB_PROTOCOL_RX_ERROR_BAD_VERSION;
        }
        if (ykb_protocol_get_type(packet->header.type_flags) >
            YKB_PROTOCOL_TYPE_ABORT) {
            return YKB_PROTOCOL_RX_ERROR_BAD_TYPE;
        }
        if (packet->header.packet_count == 0u) {
            return YKB_PROTOCOL_RX_ERROR_BAD_PACKET_COUNT;
        }
        if (packet->header.packet_idx >= packet->header.packet_count) {
            return YKB_PROTOCOL_RX_ERROR_BAD_PACKET_INDEX;
        }
        return YKB_PROTOCOL_RX_ERROR_BAD_TOTAL_LEN;
    }

    if (ykb_protocol_get_type(packet->header.type_flags) !=
        YKB_PROTOCOL_TYPE_DATA) {
        return YKB_PROTOCOL_RX_ERROR_BAD_TYPE;
    }

    crc = ykb_protocol_compute_packet_crc(packet);
    if (crc != packet->header.crc) {
        return YKB_PROTOCOL_RX_ERROR_PACKET_CRC;
    }

    if (!rx->active) {
        ykb_protocol_rx_result_t begin_res =
            ykb_protocol_rx_begin_transfer(rx, packet);
        if (begin_res < 0) {
            return begin_res;
        }
    } else {
        packet_type_flags_base = (uint8_t)(packet->header.type_flags &
                                           (uint8_t)~(YKB_PROTOCOL_FLAG_START |
                                                      YKB_PROTOCOL_FLAG_END));

        if (rx->transfer_id != packet->header.transfer_id ||
            rx->total_len != packet->header.total_len ||
            rx->packet_count != packet->header.packet_count ||
            rx->type_flags_base != packet_type_flags_base) {
            return YKB_PROTOCOL_RX_ERROR_TRANSFER_MISMATCH;
        }
    }

    payload_len = ykb_protocol_payload_len_for_index(
        rx->total_len, packet->header.packet_idx, rx->packet_count);

    if (!rx->allow_out_of_order) {
        if (packet->header.packet_idx != rx->next_expected_packet_idx) {
            return YKB_PROTOCOL_RX_ERROR_OUT_OF_ORDER;
        }
    }

    if (rx->allow_out_of_order) {
        already_received = ykb_protocol_bitmap_test(rx->received_bitmap,
                                                    packet->header.packet_idx);
        if (already_received) {
            return YKB_PROTOCOL_RX_RESULT_DUPLICATE;
        }
    }

    offset =
        (uint16_t)(packet->header.packet_idx * YKB_PROTOCOL_MAX_PAYLOAD_SIZE);

    if ((uint32_t)offset + payload_len > rx->capacity) {
        return YKB_PROTOCOL_RX_ERROR_BUFFER_TOO_SMALL;
    }

    if (payload_len > 0u) {
        memcpy(&rx->buffer[offset], packet->payload, payload_len);
    }

    rx->received_bytes = (uint16_t)(rx->received_bytes + payload_len);
    rx->received_count++;

    if (rx->allow_out_of_order) {
        ykb_protocol_bitmap_set(rx->received_bitmap, packet->header.packet_idx);
    } else {
        rx->next_expected_packet_idx++;
    }

    if (rx->received_count == rx->packet_count) {
        if (rx->received_bytes != rx->total_len) {
            return YKB_PROTOCOL_RX_ERROR_BAD_TOTAL_LEN;
        }

        rx->complete = true;
        return YKB_PROTOCOL_RX_RESULT_COMPLETE;
    }

    return YKB_PROTOCOL_RX_RESULT_ACCEPTED;
}

YKB_PROTOCOL_STATIC uint16_t
ykb_protocol_rx_collect_missing(const ykb_protocol_rx_state_t *rx,
                                uint16_t *out_indices, uint16_t max_indices) {
    uint16_t count = 0u;

    if (rx == NULL || out_indices == NULL || max_indices == 0u) {
        return 0u;
    }

    if (!rx->allow_out_of_order || rx->received_bitmap == NULL) {
        return 0u;
    }

    for (uint16_t i = 0u; i < rx->packet_count; i++) {
        if (!ykb_protocol_bitmap_test(rx->received_bitmap, i)) {
            out_indices[count++] = i;
            if (count == max_indices) {
                break;
            }
        }
    }

    return count;
}

#endif // YKB_PROTOCOL_H
