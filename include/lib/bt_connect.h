#ifndef LIB_BT_CONNECT_H
#define LIB_BT_CONNECT_H

#include <lib/usb_connect.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/iterable_sections.h>

struct bt_connect_cb {
    void (*on_connect)(bt_addr_le_t addr);
    void (*on_disconnect)(bt_addr_le_t addr);
};

#define BT_CONNECT_CB_DEFINE(name)                                             \
    static STRUCT_SECTION_ITERABLE(bt_connect_cb, __bt_connect_cb__##name)

void bt_connect_send_kb_report(hid_kb_report_t *report);

void bt_connect_send_mouse_report(hid_mouse_report_t *report);

#endif // LIB_BT_CONNECT_H
