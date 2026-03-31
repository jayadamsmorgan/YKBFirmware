#define DT_DRV_COMPAT kb_handler_splitlink_master

#include <drivers/kb_handler.h>

#include <lib/kb_settings.h>
#include <lib/usb_connect.h>

#include <drivers/kscan.h>
#include <drivers/splitlink.h>

#include <dt-bindings/kb-handler/kb-key-codes.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <math.h>
#include <string.h>

LOG_MODULE_REGISTER(kb_handler_sm, CONFIG_KB_HANDLER_LOG_LEVEL);

enum kbh_thread_msg_type {
    KBH_THREAD_MSG_KEY = 0U,
    KBH_THREAD_MSG_VALUE,
    KBH_THREAD_MSG_SLAVE_KEYS_RESET,
    KBH_THREAD_MSG_SETTINGS_SYNC,
};

struct kb_handler_sm_config {
    const struct device **kscans;
    const struct device *splitlink;

    const uint16_t key_count;
    const uint16_t key_count_slave;
    const uint16_t kscans_count;

    const uint8_t *default_keymap_layer1;
    const uint8_t *default_keymap_layer2;
    const uint8_t *default_keymap_layer3;

    const kb_mouseemu_settings_t default_mouseemu;
};

struct kb_handler_sm_data {
    struct k_thread thread;
    bool thread_started;
    k_thread_stack_t *thread_stack;
    struct k_msgq *thread_q;

    kb_settings_t settings_snapshot;
};

#define DATA_FLAG_KEYS 1U
#define DATA_FLAG_VALUES 2U
#define DATA_FLAG_BACKLIGHT 3U

struct kbh_thread_msg {
    enum kbh_thread_msg_type type;
    uint16_t key;
    bool status;
    uint16_t value;
};

static void send_kb_report(hid_kb_report_t *report) {
    usb_connect_send_kb_report(report);
    // TODO: bt
}

static void send_mouse_report(hid_mouse_report_t *report) {
    usb_connect_send_mouse_report(report);
    // TODO: bt
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

static bool add_key(uint8_t keys[6], uint8_t hid) {
    for (int i = 0; i < 6; ++i) {
        if (keys[i] == hid) {
            return true;
        }
    }
    for (int i = 0; i < 6; ++i) {
        if (keys[i] == 0) {
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
    report->report_id = REPORT_ID_KEYBOARD;
    memset(report->keys, 0, sizeof(report->keys));

    bool overflow = false;

    for (uint16_t i = 0; i < total_key_count; ++i) {
        uint8_t hid;

        if (!pressed_keys[i]) {
            continue;
        }

        hid = resolve_hid(settings, i, second_layer_active, third_layer_active);

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
        report->keys[0] = 0x01;
        report->keys[1] = 0x01;
        report->keys[2] = 0x01;
        report->keys[3] = 0x01;
        report->keys[4] = 0x01;
        report->keys[5] = 0x01;
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
            layer1_keys[*layer1_keys_count] = i;
            (*layer1_keys_count)++;
        }
        if (settings->mappings_layer1[i] == KEY_LAYER2) {
            layer2_keys[*layer2_keys_count] = i;
            (*layer2_keys_count)++;
        }
    }
}

static bool handle_layer_key(const struct kbh_thread_msg *msg,
                             const bool *pressed_keys, bool *layer_active,
                             const uint16_t *layer_keys,
                             uint8_t layer_key_count) {
    if (!msg->status) {
        bool other_layer_pressed = false;

        for (uint8_t i = 0; i < layer_key_count; ++i) {
            uint16_t layer_key = layer_keys[i];

            if (layer_key == msg->key) {
                continue;
            }
            if (pressed_keys[layer_key]) {
                other_layer_pressed = true;
                break;
            }
        }

        if (other_layer_pressed) {
            return false;
        }
    }

    *layer_active = msg->status;

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

static void reset_handler_state(bool *pressed_keys, uint16_t *current_values,
                                uint16_t total_key_count,
                                bool *second_layer_active,
                                bool *third_layer_active,
                                hid_kb_report_t *kb_report,
                                hid_mouse_report_t *mouse_report) {
    memset(pressed_keys, 0, total_key_count * sizeof(bool));
    memset(current_values, 0, total_key_count * sizeof(uint16_t));

    *second_layer_active = false;
    *third_layer_active = false;

    kb_report->report_id = REPORT_ID_KEYBOARD;
    kb_report->mods = 0;
    kb_report->reserved = 0;
    memset(kb_report->keys, 0, sizeof(kb_report->keys));
    send_kb_report(kb_report);

    mouse_report->report_id = REPORT_ID_MOUSE;
    mouse_report->buttons = 0;
    mouse_report->wheel = 0;
    mouse_report->x = 0;
    mouse_report->y = 0;
    send_mouse_report(mouse_report);
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

        return straight_vec + (left_vec / 2) + (right_vec / 2);
    }

    return straight_vec;
}

static int8_t clamp_s8(double v) {
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

    report->report_id = REPORT_ID_MOUSE;
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

        double y = (double)(y_pos - y_neg) * emu->move_y_k;
        double x = (double)(x_pos - x_neg) * emu->move_x_k;

        report->x = clamp_s8(x);
        report->y = clamp_s8(y);
    }

    if (emu->scroll_keys_count == 2U) {
        uint16_t scrollup_key_idx = emu->scroll_keys[0];
        uint16_t scrollup_val = current_values[scrollup_key_idx];
        int32_t scrollup =
            scrollup_val >= emu->scroll_keys_deadzones[0]
                ? (int32_t)(scrollup_val - emu->scroll_keys_deadzones[0])
                : 0;

        uint16_t scrolldown_key_idx = emu->scroll_keys[1];
        uint16_t scrolldown_val = current_values[scrolldown_key_idx];
        int32_t scrolldown =
            scrolldown_val >= emu->scroll_keys_deadzones[1]
                ? (int32_t)(scrolldown_val - emu->scroll_keys_deadzones[1])
                : 0;

        double scroll = (double)(scrollup - scrolldown) * emu->scroll_k;
        report->wheel = clamp_s8(scroll);
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

static void kb_handler_thread(void *device, void *_, void *__) {
    struct kbh_thread_msg msg;
    const struct device *dev = device;
    const struct kb_handler_sm_config *cfg = dev->config;
    struct kb_handler_sm_data *data = dev->data;
    kb_settings_t *settings_snapshot = &data->settings_snapshot;
    uint16_t total_key_count = cfg->key_count + cfg->key_count_slave;
    bool second_layer_active = false;
    bool third_layer_active = false;
    bool pressed_keys[total_key_count];
    uint16_t current_values[total_key_count];
    uint16_t layer1_keys[total_key_count];
    uint16_t layer2_keys[total_key_count];
    uint8_t layer1_keys_count = 0;
    uint8_t layer2_keys_count = 0;

    hid_kb_report_t kb_report = {.report_id = REPORT_ID_KEYBOARD};
    hid_kb_report_t prev_kb_report = {.report_id = REPORT_ID_KEYBOARD};
    hid_mouse_report_t mouse_report = {.report_id = REPORT_ID_MOUSE};
    hid_mouse_report_t prev_mouse_report = {.report_id = REPORT_ID_MOUSE};

    kb_mode_t active_mode = settings_snapshot->mode;

    build_layer_keys(settings_snapshot, total_key_count, layer1_keys,
                     &layer1_keys_count, layer2_keys, &layer2_keys_count);

    reset_handler_state(pressed_keys, current_values, total_key_count,
                        &second_layer_active, &third_layer_active, &kb_report,
                        &mouse_report);

    prev_kb_report = kb_report;
    prev_mouse_report = mouse_report;

    while (true) {
        int err = k_msgq_get(data->thread_q, &msg, K_FOREVER);

        if (err) {
            LOG_ERR("k_msgq_get: %d", err);
            continue;
        }

        switch (msg.type) {
        case KBH_THREAD_MSG_SETTINGS_SYNC:
            active_mode = settings_snapshot->mode;
            build_layer_keys(settings_snapshot, total_key_count, layer1_keys,
                             &layer1_keys_count, layer2_keys,
                             &layer2_keys_count);

            reset_handler_state(pressed_keys, current_values, total_key_count,
                                &second_layer_active, &third_layer_active,
                                &kb_report, &mouse_report);

            prev_kb_report = kb_report;
            prev_mouse_report = mouse_report;
            continue;

        case KBH_THREAD_MSG_SLAVE_KEYS_RESET:
            memset(&pressed_keys[cfg->key_count], 0,
                   cfg->key_count_slave * sizeof(bool));
            memset(&current_values[cfg->key_count], 0,
                   cfg->key_count_slave * sizeof(uint16_t));

            mouseemu_value_handler(settings_snapshot, current_values,
                                   pressed_keys, &mouse_report);

            if (!mouse_reports_equal(&mouse_report, &prev_mouse_report)) {
                send_mouse_report(&mouse_report);
                prev_mouse_report = mouse_report;
            }

            goto handle_kb_report;

            continue;

        case KBH_THREAD_MSG_KEY:
            if (msg.key >= total_key_count) {
                LOG_WRN("Ignoring out-of-range key %u", msg.key);
                continue;
            }

            if (active_mode == KB_MODE_RACE) {
                continue;
            }

            pressed_keys[msg.key] = msg.status;

            if (active_mode == KB_MODE_NORMAL ||
                active_mode == KB_MODE_MOUSESIM) {
                uint8_t resolved_hid =
                    resolve_hid(settings_snapshot, msg.key, second_layer_active,
                                third_layer_active);

                LOG_DBG("Key %s idx %u", msg.status ? "pressed" : "released",
                        msg.key);

                if (resolved_hid == KEY_LAYER1) {
                    if (handle_layer_key(&msg, pressed_keys,
                                         &second_layer_active, layer1_keys,
                                         layer1_keys_count)) {
                        goto handle_kb_report;
                    }
                    continue;
                }

                if (resolved_hid == KEY_LAYER2) {
                    if (handle_layer_key(&msg, pressed_keys,
                                         &third_layer_active, layer2_keys,
                                         layer2_keys_count)) {
                        goto handle_kb_report;
                    }
                    continue;
                }

                if (resolved_hid == KEY_FN) {
                    continue;
                }

            handle_kb_report:
                build_kb_report(&kb_report, settings_snapshot, pressed_keys,
                                total_key_count, second_layer_active,
                                third_layer_active);

                if (!kb_reports_equal(&kb_report, &prev_kb_report)) {
                    send_kb_report(&kb_report);
                    prev_kb_report = kb_report;
                }
                continue;
            }

            if (active_mode == KB_MODE_MOUSESIM) {
                mouseemu_value_handler(settings_snapshot, current_values,
                                       pressed_keys, &mouse_report);

                if (!mouse_reports_equal(&mouse_report, &prev_mouse_report)) {
                    send_mouse_report(&mouse_report);
                    prev_mouse_report = mouse_report;
                }
                continue;
            }

            LOG_WRN("Unknown keyboard mode %u", active_mode);
            continue;

        case KBH_THREAD_MSG_VALUE:
            if (msg.key >= total_key_count) {
                LOG_WRN("Ignoring out-of-range value key %u", msg.key);
                continue;
            }

            current_values[msg.key] = msg.value;

            if (active_mode == KB_MODE_MOUSESIM) {
                mouseemu_value_handler(settings_snapshot, current_values,
                                       pressed_keys, &mouse_report);

                if (!mouse_reports_equal(&mouse_report, &prev_mouse_report)) {
                    send_mouse_report(&mouse_report);
                    prev_mouse_report = mouse_report;
                }
                continue;
            }

            if (active_mode == KB_MODE_RACE) {
                // In race mode the key still has to be past the threshold to be
                // accounted for. That's because I don't know of a better way
                // for now.
                double max_percentage = 0;
                int32_t max_index = -1;
                bool pressed_race[total_key_count];
                memset(pressed_race, 0, sizeof(pressed_race));
                for (uint16_t i = 0; i < total_key_count; ++i) {
                    if (pressed_keys[i]) {
                        uint16_t threshold = settings_snapshot->thresholds[i];
                        double percentage_pressed =
                            (double)(current_values[i] - threshold) /
                            (settings_snapshot->maximums[i] - threshold);
                        if (percentage_pressed > max_percentage) {
                            max_percentage = percentage_pressed;
                            max_index = i;
                        }
                    }
                }

                if (max_index != -1) {
                    pressed_race[max_index] = true;
                }

                build_kb_report(&kb_report, settings_snapshot, pressed_race,
                                total_key_count, second_layer_active,
                                third_layer_active);

                if (!kb_reports_equal(&kb_report, &prev_kb_report)) {
                    send_kb_report(&kb_report);
                    prev_kb_report = kb_report;
                }
            }
            continue;

        default:
            LOG_WRN("Unknown kb handler thread msg type %u", msg.type);
            continue;
        }
    }
}

static inline void
kb_handler_sm_key_handler(struct kb_handler_sm_data *dev_data,
                          uint16_t key_index, bool status) {
    struct kbh_thread_msg data = {
        .type = KBH_THREAD_MSG_KEY,
        .key = key_index,
        .status = status,
    };

    int err = k_msgq_put(dev_data->thread_q, &data, K_NO_WAIT);
    if (err) {
        LOG_WRN("Event keys skipped (err %d)", err);
    }
}

static inline void
kb_handler_sm_value_handler(struct kb_handler_sm_data *dev_data,
                            uint16_t key_index, uint16_t value) {
    struct kbh_thread_msg data = {
        .type = KBH_THREAD_MSG_VALUE,
        .key = key_index,
        .value = value,
    };

    int err = k_msgq_put(dev_data->thread_q, &data, K_NO_WAIT);
    if (err) {
        LOG_WRN("Event values skipped (err %d)", err);
    }
}

static void parse_splitlink_keys(const struct kb_handler_sm_config *cfg,
                                 uint8_t *data, size_t data_len,
                                 struct k_msgq *msgq) {
    for (size_t i = 0; i < data_len; ++i) {
        struct kbh_thread_msg msg_data = {
            .type = KBH_THREAD_MSG_KEY,
            .key = (data[i] & 0x7F) + cfg->key_count,
            .status = (data[i] & 0x80) != 0U,
        };

        int err = k_msgq_put(msgq, &msg_data, K_NO_WAIT);
        if (err) {
            LOG_WRN("Event splitlink keys skipped (err %d)", err);
        }
    }
}

static void parse_splitlink_values(const struct kb_handler_sm_config *cfg,
                                   uint8_t *data, size_t data_len,
                                   struct k_msgq *msgq) {
    if ((data_len % 2U) != 0U) {
        LOG_WRN("Ignoring malformed splitlink values payload of len %zu",
                data_len);
        return;
    }

    for (size_t i = 0; i < data_len / 2U; ++i) {
        struct kbh_thread_msg msg_data = {
            .type = KBH_THREAD_MSG_VALUE,
            .key = cfg->key_count + i,
            .value =
                ((uint16_t)data[i * 2U]) | (((uint16_t)data[i * 2U + 1U]) << 8),
        };

        if (msg_data.key >= cfg->key_count + cfg->key_count_slave) {
            break;
        }

        int err = k_msgq_put(msgq, &msg_data, K_NO_WAIT);
        if (err) {
            LOG_WRN("Event splitlink values skipped (err %d)", err);
        }
    }
}

static void kb_handler_sm_splitlink_on_receive(const struct device *dev,
                                               struct k_msgq *msgq,
                                               uint8_t *data, size_t data_len) {
    const struct kb_handler_sm_config *cfg = dev->config;

    if (data_len == 0) {
        return;
    }

    switch (data[0]) {
    case DATA_FLAG_KEYS:
        parse_splitlink_keys(cfg, ++data, data_len - 1, msgq);
        break;
    case DATA_FLAG_VALUES:
        parse_splitlink_values(cfg, ++data, data_len - 1, msgq);
        break;
    case DATA_FLAG_BACKLIGHT:
        break;
    default:
        LOG_ERR("Unknown splitlink data flag '%d' received", data[0]);
        break;
    }
}

static inline void kb_handler_sm_splitlink_on_connect(const struct device *dev,
                                                      struct k_msgq *msgq) {
    ARG_UNUSED(dev);
    ARG_UNUSED(msgq);

    LOG_INF("Splitlink slave connected");
}

static inline void
kb_handler_sm_splitlink_on_disconnect(const struct device *dev,
                                      struct k_msgq *msgq) {
    struct kbh_thread_msg data = {
        .type = KBH_THREAD_MSG_SLAVE_KEYS_RESET,
    };

    ARG_UNUSED(dev);

    LOG_WRN("Splitlink slave disconnected");
    int err = k_msgq_put(msgq, &data, K_NO_WAIT);
    if (err) {
        LOG_WRN("Event slave keys reset skipped (err %d)", err);
    }
}

static void kscans_check(const struct kb_handler_sm_config *cfg) {
    int expected_offset = 0;
    int total_key_count = 0;

    for (uint16_t i = 0; i < cfg->kscans_count; ++i) {
        int offset = kscan_get_idx_offset(cfg->kscans[i]);
        int key_amount = kscan_get_key_amount(cfg->kscans[i]);

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

    if (total_key_count != cfg->key_count) {
        LOG_ERR("KScans total key count != DTS key count (%d != %u)",
                total_key_count, cfg->key_count);
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

static inline void kb_handler_sm_on_settings_update(kb_settings_t *settings,
                                                    const struct device *dev) {
    const struct kb_handler_sm_config *cfg = dev->config;
    struct kb_handler_sm_data *data = dev->data;
    struct kbh_thread_msg msg = {
        .type = KBH_THREAD_MSG_SETTINGS_SYNC,
    };

    if (data->thread_started) {
        k_thread_suspend(&data->thread);
        k_msgq_purge(data->thread_q);
    }

    memcpy(&data->settings_snapshot, settings, sizeof(kb_settings_t));
    mouseemu_check(cfg->key_count + cfg->key_count_slave,
                   &data->settings_snapshot.mouseemu);

    for (size_t i = 0; i < cfg->kscans_count; ++i) {
        const struct device *kscan = cfg->kscans[i];
        int idx_offset = kscan_get_idx_offset(kscan);

        if (idx_offset < 0) {
            LOG_ERR("Unable to get idx offset for KScan instance %s "
                    "(err %d)",
                    dev->name, idx_offset);
            continue;
        }

        int err =
            kscan_set_thresholds(kscan, &settings->thresholds[idx_offset]);
        if (err) {
            LOG_ERR("Unable to set thresholds for KScan instance %s "
                    "(err %d)",
                    dev->name, err);
            continue;
        }
    }

    if (data->thread_started) {
        k_thread_resume(&data->thread);
    } else {
        k_thread_create(&data->thread, data->thread_stack,
                        CONFIG_KB_HANDLER_THREAD_STACK_SIZE, kb_handler_thread,
                        (void *)dev, NULL, NULL,
                        CONFIG_KB_HANDLER_THREAD_PRIORITY, 0, K_NO_WAIT);
        k_thread_name_set(&data->thread, "kb_handler_sm");
        data->thread_started = true;
    }

    int err = k_msgq_put(data->thread_q, &msg, K_NO_WAIT);
    if (err) {
        LOG_WRN("Event settings sync skipped (err %d)", err);
    }
}

static int kb_handler_sm_init(const struct device *dev) {
    const struct kb_handler_sm_config *cfg = dev->config;
    struct kb_handler_sm_data *data = dev->data;

    data->thread_started = false;

    if (!device_is_ready(cfg->splitlink)) {
        LOG_ERR("Splitlink device '%s' is not ready", cfg->splitlink->name);
        return -ENODEV;
    }

    for (uint16_t i = 0; i < cfg->kscans_count; ++i) {
        if (!device_is_ready(cfg->kscans[i])) {
            LOG_ERR("KScan device '%s' is not ready", cfg->kscans[i]->name);
            return -ENODEV;
        }
    }

    kscans_check(cfg);
    mouseemu_check(cfg->key_count + cfg->key_count_slave,
                   &cfg->default_mouseemu);

    return 0;
}

static int get_default_thresholds(const struct device *dev, uint16_t *buffer) {
    const struct kb_handler_sm_config *cfg;

    if (!dev) {
        return -EINVAL;
    }

    cfg = dev->config;

    if (buffer) {
        for (size_t i = 0; i < cfg->kscans_count; ++i) {
            const struct device *kscan = cfg->kscans[i];
            int key_amount = kscan_get_key_amount(kscan);
            int idx_offset = kscan_get_idx_offset(kscan);
            int res;

            if (key_amount < 0) {
                return key_amount;
            }
            if (idx_offset < 0) {
                return idx_offset;
            }

            res = kscan_get_default_thresholds(kscan, &buffer[idx_offset]);
            if (res < 0) {
                return res;
            }
        }
    }

    return 0;
}

static int get_default_keymap_layer1(const struct device *dev,
                                     uint8_t *buffer) {
    const struct kb_handler_sm_config *cfg;

    if (!dev) {
        return -EINVAL;
    }

    cfg = dev->config;

    if (buffer) {
        memcpy(buffer, cfg->default_keymap_layer1,
               (cfg->key_count + cfg->key_count_slave) * sizeof(uint8_t));
    }

    return 0;
}

static int get_default_keymap_layer2(const struct device *dev,
                                     uint8_t *buffer) {
    const struct kb_handler_sm_config *cfg;

    if (!dev) {
        return -EINVAL;
    }

    cfg = dev->config;

    if (buffer) {
        memcpy(buffer, cfg->default_keymap_layer2,
               (cfg->key_count + cfg->key_count_slave) * sizeof(uint8_t));
    }

    return 0;
}

static int get_default_keymap_layer3(const struct device *dev,
                                     uint8_t *buffer) {
    const struct kb_handler_sm_config *cfg;

    if (!dev) {
        return -EINVAL;
    }

    cfg = dev->config;

    if (buffer) {
        memcpy(buffer, cfg->default_keymap_layer3,
               (cfg->key_count + cfg->key_count_slave) * sizeof(uint8_t));
    }

    return 0;
}

static int get_default_mouseemu(const struct device *dev,
                                kb_mouseemu_settings_t *buffer) {
    const struct kb_handler_sm_config *cfg;

    if (!dev) {
        return -EINVAL;
    }

    cfg = dev->config;

    if (buffer) {
        memcpy(buffer, &cfg->default_mouseemu, sizeof(*buffer));
    }

    return 0;
}

DEVICE_API(kb_handler, kb_handler_sm_api) = {
    .get_default_thresholds = get_default_thresholds,
    .get_default_keymap_layer1 = get_default_keymap_layer1,
    .get_default_keymap_layer2 = get_default_keymap_layer2,
    .get_default_keymap_layer3 = get_default_keymap_layer3,
    .get_default_mouseemu = get_default_mouseemu,
};

#define KSCAN_DEV_AND_COMMA(node_id, prop, idx)                                \
    DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define TOTAL_KB_KEY_COUNT(inst)                                               \
    (DT_INST_PROP(inst, key_count) + DT_INST_PROP(inst, key_count_slave))

#define KB_HANDLER_SM_DEFINE(inst)                                             \
    static const struct device *__kb_handler_sm_kscans__##inst[] = {           \
        DT_INST_FOREACH_PROP_ELEM(inst, kscans, KSCAN_DEV_AND_COMMA)};         \
                                                                               \
    static const uint8_t                                                       \
        __kb_handler_sm_default_keymap_layer1__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] = DT_INST_PROP(inst, default_keymap_layer1);                \
    static const uint8_t                                                       \
        __kb_handler_sm_default_keymap_layer2__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] =                                                           \
            DT_INST_PROP_OR(inst, default_keymap_layer2, {KEY_NOKEY});         \
    static const uint8_t                                                       \
        __kb_handler_sm_default_keymap_layer3__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] =                                                           \
            DT_INST_PROP_OR(inst, default_keymap_layer3, {KEY_NOKEY});         \
                                                                               \
    K_MSGQ_DEFINE(__kb_handler_sm_thread_queue__##inst,                        \
                  sizeof(struct kbh_thread_msg), CONFIG_KB_HANDLER_MSGQ_SIZE,  \
                  4);                                                          \
    K_THREAD_STACK_DEFINE(__kb_handler_sm_thread_stack__##inst,                \
                          CONFIG_KB_HANDLER_THREAD_STACK_SIZE);                \
                                                                               \
    static const struct kb_handler_sm_config __kb_handler_sm_config__##inst =  \
        {                                                                      \
            .key_count = DT_INST_PROP(inst, key_count),                        \
            .key_count_slave = DT_INST_PROP(inst, key_count_slave),            \
            .kscans = __kb_handler_sm_kscans__##inst,                          \
            .kscans_count = DT_INST_PROP_LEN(inst, kscans),                    \
            .splitlink = DEVICE_DT_GET(DT_INST_PHANDLE(inst, splitlink)),      \
            .default_keymap_layer1 =                                           \
                __kb_handler_sm_default_keymap_layer1__##inst,                 \
            .default_keymap_layer2 =                                           \
                __kb_handler_sm_default_keymap_layer2__##inst,                 \
            .default_keymap_layer3 =                                           \
                __kb_handler_sm_default_keymap_layer3__##inst,                 \
            .default_mouseemu =                                                \
                {                                                              \
                    .enabled = DT_INST_NODE_HAS_PROP(inst, mouseemu_enabled),  \
                    .direction_mode =                                          \
                        DT_INST_ENUM_IDX_OR(inst, mouseemu_direction_mode,     \
                                            KB_MOUSEEMU_DIRECTION_4_WAY),      \
                    .move_keys_count =                                         \
                        DT_INST_PROP_LEN_OR(inst, mouseemu_move_keys, 0),      \
                    .scroll_keys_count =                                       \
                        DT_INST_PROP_LEN_OR(inst, mouseemu_scroll_keys, 0),    \
                    .button_keys_count =                                       \
                        DT_INST_PROP_LEN_OR(inst, mouseemu_button_keys, 0),    \
                    .move_keys =                                               \
                        DT_INST_PROP_OR(inst, mouseemu_move_keys, {0}),        \
                    .scroll_keys =                                             \
                        DT_INST_PROP_OR(inst, mouseemu_scroll_keys, {0}),      \
                    .button_keys =                                             \
                        DT_INST_PROP_OR(inst, mouseemu_button_keys, {0}),      \
                    .move_x_k =                                                \
                        (double)DT_INST_PROP_OR(inst, mouseemu_move_x_k_mul,   \
                                                1) /                           \
                        DT_INST_PROP_OR(inst, mouseemu_move_x_k_div, 1),       \
                    .move_y_k =                                                \
                        (double)DT_INST_PROP_OR(inst, mouseemu_move_y_k_mul,   \
                                                1) /                           \
                        DT_INST_PROP_OR(inst, mouseemu_move_y_k_div, 1),       \
                    .scroll_k =                                                \
                        (double)DT_INST_PROP_OR(inst, mouseemu_scroll_k_mul,   \
                                                1) /                           \
                        DT_INST_PROP_OR(inst, mouseemu_scroll_k_div, 1),       \
                    .move_keys_deadzones = DT_INST_PROP_OR(                    \
                        inst, mouseemu_move_keys_deadzones, {0}),              \
                    .scroll_keys_deadzones = DT_INST_PROP_OR(                  \
                        inst, mouseemu_scroll_keys_deadzones, {0}),            \
                },                                                             \
    };                                                                         \
    BUILD_ASSERT(                                                              \
        DT_INST_PROP_LEN_OR(inst, mouseemu_move_keys, 0) ==                    \
            DT_INST_PROP_LEN_OR(inst, mouseemu_move_keys_deadzones, 0),        \
        "Move keys deadzones should be the same length as move keys and map "  \
        "1:1");                                                                \
    BUILD_ASSERT(                                                              \
        DT_INST_PROP_LEN_OR(inst, mouseemu_scroll_keys, 0) ==                  \
            DT_INST_PROP_LEN_OR(inst, mouseemu_scroll_keys_deadzones, 0),      \
        "Scroll keys deadzones should be the same length as scroll keys and "  \
        "map 1:1");                                                            \
                                                                               \
    static struct kb_handler_sm_data __kb_handler_sm_data__##inst = {          \
        .thread_q = &__kb_handler_sm_thread_queue__##inst,                     \
        .thread_stack = __kb_handler_sm_thread_stack__##inst,                  \
    };                                                                         \
                                                                               \
    static void kb_handler_sm_key_event_handler__##inst(uint16_t key_index,    \
                                                        bool pressed) {        \
        kb_handler_sm_key_handler(&__kb_handler_sm_data__##inst, key_index,    \
                                  pressed);                                    \
    }                                                                          \
                                                                               \
    static void kb_handler_sm_key_value_handler__##inst(uint16_t key_index,    \
                                                        uint16_t value) {      \
        kb_handler_sm_value_handler(&__kb_handler_sm_data__##inst, key_index,  \
                                    value);                                    \
    }                                                                          \
    KSCAN_CB_DEFINE(kb_handler_sm__##inst) = {                                 \
        .on_event = kb_handler_sm_key_event_handler__##inst,                   \
        .on_new_value = kb_handler_sm_key_value_handler__##inst,               \
    };                                                                         \
                                                                               \
    static void kb_handler_sm_splitlink_on_connect__##inst(                    \
        const struct device *dev) {                                            \
        kb_handler_sm_splitlink_on_connect(                                    \
            dev, &__kb_handler_sm_thread_queue__##inst);                       \
    }                                                                          \
    static void kb_handler_sm_splitlink_on_disconnect__##inst(                 \
        const struct device *dev) {                                            \
        kb_handler_sm_splitlink_on_disconnect(                                 \
            dev, &__kb_handler_sm_thread_queue__##inst);                       \
    }                                                                          \
    static void kb_handler_sm_splitlink_on_receive__##inst(                    \
        const struct device *dev, uint8_t *data, size_t data_len) {            \
        kb_handler_sm_splitlink_on_receive(                                    \
            dev, &__kb_handler_sm_thread_queue__##inst, data, data_len);       \
    }                                                                          \
    SPLITLINK_CB_DEFINE(kb_handler_sm) = {                                     \
        .connect_cb = kb_handler_sm_splitlink_on_connect__##inst,              \
        .disconnect_cb = kb_handler_sm_splitlink_on_disconnect__##inst,        \
        .on_receive_cb = kb_handler_sm_splitlink_on_receive__##inst,           \
    };                                                                         \
                                                                               \
    DEVICE_DT_INST_DEFINE(                                                     \
        inst, kb_handler_sm_init, NULL, &__kb_handler_sm_data__##inst,         \
        &__kb_handler_sm_config__##inst, POST_KERNEL,                          \
        CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &kb_handler_sm_api);               \
                                                                               \
    void kb_handler_sm_on_settings_update__##inst(kb_settings_t *settings) {   \
        kb_handler_sm_on_settings_update(settings, DEVICE_DT_INST_GET(inst));  \
    }                                                                          \
                                                                               \
    ON_SETTINGS_UPDATE_DEFINE(kb_handler_settings_update,                      \
                              kb_handler_sm_on_settings_update__##inst);

DT_INST_FOREACH_STATUS_OKAY(KB_HANDLER_SM_DEFINE)
