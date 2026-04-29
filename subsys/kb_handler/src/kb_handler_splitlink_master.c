#include "kb_handler_common.h"

#include "splitlink_handler/splitlink_handler.h"

#include <lib/ykb_protocol.h>

#include <subsys/bt_connect.h>
#include <subsys/kb_settings.h>
#include <subsys/usb_connect.h>
#include <subsys/zephyr_user_helpers.h>

#include <drivers/kscan.h>
#include <drivers/splitlink.h>

#include <dt-bindings/kb-handler/kb-key-codes.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <math.h>
#include <string.h>

LOG_MODULE_REGISTER(kb_handler, CONFIG_KB_HANDLER_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(kbh_sm_thread_stack,
                             CONFIG_KB_HANDLER_THREAD_STACK_SIZE);
static struct k_thread kbh_sm_thread;

static kb_settings_t settings_snapshot;
static bool thread_started;

enum kbh_thread_msg_type {
    KBH_THREAD_MSG_KEY = 0U,
    KBH_THREAD_MSG_VALUE,
    KBH_THREAD_MSG_SLAVE_VALUES,
    KBH_THREAD_MSG_SLAVE_KEYS_RESET,
    KBH_THREAD_MSG_SETTINGS_SYNC,
};

struct kbh_thread_msg {
    enum kbh_thread_msg_type type;
    uint16_t key;
    bool status;
    uint16_t value;
    uint16_t slave_values[KEY_COUNT_SLAVE];
};

struct kbh_runtime_state {
    kb_settings_t *settings;
    kb_mode_t active_mode;

    bool second_layer_active;
    bool third_layer_active;

    bool pressed_keys[TOTAL_KEY_COUNT];
    uint16_t current_values[TOTAL_KEY_COUNT];

    uint16_t layer1_keys[TOTAL_KEY_COUNT];
    uint16_t layer2_keys[TOTAL_KEY_COUNT];
    uint8_t layer1_keys_count;
    uint8_t layer2_keys_count;

    hid_kb_report_t kb_report;
    hid_kb_report_t prev_kb_report;
    hid_mouse_report_t mouse_report;
    hid_mouse_report_t prev_mouse_report;
};

K_MSGQ_DEFINE(kbh_sm_msgq, sizeof(struct kbh_thread_msg),
              CONFIG_KB_HANDLER_MSGQ_SIZE, 4);

static uint16_t values[TOTAL_KEY_COUNT];

static void send_kb_report(hid_kb_report_t *report) {
    if (IS_ENABLED(CONFIG_LIB_USB_CONNECT)) {
        usb_connect_handle_wakeup();
        usb_connect_send_kb_report(report);
    }
    if (IS_ENABLED(CONFIG_LIB_BT_CONNECT)) {
        // bt_connect_send_kb_report(report);
    }
}

static void send_mouse_report(hid_mouse_report_t *report) {
    if (IS_ENABLED(CONFIG_LIB_USB_CONNECT)) {
        usb_connect_handle_wakeup();
        usb_connect_send_mouse_report(report);
    }
    if (IS_ENABLED(CONFIG_LIB_BT_CONNECT)) {
        // bt_connect_send_mouse_report(report);
    }
}

static inline bool is_modifier(uint8_t hid) {
    return (hid >= KEY_LEFTCONTROL && hid <= KEY_RIGHTGUI);
}

static inline uint8_t mod_bit(uint8_t hid) {
    return 1U << (hid - KEY_LEFTCONTROL);
}

static inline uint8_t resolve_hid(const kb_settings_t *settings, uint16_t key,
                                  bool second_layer_active,
                                  bool third_layer_active) {
    if (third_layer_active) {
        return settings->mappings_layer3[key];
    }
    if (second_layer_active) {
        return settings->mappings_layer2[key];
    }

    return settings->mappings_layer1[key];
}

static inline bool add_key(uint8_t keys[6], uint8_t hid) {
    for (int i = 0; i < 6; ++i) {
        if (keys[i] == hid) {
            return true;
        }
    }

    for (int i = 0; i < 6; ++i) {
        if (keys[i] == 0U) {
            keys[i] = hid;
            return true;
        }
    }

    return false;
}

static void build_kb_report(hid_kb_report_t *report, kb_settings_t *settings,
                            const bool *pressed_keys, uint16_t total_key_count,
                            bool second_layer_active, bool third_layer_active) {
    report->mods = 0;
    report->reserved = 0;
    memset(report->keys, 0, sizeof(report->keys));

    bool overflow = false;

    for (uint16_t i = 0; i < total_key_count; ++i) {
        if (!pressed_keys[i]) {
            continue;
        }

        uint8_t hid =
            resolve_hid(settings, i, second_layer_active, third_layer_active);

        if (hid == KEY_LAYER1 || hid == KEY_LAYER2 || hid == KEY_FN ||
            hid == KEY_NOKEY) {
            continue;
        }

        if (is_modifier(hid)) {
            report->mods |= mod_bit(hid);
            continue;
        }

        if (!add_key(report->keys, hid)) {
            overflow = true;
        }
    }

    if (overflow && IS_ENABLED(CONFIG_KB_HANDLER_REPORT_ROLLOVER)) {
        memset(report->keys, 0x01, sizeof(report->keys));
    }
}

static void build_layer_keys(const kb_settings_t *settings,
                             uint16_t total_key_count, uint16_t *layer1_keys,
                             uint8_t *layer1_keys_count, uint16_t *layer2_keys,
                             uint8_t *layer2_keys_count) {
    *layer1_keys_count = 0;
    *layer2_keys_count = 0;

    memset(layer1_keys, 0, total_key_count * sizeof(uint16_t));
    memset(layer2_keys, 0, total_key_count * sizeof(uint16_t));

    for (uint16_t i = 0; i < total_key_count; ++i) {
        if (settings->mappings_layer1[i] == KEY_LAYER1) {
            layer1_keys[(*layer1_keys_count)++] = i;
        }
        if (settings->mappings_layer1[i] == KEY_LAYER2) {
            layer2_keys[(*layer2_keys_count)++] = i;
        }
    }
}

static bool handle_layer_key(uint16_t key, bool status,
                             const bool *pressed_keys, bool *layer_active,
                             const uint16_t *layer_keys,
                             uint8_t layer_key_count) {
    if (!status) {
        for (uint8_t i = 0; i < layer_key_count; ++i) {
            uint16_t layer_key = layer_keys[i];

            if (layer_key == key) {
                continue;
            }
            if (pressed_keys[layer_key]) {
                return false;
            }
        }
    }

    *layer_active = status;
    return true;
}

static inline bool kb_reports_equal(const hid_kb_report_t *a,
                                    const hid_kb_report_t *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static inline bool mouse_reports_equal(const hid_mouse_report_t *a,
                                       const hid_mouse_report_t *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static inline uint16_t mouseemu_vector_value(uint16_t *current_values,
                                             kb_mouseemu_settings_t *emu,
                                             uint8_t left_vec_idx,
                                             uint8_t straight_vec_idx,
                                             uint8_t right_vec_idx) {
    uint16_t straight_key_idx = emu->move_keys[straight_vec_idx];
    uint16_t straight_vec = current_values[straight_key_idx];
    straight_vec =
        straight_vec >= emu->move_keys_deadzones[straight_vec_idx]
            ? straight_vec - emu->move_keys_deadzones[straight_vec_idx]
            : 0;

    if (emu->direction_mode == KB_MOUSEEMU_DIRECTION_8_WAY) {
        uint16_t left_key_idx = emu->move_keys[left_vec_idx];
        uint16_t left_vec = current_values[left_key_idx];
        left_vec = left_vec >= emu->move_keys_deadzones[left_vec_idx]
                       ? left_vec - emu->move_keys_deadzones[left_vec_idx]
                       : 0;

        uint16_t right_key_idx = emu->move_keys[right_vec_idx];
        uint16_t right_vec = current_values[right_key_idx];
        right_vec = right_vec >= emu->move_keys_deadzones[right_vec_idx]
                        ? right_vec - emu->move_keys_deadzones[right_vec_idx]
                        : 0;

        return straight_vec + (left_vec / 2U) + (right_vec / 2U);
    }

    return straight_vec;
}

static inline int8_t clamp_s8(double v) {
    if (v > 127.0) {
        return 127;
    }
    if (v < -128.0) {
        return -128;
    }
    return (int8_t)lround(v);
}

static void mouseemu_value_handler(kb_settings_t *settings,
                                   uint16_t *current_values, bool *pressed_keys,
                                   hid_mouse_report_t *report) {
    kb_mouseemu_settings_t *emu = &settings->mouseemu;

    report->buttons = 0;
    report->x = 0;
    report->y = 0;
    report->wheel = 0;

    if (!emu->enabled) {
        return;
    }

    if (emu->move_keys_count > 0U) {
        int32_t y_pos =
            (int32_t)mouseemu_vector_value(current_values, emu, 4, 1, 5);
        int32_t y_neg =
            (int32_t)mouseemu_vector_value(current_values, emu, 7, 2, 6);
        int32_t x_pos =
            (int32_t)mouseemu_vector_value(current_values, emu, 5, 3, 7);
        int32_t x_neg =
            (int32_t)mouseemu_vector_value(current_values, emu, 6, 0, 4);

        report->x = clamp_s8((double)(x_pos - x_neg) * emu->move_x_k);
        report->y = clamp_s8((double)(y_pos - y_neg) * emu->move_y_k);
    }

    if (emu->scroll_keys_count == 2U) {
        uint16_t up_idx = emu->scroll_keys[0];
        uint16_t down_idx = emu->scroll_keys[1];

        int32_t up = current_values[up_idx] >= emu->scroll_keys_deadzones[0]
                         ? (int32_t)(current_values[up_idx] -
                                     emu->scroll_keys_deadzones[0])
                         : 0;

        int32_t down = current_values[down_idx] >= emu->scroll_keys_deadzones[1]
                           ? (int32_t)(current_values[down_idx] -
                                       emu->scroll_keys_deadzones[1])
                           : 0;

        report->wheel = clamp_s8((double)(up - down) * emu->scroll_k);
    }

    if (emu->button_keys_count >= 1U && pressed_keys[emu->button_keys[0]]) {
        report->buttons |= BIT(0);
    }
    if (emu->button_keys_count >= 2U && pressed_keys[emu->button_keys[1]]) {
        report->buttons |= BIT(1);
    }
    if (emu->button_keys_count >= 3U && pressed_keys[emu->button_keys[2]]) {
        report->buttons |= BIT(2);
    }
}

static inline void send_kb_report_if_changed(struct kbh_runtime_state *st) {
    build_kb_report(&st->kb_report, st->settings, st->pressed_keys,
                    TOTAL_KEY_COUNT, st->second_layer_active,
                    st->third_layer_active);

    if (!kb_reports_equal(&st->kb_report, &st->prev_kb_report)) {
        send_kb_report(&st->kb_report);
        st->prev_kb_report = st->kb_report;
    }
}

static inline void send_mouse_report_if_changed(struct kbh_runtime_state *st) {
    mouseemu_value_handler(st->settings, st->current_values, st->pressed_keys,
                           &st->mouse_report);

    if (!mouse_reports_equal(&st->mouse_report, &st->prev_mouse_report)) {
        send_mouse_report(&st->mouse_report);
        st->prev_mouse_report = st->mouse_report;
    }
}

static void send_race_report_if_changed(struct kbh_runtime_state *st) {
    double max_percentage = 0.0;
    int32_t max_index = -1;
    bool pressed_race[TOTAL_KEY_COUNT];

    memset(pressed_race, 0, sizeof(pressed_race));

    for (uint16_t i = 0; i < TOTAL_KEY_COUNT; ++i) {
        if (!st->pressed_keys[i]) {
            continue;
        }

        uint16_t threshold = st->settings->thresholds[i];
        uint16_t maximum = st->settings->maximums[i];

        if (maximum <= threshold) {
            continue;
        }

        double percentage_pressed =
            (double)(st->current_values[i] - threshold) / (maximum - threshold);

        if (percentage_pressed > max_percentage) {
            max_percentage = percentage_pressed;
            max_index = i;
        }
    }

    if (max_index >= 0) {
        pressed_race[max_index] = true;
    }

    build_kb_report(&st->kb_report, st->settings, pressed_race, TOTAL_KEY_COUNT,
                    st->second_layer_active, st->third_layer_active);

    if (!kb_reports_equal(&st->kb_report, &st->prev_kb_report)) {
        send_kb_report(&st->kb_report);
        st->prev_kb_report = st->kb_report;
    }
}

static inline void reset_handler_state(struct kbh_runtime_state *st) {
    memset(st->pressed_keys, 0, sizeof(st->pressed_keys));
    memset(st->current_values, 0, sizeof(st->current_values));

    st->second_layer_active = false;
    st->third_layer_active = false;

    memset(&st->kb_report, 0, sizeof(st->kb_report));
    memset(&st->mouse_report, 0, sizeof(st->mouse_report));

    send_kb_report(&st->kb_report);
    send_mouse_report(&st->mouse_report);

    st->prev_kb_report = st->kb_report;
    st->prev_mouse_report = st->mouse_report;
}

static inline void rebuild_layer_cache(struct kbh_runtime_state *st) {
    build_layer_keys(st->settings, TOTAL_KEY_COUNT, st->layer1_keys,
                     &st->layer1_keys_count, st->layer2_keys,
                     &st->layer2_keys_count);
}

static void process_key_transition(struct kbh_runtime_state *st, uint16_t key,
                                   bool status) {
    if (key >= TOTAL_KEY_COUNT) {
        LOG_WRN("Ignoring out-of-range key %u", key);
        return;
    }

    st->pressed_keys[key] = status;

    if (st->active_mode == KB_MODE_RACE) {
        return;
    }

    uint8_t resolved_hid = resolve_hid(
        st->settings, key, st->second_layer_active, st->third_layer_active);

    LOG_DBG("Key %s idx %u", status ? "pressed" : "released", key);

    if (resolved_hid == KEY_LAYER1) {
        if (!handle_layer_key(key, status, st->pressed_keys,
                              &st->second_layer_active, st->layer1_keys,
                              st->layer1_keys_count)) {
            return;
        }
    } else if (resolved_hid == KEY_LAYER2) {
        if (!handle_layer_key(key, status, st->pressed_keys,
                              &st->third_layer_active, st->layer2_keys,
                              st->layer2_keys_count)) {
            return;
        }
    } else if (resolved_hid == KEY_FN) {
        return;
    }

    if (st->active_mode == KB_MODE_NORMAL ||
        st->active_mode == KB_MODE_MOUSESIM) {
        send_kb_report_if_changed(st);
    }

    if (st->active_mode == KB_MODE_MOUSESIM) {
        send_mouse_report_if_changed(st);
    }
}

static void handle_slave_values(struct kbh_runtime_state *st,
                                const uint16_t slave_values[KEY_COUNT_SLAVE]) {
    bool prev_pressed_keys[TOTAL_KEY_COUNT];

    memcpy(prev_pressed_keys, st->pressed_keys, sizeof(prev_pressed_keys));
    memcpy(&st->current_values[KEY_COUNT], slave_values,
           sizeof(uint16_t) * KEY_COUNT_SLAVE);

    for (uint16_t i = KEY_COUNT; i < TOTAL_KEY_COUNT; ++i) {
        st->pressed_keys[i] =
            st->current_values[i] >= st->settings->thresholds[i];
    }

    for (uint16_t i = KEY_COUNT; i < TOTAL_KEY_COUNT; ++i) {
        if (prev_pressed_keys[i] != st->pressed_keys[i]) {
            process_key_transition(st, i, st->pressed_keys[i]);
        }
    }

    if (st->active_mode == KB_MODE_MOUSESIM) {
        send_mouse_report_if_changed(st);
    }

    if (st->active_mode == KB_MODE_RACE) {
        send_race_report_if_changed(st);
    }
}

static void kb_handler_thread(void *a, void *b, void *c) {
    struct kbh_thread_msg msg;
    struct kbh_runtime_state st = {
        .settings = &settings_snapshot,
        .active_mode = settings_snapshot.mode,
    };

    rebuild_layer_cache(&st);
    reset_handler_state(&st);

    while (true) {
        int err = k_msgq_get(&kbh_sm_msgq, &msg, K_FOREVER);
        if (err) {
            LOG_ERR("k_msgq_get: %d", err);
            continue;
        }

        switch (msg.type) {
        case KBH_THREAD_MSG_SETTINGS_SYNC:
            st.active_mode = st.settings->mode;
            rebuild_layer_cache(&st);
            reset_handler_state(&st);
            break;

        case KBH_THREAD_MSG_SLAVE_KEYS_RESET:
            memset(&st.pressed_keys[KEY_COUNT], 0,
                   KEY_COUNT_SLAVE * sizeof(bool));
            memset(&st.current_values[KEY_COUNT], 0,
                   KEY_COUNT_SLAVE * sizeof(uint16_t));

            if (st.active_mode == KB_MODE_MOUSESIM) {
                send_mouse_report_if_changed(&st);
            }

            if (st.active_mode == KB_MODE_NORMAL ||
                st.active_mode == KB_MODE_MOUSESIM) {
                send_kb_report_if_changed(&st);
            } else if (st.active_mode == KB_MODE_RACE) {
                send_race_report_if_changed(&st);
            }
            break;

        case KBH_THREAD_MSG_KEY:
            process_key_transition(&st, msg.key, msg.status);
            break;

        case KBH_THREAD_MSG_SLAVE_VALUES:
            handle_slave_values(&st, msg.slave_values);
            break;

        case KBH_THREAD_MSG_VALUE:
            if (msg.key >= TOTAL_KEY_COUNT) {
                LOG_WRN("Ignoring out-of-range value key %u", msg.key);
                break;
            }

            st.current_values[msg.key] = msg.value;

            if (st.active_mode == KB_MODE_MOUSESIM) {
                send_mouse_report_if_changed(&st);
            }

            if (st.active_mode == KB_MODE_RACE) {
                send_race_report_if_changed(&st);
            }
            break;

        default:
            LOG_WRN("Unknown kb handler thread msg type %u", msg.type);
            break;
        }
    }
}

static inline void kb_handler_sm_key_handler(uint16_t key_index, bool status) {
    struct kbh_thread_msg data = {
        .type = KBH_THREAD_MSG_KEY,
        .key = key_index,
        .status = status,
    };

    k_msgq_put(&kbh_sm_msgq, &data, K_NO_WAIT);
}

static inline void kb_handler_sm_value_handler(uint16_t key_index,
                                               uint16_t value) {
    struct kbh_thread_msg data = {
        .type = KBH_THREAD_MSG_VALUE,
        .key = key_index,
        .value = value,
    };

    values[key_index] = value;

    k_msgq_put(&kbh_sm_msgq, &data, K_NO_WAIT);
}

KSCAN_CB_DEFINE(kbh_sm) = {
    .on_event = kb_handler_sm_key_handler,
    .on_new_value = kb_handler_sm_value_handler,
};

void splitlink_handler_values_received(uint16_t *slave_values, uint16_t count) {
    if (count != KEY_COUNT_SLAVE) {
        LOG_ERR("Slave values size mismatch");
        return;
    }
    struct kbh_thread_msg data = {
        .type = KBH_THREAD_MSG_SLAVE_VALUES,
    };
    memcpy(data.slave_values, slave_values, sizeof(uint16_t) * count);

    memcpy(&values[KEY_COUNT], slave_values, sizeof(uint16_t) * count);

    k_msgq_put(&kbh_sm_msgq, &data, K_NO_WAIT);
}

void splitlink_handler_on_connect() {
    //
    LOG_INF("SplitLink slave connected");
}

void splitlink_handler_on_disconnect() {
    struct kbh_thread_msg data = {
        .type = KBH_THREAD_MSG_SLAVE_KEYS_RESET,
    };

    LOG_WRN("SplitLink slave disconnected");
    int err = k_msgq_put(&kbh_sm_msgq, &data, K_NO_WAIT);
    if (err) {
        LOG_WRN("Event slave keys reset skipped (err %d)", err);
    }
}

static void kscans_check() {
    int expected_offset = 0;
    int total_key_count = 0;

    for (uint16_t i = 0; i < ARRAY_SIZE(kscans); ++i) {
        int offset = kscan_get_idx_offset(kscans[i]);
        int key_amount = kscan_get_key_amount(kscans[i]);

        if (offset < 0) {
            LOG_ERR("Unable to get idx offset for KScan %u (err %d)", i,
                    offset);
            return;
        }
        if (key_amount < 0) {
            LOG_ERR("Unable to get key amount for KScan %u (err %d)", i,
                    key_amount);
            return;
        }

        total_key_count += key_amount;

        if (i != 0U) {
            if (expected_offset < offset) {
                LOG_ERR("Key indices gap between KScans %u and %u "
                        "(expected offset %d, got %d)",
                        i - 1U, i, expected_offset, offset);
                k_panic();
            } else if (expected_offset > offset) {
                LOG_ERR("Key indices intersection between "
                        "KScans %u and %u (expected offset %d, "
                        "got %d)",
                        i - 1U, i, expected_offset, offset);
                k_panic();
            }
        }

        expected_offset = offset + key_amount;
    }

    if (total_key_count != TOTAL_KEY_COUNT) {
        LOG_ERR("KScans total key count != DTS key count (%d != %u)",
                total_key_count, TOTAL_KEY_COUNT);
        k_panic();
    }
}

static void mouseemu_key_indices_check(const uint16_t *keys, uint8_t keys_count,
                                       uint16_t total_key_count,
                                       const char *group_name) {
    for (uint8_t i = 0; i < keys_count; ++i) {
        if (keys[i] >= total_key_count) {
            LOG_ERR("Mouseemu %s key index %u is out of range", group_name,
                    keys[i]);
            k_panic();
        }
    }
}

static void mouseemu_count_check(uint8_t count, uint8_t max_count,
                                 const char *group_name) {
    if (count > max_count) {
        LOG_ERR("Mouseemu %s count %u exceeds max %u", group_name, count,
                max_count);
        k_panic();
    }
}

static void mouseemu_check(uint16_t total_key_count,
                           const kb_mouseemu_settings_t *mouseemu) {
    if (!mouseemu->enabled) {
        return;
    }

    mouseemu_count_check(mouseemu->move_keys_count, KB_MOUSEEMU_MOVE_KEYS_MAX,
                         "move");
    mouseemu_count_check(mouseemu->scroll_keys_count,
                         KB_MOUSEEMU_SCROLL_KEYS_MAX, "scroll");
    mouseemu_count_check(mouseemu->button_keys_count,
                         KB_MOUSEEMU_BUTTON_KEYS_MAX, "button");

    if (mouseemu->direction_mode == KB_MOUSEEMU_DIRECTION_4_WAY &&
        mouseemu->move_keys_count != 4U) {
        LOG_ERR("Mouseemu 4-way mode requires exactly 4 move keys");
        k_panic();
    }

    if (mouseemu->direction_mode == KB_MOUSEEMU_DIRECTION_8_WAY &&
        mouseemu->move_keys_count != 8U) {
        LOG_ERR("Mouseemu 8-way mode requires exactly 8 move keys");
        k_panic();
    }

    if (mouseemu->scroll_keys_count != 0U &&
        mouseemu->scroll_keys_count != 2U) {
        LOG_ERR("Mouseemu scroll keys must contain exactly 2 entries");
        k_panic();
    }

    if (mouseemu->button_keys_count != 0U &&
        mouseemu->button_keys_count != 3U) {
        LOG_ERR("Mouseemu button keys must contain exactly 3 entries");
        k_panic();
    }

    mouseemu_key_indices_check(mouseemu->move_keys, mouseemu->move_keys_count,
                               total_key_count, "move");
    mouseemu_key_indices_check(mouseemu->scroll_keys,
                               mouseemu->scroll_keys_count, total_key_count,
                               "scroll");
    mouseemu_key_indices_check(mouseemu->button_keys,
                               mouseemu->button_keys_count, total_key_count,
                               "button");
}

static void kb_handler_sm_on_settings_update(kb_settings_t *settings) {
    if (thread_started) {
        k_thread_suspend(&kbh_sm_thread);
        k_msgq_purge(&kbh_sm_msgq);
    }

    memcpy(&settings_snapshot, settings, sizeof(kb_settings_t));
    mouseemu_check(TOTAL_KEY_COUNT, &settings_snapshot.mouseemu);

    for (size_t i = 0; i < ARRAY_SIZE(kscans); ++i) {
        const struct device *kscan = kscans[i];
        int idx_offset = kscan_get_idx_offset(kscan);

        if (idx_offset < 0) {
            LOG_ERR("Unable to get idx offset for KScan instance %s "
                    "(err %d)",
                    kscan->name, idx_offset);
            continue;
        }

        int err =
            kscan_set_thresholds(kscan, &settings->thresholds[idx_offset]);
        if (err) {
            LOG_ERR("Unable to set thresholds for KScan instance %s "
                    "(err %d)",
                    kscan->name, err);
            continue;
        }
    }

    if (thread_started) {
        k_thread_resume(&kbh_sm_thread);
    } else {
        k_thread_create(&kbh_sm_thread, kbh_sm_thread_stack,
                        CONFIG_KB_HANDLER_THREAD_STACK_SIZE, kb_handler_thread,
                        NULL, NULL, NULL, CONFIG_KB_HANDLER_THREAD_PRIORITY, 0,
                        K_NO_WAIT);
        k_thread_name_set(&kbh_sm_thread, "kb_handler_sm");
        thread_started = true;
    }

    struct kbh_thread_msg msg = {
        .type = KBH_THREAD_MSG_SETTINGS_SYNC,
    };
    int err = k_msgq_put(&kbh_sm_msgq, &msg, K_NO_WAIT);
    if (err) {
        LOG_WRN("Event settings sync skipped (err %d)", err);
    }
}

ON_SETTINGS_UPDATE_DEFINE(kbh_sm, kb_handler_sm_on_settings_update);

static int kb_handler_sm_init(void) {
    thread_started = false;

    for (uint16_t i = 0; i < ARRAY_SIZE(kscans); ++i) {
        if (!device_is_ready(kscans[i])) {
            LOG_ERR("KScan device '%s' is not ready", kscans[i]->name);
            return -1;
        }
    }

    int err = splitlink_handler_init();
    if (err) {
        return -2;
    }

    kscans_check();
    mouseemu_check(TOTAL_KEY_COUNT, &default_mouseemu);

    return 0;
}

SYS_INIT(kb_handler_sm_init, POST_KERNEL, CONFIG_KB_HANDLER_INIT_PRIORITY);

void kb_handler_get_values(uint16_t *_values, uint16_t count) {
    if (!_values || count == 0) {
        return;
    }

    memcpy(_values, values, sizeof(uint16_t) * count);
}
