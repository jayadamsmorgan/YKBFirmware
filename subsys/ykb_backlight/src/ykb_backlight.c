#include <subsys/ykb_backlight.h>

#include "lumiscript_vm.h"

#include <subsys/kb_settings.h>
#include <subsys/zephyr_user_helpers.h>

#include <drivers/kscan.h>

#include <math.h>
#include <zephyr/drivers/led_strip.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ykb_backlight, CONFIG_YKB_BACKLIGHT_LOG_LEVEL);

#define DEFAULT_THREAD_SLEEP_MS 5

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

#define MAX_ABS_BRIGHTNESS_PERCENT                                             \
    Z_USER_PROP_OR(ykb_backlight_max_abs_brightness, 20)
static const double max_absolute_brightness =
    ((double)MAX_ABS_BRIGHTNESS_PERCENT) / 100.0;
BUILD_ASSERT(MAX_ABS_BRIGHTNESS_PERCENT >= 1 &&
                 MAX_ABS_BRIGHTNESS_PERCENT <= 100,
             "ykb-backlight-max-abs-brightness should be in the range [1-100]");

static const uint16_t idx_offset = Z_USER_PROP_OR(ykb_backlight_idx_offset, 0);

static struct k_thread ykb_backlight_thread;
K_THREAD_STACK_DEFINE(ykb_backlight_thread_stack,
                      CONFIG_YKB_BL_THREAD_STACK_SIZE);

static K_MUTEX_DEFINE(ykb_bl_mut);

ykb_backlight_settings_t default_backlight_settings =
    {
        .on = true,
        .script_amount = 5,
        .active_script_index = 4,
        .speed = 1.0,
        .brightness = 1.0,
        .names =
            {
                "static",
                "animation",
                "nested",
                "white",
                "ripple",
            },
        .offsets = {0, 64, 217, 463, 519, 1301},
        .backlight_data =
            {
                // Static red
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
                9, 8, 1, 7, 0, 1, 3, 0, 22, 2, 3, 22, 5, 3, 25, 26,
                // Nested
                73, 77, 85, 76, 1, 0, 2, 0, 12, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
                4, 0, 0, 0, 1, 0, 0, 0, 21, 0, 0, 0, 132, 0, 0, 0, 10, 215, 163,
                61, 0, 0, 180, 67, 0, 0, 200, 66, 154, 153, 153, 62, 0, 0, 0, 0,
                0, 0, 127, 67, 51, 51, 35, 64, 154, 153, 153, 63, 0, 0,
                72, 66, 102, 102, 102, 64, 0, 0, 160, 66, 0, 0, 12, 66, 0, 0, 0,
                0, 0, 0, 0, 0, 26, 3, 0, 0, 2, 2, 2, 3, 9, 1, 0, 0, 9, 7, 1, 1,
                0, 11, 4, 0, 0, 26, 2, 4, 24, 14, 0, 1, 2, 0, 6, 0, 0,
                23, 39, 0, 5, 0, 0, 2, 2, 1, 3, 0, 9, 2, 3, 9, 8, 1, 4, 0, 1, 2,
                0, 22, 2, 3, 6, 0, 0, 5, 0, 0, 1, 4, 0, 18, 24, 72, 0, 1, 5, 0,
                5, 0, 0, 1, 6, 0, 9, 5, 0, 0, 1, 7, 0, 9, 22, 4, 3, 23, 130,
                0, 2, 0, 1, 8, 0, 16, 24, 107, 0, 3, 0, 0, 2, 1, 1, 9, 0, 9, 7,
                1, 1, 0, 11, 1, 10, 0, 1, 11, 0, 22, 5, 3, 23, 130, 0, 3, 0, 0,
                2, 0, 1, 9, 0, 9, 7, 1, 1, 0, 11, 1, 10, 0, 1, 11, 0, 22, 5, 3,
                25, 26,
                // Static full white
                73, 77, 85, 76, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 0, 0,
                127, 67, 26, 26, 1, 0, 0, 1, 0, 0, 1, 0, 0, 22, 4, 3, 25, 26,
                // Ripple
                73, 77, 85, 76, 1, 0, 2, 0, 12, 0, 0, 0, 17, 0, 0, 0, 1, 0, 0,
                0, 9, 0, 0, 0, 1, 0, 0, 0, 137, 0, 0, 0, 232, 1, 0, 0, 0, 0,
                150, 68, 0, 0, 0, 0, 0, 0, 128, 63, 0, 0, 0, 64, 0, 0,
                128, 64, 0, 0, 132, 67, 0, 0, 156, 66, 0, 0, 200, 66, 174,
                71, 225, 62, 0, 0, 160, 64, 10, 215, 163, 61, 10, 215,
                35, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 28, 70, 0,
                64, 28, 70, 0, 64, 28, 70, 0, 64, 28, 70, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 3, 12, 0,
                24, 34, 0, 3, 8, 0, 2, 2, 2, 3, 9, 7, 4, 8, 0, 3, 8, 0, 1, 0, 0,
                18, 24, 34, 0, 1, 1, 0, 4, 12, 0, 3, 13, 0, 24, 68, 0, 3, 9, 0,
                2, 2, 2, 3, 9, 7, 4, 9, 0, 3, 9, 0, 1, 0, 0, 18, 24, 68, 0, 1,
                1, 0, 4, 13, 0, 3, 14, 0, 24, 102, 0, 3, 10, 0, 2, 2, 2, 3, 9,
                7, 4, 10, 0, 3, 10, 0, 1, 0, 0, 18, 24, 102, 0, 1, 1, 0, 4,
                14, 0, 3, 15, 0, 24, 136, 0, 3, 11, 0, 2, 2, 2, 3, 9, 7, 4,
                11, 0, 3, 11, 0, 1, 0, 0, 18, 24, 136, 0, 1, 1, 0, 4, 15, 0,
                26, 2, 4, 5, 0, 0, 13, 20, 24, 151, 0, 3, 16, 0, 1, 1, 0,
                14, 24, 45, 0, 2, 0, 4, 0, 0, 2, 1, 4, 4, 0, 1, 1, 0, 4, 8, 0,
                1, 2, 0, 4, 12, 0, 23, 137, 0, 3, 16, 0, 1, 2, 0, 14, 24, 80, 0,
                2, 0, 4, 1, 0, 2, 1, 4, 5, 0, 1, 1, 0, 4, 9, 0, 1, 2, 0, 4,
                13, 0, 23, 137, 0, 3, 16, 0, 1, 3, 0, 14, 24, 115, 0, 2, 0, 4,
                2, 0, 2, 1, 4, 6, 0, 1, 1, 0, 4, 10, 0, 1, 2, 0, 4, 14, 0,
                23, 137, 0, 2, 0, 4, 3, 0, 2, 1, 4, 7, 0, 1, 1, 0, 4, 11, 0, 1,
                2, 0, 4, 15, 0, 3, 16, 0, 1, 2, 0, 7, 1, 4, 0, 11, 4, 16, 0, 2,
                4, 6, 0, 0, 1, 5, 0, 1, 6, 0, 3, 12, 0, 24, 237, 0, 1, 7, 0, 2,
                0, 2, 1, 3, 0, 0, 3, 4, 0, 22, 3, 4, 3, 8, 0, 1, 8, 0, 9, 8,
                22, 1, 1, 1, 9, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 1, 7, 0, 3,
                8, 0, 1, 10, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 9, 1, 11, 0,
                9, 23, 240, 0, 1, 1, 0, 3, 13, 0, 24, 59, 1, 1, 7, 0, 2, 0, 2,
                1, 3, 1, 0, 3, 5, 0, 22, 3, 4, 3, 9, 0, 1, 8, 0, 9, 8, 22, 1, 1,
                1, 9, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 1, 7, 0, 3, 9, 0, 1,
                10, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 9, 1, 11, 0, 9,
                23, 62, 1, 1, 1, 0, 22, 11, 2, 3, 14, 0, 24, 140, 1, 1, 7, 0, 2,
                0, 2, 1, 3, 2, 0, 3, 6, 0, 22, 3, 4, 3, 10, 0, 1, 8, 0, 9, 8,
                22, 1, 1, 1, 9, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 1, 7, 0, 3,
                10, 0, 1, 10, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 9, 1, 11, 0,
                9, 23, 143, 1, 1, 1, 0, 3, 15, 0, 24, 218, 1, 1, 7, 0, 2, 0, 2,
                1, 3, 3, 0, 3, 7, 0, 22, 3, 4, 3, 11, 0, 1, 8, 0, 9, 8, 22, 1,
                1, 1, 9, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 1, 7, 0, 3, 11, 0,
                1, 10, 0, 9, 8, 1, 1, 0, 1, 7, 0, 22, 2, 3, 9, 1, 11, 0, 9,
                23, 221, 1, 1, 1, 0, 22, 11, 2, 22, 11, 2, 22, 5, 3, 25, 26
                //
            },
};

static int64_t prev_update = 0;
static float cur_speed = 1.0;
static float cur_brightness = 1.0;
static bool on = true;

static bool script_loaded = false;

static uint16_t press[CONFIG_KB_SETTINGS_KEY_COUNT] = {0};
static bool pressed[CONFIG_KB_SETTINGS_KEY_COUNT] = {0};

static uint32_t thread_sleep_time = DEFAULT_THREAD_SLEEP_MS;

static struct led_rgb _buffer1[CONFIG_KB_SETTINGS_KEY_COUNT] = {0};
static struct led_rgb _buffer2[CONFIG_KB_SETTINGS_KEY_COUNT] = {0};
static struct led_rgb *buf_front = _buffer1;
static struct led_rgb *buf_back = _buffer2;

static inline uint8_t apply_brightness(uint8_t color) {
    double result =
        (double)color * max_absolute_brightness * (double)cur_brightness;
    return (uint8_t)ceil(result);
}

static inline void clear_state() {
    memset(buf_back, 0, sizeof(struct led_rgb) * CONFIG_KB_SETTINGS_KEY_COUNT);
    struct led_rgb *tmp = buf_front;
    buf_front = buf_back;
    buf_back = tmp;

    int err = led_strip_update_rgb(led_strip, buf_front,
                                   CONFIG_KB_SETTINGS_KEY_COUNT);
    if (err) {
        LOG_ERR("clear_state: led_strip_update_rgb: %d", err);
        return;
    }

    if (script_loaded) {
        err = lumiscript_reset_state();
        if (err) {
            LOG_ERR("lumiscript_reset_state: %d", err);
            return;
        }
    }
}

static void ykb_backlight_thread_handler(void *a, void *b, void *c) {
    while (true) {
        k_mutex_lock(&ykb_bl_mut, K_FOREVER);
        if (!script_loaded) {
            goto cont;
        }
        if (!on) {
            goto cont;
        }

        int64_t cur_uptime = k_uptime_get();
        int64_t dt = cur_uptime - prev_update;
        prev_update = cur_uptime;

        lumi_vm_inputs inputs = {
            .dt = dt,
            .speed = cur_speed,
        };
        int err = lumiscript_run_update(&inputs);
        if (err) {
            LOG_ERR("Lumiscirpt update: %d", err);
            goto cont;
        }

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
            buf_back[led_map[i]].r =
                apply_brightness((output.color >> 16) & 0xFF);
            buf_back[led_map[i]].g =
                apply_brightness((output.color >> 8) & 0xFF);
            buf_back[led_map[i]].b = apply_brightness(output.color & 0xFF);
        }
        struct led_rgb *tmp = buf_front;
        buf_front = buf_back;
        buf_back = tmp;
        err = led_strip_update_rgb(led_strip, buf_front,
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

    k_mutex_lock(&ykb_bl_mut, K_FOREVER);

    if (!init_success) {
        return;
    }

    clear_state();

    script_loaded = false;

    cur_speed = settings->backlight.speed;
    thread_sleep_time = settings->backlight.thread_sleep_ms;
    cur_brightness = settings->backlight.brightness;
    on = settings->backlight.on;

    uint16_t cur_idx = settings->backlight.active_script_index;
    if (cur_idx >= settings->backlight.script_amount) {
        LOG_ERR("Active script index is out of bounds!");
        goto defer;
    }
    uint32_t start_offset = settings->backlight.offsets[cur_idx];
    uint32_t end_offset;
    if (cur_idx == KB_SETTINGS_MAX_SCRIPTS_POSSIBLE - 1) {
        end_offset = CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_STORAGE_LEN - 1;
    } else {
        end_offset = settings->backlight.offsets[cur_idx + 1];
    }

    char *script_name = settings->backlight.names[cur_idx];
    LOG_INF("Loading lumiscript '%s'", script_name);
    int err = lumiscript_load(&settings->backlight.backlight_data[start_offset],
                              end_offset - start_offset);
    if (err) {
        LOG_ERR("Unable to load lumiscript '%s' (%d)", script_name, err);
        goto defer;
    }

    err = lumiscript_run_init();
    if (err) {
        LOG_ERR("Unable to run lumiscript '%s' init (%d)", script_name, err);
        goto defer;
    }

    script_loaded = true;
    LOG_INF("Successfully loaded lumiscript '%s'", script_name);

defer:
    k_mutex_unlock(&ykb_bl_mut);
}

ON_SETTINGS_UPDATE_DEFINE(ykb_backlight, on_settings_update);

static int ykb_backlight_init(void) {

    k_mutex_lock(&ykb_bl_mut, K_FOREVER);

    LOG_INF("BL INIT!");
    if (!device_is_ready(led_strip)) {
        LOG_ERR("LED strip is not ready");
        k_mutex_unlock(&ykb_bl_mut);
        return -1;
    }
    for (uint16_t i = 0; i < TOTAL_KEY_COUNT; ++i) {
        if (x_coordinates[i] > 1000) {
            LOG_ERR("X coordinate %d is not in the range of [0-1000]", i);
            k_mutex_unlock(&ykb_bl_mut);
            return -1;
        }
        if (y_coordinates[i] > 1000) {
            LOG_ERR("Y coordinate %d is not in the range of [0-1000]", i);
            k_mutex_unlock(&ykb_bl_mut);
            return -1;
        }
    }

    k_thread_create(&ykb_backlight_thread, ykb_backlight_thread_stack,
                    K_THREAD_STACK_SIZEOF(ykb_backlight_thread_stack),
                    ykb_backlight_thread_handler, NULL, NULL, NULL,
                    CONFIG_YKB_BL_THREAD_PRIORITY, 0, K_NO_WAIT);

    init_success = true;

    LOG_INF("YKB Backlight init ok.");

    k_mutex_unlock(&ykb_bl_mut);

    return 0;
}

SYS_INIT(ykb_backlight_init, POST_KERNEL, CONFIG_YKB_BL_INIT_PRIORITY);

const ykb_backlight_settings_t *ykb_backlight_get_default_settings() {
    return &default_backlight_settings;
}
