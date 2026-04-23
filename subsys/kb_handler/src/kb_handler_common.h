#ifndef KB_HANDLER_COMMON_H
#define KB_HANDLER_COMMON_H

#include <subsys/kb_handler.h>

#include <subsys/kb_settings.h>
#include <subsys/zephyr_user_helpers.h>

#include <drivers/kscan.h>

#include <string.h>

#define KSCAN_DEV_AND_COMMA(node_id, prop, idx)                                \
    DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define KEY_COUNT Z_USER_PROP(kb_handler_key_count)
#define KEY_COUNT_SLAVE Z_USER_PROP(kb_handler_key_count_slave)

static const struct device *kscans[] = {DT_FOREACH_PROP_ELEM(
    DT_PATH(zephyr_user), kb_handler_kscans, KSCAN_DEV_AND_COMMA)};

static const uint8_t default_keymap_layer1[TOTAL_KEY_COUNT] =
    Z_USER_PROP(kb_handler_default_keymap_layer1);
static const uint8_t default_keymap_layer2[TOTAL_KEY_COUNT] =
    Z_USER_PROP_OR(kb_handler_default_keymap_layer2, {0});
static const uint8_t default_keymap_layer3[TOTAL_KEY_COUNT] =
    Z_USER_PROP_OR(kb_handler_defualt_keymap_layer3, {0});

static const kb_mouseemu_settings_t default_mouseemu = {
    .enabled = Z_USER_HAS_PROP(mouseemu_enabled),

    .direction_mode = Z_USER_HAS_PROP(mouseemu_8way),

    .move_keys = Z_USER_PROP_OR(mouseemu_move_keys, {0}),
    .move_keys_count = Z_USER_PROP_LEN_OR(mouseemu_move_keys, 0),

    .button_keys = Z_USER_PROP_OR(mouseemu_button_keys, {0}),
    .button_keys_count = Z_USER_PROP_OR(mouseemu_button_keys, 0),

    .scroll_keys = Z_USER_PROP_OR(mouseemu_scroll_keys, {0}),
    .scroll_keys_count = Z_USER_PROP_LEN_OR(mouseemu_scroll_keys, 0),

    .move_x_k = (double)Z_USER_PROP_OR(mouseemu_move_x_k_mul, 1) /
                Z_USER_PROP_OR(mouseemu_move_x_k_div, 1),
    .move_y_k = (double)Z_USER_PROP_OR(mouseemu_move_y_k_mul, 1) /
                Z_USER_PROP_OR(mouseemu_move_y_k_div, 1),
    .scroll_k = (double)Z_USER_PROP_OR(mouseemu_scroll_k_mul, 1) /
                Z_USER_PROP_OR(mouseemu_scroll_k_div, 1),

    .move_keys_deadzones = Z_USER_PROP_OR(mouseemu_move_keys_deadzones, {0}),
    .scroll_keys_deadzones =
        Z_USER_PROP_OR(mouseemu_scroll_keys_deadzones, {0}),
};

int kb_handler_get_default_keymap_layer1(uint8_t *buffer) {
    if (buffer) {
        memcpy(buffer, default_keymap_layer1,
               TOTAL_KEY_COUNT * sizeof(uint8_t));
    }

    return 0;
}

int kb_handler_get_default_keymap_layer2(uint8_t *buffer) {
    if (buffer) {
        memcpy(buffer, default_keymap_layer2,
               TOTAL_KEY_COUNT * sizeof(uint8_t));
    }

    return 0;
}

int kb_handler_get_default_keymap_layer3(uint8_t *buffer) {
    if (buffer) {
        memcpy(buffer, default_keymap_layer3,
               TOTAL_KEY_COUNT * sizeof(uint8_t));
    }

    return 0;
}

int kb_handler_get_default_mouseemu(kb_mouseemu_settings_t *buffer) {
    if (buffer) {
        memcpy(buffer, &default_mouseemu, sizeof(kb_mouseemu_settings_t));
    }

    return 0;
}

int kb_handler_get_default_thresholds(uint16_t *buffer) {
    if (buffer) {
        for (size_t i = 0; i < ARRAY_SIZE(kscans); ++i) {
            const struct device *kscan = kscans[i];
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

#endif // KB_HANDLER_COMMON_H
