#ifndef __APP_ESB_H
#define __APP_ESB_H

#include <lib/ykb_esb.h>

#include <zephyr/kernel.h>

typedef enum {
    APP_ESB_EVT_TX_SUCCESS,
    APP_ESB_EVT_TX_FAIL,
    APP_ESB_EVT_RX
} app_esb_event_type_t;

typedef struct {
    app_esb_event_type_t evt_type;
    uint8_t *buf;
    uint32_t data_length;
} app_esb_event_t;

typedef struct {
    uint8_t data[32];
    uint32_t len;
} app_esb_data_t;

typedef void (*app_esb_callback_t)(app_esb_event_t *event);

typedef struct {
    ykb_esb_mode_t mode;
    uint8_t addr[8];
    app_esb_callback_t callback;
} app_esb_config_t;

int app_esb_init(app_esb_config_t config);

int app_esb_send(app_esb_data_t *tx_packet);

enum rpc_command {
    RPC_COMMAND_ESB_INIT = 0x01,
    RPC_COMMAND_ESB_TX = 0x02,
};

enum rpc_event {
    RPC_EVENT_ESB_CB = 0x01,
};

#endif
