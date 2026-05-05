#ifndef LIB_USB_CONNECT_H
#define LIB_USB_CONNECT_H

#include <stdint.h>
#include <zephyr/sys/iterable_sections.h>

struct usb_connect_cb {
    void (*on_connect)(void);
    void (*on_disconnect)(void);
};

#define USB_CONNECT_CB_DEFINE(name)                                            \
    static STRUCT_SECTION_ITERABLE(usb_connect_cb, __usb_connect_cb__##name)

typedef struct __packed {
    uint8_t mods;
    uint8_t reserved;
    uint8_t keys[6];
} hid_kb_report_t;

typedef struct __packed {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} hid_mouse_report_t;

void usb_connect_handle_wakeup(void);

bool usb_connect_can_send_kb_report(void);
bool usb_connect_can_send_mouse_report(void);
void usb_connect_send_kb_report(const hid_kb_report_t *report);
void usb_connect_send_mouse_report(const hid_mouse_report_t *report);

#endif // LIB_USB_CONNECT_H
