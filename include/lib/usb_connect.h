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

void usb_connect_send_report(uint8_t buffer[8]);

#endif // LIB_USB_CONNECT_H
