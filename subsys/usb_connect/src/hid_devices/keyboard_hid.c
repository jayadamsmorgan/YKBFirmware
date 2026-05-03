#include "hid_devices.h"

#include <subsys/usb_connect.h>

#include <subsys/kb_handler.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(usb_connect, CONFIG_USB_CONNECT_LOG_LEVEL);

static const uint8_t hid_kbd_report_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),

    HID_USAGE_PAGE(HID_USAGE_GEN_KEYBOARD),
    HID_USAGE_MIN8(0xE0),
    HID_USAGE_MAX8(0xE7),
    HID_LOGICAL_MIN8(0),
    HID_LOGICAL_MAX8(1),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(8),
    HID_INPUT(0x02),

    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x03),

    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(5),
    HID_USAGE_PAGE(HID_USAGE_GEN_LEDS),
    HID_USAGE_MIN8(1),
    HID_USAGE_MAX8(5),
    HID_OUTPUT(0x02),

    HID_REPORT_SIZE(3),
    HID_REPORT_COUNT(1),
    HID_OUTPUT(0x03),

    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(6),
    HID_LOGICAL_MIN8(0),
    HID_LOGICAL_MAX8(101),
    HID_USAGE_PAGE(HID_USAGE_GEN_KEYBOARD),
    HID_USAGE_MIN8(0),
    HID_USAGE_MAX8(101),
    HID_INPUT(0x00),

    HID_END_COLLECTION,
};

static const struct device *hid_kbd_dev = DEVICE_DT_GET(DT_NODELABEL(hid_kbd));

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
    LOG_INF("hid_kbd in %s mode",
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

static int usb_connect_init_kbd_hid(void) {
    if (!device_is_ready(hid_kbd_dev)) {
        LOG_ERR("hid_kbd not ready");
        return -EIO;
    }

    int err = hid_device_register(hid_kbd_dev, hid_kbd_report_desc,
                                  sizeof(hid_kbd_report_desc), &ops);
    if (err) {
        LOG_ERR("hid_kbd register: %d", err);
        return err;
    }

    return 0;
}

static void on_kb_report_ready(const hid_kb_report_t *const report) {
    bool ready = ATOMIC_LOAD(&__ready);
    usb_connect_handle_wakeup();
    if (!ready) {
        LOG_ERR("send_kb_report: not ready");
        return;
    }
    int err = hid_device_submit_report(hid_kbd_dev, sizeof(hid_kb_report_t),
                                       (const uint8_t *)report);
    if (err) {
        LOG_ERR("send_kb_report: %d", err);
    }
    LOG_INF("send_kb_report: sent");
}

static KB_HANDLER_TRANSPORT_CB_DEFINE(kbd_hid) = {
    .on_kb_report_ready = on_kb_report_ready,
};

USB_CONNECT_REGISTER_HID_DEVICE(usb_kbd, usb_connect_init_kbd_hid);
