#ifndef __DRIVERS_KSCAN_H_
#define __DRIVERS_KSCAN_H_

#include "zephyr/sys/__assert.h"
#include <zephyr/device.h>
#include <zephyr/toolchain.h>

struct kscan_cb {
    void (*on_press)(uint16_t index);
    void (*on_release)(uint16_t index);
};

#define KSCAN_CB_DEFINE(name)                                                  \
    static STRUCT_SECTION_ITERABLE(kscan_cb, __kscan_cb__##name)

__subsystem struct kscan_driver_api {
    int (*set_thresholds)(const struct device *dev, uint16_t *thresholds);
    int (*get_thresholds)(const struct device *dev, uint16_t *thresholds);
    int (*set_default_thresholds)(const struct device *dev);
    int (*get_key_amount)(const struct device *dev);
    int (*get_idx_offset)(const struct device *dev);
};

// Set thresholds for the keys kscan instance is managing.
// Thresholds are 10 bit non-zero values which define at what point each key is
// considered pressed.
// Thresholds map one to one to the keys controlled by the
// KScan instance. So thresholds length must be the same as get_key_amount()
//
// Returns 0 on success, negative value otherwise
__syscall int kscan_set_thresholds(const struct device *dev,
                                   uint16_t *thresholds);

// Get current thresholds.
// Caller must allocate thresholds array of size get_key_amount()
//
// Returns 0 on success, negative value otherwise
__syscall int kscan_get_thresholds(const struct device *dev,
                                   uint16_t *thresholds);

// Set default thresholds.
__syscall int kscan_set_default_thresholds(const struct device *dev);

// Get the amount of keys managed by the KScan instance.
//
// Returns key_amount on success, negative value otherwise
__syscall int kscan_get_key_amount(const struct device *dev);

// Get the index offset of the KScan instance.
// The value which KScan adds to the key index to report to a KScan callbacks
//
// Returns idx_offset on success, negative value otherwise
__syscall int kscan_get_idx_offset(const struct device *dev);

static inline int z_impl_kscan_set_thresholds(const struct device *dev,
                                              uint16_t *thresholds) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kscan, dev));
    return DEVICE_API_GET(kscan, dev)->set_thresholds(dev, thresholds);
}

static inline int z_impl_kscan_get_thresholds(const struct device *dev,
                                              uint16_t *thresholds) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kscan, dev));
    return DEVICE_API_GET(kscan, dev)->get_thresholds(dev, thresholds);
}

static inline int
z_impl_kscan_set_default_thresholds(const struct device *dev) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kscan, dev));
    return DEVICE_API_GET(kscan, dev)->set_default_thresholds(dev);
}

static inline int z_impl_kscan_get_key_amount(const struct device *dev) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kscan, dev));
    return DEVICE_API_GET(kscan, dev)->get_key_amount(dev);
}

static inline int z_impl_kscan_get_idx_offset(const struct device *dev) {
    __ASSERT_NO_MSG(DEVICE_API_GET(kscan, dev));
    return DEVICE_API_GET(kscan, dev)->get_idx_offset(dev);
}

#include <syscalls/kscan.h>

#endif // __DRIVERS_KSCAN_H_
