#ifndef LIB_KB_SETTINGS_H_
#define LIB_KB_SETTINGS_H_

#include <zephyr/sys/iterable_sections.h>
#include <zephyr/toolchain.h>

#include <stdbool.h>
#include <stdint.h>

#if CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE
#define TOTAL_KEY_COUNT                                                        \
    (CONFIG_KB_SETTINGS_KEY_COUNT + CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE)
#else
#define TOTAL_KEY_COUNT CONFIG_KB_SETTINGS_KEY_COUNT
#endif // CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE

typedef enum {
    KB_MODE_NORMAL = 0U,
    KB_MODE_RACE = 1U,
    KB_MODE_MOUSESIM = 2U,
} kb_mode_t;

typedef enum {
    KB_MOUSEEMU_DIRECTION_4_WAY = 0U,
    KB_MOUSEEMU_DIRECTION_8_WAY = 1U,
} kb_mouseemu_direction_t;

#define KB_MOUSEEMU_MOVE_KEYS_MAX 8U
#define KB_MOUSEEMU_SCROLL_KEYS_MAX 2U
#define KB_MOUSEEMU_BUTTON_KEYS_MAX 3U

typedef struct {
    uint8_t low_threshold;
    uint8_t crit_threshold;
    uint16_t thread_sleep_ms;
} kb_battsense_settings_t;

#if CONFIG_YKB_BACKLIGHT

#ifndef CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_STORAGE_LEN
#define CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_STORAGE_LEN                           \
    4096 // Space for all the scripts
#endif   // CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_STORAGE_LEN

#define KB_SETTINGS_YKB_BL_SCRIPT_MIN_LEN                                      \
    64 // Assuming each script takes up at least 64 bytes

#ifndef CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_NAME_MAX_LEN
#define CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_NAME_MAX_LEN 20
#endif // CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_NAME_MAX_LEN

#define KB_SETTINGS_MAX_SCRIPTS_POSSIBLE                                       \
    (CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_STORAGE_LEN /                            \
     KB_SETTINGS_YKB_BL_SCRIPT_MIN_LEN)

typedef struct {
    bool on;
    uint16_t active_script_index;
    uint16_t script_amount;
    float speed;
    float brightness;
    uint32_t thread_sleep_ms;
    uint32_t offsets[KB_SETTINGS_MAX_SCRIPTS_POSSIBLE + 1];
    char names[CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_NAME_MAX_LEN + 1]
              [KB_SETTINGS_MAX_SCRIPTS_POSSIBLE];
    uint8_t backlight_data[CONFIG_KB_SETTINGS_YKB_BL_SCRIPT_STORAGE_LEN];
} ykb_backlight_settings_t;

#endif // CONFIG_YKB_BACKLIGHT

typedef struct {

    bool enabled;
    kb_mouseemu_direction_t direction_mode;

    uint8_t move_keys_count;
    uint16_t move_keys[KB_MOUSEEMU_MOVE_KEYS_MAX];

    uint8_t scroll_keys_count;
    uint16_t scroll_keys[KB_MOUSEEMU_SCROLL_KEYS_MAX];

    uint8_t button_keys_count;
    uint16_t button_keys[KB_MOUSEEMU_BUTTON_KEYS_MAX];

    double move_x_k;
    double move_y_k;
    double scroll_k;

    uint16_t move_keys_deadzones[KB_MOUSEEMU_MOVE_KEYS_MAX];
    uint16_t scroll_keys_deadzones[KB_MOUSEEMU_SCROLL_KEYS_MAX];

} kb_mouseemu_settings_t;

typedef struct {

    kb_mode_t mode;

    uint16_t thresholds[TOTAL_KEY_COUNT];
    uint16_t maximums[TOTAL_KEY_COUNT];

    uint8_t mappings_layer1[TOTAL_KEY_COUNT];
    uint8_t mappings_layer2[TOTAL_KEY_COUNT];
    uint8_t mappings_layer3[TOTAL_KEY_COUNT];

    kb_mouseemu_settings_t mouseemu;

    kb_battsense_settings_t battsense;

#if CONFIG_YKB_BACKLIGHT
    ykb_backlight_settings_t backlight;
#endif // CONFIG_YKB_BACKLIGHT

} kb_settings_t;

struct kb_settings_cb {
    void (*on_update)(kb_settings_t *settings);
};

#define ON_SETTINGS_UPDATE_DEFINE(name, cb)                                    \
    STRUCT_SECTION_ITERABLE(kb_settings_cb, name) = {.on_update = cb}

// Copies settings to the provided kb_settings_t pointer.
//
// This function will block until settings are available to copy.
// Returns 0 on success, negative value otherwise.
int kb_settings_get(kb_settings_t *settings);

// Applies new settings and saves them
//
// This funciton will block until settings are availble to save.
// Returns 0 on success, negative value otherwise.
int kb_settings_apply(kb_settings_t *settings);

#endif // LIB_KB_SETTINGS_H_
