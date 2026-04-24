#ifndef SPLITLINK_ESB_H
#define SPLITLINK_ESB_H

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/splitlink.h>

#include <lib/ykb_esb.h>

struct splitlink_config {
    const uint8_t esb_default_address[8];
};

struct delayable_device_work {
    struct k_work_delayable d_work;
    const struct device *dev;
};

struct device_work {
    struct k_work work;
    const struct device *dev;
};

struct receiving_device_work {
    struct k_work work;
    const struct device *dev;
    uint8_t data[CONFIG_ESB_MAX_PAYLOAD_LENGTH];
    uint16_t data_len;
};

struct splitlink_data {
#if CONFIG_SPLITLINK_YKB_ESB_PTX
    struct delayable_device_work alive_work;
#endif // CONFIG_SPLITLINK_YKB_ESB_PTX

    bool connected;
    bool ready;

    struct delayable_device_work init_work;
    struct device_work connect_work;
    struct delayable_device_work disconnect_work;
    struct receiving_device_work receiving_work;
};

#define FLAG_ALIVE 0U
#define FLAG_DATA 1U

#endif // SPLITLINK_ESB_H
