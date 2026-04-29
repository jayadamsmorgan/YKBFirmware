#ifndef VENDOR_HID_PROTOCOL_H
#define VENDOR_HID_PROTOCOL_H

#include <lib/features.h>

#include <subsys/kb_settings.h>

#include <zephyr/sys/util_macro.h>

#include <stddef.h>
#include <stdint.h>

enum request_type {
    REQUEST_GET_FEATURES = 0U,
    REQUEST_GET_VALUES = 1U,
    REQUEST_GET_SETTINGS = 2U,
    REQUEST_SET_SETTINGS = 3U,
};

enum response_type {
    RESPONSE_GET_FEATURES = 0U,
    RESPONSE_GET_VALUES = 1U,
    RESPONSE_GET_SETTINGS = 2U,
    RESPONSE_SET_SETTINGS_OK = 3U,

    RESPONSE_ERROR = 255U,
};

#define MAX_PACKET_LEN (sizeof(kb_settings_t))

typedef struct __packed {
    uint8_t type;
} vendor_hid_proto_request_header_t;

typedef struct __packed {
    vendor_hid_proto_request_header_t header;
    uint8_t data[MAX_PACKET_LEN];
} vendor_hid_proto_packet_t;

int vendor_hid_protocol_parse(const uint8_t *const data, size_t data_len);

int vendor_hid_protocol_init(void);

device_features *vendor_hid_protocol_get_features(void);

kb_settings_t *vendor_hid_protocol_get_settings(void);

void vendor_hid_protocol_get_values(uint16_t *values, uint16_t count);

int vendor_hid_protocol_set_settings(kb_settings_t *settings);

#endif // VENDOR_HID_PROTOCOL_H
