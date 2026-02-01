#ifndef __LIB_YKB_ESB_H_
#define __LIB_YKB_ESB_H_

#include <stddef.h>
#include <stdint.h>

typedef void (*ykb_esb_on_receive)(uint8_t *data, size_t data_len);

typedef enum { YKB_ESB_MODE_PTX, YKB_ESB_MODE_PRX } ykb_esb_mode_t;

typedef struct {
    ykb_esb_on_receive on_receive;
    uint8_t addr[8];
    ykb_esb_mode_t mode;
} ykb_esb_init_config_t;

int ykb_esb_init(ykb_esb_init_config_t *config);

int ykb_esb_send(uint8_t data[32], size_t data_len);

#endif // __LIB_YKB_ESB_H_
