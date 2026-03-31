#include <lib/kb_settings.h>

#include "lumiscript_vm.h"

#include <zephyr/drivers/led_strip.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ykb_strip_backlight, CONFIG_YKB_BACKLIGHT_LOG_LEVEL);

#define Z_USER_PROP(prop) DT_PROP(DT_PATH(zephyr_user), prop)
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

static struct k_thread ykb_backlight_thread;
K_THREAD_STACK_DEFINE(ykb_backlight_thread_stack,
                      CONFIG_YKB_BL_THREAD_STACK_SIZE);

static K_MUTEX_DEFINE(ykb_sb_mut);

typedef struct {
    const char *name;
    const char *description;
    uint8_t *bytecode;
} lumi_script_t;

static uint8_t static_script[] = {
    73, 77, 85, 76, 1, 0, 1,  0,  3, 0, 0,  0,  0, 0, 0,   0,
    0,  0,  0,  0,  3, 0, 0,  0,  1, 0, 0,  0,  1, 0, 0,   0,
    14, 0,  0,  0,  0, 0, 22, 67, 0, 0, 72, 67, 0, 0, 127, 67,
    26, 26, 1,  0,  0, 1, 1,  0,  1, 2, 0,  22, 4, 3, 25,  26,
};

static uint8_t animation_script[] = {
    73, 77, 85, 76,  1,   0,  2,   0,   8,   0,  0, 0,  1,   0,  0, 0,  0,
    0,  0,  0,  7,   0,   0,  0,   1,   0,   0,  0, 17, 0,   0,  0, 63, 0,
    0,  0,  10, 215, 163, 60, 102, 102, 102, 64, 0, 0,  180, 67, 0, 0,  200,
    66, 0,  0,  72,  66,  0,  0,   0,   63,  0,  0, 0,  65,  0,  0, 0,  0,
    0,  0,  0,  0,   26,  3,  0,   0,   2,   2,  1, 0,  0,   9,  2, 3,  9,
    7,  4,  0,  0,   26,  2,  0,   1,   1,   0,  9, 3,  0,   0,  7, 1,  2,
    0,  11, 1,  3,   0,   1,  3,   0,   2,   0,  2, 1,  1,   4,  0, 1,  4,
    0,  22, 3,  4,   3,   0,  0,   1,   5,   0,  9, 8,  22,  1,  1, 1,  6,
    0,  9,  8,  1,   7,   0,  1,   3,   0,   22, 2, 3,  22,  5,  3, 25, 26,
};

static uint8_t *default_scripts[] = {
    static_script,
    animation_script,
};

static void ykb_backlight_thread_handler(void *a, void *b, void *c) {
    while (true) {
        k_sleep(K_MSEC(DEFAULT_THREAD_SLEEP_MS));
    }
}

static void on_settings_update(kb_settings_t *settings) {}

ON_SET_UPDATE_DEFINE(ykb_backlight) {}

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

    return 0;
}
