#ifndef __DRIVERS_KB_HANDLER_H_
#define __DRIVERS_KB_HANDLER_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

__subsystem struct kb_handler_driver_api {
    int (*get_default_thresholds)(const struct device *dev, uint16_t *buffer);
    int (*get_default_keymap_layer1)(const struct device *dev, uint8_t *buffer);
    int (*get_default_keymap_layer2)(const struct device *dev, uint8_t *buffer);
    int (*get_default_keymap_layer3)(const struct device *dev, uint8_t *buffer);
};

__syscall int kb_handler_get_default_thresholds(const struct device *dev,
                                                uint16_t *buffer);
__syscall int kb_handler_get_default_keymap_layer1(const struct device *dev,
                                                   uint8_t *buffer);

__syscall int kb_handler_get_default_keymap_layer2(const struct device *dev,
                                                   uint8_t *buffer);

__syscall int kb_handler_get_default_keymap_layer3(const struct device *dev,
                                                   uint8_t *buffer);

static inline int
z_impl_kb_handler_get_default_thresholds(const struct device *dev,
                                         uint16_t *buffer) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kb_handler, dev));
    return DEVICE_API_GET(kb_handler, dev)->get_default_thresholds(dev, buffer);
}

static inline int
z_impl_kb_handler_get_default_keymap_layer1(const struct device *dev,
                                            uint8_t *buffer) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kb_handler, dev));
    return DEVICE_API_GET(kb_handler, dev)
        ->get_default_keymap_layer1(dev, buffer);
}

static inline int
z_impl_kb_handler_get_default_keymap_layer2(const struct device *dev,
                                            uint8_t *buffer) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kb_handler, dev));
    return DEVICE_API_GET(kb_handler, dev)
        ->get_default_keymap_layer2(dev, buffer);
}

static inline int
z_impl_kb_handler_get_default_keymap_layer3(const struct device *dev,
                                            uint8_t *buffer) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kb_handler, dev));
    return DEVICE_API_GET(kb_handler, dev)
        ->get_default_keymap_layer3(dev, buffer);
}

#include <syscalls/kb_handler.h>

#endif // __DRIVERS_KB_HANDLER_H_
