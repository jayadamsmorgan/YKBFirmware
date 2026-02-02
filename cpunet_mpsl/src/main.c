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
    bt_hci_init();
    while (true) {
        k_sleep(K_MSEC(3000));
    }
}
