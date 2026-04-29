#include <subsys/usb_connect.h>

#include "hid_devices/hid_devices.h"
#include "usbd_init.h"

#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usbd_hid.h>

LOG_MODULE_REGISTER(usb_connect, CONFIG_USB_CONNECT_LOG_LEVEL);

static struct usbd_context *usbd;

static void msg_cb(struct usbd_context *const usbd_ctx,
                   const struct usbd_msg *const msg) {
    LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

    if (msg->type == USBD_MSG_CONFIGURATION) {
        LOG_INF("Configuration value %d", msg->status);
    }

    if (msg->type == USBD_MSG_RESUME) {
        STRUCT_SECTION_FOREACH(usb_connect_cb, callback) {
            if (callback->on_connect)
                callback->on_connect();
        }
    } else if (msg->type == USBD_MSG_SUSPEND) {
        STRUCT_SECTION_FOREACH(usb_connect_cb, callback) {
            if (callback->on_disconnect)
                callback->on_disconnect();
        }
    }

    if (usbd_can_detect_vbus(usbd_ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY) {
            if (usbd_enable(usbd_ctx)) {
                LOG_ERR("Failed to enable device support");
            }
        }

        if (msg->type == USBD_MSG_VBUS_REMOVED) {
            if (usbd_disable(usbd_ctx)) {
                LOG_ERR("Failed to disable device support");
            }
        }
    }
}

static int usb_connect_init(void) {
    int err;

#if CONFIG_USB_CONNECT_KBD
    err = usb_connect_init_kbd_hid();
    if (err) {
        return err;
    }
#endif // CONFIG_USB_CONNECT_KBD
#if CONFIG_USB_CONNECT_MOUSE
    err = usb_connect_init_mouse_hid();
    if (err) {
        return err;
    }
#endif // CONFIG_USB_CONNECT_MOUSE
#if CONFIG_USB_CONNECT_VENDOR
    err = usb_connect_init_vendor_hid();
    if (err) {
        return err;
    }
#endif // CONFIG_USB_CONNECT_VENDOR

    usbd = usbd_init_device(msg_cb);
    if (usbd == NULL) {
        return -ENODEV;
    }

    if (!usbd_can_detect_vbus(usbd)) {
        err = usbd_enable(usbd);
        if (err) {
            return err;
        }
    }

    return 0;
}

SYS_INIT(usb_connect_init, POST_KERNEL, CONFIG_USB_CONNECT_INIT_PRIORITY);

void usb_connect_handle_wakeup(void) {
#if CONFIG_USB_CONNECT_REMOTE_WAKEUP
    if (usbd_is_suspended(usbd)) {
        int ret = usbd_wakeup_request(usbd);
        if (ret) {
            LOG_ERR("Remote wakeup error, %d", ret);
        }
    }
#endif
}
