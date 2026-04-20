#ifndef YKB_PROTO_H
#define YKB_PROTO_H

#include <zephyr/sys/util_macro.h>
#include <zephyr/toolchain.h>

#include <stdbool.h>
#include <stdint.h>

#define FEATURES_MAX_BOARD_NAME 10
#define FEATURES_MAX_REV_NAME 5
#define FEATURES_MAX_VENDOR_NAME 10
#define FEATURES_MAX_SOC_NAME 10

#define FEATURES_VERSION_1 1U

typedef struct __packed {
    const uint8_t features_version;

    const char board_name[FEATURES_MAX_BOARD_NAME];
    const char rev_name[FEATURES_MAX_REV_NAME];
    const char vendor_name[FEATURES_MAX_VENDOR_NAME];
    const char soc_name[FEATURES_MAX_SOC_NAME];

    const uint8_t key_count;
    const uint8_t key_count_slave;

    const bool splitlink : 1;
    const bool ykb_backlight : 1;
    const bool ykb_battsense : 1;

    const bool bt_connect : 1;
    const bool bt_connect_kbd : 1;
    const bool bt_connect_mouse : 1;
    const bool bt_connect_vendor : 1;

    const bool usb_connect : 1;
    const bool usb_connect_kbd : 1;
    const bool usb_connect_mouse : 1;
    const bool usb_connect_vendor : 1;

} device_features;

#define FEATURE(name, config) .name = IS_ENABLED(config)

device_features features = {
    .features_version = FEATURES_VERSION_1,
    .board_name = CONFIG_BOARD,
    .rev_name = CONFIG_BOARD_REVISION,
    .vendor_name = "YarmanKB",
    .soc_name = CONFIG_SOC,

    .key_count = CONFIG_KB_SETTINGS_KEY_COUNT,
#if CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE
    .key_count_slave = CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE,
#endif // CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE

    FEATURE(splitlink, CONFIG_SPLITLINK),
    FEATURE(ykb_backlight, CONFIG_YKB_BACKLIGHT),
    FEATURE(ykb_battsense, CONFIG_YKB_BATTSENSE),

    FEATURE(bt_connect, CONFIG_LIB_BT_CONNECT),

    FEATURE(usb_connect, CONFIG_LIB_USB_CONNECT),
};

#endif // YKB_PROTO_H
