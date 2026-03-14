#ifndef __DRIVERS_SPLITLINK_H_
#define __DRIVERS_SPLITLINK_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

struct splitlink_cb {
    void (*on_receive_cb)(const struct device *dev, uint8_t *data,
                          size_t data_len);
    void (*connect_cb)(const struct device *dev);
    void (*disconnect_cb)(const struct device *dev);
};

#define SPLITLINK_CB_DEFINE(name)                                              \
    static STRUCT_SECTION_ITERABLE(splitlink_cb, __splitlink_cb_##name)

__subsystem struct splitlink_driver_api {
    int (*send)(const struct device *dev, uint8_t *data, size_t data_len);
};

__syscall int splitlink_send(const struct device *dev, uint8_t *data,
                             size_t data_len);

int splitlink_esb_init(const struct device *dev);

static inline int z_impl_splitlink_send(const struct device *dev, uint8_t *data,
                                        size_t data_len) {
    __ASSERT_NO_MSG(DEVICE_API_IS(splitlink, dev));
    return DEVICE_API_GET(splitlink, dev)->send(dev, data, data_len);
}

#include <syscalls/splitlink.h>

#endif // __DRIVERS_SPLITLINK_H_
