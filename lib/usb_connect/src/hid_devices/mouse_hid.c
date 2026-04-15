#include "hid_devices.h"

#include <lib/usb_connect.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(usb_connect, CONFIG_USB_CONNECT_LOG_LEVEL);

static const uint8_t hid_mouse_report_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_MOUSE),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),

    HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),
    HID_COLLECTION(HID_COLLECTION_PHYSICAL),

    HID_USAGE_PAGE(HID_USAGE_GEN_BUTTON),
    HID_USAGE_MIN8(1),
    HID_USAGE_MAX8(3),
    HID_LOGICAL_MIN8(0),
    HID_LOGICAL_MAX8(1),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(3),
    HID_INPUT(0x02),

    HID_REPORT_SIZE(5),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x01),

    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_X),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_WHEEL),
    HID_LOGICAL_MIN8(-127),
    HID_LOGICAL_MAX8(127),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(3),
    HID_INPUT(0x06),

    HID_END_COLLECTION,

    HID_END_COLLECTION,
};

static const struct device *hid_mouse_dev =
    DEVICE_DT_GET(DT_NODELABEL(hid_mouse));

static atomic_bool __ready;
static uint32_t __duration;
static atomic_bool boot_mode;

static void iface_ready(const struct device *dev, const bool ready) {
    LOG_INF("HID device %s interface is %s", dev->name,
            ready ? "ready" : "not ready");
    ATOMIC_STORE(&__ready, ready);
}

static int get_report(const struct device *dev, const uint8_t type,
                      const uint8_t id, const uint16_t len,
                      uint8_t *const buf) {
    LOG_WRN("Get Report not implemented, Type %u ID %u", type, id);
    return 0;
}

static int set_report(const struct device *dev, const uint8_t type,
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

    return 0;
}

static void set_idle(const struct device *dev, const uint8_t id,
                     const uint32_t duration) {
    LOG_INF("Set Idle %u to %u", id, duration);
    __duration = duration;
}

static uint32_t get_idle(const struct device *dev, const uint8_t id) {
    LOG_INF("Get Idle %u to %u", id, __duration);
    return __duration;
}

static void output_report(const struct device *dev, const uint16_t len,
                          const uint8_t *const buf) {
    LOG_HEXDUMP_DBG(buf, len, "o.r.");
    set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
}

static void set_protocol(const struct device *dev, const uint8_t proto) {
    ATOMIC_STORE(&boot_mode, proto == HID_PROTOCOL_BOOT);
    LOG_INF("hid_mouse in %s mode",
            proto == HID_PROTOCOL_BOOT ? "boot" : "report");
}

static struct hid_device_ops ops = {
    .set_protocol = set_protocol,
    .iface_ready = iface_ready,
    .get_report = get_report,
    .set_report = set_report,
    .set_idle = set_idle,
    .get_idle = get_idle,
    .output_report = output_report,
};

int usb_connect_init_mouse_hid(void) {
    if (!device_is_ready(hid_mouse_dev)) {
        LOG_ERR("hid_mouse not ready");
        return -EIO;
    }

    int err = hid_device_register(hid_mouse_dev, hid_mouse_report_desc,
                                  sizeof(hid_mouse_report_desc), &ops);
    if (err) {
        LOG_ERR("hid_mouse register: %d", err);
        return err;
    }

    return 0;
}

void usb_connect_send_mouse_report(const hid_mouse_report_t *report) {
    bool ready = ATOMIC_LOAD(&__ready);
    if (!ready) {
        return;
    }
    int err = hid_device_submit_report(
        hid_mouse_dev, sizeof(hid_mouse_report_t), (const uint8_t *)report);
    if (err) {
        LOG_ERR("Error submitting mouse report: %d", err);
    }
}
