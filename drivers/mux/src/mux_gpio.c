#define DT_DRV_COMPAT mux_gpio

#include <drivers/mux.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mux_gpio, CONFIG_MUX_LOG_LEVEL);

struct mux_gpio_config {
    struct gpio_dt_spec sel[CONFIG_MUX_GPIO_MAX_SEL_CNT];
    uint8_t sel_cnt;

    struct gpio_dt_spec en;
    bool has_en;

    uint16_t idle_chan;
    bool has_idle_chan;

    uint16_t channels; // logical usable channels

    uint32_t settle_us;

    const uint32_t *channel_map; // logical index -> raw bit pattern
    uint16_t channel_map_len;
};

struct mux_gpio_data {
    uint16_t current; // logical channel index
    bool enabled;
};

static int drive_bits(const struct mux_gpio_config *cfg, uint32_t pattern) {
    for (uint8_t i = 0; i < cfg->sel_cnt; ++i) {
        int level = (pattern >> i) & 0x1;
        int ret = gpio_pin_set_dt(&cfg->sel[i], level);
        if (ret) {
            return ret;
        }
    }
    return 0;
}

static inline void settle_delay(uint32_t settle_us) {
    if (settle_us) {
        k_busy_wait(settle_us);
    }
}

static int mux_gpio_select(const struct device *dev, uint32_t channel) {
    const struct mux_gpio_config *cfg = dev->config;
    struct mux_gpio_data *data = dev->data;

    if (channel >= cfg->channels) {
        return -EINVAL;
    }

    uint32_t pattern = cfg->channel_map ? cfg->channel_map[channel] : channel;

    int ret = drive_bits(cfg, pattern);
    if (ret) {
        return ret;
    }

    settle_delay(cfg->settle_us);

    data->current = (uint16_t)channel;
    return 0;
}

static int mux_gpio_select_next(const struct device *dev) {
    const struct mux_gpio_config *cfg = dev->config;
    const struct mux_gpio_data *data = dev->data;

    uint16_t next = (uint16_t)((data->current + 1U) % cfg->channels);
    return mux_gpio_select(dev, next);
}

static int mux_gpio_enable(const struct device *dev) {
    const struct mux_gpio_config *cfg = dev->config;
    struct mux_gpio_data *data = dev->data;

    if (data->enabled) {
        return 0;
    }

    if (!cfg->has_en) {
        data->enabled = true;
        return 0;
    }

    int ret = gpio_pin_set_dt(&cfg->en, 1);
    if (ret) {
        return ret;
    }

    data->enabled = true;
    return 0;
}

static int mux_gpio_disable(const struct device *dev) {
    const struct mux_gpio_config *cfg = dev->config;
    struct mux_gpio_data *data = dev->data;

    if (!data->enabled) {
        return 0;
    }

    if (cfg->has_idle_chan) {
        int ret = mux_gpio_select(dev, cfg->idle_chan);
        if (ret) {
            return ret;
        }
    }

    if (!cfg->has_en) {
        data->enabled = false;
        return 0;
    }

    int ret = gpio_pin_set_dt(&cfg->en, 0);
    if (ret) {
        return ret;
    }

    data->enabled = false;
    return 0;
}

static int mux_gpio_get_current_channel(const struct device *dev) {
    const struct mux_gpio_data *data = dev->data;
    return (int)data->current;
}

static int mux_gpio_get_channel_amount(const struct device *dev) {
    const struct mux_gpio_config *cfg = dev->config;
    return (int)cfg->channels;
}

static bool mux_gpio_is_enabled(const struct device *dev) {
    const struct mux_gpio_data *data = dev->data;
    return data->enabled;
}

static DEVICE_API(mux, mux_gpio_api) = {
    .select = &mux_gpio_select,
    .select_next = &mux_gpio_select_next,

    .enable = &mux_gpio_enable,
    .disable = &mux_gpio_disable,
    .is_enabled = &mux_gpio_is_enabled,

    .get_current_channel = &mux_gpio_get_current_channel,
    .get_channel_amount = &mux_gpio_get_channel_amount,
};

static int mux_gpio_init(const struct device *dev) {
    const struct mux_gpio_config *cfg = dev->config;
    struct mux_gpio_data *data = dev->data;

    for (uint8_t i = 0; i < cfg->sel_cnt; ++i) {
        if (!device_is_ready(cfg->sel[i].port)) {
            return -ENODEV;
        }
        int ret = gpio_pin_configure_dt(&cfg->sel[i], GPIO_OUTPUT_INACTIVE);
        if (ret) {
            return ret;
        }
    }

    if (cfg->has_en) {
        if (!device_is_ready(cfg->en.port)) {
            return -ENODEV;
        }
        int ret = gpio_pin_configure_dt(&cfg->en, GPIO_OUTPUT_INACTIVE);
        if (ret) {
            return ret;
        }
        data->enabled = false;
    } else {
        data->enabled = true;
    }

    uint16_t init_chan = cfg->has_idle_chan ? cfg->idle_chan : 0U;

    int ret = mux_gpio_select(dev, init_chan);
    if (ret) {
        return ret;
    }

    return 0;
}

#define SEL_CNT(inst) DT_INST_PROP_LEN(inst, sel_gpios)

#define GET_SEL_SPEC(idx, node_id)                                             \
    GPIO_DT_SPEC_GET_BY_IDX(node_id, sel_gpios, idx)

#define CHANNEL_MAP_OR_NULL(node_id)                                           \
    COND_CODE_1(DT_NODE_HAS_PROP(node_id, channel_map),                        \
                (DT_PROP(node_id, channel_map)), (NULL))

#define CHANNEL_MAP_LEN(node_id)                                               \
    COND_CODE_1(DT_NODE_HAS_PROP(node_id, channel_map),                        \
                (DT_PROP_LEN(node_id, channel_map)), (0))

#define CHANNELS_VAL(node_id)                                                  \
    COND_CODE_1(DT_NODE_HAS_PROP(node_id, channel_map),                        \
                (DT_PROP_LEN(node_id, channel_map)),                           \
                (COND_CODE_1(DT_NODE_HAS_PROP(node_id, channels),              \
                             (DT_PROP(node_id, channels)),                     \
                             (1 << DT_PROP_LEN(node_id, sel_gpios)))))

#define MUX_GPIO_DEFINE(inst)                                                  \
    static struct mux_gpio_data mux_gpio_data##inst;                           \
                                                                               \
    static const struct mux_gpio_config mux_gpio_config##inst = {              \
        .sel = {LISTIFY(DT_INST_PROP_LEN(inst, sel_gpios), GET_SEL_SPEC, (, ), \
                        DT_DRV_INST(inst))},                                   \
        .sel_cnt = DT_PROP_LEN(DT_DRV_INST(inst), sel_gpios),                  \
        .en = GPIO_DT_SPEC_GET_OR(DT_DRV_INST(inst), enable_gpios, {0}),       \
        .has_en = DT_NODE_HAS_PROP(DT_DRV_INST(inst), enable_gpios),           \
        .channels = CHANNELS_VAL(DT_DRV_INST(inst)),                           \
        .has_idle_chan = DT_NODE_HAS_PROP(DT_DRV_INST(inst), idle_channel),    \
        .idle_chan = DT_PROP_OR(DT_DRV_INST(inst), idle_channel, 0),           \
        .settle_us = DT_PROP_OR(DT_DRV_INST(inst), settle_us, 0),              \
        .channel_map = CHANNEL_MAP_OR_NULL(DT_DRV_INST(inst)),                 \
        .channel_map_len = CHANNEL_MAP_LEN(DT_DRV_INST(inst)),                 \
    };                                                                         \
                                                                               \
    BUILD_ASSERT(CHANNELS_VAL(DT_DRV_INST(inst)) > 0,                          \
                 "mux-gpio: channels must be > 0");                            \
                                                                               \
    BUILD_ASSERT(SEL_CNT(inst) > 0, "mux-gpio: need at least 1 sel-gpio");     \
    BUILD_ASSERT(                                                              \
        SEL_CNT(inst) < 32,                                                    \
        "mux-gpio: sel-gpios count must be < 32 for channel-map checks");      \
                                                                               \
    COND_CODE_1(DT_NODE_HAS_PROP(DT_DRV_INST(inst), channel_map),              \
                (LISTIFY(DT_PROP_LEN(DT_DRV_INST(inst), channel_map),          \
                         ASSERT_CHMAP_FITS, (;), inst);),                      \
                ())                                                            \
                                                                               \
    DEVICE_DT_INST_DEFINE(inst, mux_gpio_init, NULL, &mux_gpio_data##inst,     \
                          &mux_gpio_config##inst, POST_KERNEL,                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &mux_gpio_api);

DT_INST_FOREACH_STATUS_OKAY(MUX_GPIO_DEFINE)
