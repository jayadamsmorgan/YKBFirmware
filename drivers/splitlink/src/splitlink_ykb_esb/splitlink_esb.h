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
    struct k_thread alive_thread;
    k_thread_stack_t *alive_thread_stack;
#endif // CONFIG_SPLITLINK_YKB_ESB_PTX

    bool connected;

    struct delayable_device_work init_work;
    struct device_work connect_work;
    struct delayable_device_work disconnect_work;
    struct receiving_device_work receiving_work;
};

int splitlink_ykb_esb_send(const struct device *dev, uint8_t *data,
                           size_t data_len);

#define FLAG_ALIVE 0U
#define FLAG_DATA 1U

#endif // SPLITLINK_ESB_H
