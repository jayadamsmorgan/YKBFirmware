#include "kb_handler_internal.h"

#include <subsys/bt_connect.h>
#include <subsys/usb_connect.h>

void kb_handler_transport_send_kb_report(hid_kb_report_t *report) {
    STRUCT_SECTION_FOREACH(kb_handler_transport_cb, iterator) {
        if (iterator->on_kb_report_ready) {
            iterator->on_kb_report_ready(report);
        }
    }
}

void kb_handler_transport_send_mouse_report(hid_mouse_report_t *report) {
    STRUCT_SECTION_FOREACH(kb_handler_transport_cb, iterator) {
        if (iterator->on_mouse_report_ready) {
            iterator->on_mouse_report_ready(report);
        }
    }
}
