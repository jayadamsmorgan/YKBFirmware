#ifndef YKB_BACKLIGHT_H
#define YKB_BACKLIGHT_H

#include <subsys/kb_settings.h>

#define YKB_BACKLIGHT_MAX_ABS_BRIGHTNESS_PERCENT                               \
    Z_USER_PROP_OR(ykb_backlight_max_abs_brightness, 20)

const ykb_backlight_settings_t *ykb_backlight_get_default_settings(void);

#endif // YKB_BACKLIGHT_H
