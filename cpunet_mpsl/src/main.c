#include <zephyr/console/console.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include "hci_rpmsg_module.h"

#include <esb.h>

#include <lib/ykb_esb.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {

    ykb_esb_init(NULL);

    hci_rpmsg_init();

    while (1) {
        k_sleep(K_MSEC(2000));
    }
}
