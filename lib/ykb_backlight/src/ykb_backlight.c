#include <lib/ykb_backlight.h>

#include "lumiscript_vm.h"

#include <lib/kb_settings.h>

#include <drivers/kscan.h>

#include <zephyr/drivers/led_strip.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ykb_strip_backlight, CONFIG_YKB_BACKLIGHT_LOG_LEVEL);

#define Z_USER_PROP(prop) DT_PROP(DT_PATH(zephyr_user), prop)
#define Z_USER_PROP_OR(prop, val) DT_PROP_OR(DT_PATH(zephyr_user), prop, val)
#define Z_USER_DEV(prop) DEVICE_DT_GET(Z_USER_PROP(prop))

#define DEFAULT_THREAD_SLEEP_MS 100

static const struct device *led_strip = Z_USER_DEV(ykb_backlight);

static const uint16_t led_map[] = Z_USER_PROP(ykb_backlight_maps);
BUILD_ASSERT(ARRAY_SIZE(led_map) == TOTAL_KEY_COUNT,
             "ykb-backlight-maps length should be the same as total key count");

static const uint16_t x_coordinates[] = Z_USER_PROP(ykb_backlight_xs);
BUILD_ASSERT(ARRAY_SIZE(x_coordinates) == TOTAL_KEY_COUNT,
             "ykb-backlight-xs length should be the same as total key count");
static const uint16_t y_coordinates[] = Z_USER_PROP(ykb_backlight_ys);
BUILD_ASSERT(ARRAY_SIZE(y_coordinates) == TOTAL_KEY_COUNT,
             "ykb-backlight-ys length should be the same as total key count");

static const uint16_t idx_offset = Z_USER_PROP_OR(ykb_backlight_idx_offset, 0);

static struct k_thread ykb_backlight_thread;
K_THREAD_STACK_DEFINE(ykb_backlight_thread_stack,
                      CONFIG_YKB_BL_THREAD_STACK_SIZE);

static K_MUTEX_DEFINE(ykb_bl_mut);

ykb_backlight_settings_t default_backlight_settings =
    {
        .script_amount = 2,
        .active_script_index = 0,
        .speed = 1.0,
        .names =
            {
                "static",
                "animation",
            },
        .offsets = {0, 64, 217},
        .backlight_data =
            {
                // Static
                73, 77, 85, 76, 1, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 0, 0, 22, 67,
                0, 0, 72, 67, 0, 0, 127, 67, 26, 26, 1, 0, 0, 1, 1, 0, 1, 2, 0,
                22, 4, 3, 25, 26,
                // Animation
                73, 77, 85, 76, 1, 0, 2, 0, 8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                7, 0, 0, 0, 1, 0, 0, 0, 17, 0, 0, 0, 63, 0, 0, 0, 10, 215, 163,
                60, 102, 102, 102, 64, 0, 0, 180, 67, 0, 0, 200, 66, 0, 0,
                72, 66, 0, 0, 0, 63, 0, 0, 0, 65, 0, 0, 0, 0, 0, 0, 0, 0, 26, 3,
                0, 0, 2, 2, 1, 0, 0, 9, 2, 3, 9, 7, 4, 0, 0, 26, 2, 0, 1, 1, 0,
                9, 3, 0, 0, 7, 1, 2, 0, 11, 1, 3, 0, 1, 3, 0, 2, 0, 2, 1, 1, 4,
                0, 1, 4, 0, 22, 3, 4, 3, 0, 0, 1, 5, 0, 9, 8, 22, 1, 1, 1, 6, 0,
                9, 8, 1, 7, 0, 1, 3, 0, 22, 2, 3, 22, 5, 3, 25, 26
                //
            },
};

static int64_t prev_update = 0;
static float cur_speed = 1.0;
static bool script_loaded = false;

static uint16_t press[CONFIG_KB_SETTINGS_KEY_COUNT] = {0};
static bool pressed[CONFIG_KB_SETTINGS_KEY_COUNT] = {0};

static uint32_t thread_sleep_time = DEFAULT_THREAD_SLEEP_MS;

static void ykb_backlight_thread_handler(void *a, void *b, void *c) {
    while (true) {
        k_mutex_lock(&ykb_bl_mut, K_FOREVER);
        if (!script_loaded) {
            goto cont;
        }

        int64_t cur_uptime = k_uptime_get();
        int64_t dt = cur_uptime - prev_update;
        prev_update = cur_uptime;

        lumi_vm_inputs inputs = {.dt = dt, .speed = cur_speed};
        int err = lumiscript_run_update(&inputs);
        if (err) {
            LOG_ERR("Lumiscirpt update: %d", err);
            goto cont;
        }

        struct led_rgb pixels[CONFIG_KB_SETTINGS_KEY_COUNT];
        for (uint16_t i = idx_offset;
             i < idx_offset + CONFIG_KB_SETTINGS_KEY_COUNT; ++i) {
            lumi_vm_output output;
            inputs.x = x_coordinates[i];
            inputs.y = y_coordinates[i];
            inputs.pressed = pressed[i];
            inputs.press = press[i];
            err = lumiscript_run_render(&inputs, i, &output);
            if (err) {
                LOG_ERR("Lumiscript render: %d", err);
                goto cont;
            }
            pixels[led_map[i]].r = (output.color >> 4) & 0xFF;
            pixels[led_map[i]].g = (output.color >> 2) & 0xFF;
            pixels[led_map[i]].b = (output.color) & 0xFF;
        }
        err = led_strip_update_rgb(led_strip, pixels,
                                   CONFIG_KB_SETTINGS_KEY_COUNT);
        if (err) {
            LOG_ERR("led_strip_update_rgb: %d", err);
        }

    cont:
        k_mutex_unlock(&ykb_bl_mut);

        k_sleep(K_MSEC(thread_sleep_time));
    }
}

static void kscan_on_event(uint16_t index, bool value) {
    k_mutex_lock(&ykb_bl_mut, K_FOREVER);
    pressed[index] = value;
    k_mutex_unlock(&ykb_bl_mut);
}

static void kscan_on_value_changed(uint16_t index, uint16_t value) {
    k_mutex_lock(&ykb_bl_mut, K_FOREVER);
    press[index] = value;
    k_mutex_unlock(&ykb_bl_mut);
}

KSCAN_CB_DEFINE(ykb_backlight) = {
    .on_event = kscan_on_event,
    .on_new_value = kscan_on_value_changed,
};

static bool init_success = false;

static void on_settings_update(kb_settings_t *settings) {

    if (!init_success) {
        return;
    }

    k_mutex_lock(&ykb_bl_mut, K_FOREVER);

    cur_speed = settings->backlight.speed;
    thread_sleep_time = settings->backlight.thread_sleep_ms;

    uint16_t cur_idx = settings->backlight.active_script_index;
    if (cur_idx >= settings->backlight.script_amount) {
        LOG_ERR("Active script index is out of bounds!");
        k_mutex_unlock(&ykb_bl_mut);
        return;
    }
    uint32_t start_offset = settings->backlight.offsets[cur_idx];
    uint32_t end_offset;
    if (cur_idx == KB_SETTINGS_MAX_SCRIPTS_POSSIBLE - 1) {
        end_offset = CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_STORAGE_LEN - 1;
    } else {
        end_offset = settings->backlight.offsets[cur_idx + 1];
    }

    int err = lumiscript_load(&settings->backlight.backlight_data[start_offset],
                              end_offset - start_offset);
    if (err) {
        LOG_ERR("Unable to load lumiscript (%d)", err);
    } else {
        err = lumiscript_run_init();
        if (err) {
            LOG_ERR("Unable to run lumiscript init (%d)", err);
        } else {
            script_loaded = true;
        }
    }

    k_mutex_unlock(&ykb_bl_mut);
}

ON_SETTINGS_UPDATE_DEFINE(ykb_backlight, on_settings_update);

static int ykb_backlight_init(void) {
    if (!device_is_ready(led_strip)) {
        LOG_ERR("LED strip is not ready");
        return -1;
    }
    for (uint16_t i = 0; i < TOTAL_KEY_COUNT; ++i) {
        if (x_coordinates[i] > 1000) {
            LOG_ERR("X coordinate %d is not in the range of [0-1000]", i);
            return -1;
        }
        if (y_coordinates[i] > 1000) {
            LOG_ERR("Y coordinate %d is not in the range of [0-1000]", i);
            return -1;
        }
    }

    k_thread_create(&ykb_backlight_thread, ykb_backlight_thread_stack,
                    K_THREAD_STACK_SIZEOF(ykb_backlight_thread_stack),
                    ykb_backlight_thread_handler, NULL, NULL, NULL,
                    CONFIG_YKB_BL_THREAD_PRIORITY, 0, K_NO_WAIT);

    init_success = true;

    LOG_INF("YKB Backlight init ok.");

    return 0;
}

SYS_INIT(ykb_backlight_init, POST_KERNEL, CONFIG_YKB_BL_INIT_PRIORITY);

const ykb_backlight_settings_t *ykb_backlight_get_default_settings() {
    return &default_backlight_settings;
}
