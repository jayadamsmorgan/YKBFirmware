#include <sys/errno.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>

#include "lib/ykb_esb.h"
#include "ykb_esb_mpsl_common.h"
#include "zephyr/sys/__assert.h"

#include <mdk/nrf.h>

#include <nrf_rpc/nrf_rpc_ipc.h>
#include <nrf_rpc_cbor.h>
#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#define CBOR_BUF_SIZE 16

LOG_MODULE_REGISTER(rpc_app, LOG_LEVEL_DBG);

/* This defines a transport for our RPC command group.
 * Here we use the IPC transport (nrf_rpc_ipc.h):
 * - we tell it to use the IPC device `ipc0` (in the devicetree)
 * - we tell it to use an endpoint named `nrf_rpc_ept`. There can be multiple
 * endpoints, e.g. one for HCI and one for nRF RPC. Usually it's not one ept per
 * API, it's one ept per library (hci uses one, 802154 another, nRF RPC another,
 * and so on).
 */
NRF_RPC_IPC_TRANSPORT(esb_group_tr, DEVICE_DT_GET(DT_NODELABEL(ipc0)),
                      "nrf_rpc_ept");

/* This defines the group for our API.
 *
 * Command groups are used to logically separate APIs called over nRF RPC: e.g.,
 * we can have a Bluetooth group (e.g. for Bluetooth host over nRF RPC, which is
 * not used in this sample), an ESB group and let's say a crypto group.
 *
 * This aids the application developer, as he now doesn't have to keep track of
 * all the registered nRF RPC command IDs. It also allows for more flexibility,
 * as modules making use of nRF RPC can be compiled in and out without needed to
 * edit the command IDs every time.
 */
NRF_RPC_GROUP_DEFINE(esb_group, "esb_group_id", &esb_group_tr, NULL, NULL,
                     NULL);

static ykb_esb_on_receive on_receive = NULL;
static app_esb_event_t m_event;
static uint8_t m_rx_buf[32];

void on_esb_callback(app_esb_event_t *event) {
    static uint32_t last_counter = 0;
    static uint32_t counter;
    switch (event->evt_type) {
    case APP_ESB_EVT_TX_SUCCESS:
        LOG_INF("ESB TX success");
        break;
    case APP_ESB_EVT_TX_FAIL:
        LOG_INF("ESB TX failed");
        break;
    case APP_ESB_EVT_RX:
        memcpy((uint8_t *)&counter, event->buf, sizeof(counter));
        if (counter != (last_counter + 1)) {
            LOG_WRN("Packet content error! Counter: %i, last counter %i",
                    counter, last_counter);
        }
        LOG_INF("ESB RX: 0x%.2X-0x%.2X-0x%.2X-0x%.2X", event->buf[0],
                event->buf[1], event->buf[2], event->buf[3]);
        last_counter = counter;
        if (on_receive) {
            on_receive(event->buf, event->data_length);
        }
        break;
    default:
        LOG_ERR("Unknown APP ESB event!");
        break;
    }
}

/* - Pull an error code from the RPC CBOR buffer
 * - Place it in `handler_data`, retrieved in the ESB API and passed to the
 * application
 * - Also return it (as a convenience)
 */
static int decode_error(const struct nrf_rpc_group *group,
                        struct nrf_rpc_cbor_ctx *ctx, void *handler_data) {
    int err;
    int32_t *p_err = (int32_t *)handler_data;

    if (!zcbor_int32_decode(ctx->zs, &err)) {
        err = -EBADMSG;
    }

    if (p_err) {
        *p_err = err;
    }

    return err;
}

/* Default response handler. Decodes the error returned by the ESB API (ran
 * on the other core) and frees the CBOR buffer.
 */
static void rpc_rsp_handler(const struct nrf_rpc_group *group,
                            struct nrf_rpc_cbor_ctx *ctx, void *handler_data) {
    int err = decode_error(group, ctx, handler_data);
    LOG_INF("rsp_handler addr ctx %x, error %i", (uint32_t)ctx, err);
    nrf_rpc_cbor_decoding_done(&esb_group, ctx);
}

static int rpc_esb_init(app_esb_config_t *p_config) {
    int32_t err = 0;
    int err_rpc;
    struct nrf_rpc_cbor_ctx ctx;
    size_t config_len = sizeof(app_esb_config_t);

    NRF_RPC_CBOR_ALLOC(&esb_group, ctx, CBOR_BUF_SIZE + config_len);

    /* Serialize the `config` struct to a byte array, encode it and place it
     * in the CBOR buffer.
     *
     * We play fast and loose with the memory layout because we assume that
     * the other core's FW was compiled with the exact same toolchain and
     * compiler options, resulting in the same memory layout on the other
     * side.
     *
     * Note: a gotcha is that the `zcbor_` APIs return `true` on success,
     * whereas almost all zephyr (and other NCS) APIs return a `0` on success.
     */
    if (!zcbor_bstr_encode_ptr(ctx.zs, (const uint8_t *)p_config, config_len)) {
        return -EINVAL;
    }

    LOG_DBG("RPC ESB Init cmd. Err %i", err);

    err_rpc = nrf_rpc_cbor_cmd(&esb_group, RPC_COMMAND_ESB_INIT, &ctx,
                               rpc_rsp_handler, &err);

    /* Return a fixed error code if the RPC transport had an error. Else,
     * return the result of the API called on the other core.
     */
    if (err_rpc) {
        return -EINVAL;
    } else {
        return err;
    }
}

static int rpc_esb_tx(app_esb_data_t *packet) {
    int32_t err;
    int err_rpc;
    struct nrf_rpc_cbor_ctx ctx;
    size_t packet_len = sizeof(app_esb_data_t);

    NRF_RPC_CBOR_ALLOC(&esb_group, ctx, CBOR_BUF_SIZE + packet_len);

    /* Serialize the `config` struct to a byte array, encode it and place it
     * in the CBOR buffer.
     *
     * We play fast and loose with the memory layout because we assume that
     * the other core's FW was compiled with the exact same toolchain and
     * compiler options, resulting in the same memory layout on the other
     * side.
     *
     * Note: a gotcha is that the `zcbor_` APIs return `true` on success,
     * whereas almost all zephyr (and other NCS) APIs return a `0` on success.
     */
    if (!zcbor_bstr_encode_ptr(ctx.zs, (const uint8_t *)packet, packet_len)) {
        return -EINVAL;
    }

    LOG_DBG("RPC ESB TX cmd: Byte 0: %x", packet->data[0]);

    err_rpc = nrf_rpc_cbor_cmd(&esb_group, RPC_COMMAND_ESB_TX, &ctx,
                               rpc_rsp_handler, &err);

    /* Return a fixed error code if the RPC transport had an error. Else,
     * return the result of the API called on the other core.
     */
    if (err_rpc) {
        return -EINVAL;
    } else {
        return err;
    }
}

static void rpc_esb_event_handler(const struct nrf_rpc_group *group,
                                  struct nrf_rpc_cbor_ctx *ctx,
                                  void *handler_data) {
    int err;
    uint32_t rx_payload_length = 0;
    struct zcbor_string zst;
    int evt_type;

    /* Try pulling the error code. */
    err = decode_error(group, ctx, handler_data);

    if (err || !zcbor_uint32_decode(ctx->zs, &evt_type)) {
        err = -EBADMSG;
    }

    if (err || !zcbor_uint32_decode(ctx->zs, &rx_payload_length)) {
        err = -EBADMSG;
    }

    if (rx_payload_length > 0) {
        // An RX payload is included in the message. Try to parse it.
        if (err || !zcbor_bstr_decode(ctx->zs, &zst)) {
            err = -EBADMSG;
        }

        if (zst.len != rx_payload_length) {
            LOG_ERR("struct size mismatch: expect %d got %d", rx_payload_length,
                    zst.len);
            err = -EMSGSIZE;
        }

        if (!err) {
            memcpy(m_rx_buf, zst.value, zst.len);
            LOG_DBG("decoding ok: rx_payload length %i", rx_payload_length);
        } else {
            LOG_ERR("%s: decoding error %d", __func__, err);
        }
    }

    LOG_INF("evt_type %i, p_rx_payload length %i", evt_type, rx_payload_length);

    nrf_rpc_cbor_decoding_done(&esb_group, ctx);

    /* Notify the app new data has been received. */
    if (!err) {
        LOG_DBG("decoding ok: rx_payload length %i", rx_payload_length);

        // Call the event handler registered by the application
        m_event.evt_type = evt_type;
        m_event.buf = m_rx_buf;
        m_event.data_length = rx_payload_length;
        on_esb_callback(&m_event);
    } else {
        LOG_ERR("%s: decoding error %d", __func__, err);
    }
}

/* Register the RX event handler (function above). This will be sent from the
 * other side whenever we are in async mode and a packet has been received.
 */
NRF_RPC_CBOR_EVT_DECODER(esb_group, rx_cb_handler, RPC_EVENT_ESB_CB,
                         rpc_esb_event_handler, NULL);

/* Initialize nRF RPC right after kernel boots, but before the application is
 * run.
 */
static void err_handler(const struct nrf_rpc_err_report *report) {
    LOG_ERR("nRF RPC error %d. Enable nRF RPC logs for details.", report->code);

    k_oops();
}

static int serialization_init(void) {
    int err;

    LOG_DBG("esb rpc init begin");

    err = nrf_rpc_init(err_handler);
    if (err) {
        return -EINVAL;
    }

    LOG_DBG("esb rpc init ok");

    return 0;
}

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

int ykb_esb_init(ykb_esb_init_config_t *config) {
    on_receive = config->on_receive;
    app_esb_config_t app_config = {
        .callback = on_esb_callback,
        .mode = config->mode,
    };
    memcpy(app_config.addr, config->addr, sizeof(app_config.addr));

    return rpc_esb_init(&app_config);
}

int ykb_esb_send(uint8_t data[32], size_t data_len) {
    if (data_len > 32) {
        return -EINVAL;
    }
    app_esb_data_t packet;
    memcpy(packet.data, data, data_len);
    packet.len = data_len;

    rpc_esb_tx(&packet);
    return 0;
}
