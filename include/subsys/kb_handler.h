#ifndef __SUBSYS_KB_HANDLER_H_
#define __SUBSYS_KB_HANDLER_H_

#include <subsys/kb_settings.h>
#include <subsys/usb_connect.h>

#include <zephyr/sys/iterable_sections.h>
#include <zephyr/toolchain.h>

struct kb_handler_transport_cb {
    void (*on_kb_report_ready)(const hid_kb_report_t *const report);
    void (*on_mouse_report_ready)(const hid_mouse_report_t *const report);
};

#define KB_HANDLER_TRANSPORT_CB_DEFINE(name)                                   \
    STRUCT_SECTION_ITERABLE(kb_handler_transport_cb, name)

void kb_handler_get_values(uint16_t *values, uint16_t count);

int kb_handler_get_default_thresholds(uint16_t *buffer);

int kb_handler_get_default_keymap_layer1(uint8_t *buffer);
int kb_handler_get_default_keymap_layer2(uint8_t *buffer);
int kb_handler_get_default_keymap_layer3(uint8_t *buffer);

int kb_handler_get_default_mouseemu(kb_mouseemu_settings_t *buffer);

#endif // __SUBSYS_KB_HANDLER_H_
