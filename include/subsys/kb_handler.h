#ifndef __SUBSYS_KB_HANDLER_H_
#define __SUBSYS_KB_HANDLER_H_

#include <subsys/kb_settings.h>

#include <zephyr/toolchain.h>

int kb_handler_get_default_thresholds(uint16_t *buffer);

int kb_handler_get_default_keymap_layer1(uint8_t *buffer);
int kb_handler_get_default_keymap_layer2(uint8_t *buffer);
int kb_handler_get_default_keymap_layer3(uint8_t *buffer);

int kb_handler_get_default_mouseemu(kb_mouseemu_settings_t *buffer);

#endif // __SUBSYS_KB_HANDLER_H_
