#define DT_DRV_COMPAT kb_handler_splitlink_master

#include <drivers/kb_handler.h>

#include <lib/kb_settings.h>

#include <drivers/kscan.h>
#include <drivers/splitlink.h>

#include <dt-bindings/kb-handler/kb-key-codes.h>

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include <zephyr/usb/class/usbd_hid.h>

LOG_MODULE_REGISTER(kb_handler_sm, CONFIG_KB_HANDLER_LOG_LEVEL);

struct kb_handler_sm_config {
    const struct device **kscans;
    const struct device *splitlink;

    const uint16_t kscans_count;
    const uint16_t key_count;
    const uint16_t key_count_slave;

    const uint8_t *default_keymap_layer1;
    const uint8_t *default_keymap_layer2;
    const uint8_t *default_keymap_layer3;
};

struct kb_handler_sm_data {
    struct k_thread thread;
    bool thread_started;
    k_thread_stack_t *thread_stack;
    struct k_msgq *thread_q;

    kb_settings_t settings_snapshot;
};

#define DATA_FLAG_KEYS 1U
#define DATA_FLAG_VALUES 2U
#define DATA_FLAG_BACKLIGHT 3U

struct kbh_thread_msg {
    uint8_t key;
    bool status;

    bool slave_keys_reset;
};

static void send_report(uint8_t report[8]) {}

static inline bool is_modifier(uint8_t hid) {
    return (hid >= KEY_LEFTCONTROL && hid <= KEY_RIGHTGUI);
}

static inline uint8_t mod_bit(uint8_t hid) {
    return 1U << (hid - KEY_LEFTCONTROL);
}

static bool add_key(uint8_t keys[6], uint8_t hid) {
    for (int i = 0; i < 6; i++) {
        if (keys[i] == hid)
            return true;
    }
    for (int i = 0; i < 6; i++) {
        if (keys[i] == 0) {
            keys[i] = hid;
            return true;
        }
    }
    return false; // no space
}

static void build_report(uint8_t report[8], const bool *pressed_keys,
                         uint16_t total_key_count, bool second_layer_active,
                         bool third_layer_active, const uint8_t *keymap_l1,
                         const uint8_t *keymap_l2, const uint8_t *keymap_l3) {
    memset(report, 0, 8);

    uint8_t *mods = &report[0];
    uint8_t *keys = &report[2];

    bool overflow = false;

    for (uint16_t i = 0; i < total_key_count; i++) {
        if (!pressed_keys[i])
            continue;

        uint8_t hid = third_layer_active    ? keymap_l3[i]
                      : second_layer_active ? keymap_l2[i]
                                            : keymap_l1[i];

        // ignore non-report keys
        if (hid == KEY_LAYER1 || hid == KEY_LAYER2 || hid == KEY_FN)
            continue;

        if (is_modifier(hid)) {
            *mods |= mod_bit(hid);
            continue;
        }

        if (!add_key(keys, hid)) {
            overflow = true;
        }
    }

    if (overflow && IS_ENABLED(CONFIG_KB_HANDLER_REPORT_ROLLOVER)) {
        // Boot protocol rollover error
        report[2] = report[3] = report[4] = report[5] = report[6] = report[7] =
            0x01;
    }
}

static bool handle_layer_key(struct kbh_thread_msg msg, bool *pressed_keys,
                             bool *layer_active, uint16_t *layer_keys,
                             uint16_t layer_key_count) {
    // Switch layer
    if (msg.status == false) {
        // Layer key was released, check that we don't have any more
        // layer keys pressed at the moment
        bool other_layer_pressed = false;
        for (uint8_t i = 0; i < layer_key_count; ++i) {
            uint16_t layer_key = layer_keys[i];
            if (layer_key == msg.key) {
                continue;
            }
            if (pressed_keys[layer_key]) {
                other_layer_pressed = true;
                break;
            }
        }
        if (other_layer_pressed) {
            // Some other layer key is still pressed,
            // doing nothing here.
            return false;
        }
    }
    *layer_active = msg.status;
    return true;
}

static void kb_handler_thread(void *device, void *_, void *__) {
    struct kbh_thread_msg msg;

    const struct device *dev = device;
    const struct kb_handler_sm_config *cfg = dev->config;
    struct kb_handler_sm_data *data = dev->data;

    uint16_t total_key_count = cfg->key_count + cfg->key_count_slave;
    bool second_layer_active = false;
    bool third_layer_active = false;

    bool pressed_keys[total_key_count];
    memset(pressed_keys, 0, sizeof(pressed_keys));

    kb_settings_t *settings_snapshot = &data->settings_snapshot;

    uint16_t layer1_keys[total_key_count];
    uint8_t layer1_keys_count = 0;
    uint16_t layer2_keys[total_key_count];
    uint8_t layer2_keys_count = 0;
    memset(layer1_keys, 0, sizeof(layer1_keys));
    memset(layer2_keys, 0, sizeof(layer2_keys));
    for (uint16_t i = 0; i < total_key_count; ++i) {
        if (settings_snapshot->mappings_layer1[i] == KEY_LAYER1) {
            layer1_keys[layer1_keys_count] = i;
            layer1_keys_count++;
        }
        if (settings_snapshot->mappings_layer1[i] == KEY_LAYER2) {
            layer2_keys[layer2_keys_count] = i;
            layer2_keys_count++;
        }
    }

    uint8_t report[8] = {0};

    while (true) {

        int err = k_msgq_get(data->thread_q, &msg, K_FOREVER);
        if (err) {
            LOG_ERR("k_msgq_get: %d", err);
            continue;
        }

        if (msg.slave_keys_reset) {
            // Slave probably got disconnected and we need to release
            // all slave keys.
            // Assuming here that slave keys have an offset of key_count,
            // but that may not be a good way...
            memset(&pressed_keys[cfg->key_count], 0,
                   cfg->key_count_slave * sizeof(bool));
            LOG_DBG("Slave keys reset");
            goto handle_report;
        }

        if (settings_snapshot->mode == KB_MODE_NORMAL) {
            pressed_keys[msg.key] = msg.status;

            uint8_t resolved_hid =
                third_layer_active ? settings_snapshot->mappings_layer3[msg.key]
                : second_layer_active
                    ? settings_snapshot->mappings_layer2[msg.key]
                    : settings_snapshot->mappings_layer1[msg.key];

            LOG_DBG("Key %s idx %d", msg.status ? "pressed" : "released",
                    msg.key);

            if (resolved_hid == KEY_LAYER1) {
                if (handle_layer_key(msg, pressed_keys, &second_layer_active,
                                     layer1_keys, layer1_keys_count)) {
                    goto handle_report;
                }
                continue;
            }
            if (resolved_hid == KEY_LAYER2) {
                if (handle_layer_key(msg, pressed_keys, &third_layer_active,
                                     layer2_keys, layer2_keys_count)) {
                    goto handle_report;
                }
                continue;
            }
            if (resolved_hid == KEY_FN) {
                // TODO: Do FN stuff here
                continue;
            }
        handle_report:
            build_report(report, pressed_keys, total_key_count,
                         second_layer_active, third_layer_active,
                         settings_snapshot->mappings_layer1,
                         settings_snapshot->mappings_layer2,
                         settings_snapshot->mappings_layer3);
            send_report(report);
        }

        if (settings_snapshot->mode == KB_MODE_MOUSESIM) {
        }
    }
}

static inline void
kb_handler_sm_key_handler(struct kb_handler_sm_data *dev_data,
                          uint16_t key_index, bool status) {
    if (dev_data->settings_snapshot.mode != KB_MODE_NORMAL) {
        return;
    }
    struct kbh_thread_msg data = {
        .key = key_index,
        .status = status,
        .slave_keys_reset = false,
    };
    k_msgq_put(dev_data->thread_q, &data, K_NO_WAIT);
}

static void parse_splitlink_keys(uint8_t *data, size_t data_len,
                                 struct k_msgq *msgq) {
    for (size_t i = 0; i < data_len; i++) {
        // 7 right bytes are the index, the MSB is 0/1 status
        // (pressed/released)
        struct kbh_thread_msg msg_data = {
            .key = data[i] & 0x7F,
            .status = data[i] & 0x80,
            .slave_keys_reset = false,
        };
        k_msgq_put(msgq, &msg_data, K_NO_WAIT);
    }
}

static void parse_splitlink_values(uint8_t *data, size_t data_len,
                                   struct k_msgq *msgq) {
    for (size_t i = 0; i < data_len; i++) {
    }
}

static void kb_handler_sm_splitlink_on_receive(const struct device *dev,
                                               struct k_msgq *msgq,
                                               uint8_t *data, size_t data_len) {
    if (data_len == 0) {
        return;
    }
    switch (data[0]) {
    case DATA_FLAG_KEYS:
        parse_splitlink_keys(++data, data_len - 1, msgq);
        break;
    case DATA_FLAG_VALUES:
        parse_splitlink_values(++data, data_len - 1, msgq);
        break;
    case DATA_FLAG_BACKLIGHT:
        // TODO
        break;
    default:
        LOG_ERR("Unknown splitlink data flag '%d' received", data[0]);
        break;
    }
};

static inline void kb_handler_sm_splitlink_on_connect(const struct device *dev,
                                                      struct k_msgq *msgq) {
    LOG_INF("Splitlink slave connected!");
}

static inline void
kb_handler_sm_splitlink_on_disconnect(const struct device *dev,
                                      struct k_msgq *msgq) {
    LOG_WRN("Splitlink slave disconnected!");
    // Issue slave keys reset message
    struct kbh_thread_msg data = {
        .slave_keys_reset = true,
    };
    k_msgq_put(msgq, &data, K_NO_WAIT);
}

static void kscans_check(const struct kb_handler_sm_config *cfg) {

    int expected_offset;
    int total_key_count = 0;

    for (uint16_t i = 0; i < cfg->kscans_count; ++i) {
        int offset = kscan_get_idx_offset(cfg->kscans[i]);
        if (offset < 0) {
            LOG_ERR("Unable to get idx offset for KScan %d (err %d)", i,
                    offset);
            return;
        }
        int key_amount = kscan_get_key_amount(cfg->kscans[i]);
        if (key_amount < 0) {
            LOG_ERR("Unable to get key amount for KScan %d (err %d)", i,
                    key_amount);
            return;
        }
        total_key_count += key_amount;
        if (i != 0) {
            if (expected_offset < offset) {
                LOG_ERR("Key indices intersection between KScans %d and %d "
                        "(expected offset %d, got %d)",
                        i - 1, i, expected_offset, offset);
                k_panic();
            } else if (expected_offset > offset) {
                LOG_ERR("Key indices gap between KScans %d and %d "
                        "(expected offset %d, got %d)",
                        i - 1, i, expected_offset, offset);
                k_panic();
            }
        }
        expected_offset = offset + key_amount;
    }

    if (total_key_count != cfg->key_count) {
        LOG_ERR("KScans total key count != DTS key count (%d != %d)",
                total_key_count, cfg->key_count);
        k_panic();
    }
}

static inline void kb_handler_sm_on_settings_update(kb_settings_t *settings,
                                                    const struct device *dev) {
    const struct kb_handler_sm_config *cfg = dev->config;
    struct kb_handler_sm_data *data = dev->data;
    if (data->thread_started) {
        // Pause the thread if it is active
        k_thread_suspend(&data->thread);
    }

    // Copy settings
    memcpy(&data->settings_snapshot, settings, sizeof(kb_settings_t));

    for (size_t i = 0; i < cfg->kscans_count; ++i) {
        // Update thresholds for every KScan instance
        const struct device *kscan = cfg->kscans[i];
        int idx_offset = kscan_get_idx_offset(kscan);
        if (idx_offset < 0) {
            LOG_ERR("Unable to get idx offset for KScan instance %s (err %d)",
                    dev->name, idx_offset);
            continue;
        }
        int err =
            kscan_set_thresholds(kscan, &settings->thresholds[idx_offset]);
        if (err) {
            LOG_ERR("Unable to set thresholds for KScan instance %s (err %d)",
                    dev->name, err);
            continue;
        }
    }

    if (data->thread_started) {
        // Resume it
        k_thread_resume(&data->thread);
    } else {
        // Thread was not created before, create and start:
        k_thread_create(&data->thread, data->thread_stack,
                        CONFIG_KB_HANDLER_THREAD_STACK_SIZE, kb_handler_thread,
                        (void *)dev, NULL, NULL,
                        CONFIG_KB_HANDLER_THREAD_PRIORITY, 0, K_NO_WAIT);
        k_thread_name_set(&data->thread, "kb_handler_sm");
        data->thread_started = true;
    }
}

static int kb_handler_sm_init(const struct device *dev) {
    const struct kb_handler_sm_config *cfg = dev->config;

    struct kb_handler_sm_data *data = dev->data;
    data->thread_started = false;

    // First we need to check that there are no overlaps/gaps between the KScan
    // instances in offset indices and give some warnings if so.
    // KScan instances should be in order of managed indices.
    // Overlaps/gaps mean that most likely you have some misconfiguration in
    // dts.
    //
    // The philosophy of KScans is for each to have idx-offset so every
    // next KScan will have continuation of indices ([0-11], [12-15]...)
    kscans_check(cfg);

    // Now we wait for the first on_settings_update call to start the thread.

    return 0;
}

static int get_default_thresholds(const struct device *dev, uint16_t *buffer) {
    if (!dev) {
        return -EINVAL;
    }
    const struct kb_handler_sm_config *cfg = dev->data;
    if (buffer) {
        for (size_t i = 0; i < cfg->kscans_count; ++i) {
            const struct device *kscan = cfg->kscans[i];
            int key_amount = kscan_get_key_amount(kscan);
            if (key_amount < 0) {
                return key_amount;
            }
            int idx_offset = kscan_get_idx_offset(kscan);
            if (idx_offset < 0) {
                return idx_offset;
            }
            int res = kscan_get_default_thresholds(kscan, &buffer[idx_offset]);
            if (res < 0) {
                return res;
            }
        }
    }

    return 0;
}

static int get_default_keymap_layer1(const struct device *dev,
                                     uint8_t *buffer) {
    if (!dev) {
        return -EINVAL;
    }
    const struct kb_handler_sm_config *cfg = dev->data;
    if (buffer) {
        memcpy(buffer, cfg->default_keymap_layer1,
               sizeof(uint8_t) * TOTAL_KEY_COUNT);
    }
    return 0;
}

static int get_default_keymap_layer2(const struct device *dev,
                                     uint8_t *buffer) {
    if (!dev) {
        return -EINVAL;
    }
    const struct kb_handler_sm_config *cfg = dev->data;
    if (buffer) {
        memcpy(buffer, cfg->default_keymap_layer2,
               sizeof(uint8_t) * TOTAL_KEY_COUNT);
    }
    return 0;
}

static int get_default_keymap_layer3(const struct device *dev,
                                     uint8_t *buffer) {
    if (!dev) {
        return -EINVAL;
    }
    const struct kb_handler_sm_config *cfg = dev->data;
    if (buffer) {
        memcpy(buffer, cfg->default_keymap_layer3,
               sizeof(uint8_t) * TOTAL_KEY_COUNT);
    }
    return 0;
}

DEVICE_API(kb_handler, kb_handler_sm_api) = {
    .get_default_thresholds = get_default_thresholds,
    .get_default_keymap_layer1 = get_default_keymap_layer1,
    .get_default_keymap_layer2 = get_default_keymap_layer2,
    .get_default_keymap_layer3 = get_default_keymap_layer3,
};

#define KSCAN_DEV_AND_COMMA(node_id, prop, idx)                                \
    DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define TOTAL_KB_KEY_COUNT(inst)                                               \
    (DT_INST_PROP(inst, key_count) + DT_INST_PROP(inst, key_count_slave))

#define KB_HANDLER_SM_DEFINE(inst)                                             \
                                                                               \
    static const struct device *__kb_handler_sm_kscans__##inst[] = {           \
        DT_INST_FOREACH_PROP_ELEM(inst, kscans, KSCAN_DEV_AND_COMMA)};         \
                                                                               \
    static const uint8_t                                                       \
        __kb_handler_sm_default_keymap_layer1__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] = DT_INST_PROP(inst, default_keymap_layer1);                \
    static const uint8_t                                                       \
        __kb_handler_sm_default_keymap_layer2__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] =                                                           \
            DT_INST_PROP_OR(inst, default_keymap_layer2, {KEY_NOKEY});         \
    static const uint8_t                                                       \
        __kb_handler_sm_default_keymap_layer3__##inst[TOTAL_KB_KEY_COUNT(      \
            inst)] =                                                           \
            DT_INST_PROP_OR(inst, default_keymap_layer3, {KEY_NOKEY});         \
                                                                               \
    K_MSGQ_DEFINE(__kb_handler_sm_thread_queue__##inst,                        \
                  sizeof(struct kbh_thread_msg), CONFIG_KB_HANDLER_MSGQ_SIZE,  \
                  4);                                                          \
    K_THREAD_STACK_DEFINE(__kb_handler_sm_thread_stack__##inst,                \
                          CONFIG_KB_HANDLER_THREAD_STACK_SIZE);                \
                                                                               \
    static const struct kb_handler_sm_config __kb_handler_sm_config__##inst =  \
        {                                                                      \
            .key_count = DT_INST_PROP(inst, key_count),                        \
            .default_keymap_layer1 =                                           \
                __kb_handler_sm_default_keymap_layer1__##inst,                 \
            .default_keymap_layer2 =                                           \
                __kb_handler_sm_default_keymap_layer2__##inst,                 \
            .default_keymap_layer3 =                                           \
                __kb_handler_sm_default_keymap_layer3__##inst,                 \
            .key_count_slave = DT_INST_PROP(inst, key_count_slave),            \
            .kscans = __kb_handler_sm_kscans__##inst,                          \
            .kscans_count = DT_INST_PROP_LEN(inst, kscans),                    \
    };                                                                         \
    static struct kb_handler_sm_data __kb_handler_sm_data__##inst = {          \
        .thread_q = &__kb_handler_sm_thread_queue__##inst,                     \
        .thread_stack = __kb_handler_sm_thread_stack__##inst,                  \
    };                                                                         \
                                                                               \
    static void kb_handler_sm_key_pressed_handler__##inst(                     \
        uint16_t key_index) {                                                  \
        kb_handler_sm_key_handler(&__kb_handler_sm_data__##inst, key_index,    \
                                  true);                                       \
    }                                                                          \
                                                                               \
    static void kb_handler_sm_key_released_handler__##inst(                    \
        uint16_t key_index) {                                                  \
        kb_handler_sm_key_handler(&__kb_handler_sm_data__##inst, key_index,    \
                                  false);                                      \
    }                                                                          \
    KSCAN_CB_DEFINE(kb_handler_sm) = {                                         \
        .on_press = kb_handler_sm_key_pressed_handler__##inst,                 \
        .on_release = kb_handler_sm_key_released_handler__##inst,              \
    };                                                                         \
                                                                               \
    static void kb_handler_sm_splitlink_on_connect__##inst(                    \
        const struct device *dev) {                                            \
        kb_handler_sm_splitlink_on_connect(                                    \
            dev, &__kb_handler_sm_thread_queue__##inst);                       \
    }                                                                          \
    static void kb_handler_sm_splitlink_on_disconnect__##inst(                 \
        const struct device *dev) {                                            \
        kb_handler_sm_splitlink_on_disconnect(                                 \
            dev, &__kb_handler_sm_thread_queue__##inst);                       \
    }                                                                          \
    static void kb_handler_sm_splitlink_on_receive__##inst(                    \
        const struct device *dev, uint8_t *data, size_t data_len) {            \
        kb_handler_sm_splitlink_on_receive(                                    \
            dev, &__kb_handler_sm_thread_queue__##inst, data, data_len);       \
    }                                                                          \
    SPLITLINK_CB_DEFINE(kb_handler_sm) = {                                     \
        .connect_cb = kb_handler_sm_splitlink_on_connect__##inst,              \
        .disconnect_cb = kb_handler_sm_splitlink_on_disconnect__##inst,        \
        .on_receive_cb = kb_handler_sm_splitlink_on_receive__##inst,           \
    };                                                                         \
                                                                               \
    DEVICE_DT_INST_DEFINE(inst, kb_handler_sm_init, NULL,                      \
                          &__kb_handler_sm_data__##inst,                       \
                          &__kb_handler_sm_config__##inst, POST_KERNEL,        \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);           \
                                                                               \
    void kb_handler_sm_on_settings_update__##inst(kb_settings_t *settings) {   \
        kb_handler_sm_on_settings_update(settings, DEVICE_DT_INST_GET(inst));  \
    };                                                                         \
                                                                               \
    ON_SETTINGS_UPDATE_DEFINE(kb_handler_settings_update,                      \
                              kb_handler_sm_on_settings_update__##inst);

DT_INST_FOREACH_STATUS_OKAY(KB_HANDLER_SM_DEFINE)
