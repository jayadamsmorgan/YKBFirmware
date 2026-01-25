#include <zephyr/console/console.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include <esb.h>

#include <zephyr/drivers/gpio.h>

#include <lib/esb_ble_mpsl.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {

    mpsl_hci_rpmsg_init();

    while (1) {
        k_sleep(K_MSEC(2000));
    }
}
