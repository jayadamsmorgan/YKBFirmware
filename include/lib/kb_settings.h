#ifndef LIB_KB_SETTINGS_H_
#define LIB_KB_SETTINGS_H_

#include <zephyr/sys/iterable_sections.h>
#include <zephyr/toolchain.h>

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

typedef struct {

    kb_mode_t mode;

    uint16_t thresholds[TOTAL_KEY_COUNT];
    uint16_t minimums[TOTAL_KEY_COUNT];
    uint16_t maximums[TOTAL_KEY_COUNT];

    uint8_t mappings_layer1[TOTAL_KEY_COUNT];
    uint8_t mappings_layer2[TOTAL_KEY_COUNT];
    uint8_t mappings_layer3[TOTAL_KEY_COUNT];

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
