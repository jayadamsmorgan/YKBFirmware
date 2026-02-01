#define DT_DRV_COMPAT splitlink_esb_master

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/devicetree/io-channels.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/splitlink.h>

#include <lib/ykb_esb.h>

LOG_MODULE_REGISTER(splitlink_esb_master, CONFIG_SPLITLINK_LOG_LEVEL);

struct splitlink_config {};
