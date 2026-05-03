#include <zephyr/console/console.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include <esb.h>

#include <zephyr/drivers/gpio.h>

#include "bt_hci.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {
    while (true) {
        int err = bt_hci_init();
        if (err) {
            LOG_ERR("bt_hci_init failed: %d", err);
            k_sleep(K_MSEC(250));
            continue;
        }

        err = bt_hci_process();
        if (err) {
            LOG_ERR("bt_hci_process failed: %d", err);
            k_sleep(K_MSEC(250));
        }
    }
}
