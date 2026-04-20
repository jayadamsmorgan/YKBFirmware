#ifndef HID_DEVICES_H__
#define HID_DEVICES_H__

#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usbd_hid.h>

#include <stdatomic.h>

#define ATOMIC_STORE(var, val)                                                 \
    atomic_store_explicit(var, val, memory_order_relaxed)
#define ATOMIC_LOAD(var) atomic_load_explicit(var, memory_order_relaxed)

int usb_connect_init_kbd_hid(void);
int usb_connect_init_mouse_hid(void);
int usb_connect_init_vendor_hid(void);

#endif // HID_DEVICES_H__
