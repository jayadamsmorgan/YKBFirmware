#include "kb_handler_internal.h"

#include <subsys/bt_connect.h>
#include <subsys/usb_connect.h>

void kb_handler_transport_send_kb_report(hid_kb_report_t *report) {
    if (IS_ENABLED(CONFIG_USB_CONNECT)) {
        usb_connect_handle_wakeup();
        usb_connect_send_kb_report(report);
    }
    if (IS_ENABLED(CONFIG_BT_CONNECT)) {
        // bt_connect_send_kb_report(report);
    }
}

void kb_handler_transport_send_mouse_report(hid_mouse_report_t *report) {
    if (IS_ENABLED(CONFIG_USB_CONNECT)) {
        usb_connect_handle_wakeup();
        usb_connect_send_mouse_report(report);
    }
    if (IS_ENABLED(CONFIG_BT_CONNECT)) {
        // bt_connect_send_mouse_report(report);
    }
}
