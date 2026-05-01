#ifndef YKB_BACKLIGHT_H
#define YKB_BACKLIGHT_H

#include <subsys/kb_settings.h>

#include <stddef.h>
#include <stdint.h>

#define DEFAULT_THREAD_SLEEP_MS 5

#define YKB_BACKLIGHT_MAX_ABS_BRIGHTNESS_PERCENT                               \
    Z_USER_PROP_OR(ykb_backlight_max_abs_brightness, 20)

typedef struct {
    size_t key_count;
    const uint16_t *led_map;
    const uint16_t *x_coordinates;
    const uint16_t *y_coordinates;
} ykb_backlight_layout_t;

const ykb_backlight_layout_t *ykb_backlight_get_layout(void);

const ykb_backlight_settings_t *ykb_backlight_get_default_settings(void);

#endif // YKB_BACKLIGHT_H
