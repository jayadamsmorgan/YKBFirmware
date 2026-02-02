#include <lib/ykb_esb.h>

#include <lib/ykb_timeslot.h>

#include <esb.h>

#include <stdint.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ykb_esb, CONFIG_YKB_ESB_LOG_LEVEL);

/* ------------------------- App-facing state ------------------------- */

static ykb_esb_callback_t m_callback;
static ykb_esb_event_t m_event;

static ykb_esb_mode_t m_mode;
static bool m_active;
static uint8_t m_base_addr_0[4];
static uint8_t m_base_addr_1[4];

/* ------------------------- PTX TX queue ---------------------------- */
/* Used only for PTX "initiated" transmissions. */
K_MSGQ_DEFINE(m_msgq_tx_payloads, sizeof(struct esb_payload), 8, 4);

/* ------------------------- RX buffer ------------------------------- */
static struct esb_payload rx_payload;

/* ------------------------- PRX ACK cache (Pattern A) --------------- */

static struct esb_payload m_prx_ack_cache;
static bool m_prx_ack_cache_valid;

/* ------------------------- Forward decls --------------------------- */

static int clocks_start(void);
static int esb_initialize(ykb_esb_mode_t mode);
static void on_timeslot_start_stop(timeslot_callback_type_t type);

static int ptx_kick_next_from_msgq(void);

static void prx_preload_cached_ack(void);
static void prx_set_cached_ack_from_data(const uint8_t *data, size_t len);

/* ------------------------- ESB event handler ----------------------- */

static void event_handler(struct esb_evt const *event) {
    static struct esb_payload tmp_payload;

    switch (event->evt_id) {

    case ESB_EVENT_TX_SUCCESS:
        LOG_DBG("ESB_EVENT_TX_SUCCESS");

        /* PTX: pop the item we previously peeked */
        if (m_mode == YKB_ESB_MODE_PTX) {
            (void)k_msgq_get(&m_msgq_tx_payloads, &tmp_payload, K_NO_WAIT);
        }

        /* Many ESB implementations deliver ACK payload to RX FIFO after TX. */
        while (esb_read_rx_payload(&rx_payload) == 0) {
            m_event.evt_type = YKB_ESB_EVT_RX;
            m_event.buf = rx_payload.data;
            m_event.data_length = rx_payload.length;
            m_callback(&m_event);
        }

        m_event.evt_type = YKB_ESB_EVT_TX_SUCCESS;
        m_event.data_length = 0;
        m_callback(&m_event);

        /* PTX: send next queued payload */
        if (m_mode == YKB_ESB_MODE_PTX) {
            (void)ptx_kick_next_from_msgq();
        }

        break;

    case ESB_EVENT_TX_FAILED:
        LOG_DBG("ESB_EVENT_TX_FAILED");

        if (m_mode == YKB_ESB_MODE_PTX) {
            /* Flush and retry later; your original behavior. */
            esb_flush_tx();
            (void)ptx_kick_next_from_msgq();
        }

        m_event.evt_type = YKB_ESB_EVT_TX_FAIL;
        m_event.data_length = 0;
        m_callback(&m_event);
        break;

    case ESB_EVENT_RX_RECEIVED:
        while (esb_read_rx_payload(&rx_payload) == 0) {
            LOG_DBG("RX len=%u", rx_payload.length);

            m_event.evt_type = YKB_ESB_EVT_RX;
            m_event.buf = rx_payload.data;
            m_event.data_length = rx_payload.length;
            m_callback(&m_event);
        }
        break;

    default:
        break;
    }
}

/* ------------------------- Clock start ----------------------------- */

static int clocks_start(void) {
    int err;
    int res;
    struct onoff_manager *clk_mgr;
    struct onoff_client clk_cli;

    clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
    if (!clk_mgr) {
        LOG_ERR("Unable to get HF clock manager");
        return -ENXIO;
    }

    sys_notify_init_spinwait(&clk_cli.notify);

    err = onoff_request(clk_mgr, &clk_cli);
    if (err < 0) {
        LOG_ERR("Clock request failed: %d", err);
        return err;
    }

    do {
        err = sys_notify_fetch_result(&clk_cli.notify, &res);
        if (!err && res) {
            LOG_ERR("Clock could not be started: %d", res);
            return res;
        }
    } while (err);

    LOG_DBG("HF clock started");
    return 0;
}

/* -------------------- PRX cached ACK payload helpers ---------------- */

static void prx_preload_cached_ack(void) {
    if (m_mode != YKB_ESB_MODE_PRX) {
        return;
    }
    if (!m_prx_ack_cache_valid) {
        return;
    }

    /* "Latest wins": clear ESB TX FIFO and preload one payload. */
    esb_flush_tx();

    int ret = esb_write_payload(&m_prx_ack_cache);
    LOG_DBG("PRX preload ack: ret=%d len=%u", ret, m_prx_ack_cache.length);

    /* If ret < 0, you can log; retry will happen on next resume/update. */
}

static void prx_set_cached_ack_from_data(const uint8_t *data, size_t len) {
    if (m_mode != YKB_ESB_MODE_PRX) {
        return;
    }

    if (!data || len > sizeof(m_prx_ack_cache.data)) {
        return;
    }

    m_prx_ack_cache.pipe = 0;
    m_prx_ack_cache.noack = false;
    m_prx_ack_cache.length = (uint8_t)len;
    memcpy(m_prx_ack_cache.data, data, len);
    m_prx_ack_cache_valid = true;

    if (m_active) {
        prx_preload_cached_ack();
    }
}

/* Optional public helper:
 * If you want to explicitly set the PRX ACK payload from the app, expose this
 * in your header. If you don't want a new API, see ykb_esb_send() PRX handling
 * below.
 */
int ykb_esb_prx_set_ack_payload(const uint8_t *data, size_t len) {
    if (m_mode != YKB_ESB_MODE_PRX) {
        return -EINVAL;
    }
    if (!data || len > sizeof(m_prx_ack_cache.data)) {
        return -EMSGSIZE;
    }

    prx_set_cached_ack_from_data(data, len);
    return 0;
}

/* ------------------------- ESB init -------------------------------- */

static int esb_initialize(ykb_esb_mode_t mode) {
    int err;

    static const uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4,
                                           0xC5, 0xC6, 0xC7, 0xC8};

    struct esb_config config = ESB_DEFAULT_CONFIG;

    config.protocol = ESB_PROTOCOL_ESB_DPL;
    config.crc = ESB_CRC_8BIT;
    config.retransmit_delay = 600;
    config.retransmit_count = 1;
    config.bitrate = ESB_BITRATE_2MBPS;
    config.event_handler = event_handler;
    config.mode = (mode == YKB_ESB_MODE_PTX) ? ESB_MODE_PTX : ESB_MODE_PRX;
    config.tx_mode = ESB_TXMODE_MANUAL_START;
    config.selective_auto_ack = false;
#if CONFIG_LIB_YKB_ESB_FAST_RAMP_UP
    config.use_fast_ramp_up = true;
#endif // CONFIG_LIB_YKB_ESB_FAST_RAMP_UP

    err = esb_init(&config);
    if (err) {
        LOG_ERR("esb_init err=%d", err);
        return err;
    }

    err = esb_set_base_address_0(m_base_addr_0);
    if (err)
        return err;

    err = esb_set_base_address_1(m_base_addr_1);
    if (err)
        return err;

    err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
    if (err)
        return err;

    NVIC_SetPriority(RADIO_IRQn, 0);

#if CONFIG_LIB_YKB_ESB_MPSL
    if (mode == YKB_ESB_MODE_PRX) {
        /* Restore cached ACK payload after every init/resume */
        prx_preload_cached_ack();
        esb_start_rx();
    }
#endif // CONFIG_LIB_YKB_ESB_MPSL

    return 0;
}

/* -------------------- PTX msgq -> ESB TX ---------------------------- */

#if CONFIG_LIB_YKB_ESB_MPSL
static int ptx_kick_next_from_msgq(void) {
    int ret;
    static struct esb_payload tx_payload;

    /* PTX: Peek, and remove only on TX_SUCCESS */
    if (k_msgq_peek(&m_msgq_tx_payloads, &tx_payload) != 0) {
        return -ENOMEM;
    }

    ret = esb_write_payload(&tx_payload);
    if (ret < 0) {
        return ret;
    }

    esb_start_tx();
    return 0;
}
#endif // CONFIG_LIB_YKB_ESB_MPSL

/* ------------------------- Public API ------------------------------ */

int ykb_esb_init(ykb_esb_mode_t mode, ykb_esb_callback_t callback,
                 uint8_t base_addr_0[4], uint8_t base_addr_1[4]) {
    int ret;

    m_callback = callback;
    m_mode = mode;
    m_active = false;
    if (base_addr_0 && base_addr_1) {
        memcpy(m_base_addr_0, base_addr_0, sizeof(m_base_addr_0));
        memcpy(m_base_addr_1, base_addr_1, sizeof(m_base_addr_1));
    }

#if CONFIG_LIB_YKB_ESB_MPSL
    /* Debug GPIOs (optional) */
    // NRF_P0->DIRSET = BIT(28) | BIT(29) | BIT(30) | BIT(31) | BIT(4);
    // NRF_P0->OUTCLR = BIT(28) | BIT(29) | BIT(30) | BIT(31);

    ret = clocks_start();
    if (ret < 0) {
        return ret;
    }

    LOG_INF("Timeslot handler init");
    timeslot_handler_init(on_timeslot_start_stop);
#endif // CONFIG_LIB_YKB_ESB_MPSL

    return 0;
}

/*
 * ykb_esb_send():
 *  - PTX: queue packet for normal transmit.
 *  - PRX (Pattern A): update the "latest ACK payload" cache, so that the next
 *    PTX TX will receive it as ACK payload.
 *
 * If you prefer to keep the old semantics (PRX shouldn't accept send),
 * you can return -ENOTSUP for PRX and use ykb_esb_prx_set_ack_payload()
 * instead.
 */
int ykb_esb_send(ykb_esb_data_t *tx_packet) {
    if (!tx_packet) {
        return -EINVAL;
    }

    if (tx_packet->len > sizeof(((struct esb_payload *)0)->data)) {
        return -EMSGSIZE;
    }

#if CONFIG_LIB_YKB_ESB_MPSL
    if (m_mode == YKB_ESB_MODE_PRX) {
        /* Overwrite cached ACK payload */
        prx_set_cached_ack_from_data(tx_packet->data, tx_packet->len);
        return 0;
    }
#endif // CONFIG_LIB_YKB_ESB_MPSL

    /* PTX path */
    struct esb_payload tx_payload;
    tx_payload.pipe = 0;
    tx_payload.noack = false;
    tx_payload.length = tx_packet->len;
    memcpy(tx_payload.data, tx_packet->data, tx_packet->len);

    int ret = k_msgq_put(&m_msgq_tx_payloads, &tx_payload, K_NO_WAIT);
    if (ret != 0) {
        return -ENOMEM;
    }

    if (m_active) {
        (void)ptx_kick_next_from_msgq();
    }

    return 0;
}

/* ------------------------- Suspend/Resume --------------------------- */

#if CONFIG_LIB_YKB_ESB_MPSL
static int ykb_esb_suspend(void) {
    m_active = false;

    NRF_P0->OUTSET = BIT(29);

    if (m_mode == YKB_ESB_MODE_PTX) {
        uint32_t irq_key = irq_lock();

        irq_disable(RADIO_IRQn);
        NVIC_DisableIRQ(RADIO_IRQn);

        NRF_RADIO->SHORTS = 0;

        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->TASKS_DISABLE = 1;
        while (NRF_RADIO->EVENTS_DISABLED == 0) {
        }

        NRF_TIMER2->TASKS_STOP = 1;
        NRF_RADIO->INTENCLR = 0xFFFFFFFF;

        esb_disable();

        NVIC_ClearPendingIRQ(RADIO_IRQn);
        irq_unlock(irq_key);
    } else {
        /* PRX: stop RX and fully tear down if you're doing that each slot */
        esb_stop_rx();
        esb_disable();
    }

    NRF_P0->OUTCLR = BIT(29);

    return 0;
}

static int ykb_esb_resume(void) {
    int err;

    NRF_P0->OUTSET = BIT(29);

    err = esb_initialize(m_mode);
    m_active = (err == 0);

    NRF_P0->OUTCLR = BIT(29);

    if (err) {
        return err;
    }

    /* PTX: kick TX if anything queued */
    if (m_mode == YKB_ESB_MODE_PTX) {
        (void)ptx_kick_next_from_msgq();
    } else {
        /* PRX: ensure cached ACK payload is loaded this slot */
        prx_preload_cached_ack();
    }

    return 0;
}

/* ------------------------- Timeslot callback ------------------------ */

static void on_timeslot_start_stop(timeslot_callback_type_t type) {
    switch (type) {
    case APP_TS_STARTED:
        NRF_P0->OUTSET = BIT(31);
        (void)ykb_esb_resume();
        break;

    case APP_TS_STOPPED:
        NRF_P0->OUTCLR = BIT(31);
        (void)ykb_esb_suspend();
        break;

    default:
        break;
    }
}

#endif // CONFIG_LIB_YKB_ESB_MPSL
