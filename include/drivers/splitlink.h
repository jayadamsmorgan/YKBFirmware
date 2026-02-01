#ifndef __DRIVERS_SPLITLINK_H_
#define __DRIVERS_SPLITLINK_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

struct splitlink_cb {
    void (*on_receive_cb)(uint8_t *data, size_t data_len);
    void (*connect_cb)(void);
    void (*disconnect_cb)(void);
};

#define SPLITLINK_CB_DEFINE(name)                                              \
    static STRUCT_SECTION_ITERABLE(splitlink_cb, __splitlink_cb_##name)

typedef void (*splitlink_on_send_ack)(void);

__subsystem struct splitlink_driver_api {
    int (*send)(const struct device *dev, uint8_t *data, size_t data_len,
                splitlink_on_send_ack cb);
    int (*init)(const struct device *dev);
};

__syscall int splitlink_send(const struct device *dev, uint8_t *data,
                             size_t data_len);

__syscall int splitlink_set_recv_cb(const struct device *dev, uint8_t *data,
                                    size_t data_len);

__syscall bool splitlink_is_connected(const struct device *dev);

static inline int z_impl_splitlink_send(const struct device *dev, uint8_t *data,
                                        size_t data_len) {
    __ASSERT_NO_MSG(DEVICE_API_IS(splitlink, dev));
    return DEVICE_API_GET(splitlink, dev)->send(dev, data, data_len);
}

static inline void z_impl_splitlink_set_recv_cb(const struct device *dev,
                                                splitlink_recv_cb cb) {
    __ASSERT_NO_MSG(DEVICE_API_IS(splitlink, dev));
    return DEVICE_API_GET(splitlink, dev)->set_recv_cb(dev, cb);
}

static inline bool z_impl_splitlink_is_connected(const struct device *dev) {
    __ASSERT_NO_MSG(DEVICE_API_IS(splitlink, dev));
    return DEVICE_API_GET(splitlink, dev)->is_connected(dev);
}

#include <syscalls/splitlink.h>

#endif // __DRIVERS_SPLITLINK_H_
