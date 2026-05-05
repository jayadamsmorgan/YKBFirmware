#ifndef KB_HANDLER_INTERNAL_H
#define KB_HANDLER_INTERNAL_H

#include <subsys/kb_handler.h>
#include <subsys/kb_settings.h>
#include <subsys/usb_connect.h>
#include <subsys/zephyr_user_helpers.h>

#include <drivers/kscan.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KEY_COUNT Z_USER_PROP(kb_handler_key_count)
#define KEY_COUNT_SLAVE Z_USER_PROP_OR(kb_handler_key_count_slave, 0)

size_t kb_handler_kscan_count(void);
const struct device *kb_handler_get_kscan(size_t idx);

int kb_handler_check_kscans_ready(void);
int kb_handler_validate_kscan_topology(uint16_t expected_key_count);

void kb_handler_transport_send_kb_report(
    hid_kb_report_t *report, enum kb_handler_transport_priority prio);
void kb_handler_transport_send_mouse_report(
    hid_mouse_report_t *report, enum kb_handler_transport_priority prio);

int kb_handler_core_init(void);
void kb_handler_core_handle_key_event(uint16_t key_index, bool pressed);
void kb_handler_core_handle_value(uint16_t key_index, uint16_t value);
void kb_handler_core_handle_slave_values(const uint16_t *values,
                                         uint16_t count);
void kb_handler_core_handle_slave_reset(void);
void kb_handler_core_get_values(uint16_t *values, uint16_t count);
int kb_handler_core_get_settings_snapshot(kb_settings_t *settings);

void kb_handler_impl_after_settings_update(const kb_settings_t *settings);

#endif // KB_HANDLER_INTERNAL_H
