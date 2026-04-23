#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/kscan.h>
// #include <drivers/splitlink.h>
// #include <lib/ykb_esb.h>

#include <nrfx_clock.h>

#include <app_version.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);
//
// const struct device *splitlink = DEVICE_DT_GET(DT_NODELABEL(splitlink));
//
// static void connect(const struct device *dev) {
//     //
//     LOG_INF("Second half connected.");
// }
//
// static void disconnect(const struct device *dev) {
//     //
//     LOG_INF("Second half disconnected.");
// }
//
// #if CONFIG_BOARD_DACTYL_V1_NRF5340_CPUAPP_LEFT
//
// #include "bt.h"
//
// struct kbh_thread_msg {
//     char key;
//     bool status;
// };
//
// K_MSGQ_DEFINE(key_q, sizeof(struct kbh_thread_msg), 8, 4);
// K_THREAD_STACK_DEFINE(key_thread_stack, 1024);
// static struct k_thread key_thread;
//
// static void key_process_thread(void *a, void *b, void *c) {
//     while (true) {
//         struct kbh_thread_msg data;
//         k_msgq_get(&key_q, &data, K_FOREVER);
//         if (data.status) {
//             send_report_press(data.key);
//         } else {
//             send_report_release(data.key);
//         }
//     }
// }
//
// static void receive(const struct device *dev, uint8_t *data, size_t len) {
//     //
//     if (len != 2) {
//         LOG_ERR("Unknown packet len %d", len);
//         return;
//     }
//     if (data[0] == 1) {
//         struct kbh_thread_msg data = {
//             .key = 'b',
//             .status = 1,
//         };
//         k_msgq_put(&key_q, &data, K_NO_WAIT);
//     } else {
//         struct kbh_thread_msg data = {
//             .key = 'b',
//             .status = 0,
//         };
//         k_msgq_put(&key_q, &data, K_NO_WAIT);
//     }
// }
//
static void on_event(uint16_t key_index, bool pressed) {
    LOG_INF("Key %d %s", key_index, pressed ? "pressed" : "released");
}

// static void on_value_changed(uint16_t key_index, uint16_t value) {
//     if (key_index == 1) {
//         LOG_INF("%d %d", key_index, value);
//     }
// }

KSCAN_CB_DEFINE(main) = {
    .on_event = on_event,
    // .on_new_value = on_value_changed,
};
//
// SPLITLINK_CB_DEFINE(main) = {
//     .connect_cb = connect,
//     .disconnect_cb = disconnect,
//     .on_receive_cb = receive,
// };

int main(void) {

    LOG_INF("WOW WTF!!!");
    nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);

    // int err;
    //
    // k_thread_create(&key_thread, key_thread_stack,
    //                 K_THREAD_STACK_SIZEOF(key_thread_stack),
    //                 key_process_thread, NULL, NULL, NULL, 15, 0, K_NO_WAIT);
    //
    // k_sleep(K_MSEC(500));
    // err = splitlink_esb_init(splitlink);
    // if (err) {
    //     LOG_ERR("ESB: %d", err);
    //     return 0;
    // }
    //
    // err = bt_setup();
    // if (err) {
    //     LOG_ERR("bt_setup: %d", err);
    //     return 0;
    // }
    //
    // while (true) {
    //     k_sleep(K_MSEC(2000));
    //     bas_notify();
    // }
    //
    // return 0;
}

// #endif // CONFIG_BOARD_DACTYL_V1_NRF5340_CPUAPP_LEFT

// #if CONFIG_BOARD_DACTYL_V1_NRF5340_CPUAPP_RIGHT
//
// SPLITLINK_CB_DEFINE(main) = {
//     .connect_cb = connect,
//     .disconnect_cb = disconnect,
// };
//
// static void on_event(uint16_t key_index, bool pressed) {
//     uint8_t data[2] = {1, 0};
//     ARG_UNUSED(key_index);
//
//     data[0] = pressed ? 1 : 0;
//     splitlink_send(splitlink, data, 2);
// }
//
// static void on_value_changed(uint16_t key_index, uint16_t value) {
//     ARG_UNUSED(key_index);
//     ARG_UNUSED(value);
// }
//
// KSCAN_CB_DEFINE(main) = {
//     .on_event = on_event,
//     .on_value_changed = on_value_changed,
// };
//
// int main(void) {
//
//     k_sleep(K_MSEC(500));
//     int err = splitlink_esb_init(splitlink);
//     if (err) {
//         LOG_ERR("ESB: %d", err);
//         return 0;
//     }
//
//     while (true) {
//         k_sleep(K_MSEC(2000));
//     }
//     return 0;
// }
//
// #endif // CONFIG_BOARD_DACTYL_V1_NRF5340_CPUAPP_RIGHT
