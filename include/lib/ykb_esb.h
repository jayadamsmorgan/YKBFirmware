#ifndef __LIB_YKB_ESB_H_
#define __LIB_YKB_ESB_H_

#include <stdint.h>
#include <zephyr/kernel.h>

typedef enum {
    YKB_ESB_EVT_TX_SUCCESS,
    YKB_ESB_EVT_TX_FAIL,
    YKB_ESB_EVT_RX
} ykb_esb_event_type_t;
typedef enum { YKB_ESB_MODE_PTX, YKB_ESB_MODE_PRX } ykb_esb_mode_t;

typedef struct {
    ykb_esb_event_type_t evt_type;
    uint8_t *buf;
    uint32_t data_length;
} ykb_esb_event_t;

typedef struct {
    uint8_t data[CONFIG_ESB_MAX_PAYLOAD_LENGTH];
    uint32_t len;
} ykb_esb_data_t;

typedef struct {
    ykb_esb_mode_t mode;
    void *user_ptr;
    uint8_t base_addr_0[4];
    uint8_t base_addr_1[4];
} ykb_esb_config_t;

typedef void (*ykb_esb_callback_t)(ykb_esb_event_t *event, void *user_ptr);

int ykb_esb_init(ykb_esb_config_t *config, ykb_esb_callback_t callback);

int ykb_esb_send(ykb_esb_data_t *tx_packet);

int ykb_esb_rpc_start(void);

#endif // __LIB_YKB_ESB_H_
