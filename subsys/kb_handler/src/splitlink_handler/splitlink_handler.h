#ifndef SPLITLINK_HANDLER_H
#define SPLITLINK_HANDLER_H

#include <lib/kb_settings.h>

#include <stddef.h>
#include <stdint.h>

void splitlink_handler_values_received(uint16_t *values, uint16_t count);

void splitlink_handler_send_values(uint16_t *values, uint16_t count);

void splitlink_handler_settings_received(kb_settings_t *settings);

void splitlink_handler_send_settings(kb_settings_t *settings);

#endif // SPLITLINK_HANDLER_H
