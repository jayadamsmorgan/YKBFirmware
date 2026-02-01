#ifndef __KSCAN_COMMON_H_
#define __KSCAN_COMMON_H_

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/devicetree/io-channels.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/kscan.h>

int read_io_channel(const struct adc_dt_spec *spec, uint16_t *val);

#endif // __KSCAN_COMMON_H_
