#include "kb_handler_internal.h"

#include <zephyr/logging/log.h>

#include "splitlink_handler/splitlink_handler.h"

LOG_MODULE_DECLARE(kb_handler);

BUILD_ASSERT(KEY_COUNT_SLAVE > 0,
             "KB_HANDLER_SPLITLINK_SLAVE requires kb-handler-key-count-slave");

static uint16_t values[KEY_COUNT_SLAVE] = {0};

static uint16_t new_value_counter = 0;

static void on_new_value(uint16_t idx, uint16_t value) {
    if (idx >= KEY_COUNT_SLAVE) {
        LOG_WRN("Ignoring out-of-range slave key value idx %u", idx);
        return;
    }

    values[idx] = value;

    new_value_counter++;
    if (new_value_counter >= KEY_COUNT_SLAVE) {
        new_value_counter = 0;
        splitlink_handler_send_values(values, KEY_COUNT_SLAVE);
    }
}

static void on_event(uint16_t idx, bool pressed) {
    if (idx >= KEY_COUNT_SLAVE) {
        LOG_WRN("Ignoring out-of-range slave key event idx %u", idx);
        return;
    }

    values[idx] = pressed;
}

KSCAN_CB_DEFINE(kbh_sm) = {
    .on_new_value = on_new_value,
    .on_event = on_event,
};

void splitlink_handler_settings_received(const kb_settings_t *settings) {
    int err = kb_settings_apply(settings);
    if (err) {
        LOG_ERR("kb_settings_apply: %d", err);
    }
}

void splitlink_handler_on_connect() {
    LOG_INF("SplitLink connected");
}

void splitlink_handler_on_disconnect() {
    LOG_INF("SplitLink disconnected");
}

static int kb_handler_ss_init(void) {
    int err;

    err = kb_handler_check_kscans_ready();
    if (err) {
        return err;
    }

    err = kb_handler_validate_kscan_topology(KEY_COUNT_SLAVE);
    if (err) {
        return err;
    }

    return splitlink_handler_init();
}

SYS_INIT(kb_handler_ss_init, POST_KERNEL, CONFIG_KB_HANDLER_INIT_PRIORITY);
