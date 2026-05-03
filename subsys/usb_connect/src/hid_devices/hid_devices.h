#ifndef HID_DEVICES_H__
#define HID_DEVICES_H__

#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usbd_hid.h>

#include <stdatomic.h>

#define ATOMIC_STORE(var, val)                                                 \
    atomic_store_explicit(var, val, memory_order_relaxed)
#define ATOMIC_LOAD(var) atomic_load_explicit(var, memory_order_relaxed)

struct usb_connect_hid_dev {
    char *name;
    int (*init)(void);
};

#define USB_CONNECT_REGISTER_HID_DEVICE(_name, _init_fn)                       \
    STRUCT_SECTION_ITERABLE(usb_connect_hid_dev, _name) = {                    \
        .name = #_name,                                                        \
        .init = _init_fn,                                                      \
    }

#endif // HID_DEVICES_H__
