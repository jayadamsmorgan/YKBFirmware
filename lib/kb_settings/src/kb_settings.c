#include <lib/kb_settings.h>

#ifdef CONFIG_YKB_BACKLIGHT
#include <lib/ykb_backlight.h>
#endif // CONFIG_YKB_BACKLIGHT

#include <drivers/kb_handler.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/toolchain.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define KB_SETTINGS_NS "kb"
#define KB_SETTINGS_ITEM "blob"
#define KB_SETTINGS_KEY KB_SETTINGS_NS "/" KB_SETTINGS_ITEM

LOG_MODULE_REGISTER(kb_settings, CONFIG_KB_SETTINGS_LOG_LEVEL);

const struct device *kb_handler_dev = DEVICE_DT_GET(DT_NODELABEL(kb_handler));

// Increment every time kb_settings_image_t or it's contents change
#define KB_SETTINGS_IMAGE_VERSION 2

typedef struct {
    uint16_t version;
    kb_settings_t settings;
} kb_settings_image_t;

static kb_settings_t kb_settings;
static bool settings_registered = false;
static bool successfully_loaded = false;

static K_MUTEX_DEFINE(kb_settings_mut);

static void
kb_settings_handle_on_update_snapshot(const kb_settings_t *settings) {
    STRUCT_SECTION_FOREACH(kb_settings_cb, callbacks) {
        if (callbacks->on_update) {
            // Creating temp just to make sure user never touches
            // actual settings
            kb_settings_t temp = *settings;
            callbacks->on_update(&temp);
        }
    }
}

static int kb_settings_load_defaults(void) {
    int err;

    k_mutex_lock(&kb_settings_mut, K_FOREVER);

    kb_settings.mode = KB_MODE_NORMAL;

    err = kb_handler_get_default_keymap_layer1(kb_handler_dev,
                                               kb_settings.mappings_layer1);
    if (err) {
        goto cleanup;
    }
    err = kb_handler_get_default_keymap_layer2(kb_handler_dev,
                                               kb_settings.mappings_layer2);
    if (err) {
        goto cleanup;
    }
    err = kb_handler_get_default_keymap_layer3(kb_handler_dev,
                                               kb_settings.mappings_layer3);
    if (err) {
        goto cleanup;
    }

    err =
        kb_handler_get_default_mouseemu(kb_handler_dev, &kb_settings.mouseemu);
    if (err) {
        goto cleanup;
    }

    uint16_t thresholds[TOTAL_KEY_COUNT];
    err = kb_handler_get_default_thresholds(kb_handler_dev, thresholds);
    if (err) {
        goto cleanup;
    }
    for (uint16_t i = 0; i < TOTAL_KEY_COUNT; ++i) {
        kb_settings.thresholds[i] = thresholds[i];
        kb_settings.maximums[i] = 1023;
    }

#if CONFIG_YKB_BACKLIGHT
    const ykb_backlight_settings_t *default_backlight_settings =
        ykb_backlight_get_default_settings();
    memcpy(&kb_settings.backlight, default_backlight_settings,
           sizeof(kb_settings.backlight));
#endif // CONFIG_YKB_BACKLIGHT

cleanup:

    k_mutex_unlock(&kb_settings_mut);

    return err;
}

int kb_settings_handler_set(const char *key, size_t len,
                            settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(key, KB_SETTINGS_ITEM) != 0) {
        return -ENOENT;
    }

    const size_t kb_settings_img_size = sizeof(kb_settings_image_t);
    if (len != kb_settings_img_size) {
        LOG_ERR("Keyboard settings image size mismatch: got %zu, want %zu", len,
                kb_settings_img_size);
        return -EINVAL;
    }

    kb_settings_image_t img;
    ssize_t rlen = read_cb(cb_arg, &img, sizeof(img));

    if (rlen < 0) {
        LOG_ERR("Keyboard settings read_cb error: %d", (int)rlen);
        return -EINVAL;
    }

    if ((size_t)rlen != sizeof(img)) {
        LOG_ERR("Keyboard settings truncated: %zd", rlen);
        return -EINVAL;
    }

    if (img.version != KB_SETTINGS_IMAGE_VERSION) {
        LOG_ERR("Keyboad settings image version mismatch: got %u, want %u",
                img.version, KB_SETTINGS_IMAGE_VERSION);
        return -EINVAL;
    }

    k_mutex_lock(&kb_settings_mut, K_FOREVER);

    kb_settings_t snapshot;
    memcpy(&kb_settings, &img.settings, sizeof(kb_settings_t));
    memcpy(&snapshot, &kb_settings, sizeof(kb_settings_t));
    successfully_loaded = true;

    k_mutex_unlock(&kb_settings_mut);

    kb_settings_handle_on_update_snapshot(&snapshot);

    return 0;
}

int kb_settings_handler_export(int (*export_func)(const char *name,
                                                  const void *val,
                                                  size_t val_len)) {
    kb_settings_image_t img = {
        .version = KB_SETTINGS_IMAGE_VERSION,
    };
    k_mutex_lock(&kb_settings_mut, K_FOREVER);
    memcpy(&img.settings, &kb_settings, sizeof(kb_settings_t));
    k_mutex_unlock(&kb_settings_mut);
    return export_func(KB_SETTINGS_ITEM, &img, sizeof(img));
}

static struct settings_handler kb_settings_handler = {
    .name = KB_SETTINGS_NS,
    .h_set = kb_settings_handler_set,
    .h_export = kb_settings_handler_export,
};

static void kb_settings_save() {
    if (!settings_registered) {
        LOG_WRN(
            "Attempt to save kb_settings but settings API was not registered.");
        return;
    }
    kb_settings_image_t img = {
        .version = KB_SETTINGS_IMAGE_VERSION,
    };
    memcpy(&img.settings, &kb_settings, sizeof(kb_settings));
    int err = settings_save_one(KB_SETTINGS_KEY, &img, sizeof(img));
    if (err) {
        LOG_WRN("Could not save keyboard settings: %d", err);
        return;
    }
    LOG_INF("Keyboard settings saved.");
}

static int kb_settings_init(void) {
    int err;
    int res;
    successfully_loaded = false;
    kb_settings_t snapshot;

    if (!settings_registered) {
        err = settings_subsys_init();
        if (err) {
            LOG_ERR("settings_subsys_init: %d", err);
            goto load_defaults;
        }

        err = settings_register(&kb_settings_handler);
        if (err) {
            LOG_ERR("settings_register: %d", err);
            goto load_defaults;
        }
        settings_registered = true;
    }

    err = settings_load_subtree(KB_SETTINGS_NS);

    if (err || !successfully_loaded) {
        LOG_WRN("No valid keyboard settings found (err %d). Loading defaults.",
                err);
        goto load_defaults;
    }

    return 0;

load_defaults:

    res = kb_settings_load_defaults();
    if (res) {
        LOG_ERR("kb_settings_load_defaults: %d", res);
        k_panic();
        return res;
    }

    kb_settings_save();

    k_mutex_lock(&kb_settings_mut, K_FOREVER);
    memcpy(&snapshot, &kb_settings, sizeof(kb_settings_t));
    k_mutex_unlock(&kb_settings_mut);

    kb_settings_handle_on_update_snapshot(&snapshot);

    return err;
}

SYS_INIT(kb_settings_init, POST_KERNEL, CONFIG_KB_SETTINGS_INIT_PRIORITY);

int kb_settings_get(kb_settings_t *settings) {
    if (!settings) {
        return -EINVAL;
    }
    k_mutex_lock(&kb_settings_mut, K_FOREVER);

    memcpy(settings, &kb_settings, sizeof(kb_settings));

    k_mutex_unlock(&kb_settings_mut);

    return 0;
}

int kb_settings_apply(kb_settings_t *settings) {
    if (!settings) {
        return -EINVAL;
    }
    k_mutex_lock(&kb_settings_mut, K_FOREVER);

    kb_settings_t snapshot;

    memcpy(&kb_settings, settings, sizeof(kb_settings_t));
    memcpy(&snapshot, settings, sizeof(kb_settings_t));

    k_mutex_unlock(&kb_settings_mut);

    kb_settings_save();

    kb_settings_handle_on_update_snapshot(&snapshot);

    return 0;
}
