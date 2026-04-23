#ifndef ZEPHYR_USER_HELPERS_H
#define ZEPHYR_USER_HELPERS_H

#include <zephyr/kernel.h>

#define Z_USER_PATH DT_PATH(zephyr_user)
#define Z_USER_PROP(prop) DT_PROP(Z_USER_PATH, prop)
#define Z_USER_PROP_OR(prop, val) DT_PROP_OR(Z_USER_PATH, prop, val)
#define Z_USER_HAS_PROP(prop) DT_NODE_HAS_PROP(Z_USER_PATH, prop)
#define Z_USER_DEV(prop) DEVICE_DT_GET(Z_USER_PROP(prop))
#define Z_USER_PROP_LEN(prop) DT_PROP_LEN(Z_USER_PATH, prop)
#define Z_USER_PROP_LEN_OR(prop, val) DT_PROP_LEN_OR(Z_USER_PATH, prop, val)

#endif // ZEPHYR_USER_HELPERS_H
