#include "kb_handler_internal.h"

#include <dt-bindings/kb-handler/kb-key-codes.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <string.h>

LOG_MODULE_DECLARE(kb_handler);

#define KSCAN_DEV_AND_COMMA(node_id, prop, idx)                                \
    DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *const kscans[] = {DT_FOREACH_PROP_ELEM(
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
    .button_keys_count = Z_USER_PROP_LEN_OR(mouseemu_button_keys, 0),
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

size_t kb_handler_kscan_count(void) { return ARRAY_SIZE(kscans); }

const struct device *kb_handler_get_kscan(size_t idx) {
    if (idx >= ARRAY_SIZE(kscans)) {
        return NULL;
    }

    return kscans[idx];
}

int kb_handler_check_kscans_ready(void) {
    for (size_t i = 0; i < ARRAY_SIZE(kscans); ++i) {
        if (!device_is_ready(kscans[i])) {
            LOG_ERR("KScan device '%s' is not ready", kscans[i]->name);
            return -ENODEV;
        }
    }

    return 0;
}

int kb_handler_validate_kscan_topology(uint16_t expected_key_count) {
    int expected_offset = 0;
    int total_key_count = 0;

    for (size_t i = 0; i < ARRAY_SIZE(kscans); ++i) {
        int offset = kscan_get_idx_offset(kscans[i]);
        int key_amount = kscan_get_key_amount(kscans[i]);

        if (offset < 0) {
            LOG_ERR("Unable to get idx offset for KScan %u (err %d)", (unsigned)i,
                    offset);
            return offset;
        }
        if (key_amount < 0) {
            LOG_ERR("Unable to get key amount for KScan %u (err %d)", (unsigned)i,
                    key_amount);
            return key_amount;
        }

        total_key_count += key_amount;

        if (i != 0U) {
            if (expected_offset < offset) {
                LOG_ERR("Key indices gap between KScans %u and %u "
                        "(expected offset %d, got %d)",
                        (unsigned)i - 1U, (unsigned)i, expected_offset, offset);
                return -EINVAL;
            }
            if (expected_offset > offset) {
                LOG_ERR("Key indices intersection between KScans %u and %u "
                        "(expected offset %d, got %d)",
                        (unsigned)i - 1U, (unsigned)i, expected_offset, offset);
                return -EINVAL;
            }
        }

        expected_offset = offset + key_amount;
    }

    if (total_key_count != expected_key_count) {
        LOG_ERR("KScans total key count != expected key count (%d != %u)",
                total_key_count, expected_key_count);
        return -EINVAL;
    }

    return 0;
}

int kb_handler_get_default_keymap_layer1(uint8_t *buffer) {
    if (buffer) {
        memcpy(buffer, default_keymap_layer1, sizeof(default_keymap_layer1));
    }

    return 0;
}

int kb_handler_get_default_keymap_layer2(uint8_t *buffer) {
    if (buffer) {
        memcpy(buffer, default_keymap_layer2, sizeof(default_keymap_layer2));
    }

    return 0;
}

int kb_handler_get_default_keymap_layer3(uint8_t *buffer) {
    if (buffer) {
        memcpy(buffer, default_keymap_layer3, sizeof(default_keymap_layer3));
    }

    return 0;
}

int kb_handler_get_default_mouseemu(kb_mouseemu_settings_t *buffer) {
    if (buffer) {
        memcpy(buffer, &default_mouseemu, sizeof(default_mouseemu));
    }

    return 0;
}

int kb_handler_get_default_thresholds(uint16_t *buffer) {
    if (!buffer) {
        return 0;
    }

    memset(buffer, 0, TOTAL_KEY_COUNT * sizeof(uint16_t));

    for (size_t i = 0; i < ARRAY_SIZE(kscans); ++i) {
        const struct device *kscan = kscans[i];
        int idx_offset = kscan_get_idx_offset(kscan);
        int key_amount = kscan_get_key_amount(kscan);
        int err;

        if (key_amount < 0) {
            return key_amount;
        }
        if (idx_offset < 0) {
            return idx_offset;
        }

        err = kscan_get_default_thresholds(kscan, &buffer[idx_offset]);
        if (err < 0) {
            return err;
        }
    }

    if (KEY_COUNT_SLAVE > 0U && KEY_COUNT == KEY_COUNT_SLAVE) {
        memcpy(&buffer[KEY_COUNT], buffer, KEY_COUNT * sizeof(uint16_t));
    }

    return 0;
}
