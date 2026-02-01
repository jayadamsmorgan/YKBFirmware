#ifndef LIB_ESB_BLE_MPSL_H_
#define LIB_ESB_BLE_MPSL_H_

#include <stdint.h>
#include <zephyr/kernel.h>

typedef enum {
    MPSL_ESB_EVT_TX_SUCCESS,
    MPSL_ESB_EVT_TX_FAIL,
    MPSL_ESB_EVT_RX
} mpsl_esb_event_type_t;

typedef struct {
    mpsl_esb_event_type_t evt_type;
    uint8_t *buf;
    uint32_t data_length;
} mpsl_esb_event_t;

typedef void (*mpsl_esb_callback_t)(mpsl_esb_event_t *event);

typedef enum { APP_TS_STARTED, APP_TS_STOPPED } timeslot_callback_type_t;

typedef void (*timeslot_callback_t)(timeslot_callback_type_t type);

void timeslot_handler_init(timeslot_callback_t callback);

#endif // LIB_ESB_BLE_MPSL_H_
