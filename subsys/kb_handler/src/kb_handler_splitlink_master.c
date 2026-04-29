#include "kb_handler_internal.h"

#include "splitlink_handler/splitlink_handler.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(kb_handler);

KSCAN_CB_DEFINE(kbh_sm) = {
    .on_event = kb_handler_core_handle_key_event,
    .on_new_value = kb_handler_core_handle_value,
};

void splitlink_handler_values_received(uint16_t *slave_values, uint16_t count) {
    kb_handler_core_handle_slave_values(slave_values, count);
}

void splitlink_handler_on_connect() {
    kb_settings_t settings;

    LOG_INF("SplitLink slave connected");

    if (!kb_handler_core_get_settings_snapshot(&settings)) {
        splitlink_handler_send_settings(&settings);
    }
}

void splitlink_handler_on_disconnect() {
    LOG_WRN("SplitLink slave disconnected");
    kb_handler_core_handle_slave_reset();
}

void kb_handler_impl_after_settings_update(const kb_settings_t *settings) {
    kb_settings_t settings_copy = *settings;

    splitlink_handler_send_settings(&settings_copy);
}

static int kb_handler_sm_init(void) {
    int err = splitlink_handler_init();

    if (err) {
        return err;
    }

    return kb_handler_core_init();
}

SYS_INIT(kb_handler_sm_init, POST_KERNEL, CONFIG_KB_HANDLER_INIT_PRIORITY);
