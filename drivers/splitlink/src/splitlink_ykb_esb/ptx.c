#define DT_DRV_COMPAT splitlink_ykb_esb_ptx

#include "splitlink_esb.h"

LOG_MODULE_REGISTER(splitlink_esb_ptx, CONFIG_SPLITLINK_LOG_LEVEL);

static void ptx_alive_thread(void *a, void *b, void *c) {
    ykb_esb_data_t alive_data = {
        .data = {FLAG_ALIVE},
        .len = 1,
    };
    while (true) {
        int err = ykb_esb_send(&alive_data);
        if (err) {
            LOG_ERR("Unable to send on the ptx alive thread: %d", err);
        }
        k_sleep(K_MSEC(CONFIG_SPLITLINK_YKB_ESB_PTX_ALIVE_THREAD_TIME));
    }
}

static void connect_work_handler(struct k_work *work) {
    struct device_work *dev_work = CONTAINER_OF(work, struct device_work, work);

    // We set the 'connected' before calling this work

    STRUCT_SECTION_FOREACH(splitlink_cb, callback) {
        if (callback->connect_cb) {
            callback->connect_cb(dev_work->dev);
        }
    }
}

static void disconnect_work_handler(struct k_work *work) {
    // Oopsie, we got a disconnect...
    struct delayable_device_work *dev_work =
        CONTAINER_OF(work, struct delayable_device_work, d_work.work);

    struct splitlink_data *data = dev_work->dev->data;
    data->connected = false;

    STRUCT_SECTION_FOREACH(splitlink_cb, callback) {
        if (callback->disconnect_cb) {
            callback->disconnect_cb(dev_work->dev);
        }
    }
}

static void receiving_work_handler(struct k_work *work) {
    struct receiving_device_work *dev_work =
        CONTAINER_OF(work, struct receiving_device_work, work);

    STRUCT_SECTION_FOREACH(splitlink_cb, callback) {
        if (callback->on_receive_cb) {
            callback->on_receive_cb(dev_work->dev, dev_work->data,
                                    dev_work->data_len);
        }
    }
}

static void on_esb_callback(ykb_esb_event_t *event, void *user_ptr) {
    const struct device *dev = user_ptr;
    struct splitlink_data *dev_data = dev->data;
    if (event->evt_type == YKB_ESB_EVT_RX && event->data_length > 2 &&
        event->buf[0] == FLAG_DATA) {
        dev_data->receiving_work.data_len = event->data_length - 1;
        memcpy(dev_data->receiving_work.data, &event->buf[1],
               event->data_length - 1);
        k_work_submit(&dev_data->receiving_work.work);
    } else if (event->evt_type == YKB_ESB_EVT_TX_SUCCESS) {
        // If not connected, we got a connection now
        if (!dev_data->connected) {
            dev_data->connected = true;
            k_work_submit(&dev_data->connect_work.work);
        }
        // Cancel disconnect work if it was scheduled before
        k_work_cancel_delayable(&dev_data->disconnect_work.d_work);
        // And schedule it again
        k_work_schedule(&dev_data->disconnect_work.d_work,
                        K_MSEC(CONFIG_SPLITLINK_YKB_ESB_ALIVE_TIMEOUT));
        LOG_DBG("TX success");
    }
#if CONFIG_SPLITLINK_LOG_LEVEL == LOG_LEVEL_DBG
    if (event->evt_type == YKB_ESB_EVT_TX_FAIL) {
        LOG_DBG("TX fail");
    }
#endif // CONFIG_SPLITLINK_LOG_LEVEL == LOG_LEVEL_DBG
}

int splitlink_esb_init(const struct device *dev) {
    const struct splitlink_config *cfg = dev->config;
    struct splitlink_data *data = dev->data;

    // TODO: ESB only initializes after a small delay for some reason, fix later
    k_sleep(K_MSEC(100));

    ykb_esb_config_t esb_cfg = {
        .mode = YKB_ESB_MODE_PTX,
        .user_ptr = (void *)dev,
    };
    memcpy(esb_cfg.base_addr_0, cfg->esb_default_address,
           sizeof(esb_cfg.base_addr_0));
    memcpy(esb_cfg.base_addr_1, &cfg->esb_default_address[4],
           sizeof(esb_cfg.base_addr_1));

    int err = ykb_esb_init(&esb_cfg, on_esb_callback);
    if (err) {
        LOG_ERR("Unable to initialize YKB ESB: %d", err);
        return -ENODEV;
    }

    data->connect_work.dev = dev;
    data->disconnect_work.dev = dev;
    data->receiving_work.dev = dev;

    k_work_init_delayable(&data->disconnect_work.d_work,
                          disconnect_work_handler);
    k_work_init(&data->connect_work.work, connect_work_handler);
    k_work_init(&data->receiving_work.work, receiving_work_handler);

    k_thread_create(&data->alive_thread, data->alive_thread_stack,
                    CONFIG_SPLITLINK_YKB_ESB_PTX_ALIVE_THREAD_STACK_SIZE,
                    ptx_alive_thread, NULL, NULL, NULL,
                    CONFIG_SPLITLINK_YKB_ESB_PTX_ALIVE_THREAD_PRIORITY, 0,
                    K_NO_WAIT);
    k_thread_name_set(&data->alive_thread, "PTX alive thread");

    return 0;
}

DEVICE_API(splitlink, splitlink_esb_api) = {
    .send = splitlink_ykb_esb_send,
};

#define SPLITLINK_YKB_ESB_PTX_DEFINE(inst)                                     \
    BUILD_ASSERT(DT_INST_PROP_LEN(inst, esb_default_address) == 8,             \
                 "esb-default-address should be the length of 8 bytes.");      \
    K_THREAD_STACK_DEFINE(                                                     \
        __splitlink_ykb_esb_ptx_alive_thread_stack__##inst,                    \
        CONFIG_SPLITLINK_YKB_ESB_PTX_ALIVE_THREAD_STACK_SIZE);                 \
    static const struct splitlink_config                                       \
        __splitlink_ykb_esb_ptx_config__##inst = {                             \
            .esb_default_address = DT_INST_PROP(inst, esb_default_address),    \
    };                                                                         \
    static struct splitlink_data __splitlink_ykb_esb_ptx_data__##inst = {      \
        .connected = false,                                                    \
        .alive_thread_stack =                                                  \
            __splitlink_ykb_esb_ptx_alive_thread_stack__##inst,                \
    };                                                                         \
    DEVICE_DT_INST_DEFINE(                                                     \
        inst, NULL, NULL, &__splitlink_ykb_esb_ptx_data__##inst,               \
        &__splitlink_ykb_esb_ptx_config__##inst, POST_KERNEL,                  \
        CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &splitlink_esb_api);

DT_INST_FOREACH_STATUS_OKAY(SPLITLINK_YKB_ESB_PTX_DEFINE)
