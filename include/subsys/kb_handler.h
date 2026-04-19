#ifndef __SUBSYS_KB_HANDLER_H_
#define __SUBSYS_KB_HANDLER_H_

#include <lib/kb_settings.h>

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

int kb_handler_get_default_thresholds(const struct device *dev,
                                      uint16_t *buffer);
int kb_handler_get_default_keymap_layer1(const struct device *dev,
                                         uint8_t *buffer);

int kb_handler_get_default_keymap_layer2(const struct device *dev,
                                         uint8_t *buffer);

int kb_handler_get_default_keymap_layer3(const struct device *dev,
                                         uint8_t *buffer);
int kb_handler_get_default_mouseemu(const struct device *dev,
                                    kb_mouseemu_settings_t *buffer);

#endif // __SUBSYS_KB_HANDLER_H_
