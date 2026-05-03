#ifndef BT_CONNECT_HID_DEVICES_H__
#define BT_CONNECT_HID_DEVICES_H__

#include <bluetooth/services/hids.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/sys/iterable_sections.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bt_connect_conn_state {
    struct bt_conn *conn;
    bool in_boot_mode;
};

typedef int (*bt_connect_hid_append_init_fn_t)(struct bt_hids_init_param *init,
                                               uint8_t *input_count,
                                               uint8_t *output_count,
                                               uint8_t *feature_count);

struct bt_connect_hid_report {
    const char *name;
    const uint8_t *report_map;
    size_t report_map_size;
    bt_connect_hid_append_init_fn_t append_init;
};

#define BT_CONNECT_REGISTER_HID_REPORT(_name, _report_map, _append_init)       \
    STRUCT_SECTION_ITERABLE(bt_connect_hid_report, _name) = {                  \
        .name = #_name,                                                        \
        .report_map = _report_map,                                             \
        .report_map_size = sizeof(_report_map),                                \
        .append_init = _append_init,                                           \
    }

typedef int (*bt_connect_conn_iter_fn_t)(
    const struct bt_connect_conn_state *state, void *user_data);

struct bt_hids *bt_connect_hids_obj(void);

void bt_connect_foreach_conn(bt_connect_conn_iter_fn_t fn, void *user_data);

#endif // BT_CONNECT_HID_DEVICES_H__
