#ifndef LIB_BT_CONNECT_H
#define LIB_BT_CONNECT_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/iterable_sections.h>

struct bt_connect_cb {
    void (*on_connect)(bt_addr_le_t addr);
    void (*on_disconnect)(bt_addr_le_t addr);
};

#define BT_CONNECT_CB_DEFINE(name)                                             \
    static STRUCT_SECTION_ITERABLE(bt_connect_cb, __bt_connect_cb__##name)

void bt_connect_new_battery();

void bt_connect_set_battery_level();

void bt_connect_send_report();

#endif // LIB_BT_CONNECT_H
