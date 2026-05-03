#ifndef VENDOR_HID_PROTOCOL_H
#define VENDOR_HID_PROTOCOL_H

#include <lib/features.h>
#include <lib/ykb_protocol.h>

#include <subsys/kb_settings.h>

#include <zephyr/kernel.h>

#include <stdbool.h>
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

typedef int (*vendor_hid_send_packet_cb_t)(const uint8_t *data, size_t len,
                                           void *user_data);

typedef struct {
    uint8_t rx_buffer[sizeof(vendor_hid_proto_packet_t)];
    ykb_protocol_rx_state_t rx;
    struct k_work response_work;
    enum response_type current_response;
    vendor_hid_send_packet_cb_t send_packet;
    void *user_data;
    bool busy;
} vendor_hid_protocol_ctx_t;

int vendor_hid_protocol_init(vendor_hid_protocol_ctx_t *ctx,
                             vendor_hid_send_packet_cb_t send_packet,
                             void *user_data);

int vendor_hid_protocol_parse(vendor_hid_protocol_ctx_t *ctx,
                              const uint8_t *data, size_t len);

#endif // VENDOR_HID_PROTOCOL_H
