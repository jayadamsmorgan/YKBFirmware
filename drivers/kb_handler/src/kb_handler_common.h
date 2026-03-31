#ifndef KB_HANDLER_COMMON_H
#define KB_HANDLER_COMMON_H

#include <lib/kb_settings.h>
#include <lib/usb_connect.h>

#include <dt-bindings/kb-handler/kb-key-codes.h>

#include <zephyr/sys/util_macro.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum kbh_thread_msg_type {
    KBH_THREAD_MSG_KEY = 0U,
    KBH_THREAD_MSG_VALUE,
    KBH_THREAD_MSG_SLAVE_KEYS_RESET,
    KBH_THREAD_MSG_SETTINGS_SYNC,
};

struct kbh_thread_msg {
    enum kbh_thread_msg_type type;
    uint16_t key;
    bool status;
    uint16_t value;
};

static inline void send_report(uint8_t report[8]) {
    usb_connect_send_report(report);
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
        if (keys[i] == 0) {
            keys[i] = hid;
            return true;
        }
    }

    return false;
}

static inline void
build_layer_keys(const kb_settings_t *settings, uint16_t total_key_count,
                 uint16_t *layer1_keys, uint8_t *layer1_keys_count,
                 uint16_t *layer2_keys, uint8_t *layer2_keys_count) {
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

static inline bool handle_layer_key(const struct kbh_thread_msg *msg,
                                    const bool *pressed_keys,
                                    bool *layer_active,
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

static inline void
build_report(uint8_t report[8], const bool *pressed_keys,
             uint16_t total_key_count, bool second_layer_active,
             bool third_layer_active, const uint8_t *keymap_l1,
             const uint8_t *keymap_l2, const uint8_t *keymap_l3) {
    memset(report, 0, 8);

    uint8_t *mods = &report[0];
    uint8_t *keys = &report[2];
    bool overflow = false;

    for (uint16_t i = 0; i < total_key_count; ++i) {
        uint8_t hid;

        if (!pressed_keys[i]) {
            continue;
        }

        hid = third_layer_active    ? keymap_l3[i]
              : second_layer_active ? keymap_l2[i]
                                    : keymap_l1[i];

        if (hid == KEY_LAYER1 || hid == KEY_LAYER2 || hid == KEY_FN ||
            hid == KEY_NOKEY) {
            continue;
        }

        if (is_modifier(hid)) {
            *mods |= mod_bit(hid);
            continue;
        }

        if (!add_key(keys, hid)) {
            overflow = true;
        }
    }

    if (overflow && IS_ENABLED(CONFIG_KB_HANDLER_REPORT_ROLLOVER)) {
        report[2] = 0x01;
        report[3] = 0x01;
        report[4] = 0x01;
        report[5] = 0x01;
        report[6] = 0x01;
        report[7] = 0x01;
    }
}

#endif // KB_HANDLER_COMMON_H
