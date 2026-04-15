#ifndef YKB_PROTOCOL_H
#define YKB_PROTOCOL_H

#include <stdint.h>
#include <string.h>

#define YKB_PROTOCOL_VERSION 1U

#ifndef YKB_PROTOCOL_DATA_LENGTH
// Length of data buffer per one transfer
#define YKB_PROTOCOL_DATA_LENGTH 57U
#endif // YKB_PROTOCOL_DATA_LENGTH

/* Errors */

#define YKB_PARSE_ERR_DATA_NULL -1
#define YKB_PARSE_ERR_BAD_VERSION -2
#define YKB_PARSE_ERR_BAD_REQUEST -3
#define YKB_PARSE_ERR_PACKET_COUNT_INVALID -4
#define YKB_PARSE_ERR_PACKET_NUMBER_INVALID -5
#define YKB_PARSE_ERR_PACKET_SIZE_INVALID -6
#define YKB_PARSE_ERR_CRC_INVALID -7

#define YKB_PARSE_CONT_ERR_BADARGS -11
#define YKB_PARSE_CONT_ERR_OVERFLOW -12

#define YKB_PACK_ERR_BADARGS -1
#define YKB_PACK_ERR_NOMEM -2

/* Requests */

#define YKB_REQUEST_NONE 0U

#define YKB_REQUEST_SET_SETTINGS_OK (4U << 4)
#define YKB_REQUEST_

/* Request errors */

#define YKB_REQUEST_ERROR_PARSING (10U << 4)
#define YKB_REQUEST_ERROR_OUT_OF_MEM (11U << 4)
#define YKB_REQUEST_ERROR_UNKNOWN (12U << 4)

#define IS_YKB_REQUEST(ARG)                                                    \
    ((ARG == YKB_REQUEST_NONE) || IS_YKB_GET_REQUEST(ARG) ||                   \
     IS_YKB_SET_REQUEST(ARG) || IS_YKB_REQUEST_ERROR(ARG))

#define IS_YKB_GET_REQUEST(ARG)                                                \
    ((ARG == YKB_REQUEST_GET_SETTINGS) || (ARG == YKB_REQUEST_GET_MAPPINGS) || \
     (ARG == YKB_REQUEST_GET_VALUES) || (ARG == YKB_REQUEST_GET_THRESHOLDS))

#define IS_YKB_SET_REQUEST(ARG)                                                \
    ((ARG == YKB_REQUEST_SET_MAPPINGS) || (ARG == YKB_REQUEST_SET_SETTINGS) || \
     (ARG == YKB_REQUEST_SET_THRESHOLDS) ||                                    \
     (ARG == YKB_REQUEST_FIRMWARE_UPDATE) ||                                   \
     (ARG == YKB_REQUEST_BOOTLOADER_UPDATE))

#define IS_YKB_REQUEST_ERROR(ARG)                                              \
    ((ARG == YKB_REQUEST_ERROR_PARSING) ||                                     \
     (ARG == YKB_REQUEST_ERROR_OUT_OF_MEM) ||                                  \
     (ARG == YKB_REQUEST_ERROR_UNKNOWN))

#ifdef __GNUC__
#define YKB_PACKED __attribute__((__packed__))
#elif defined(_MSC_VER)
#define YKB_PACKED
#pragma pack(push, 1) // For MSVC
#else
#define YKB_PACKED
#warning                                                                       \
    "Packed attribute not defined for this compiler, struct padding might occur"
#endif

typedef struct YKB_PACKED {

    uint8_t request_and_version;
    // [0-3] Protocol request
    // [4-7] Protocol version. Current version: 1

    uint8_t packet_size; // Amount of bytes in data array

    uint16_t packet_number; // Current packet index.

    uint16_t crc; // CRC of data

    uint8_t data[YKB_PROTOCOL_DATA_LENGTH];

} ykb_protocol_t;

#ifdef _MSC_VER
#pragma pack(pop) // Restore packing for MSVC
#endif

static inline uint16_t ykb_crc16(const uint8_t *data, size_t length) {

    uint16_t crc = 0xFFFF;

    if (!data || length == 0) {
        return crc;
    }

    const uint16_t polynomial = 0xA001;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

// Get the minimum protocol pack size.
//
// Pack size is always >= 1
static inline uint16_t ykb_protocol_get_pack_size(uint32_t data_length) {
    return (data_length / YKB_PROTOCOL_DATA_LENGTH) + 1;
}

// Packs the data array with the length of `data_length`
// into the `result` protocol array.
//
// The caller is responsive for allocating memory for the result array.
// Size of the allocation:
// `ykb_protocol_get_pack_size(data_length) * sizeof(ykb_protocol_t)`
static inline int ykb_protocol_pack(ykb_protocol_t *result, uint8_t request,
                                    const uint8_t *data, uint32_t data_length) {

    if (!result || !IS_YKB_REQUEST(request)) {
        return YKB_PACK_ERR_BADARGS;
    }

    if (IS_YKB_GET_REQUEST(request) && (!data || data_length == 0)) {
        ykb_protocol_t p = {.data = {0},
                            .request_and_version =
                                request | YKB_PROTOCOL_VERSION,
                            .packet_number = 0,
                            .packet_size = 0,
                            .crc = 0};
        p.crc = ykb_crc16(p.data, 0);
        memcpy(result, &p, sizeof(ykb_protocol_t));
        return 0;
    }

    uint16_t total_packets = ykb_protocol_get_pack_size(data_length);

    for (uint16_t i = 0; i < total_packets; i++) {
        ykb_protocol_t p;

        p.request_and_version = request | YKB_PROTOCOL_VERSION;
        p.packet_number = i;

        uint16_t offset = i * YKB_PROTOCOL_DATA_LENGTH;
        uint16_t bytes_left =
            (data_length > offset) ? (data_length - offset) : 0;
        if (bytes_left > YKB_PROTOCOL_DATA_LENGTH) {
            bytes_left = YKB_PROTOCOL_DATA_LENGTH;
        }

        p.packet_size = bytes_left;

        if (data) {
            if (bytes_left > 0) {
                memcpy(p.data, data + offset, bytes_left);
            }
            if (bytes_left < YKB_PROTOCOL_DATA_LENGTH) {
                memset(p.data + bytes_left, 0,
                       YKB_PROTOCOL_DATA_LENGTH - bytes_left);
            }
        }

        p.crc = ykb_crc16(p.data, bytes_left);

        memcpy(result + (sizeof(ykb_protocol_t) * i), &p,
               sizeof(ykb_protocol_t));
    }

    return 0;
}

// Parses ykb_protocol from `data` buffer.
//
// `previous` should be NULL if first packet expected.
//
// Returns 0 on success.
static inline int ykb_protocol_parse(ykb_protocol_t *next,
                                     const uint8_t *data) {

    if (!data) {
        return YKB_PARSE_ERR_DATA_NULL;
    }

    ykb_protocol_t new_packet;
    memcpy(&new_packet, data, sizeof(new_packet));

    uint8_t version = new_packet.request_and_version & 0x0F;
    if (version != YKB_PROTOCOL_VERSION) {
        return YKB_PARSE_ERR_BAD_VERSION;
    }

    uint8_t request = new_packet.request_and_version & 0xF0;
    if (!IS_YKB_REQUEST(request)) {
        return YKB_PARSE_ERR_BAD_REQUEST;
    }

    if (new_packet.packet_size > YKB_PROTOCOL_DATA_LENGTH) {
        return YKB_PARSE_ERR_PACKET_SIZE_INVALID;
    }

    uint16_t crc = ykb_crc16(new_packet.data, new_packet.packet_size);
    if (crc != new_packet.crc) {
        return YKB_PARSE_ERR_CRC_INVALID;
    }

    if (next) {
        *next = new_packet;
    }

    return 0;
}

#endif // YKB_PROTOCOL_H
