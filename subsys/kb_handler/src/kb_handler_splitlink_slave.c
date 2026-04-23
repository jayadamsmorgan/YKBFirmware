#include "kb_handler_common.h"

#include <zephyr/logging/log.h>

#include "splitlink_handler/splitlink_handler.h"

LOG_MODULE_REGISTER(kb_handler, CONFIG_KB_HANDLER_LOG_LEVEL);

// static K_THREAD_STACK_DEFINE(kbh_ss_thread_stack,
//                              CONFIG_KB_HANDLER_THREAD_STACK_SIZE);
//
// static struct k_thread kbh_ss_thread;

enum kbh_thread_msg_type {
    KBH_THREAD_MSG_KEY = 0U,
};

static uint16_t values[KEY_COUNT_SLAVE] = {0};

static uint16_t new_value_counter = 0;

static void on_new_value(uint16_t idx, uint16_t value) {
    values[idx - KEY_COUNT] = value;

    new_value_counter++;
    if (new_value_counter >= KEY_COUNT_SLAVE) {
        new_value_counter = 0;
        splitlink_handler_send_values(values, KEY_COUNT_SLAVE);
    }
}

static void on_event(uint16_t idx, bool pressed) {
    values[idx - KEY_COUNT] = pressed;
}

KSCAN_CB_DEFINE(kbh_sm) = {
    .on_new_value = on_new_value,
    .on_event = on_event,
};

void splitlink_handler_settings_received(kb_settings_t *settings) {
    int err = kb_settings_apply(settings);
    if (err) {
        LOG_ERR("kb_settings_apply: %d", err);
    }
}

void splitlink_handler_on_connect() {
    //
    LOG_INF("SplitLink connected");
}

void splitlink_handler_on_disconnect() {
    //
    LOG_INF("SplitLink disconnected");
}
