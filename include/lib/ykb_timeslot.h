#ifndef __LIB_YKB_TIMESLOT_H_
#define __LIB_YKB_TIMESLOT_H_

typedef enum { APP_TS_STARTED, APP_TS_STOPPED } timeslot_callback_type_t;
typedef void (*timeslot_callback_t)(timeslot_callback_type_t type);

void timeslot_handler_init(timeslot_callback_t callback);

#endif // __LIB_YKB_TIMESLOT_H_
