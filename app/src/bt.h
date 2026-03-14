#ifndef MAIN_BT_H
#define MAIN_BT_H

#include <stddef.h>
#include <stdint.h>

int hid_buttons_press(const uint8_t *keys, size_t cnt);
int hid_buttons_release(const uint8_t *keys, size_t cnt);

void send_report_press(uint8_t key);
void send_report_release(uint8_t key);

void bas_notify(void);
int bt_setup(void);

#endif // MAIN_BT_H
