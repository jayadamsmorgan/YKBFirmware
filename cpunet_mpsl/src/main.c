#include <zephyr/console/console.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include <esb.h>

#include <lib/ykb_esb.h>

#include <zephyr/drivers/gpio.h>

#include "bt_hci.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define ESB_RPC_START_DELAY_MS 3000

static void esb_rpc_start_thread(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    k_sleep(K_MSEC(ESB_RPC_START_DELAY_MS));

    int err = ykb_esb_rpc_start();
    if (err) {
        LOG_ERR("ykb_esb_rpc_start failed: %d", err);
    }
}

K_THREAD_DEFINE(esb_rpc_start_thread_id, 1024, esb_rpc_start_thread, NULL,
                NULL, NULL, K_PRIO_PREEMPT(8), 0, 0);

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
