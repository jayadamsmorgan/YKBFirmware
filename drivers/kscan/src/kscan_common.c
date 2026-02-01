#include "kscan_common.h"

int read_io_channel(const struct adc_dt_spec *spec, uint16_t *val) {
    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(uint16_t),
    };
    adc_sequence_init_dt(spec, &sequence);
    int err = adc_read_dt(spec, &sequence);
    if (err < 0) {
        return err;
    }
    *val = buf;
    return 0;
}
