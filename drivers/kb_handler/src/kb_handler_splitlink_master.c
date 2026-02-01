#include "zephyr/logging/log.h"
#include <drivers/kb_handler.h>

#include <drivers/kscan.h>

LOG_MODULE_REGISTER(kb_handler_sm, CONFIG_KB_HANDLER_LOG_LEVEL);

static void on_press(uint16_t key_index) {
    //
}

static void on_release(uint16_t key_index) {
    //
}

KSCAN_CB_DEFINE(kb_handler_sm_cbs) = {
    .on_press = on_press,
    .on_release = on_release,
};
