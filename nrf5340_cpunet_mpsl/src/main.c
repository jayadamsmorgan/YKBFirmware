#include <zephyr/console/console.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include <esb.h>

#include <zephyr/drivers/gpio.h>

#include "hci_rpmsg_module.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {

    hci_rpmsg_init();

    while (1) {
        k_sleep(K_MSEC(2000));
    }
}
