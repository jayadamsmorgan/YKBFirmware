#define DT_DRV_COMPAT kb_handler_splitlink_master

#include <drivers/kb_handler.h>

#include <drivers/kscan.h>

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kb_handler_sm, CONFIG_KB_HANDLER_LOG_LEVEL);

struct kb_handler_sm_config {};

struct kb_handler_sm_data {};

enum kbh_event_type {
    PRESS,
    RELEASE,
};

struct kbh_event {
    enum kbh_event_type type;
    uint16_t idx;
};

#ifndef CONFIG_KB_HANDLER_MSGQ_SIZE
#define CONFIG_KB_HANDLER_MSGQ_SIZE 8
#endif // CONFIG_KB_HANDLER_MSGQ_SIZE

#ifndef CONFIG_KB_HANDLER_THREAD_STACK_SIZE
#define CONFIG_KB_HANDLER_THREAD_STACK_SIZE 1024
#endif // CONFIG_KB_HANDLER_THREAD_STACK_SIZE

K_MSGQ_DEFINE(kbh_event_msgq, sizeof(struct kbh_event),
              CONFIG_KB_HANDLER_MSGQ_SIZE, 4);

K_THREAD_STACK_DEFINE(kb_handler_thread_stack,
                      CONFIG_KB_HANDLER_THREAD_STACK_SIZE);

static void kb_handler_thread(void *device, void *_, void *__) {
    struct kbh_event event;
    CONFIG_MAIN_STACK_SIZE;

    uint8_t buffer[8];

    while (true) {
        int err = k_msgq_get(&kbh_event_msgq, &event, K_FOREVER);
        if (err) {
            LOG_ERR("k_msgq_get: %d", err);
            continue;
        }
        LOG_INF("Key %s idx %d", event.type == PRESS ? "pressed" : "released",
                event.idx);
    }
}

// Cannot do anything heavy in callbacks
static void on_press(uint16_t key_index) {
    struct kbh_event event = {
        .idx = key_index,
        .type = PRESS,
    };
    k_msgq_put(&kbh_event_msgq, &event, K_NO_WAIT);
}

static void on_release(uint16_t key_index) {
    struct kbh_event event = {
        .idx = key_index,
        .type = RELEASE,
    };
    k_msgq_put(&kbh_event_msgq, &event, K_NO_WAIT);
}

KSCAN_CB_DEFINE(kb_handler_sm_cbs) = {
    .on_press = on_press,
    .on_release = on_release,
};

static struct k_thread thread;

static int kb_handler_sm_init(const struct device *dev) {
    k_thread_create(&thread, kb_handler_thread_stack,
                    CONFIG_KB_HANDLER_THREAD_STACK_SIZE, kb_handler_thread,
                    (void *)dev, NULL, NULL, 5, 0, K_NO_WAIT);
    k_thread_name_set(&thread, "kb_handler_sm");
    //
    return 0;
}

#define KB_HANDLER_SM_DEFINE(inst)                                             \
    static const struct kb_handler_sm_config __kb_handler_sm_config__##inst =  \
        {                                                                      \
            /* */                                                              \
    };                                                                         \
    static struct kb_handler_sm_data __kb_handler_sm_data__##inst = {          \
        /* */                                                                  \
    };                                                                         \
    DEVICE_DT_INST_DEFINE(inst, kb_handler_sm_init, NULL,                      \
                          &__kb_handler_sm_data__##inst,                       \
                          &__kb_handler_sm_config__##inst, POST_KERNEL,        \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);

DT_INST_FOREACH_STATUS_OKAY(KB_HANDLER_SM_DEFINE)
