#include <lib/usb_connect.h>

#include "usbd_init.h"

#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usbd_hid.h>

LOG_MODULE_REGISTER(usb_connect, CONFIG_USB_CONNECT_LOG_LEVEL);

#define REPORT_ID_KEYBOARD 0x01
#define REPORT_ID_MOUSE 0x02

#define HID_KEYBOARD_REPORT_DESC_ID(_id)                                       \
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                                     \
        HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),                             \
        HID_COLLECTION(HID_COLLECTION_APPLICATION), HID_REPORT_ID(_id),        \
        HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD), HID_USAGE_MIN8(0xE0),    \
        HID_USAGE_MAX8(0xE7), HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(1),        \
        HID_REPORT_SIZE(1), HID_REPORT_COUNT(8), HID_INPUT(0x02),              \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(1), HID_INPUT(0x03),              \
        HID_REPORT_SIZE(1), HID_REPORT_COUNT(5),                               \
        HID_USAGE_PAGE(HID_USAGE_GEN_LEDS), HID_USAGE_MIN8(1),                 \
        HID_USAGE_MAX8(5), HID_OUTPUT(0x02), HID_REPORT_SIZE(3),               \
        HID_REPORT_COUNT(1), HID_OUTPUT(0x03), HID_REPORT_SIZE(8),             \
        HID_REPORT_COUNT(6), HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(101),       \
        HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD), HID_USAGE_MIN8(0),       \
        HID_USAGE_MAX8(101), HID_INPUT(0x00), HID_END_COLLECTION

#define HID_MOUSE_REPORT_DESC_ID(_id, _bcnt)                                   \
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                                     \
        HID_USAGE(HID_USAGE_GEN_DESKTOP_MOUSE),                                \
        HID_COLLECTION(HID_COLLECTION_APPLICATION), HID_REPORT_ID(_id),        \
        HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),                              \
        HID_COLLECTION(HID_COLLECTION_PHYSICAL),                               \
        HID_USAGE_PAGE(HID_USAGE_GEN_BUTTON), HID_USAGE_MIN8(1),               \
        HID_USAGE_MAX8(_bcnt), HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(1),       \
        HID_REPORT_SIZE(1), HID_REPORT_COUNT(_bcnt), HID_INPUT(0x02),          \
        HID_REPORT_SIZE(8 - (_bcnt)), HID_REPORT_COUNT(1), HID_INPUT(0x01),    \
        HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                                 \
        HID_USAGE(HID_USAGE_GEN_DESKTOP_X),                                    \
        HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),                                    \
        HID_USAGE(HID_USAGE_GEN_DESKTOP_WHEEL), HID_LOGICAL_MIN8(-127),        \
        HID_LOGICAL_MAX8(127), HID_REPORT_SIZE(8), HID_REPORT_COUNT(3),        \
        HID_INPUT(0x06), HID_END_COLLECTION, HID_END_COLLECTION

static const uint8_t hid_report_desc[] = {
    HID_KEYBOARD_REPORT_DESC_ID(REPORT_ID_KEYBOARD),
    HID_MOUSE_REPORT_DESC_ID(REPORT_ID_MOUSE, 3),
};

static uint32_t kb_duration;
static bool kb_ready;

static struct usbd_context *usbd;
static const struct device *hid_dev;

static void kb_iface_ready(const struct device *dev, const bool ready) {
    LOG_INF("HID device %s interface is %s", dev->name,
            ready ? "ready" : "not ready");
    kb_ready = ready;
}

static int kb_get_report(const struct device *dev, const uint8_t type,
                         const uint8_t id, const uint16_t len,
                         uint8_t *const buf) {
    LOG_WRN("Get Report not implemented, Type %u ID %u", type, id);
    return 0;
}

static int kb_set_report(const struct device *dev, const uint8_t type,
                         const uint8_t id, const uint16_t len,
                         const uint8_t *const buf) {
    if (type != HID_REPORT_TYPE_OUTPUT) {
        LOG_WRN("Unsupported report type");
        return -ENOTSUP;
    }

    if (len < 2) {
        LOG_WRN("Short output report");
        return -EINVAL;
    }

    if (buf[0] != REPORT_ID_KEYBOARD) {
        LOG_WRN("Unexpected output report ID %u", buf[0]);
        return -ENOTSUP;
    }

    // uint8_t leds = buf[1];

    // TODO: handle leds

    return 0;
}

/* Idle duration is stored but not used to calculate idle reports. */
static void kb_set_idle(const struct device *dev, const uint8_t id,
                        const uint32_t duration) {
    LOG_INF("Set Idle %u to %u", id, duration);
    kb_duration = duration;
}

static uint32_t kb_get_idle(const struct device *dev, const uint8_t id) {
    LOG_INF("Get Idle %u to %u", id, kb_duration);
    return kb_duration;
}

static void kb_set_protocol(const struct device *dev, const uint8_t proto) {
    LOG_INF("Protocol changed to %s",
            proto == 0U ? "Boot Protocol" : "Report Protocol");
}

static void kb_output_report(const struct device *dev, const uint16_t len,
                             const uint8_t *const buf) {
    LOG_HEXDUMP_DBG(buf, len, "o.r.");
    kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
}

struct hid_device_ops kb_ops = {
    .iface_ready = kb_iface_ready,
    .get_report = kb_get_report,
    .set_report = kb_set_report,
    .set_idle = kb_set_idle,
    .get_idle = kb_get_idle,
    .set_protocol = kb_set_protocol,
    .output_report = kb_output_report,
};

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
    int ret;

    hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
    if (!device_is_ready(hid_dev)) {
        return -EIO;
    }

    ret = hid_device_register(hid_dev, hid_report_desc, sizeof(hid_report_desc),
                              &kb_ops);
    if (ret != 0) {
        return ret;
    }

    usbd = usbd_init_device(msg_cb);
    if (usbd == NULL) {
        return -ENODEV;
    }

    if (!usbd_can_detect_vbus(usbd)) {
        ret = usbd_enable(usbd);
        if (ret) {
            return ret;
        }
    }

    return 0;
}

SYS_INIT(usb_connect_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

static inline void handle_wakeup() {
#if CONFIG_LIB_USB_CONNECT_REMOTE_WAKEUP
    if (usbd_is_suspended(usbd)) {
        int ret = usbd_wakeup_request(usbd);
        if (ret) {
            LOG_ERR("Remote wakeup error, %d", ret);
        }
    }
#endif
}

void usb_connect_send_kb_report(const hid_kb_report_t *report) {
    handle_wakeup();
    int ret = hid_device_submit_report(hid_dev, sizeof(*report),
                                       (const uint8_t *)report);
    if (ret) {
        LOG_ERR("Keyboard HID submit report error, %d", ret);
    }
}

void usb_connect_send_mouse_report(const hid_mouse_report_t *report) {
    handle_wakeup();
    int ret = hid_device_submit_report(hid_dev, sizeof(*report),
                                       (const uint8_t *)report);
    if (ret) {
        LOG_ERR("Mouse HID submit report error, %d", ret);
    }
}

bool usb_connect_is_ready() { return kb_ready; }

uint32_t usb_connect_duration() { return kb_duration; }
