#include "splitlink_esb.h"

#if CONFIG_SPLITLINK_YKB_ESB_PTX
LOG_MODULE_DECLARE(splitlink_esb_ptx);
#endif // CONFIG_SPLITLINK_YKB_ESB_PTX
#if CONFIG_SPLITLINK_YKB_ESB_PRX
LOG_MODULE_DECLARE(splitlink_esb_prx);
#endif // CONFIG_SPLITLINK_YKB_ESB_PTX

int splitlink_ykb_esb_send(const struct device *dev, uint8_t *data,
                           size_t data_len) {
    if (data_len == 0 || data == NULL) {
        LOG_ERR("Invalid argument.");
        return -EINVAL;
    }
    if (data_len > CONFIG_ESB_MAX_PAYLOAD_LENGTH - 1) {
        LOG_ERR("Packet length is too high (%u > %u)", data_len,
                CONFIG_ESB_MAX_PAYLOAD_LENGTH - 1);
        return -EINVAL;
    }

    ykb_esb_data_t packet = {
        .len = data_len + 1,
    };
    memcpy(&packet.data[1], data, data_len);
    packet.data[0] = FLAG_DATA;

    return ykb_esb_send(&packet);
}
