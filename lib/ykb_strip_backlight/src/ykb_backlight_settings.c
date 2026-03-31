#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/toolchain.h>

#define YKB_SB_SETTINGS_NS "backlight"
#define YKB_SB_SETTINGS_ITEM "blob"
#define YKB_SB_SETTINGS_KEY YKB_SB_SETTINGS_NS "/" YKB_SB_SETTINGS_ITEM

LOG_MODULE_DECLARE(ykb_strip_backlight);

typedef struct {
} ykb_backlight_settings_t;
