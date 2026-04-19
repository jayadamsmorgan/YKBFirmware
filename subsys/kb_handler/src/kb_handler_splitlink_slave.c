#define DT_DRV_COMPAT kb_handler_splitlink_slave

#include <drivers/kb_handler.h>

#include <lib/kb_settings.h>

#include <drivers/kscan.h>
#include <drivers/splitlink.h>

#include <dt-bindings/kb-handler/kb-key-codes.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <string.h>

struct kb_handler_ss_config {
    const struct device **kscans;

    const uint16_t key_count;
    const uint16_t key_count_master;
    const uint16_t kscans_count;

    const struct device *splitlink;

    const uint8_t *default_keymap_layer1;
    const uint8_t *default_keymap_layer2;
    const uint8_t *default_keymap_layer3;

    const kb_mouseemu_settings_t *default_mouseemu;
};

struct kb_handler_ss_data {};

LOG_MODULE_REGISTER(kb_handler_ss, CONFIG_KB_HANDLER_LOG_LEVEL);

#define KSCAN_DEV_AND_COMMA(node_id, prop, idx)                                \
    DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define TOTAL_KB_KEY_COUNT(inst)                                               \
    (DT_INST_PROP(inst, key_count) + DT_INST_PROP(inst, key_count_slave))

#define KB_HANDLER_SS_DEFINE(inst)                                             \
    static const struct device *__kb_handler_ss_kscans__##inst[] = {           \
        DT_INST_FOREACH_PROP_ELEM(inst, kscans, KSCAN_DEV_AND_COMMA)};         \
    static const uint8_t                                                       \
        __kb_handler_ss_default_keymap_layer1__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] = DT_INST_PROP(inst, default_keymap_layer1);                \
    static const uint8_t                                                       \
        __kb_handler_ss_default_keymap_layer2__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] =                                                           \
            DT_INST_PROP_OR(inst, default_keymap_layer2, {KEY_NOKEY});         \
    static const uint8_t                                                       \
        __kb_handler_ss_default_keymap_layer3__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] =                                                           \
            DT_INST_PROP_OR(inst, default_keymap_layer3, {KEY_NOKEY});         \
    K_MSGQ_DEFINE(__kb_handler_ss_thread_queue__##inst,                        \
                  sizeof(struct kbh_thread_msg), CONFIG_KB_HANDLER_MSGQ_SIZE); \
    K_STACK_DEFINE(__kb_handler_ss_thread_stack__##inst,                       \
                   CONFIG_KB_HANDLER_THREAD_STACK_SIZE);
