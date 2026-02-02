#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <lib/ykb_esb.h>

#include <app_version.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define NUM_PIXELS 5

static const struct device *strip = DEVICE_DT_GET(DT_CHOSEN(ykb_led_strip));

static struct led_rgb pixels[NUM_PIXELS] __nocache __aligned(4);

static int64_t uptime = 0;

static const char *ptx_str = "Hello from ptx";

static void on_esb_callback(ykb_esb_event_t *event) {
    switch (event->evt_type) {
    case YKB_ESB_EVT_TX_SUCCESS: {
        int64_t delta = k_uptime_get() - uptime;
        if (delta > 3) {
            printk("%lld\n", delta);
        }
        break;
    }
    case YKB_ESB_EVT_TX_FAIL:
        LOG_INF("ESB TX failed");
        break;
    case YKB_ESB_EVT_RX: {
        char str_buf[32];
        memcpy(str_buf, event->buf, event->data_length);
        LOG_INF("Received: %s", str_buf);
        break;
    }
    }
}

int main(void) {

    if (!device_is_ready(strip)) {
        printk("LED strip device not ready\n");
        return 0;
    }

    pixels[0].r = 0;
    pixels[0].g = 0;
    pixels[0].b = 10;
    pixels[1].r = 0;
    pixels[1].g = 10;
    pixels[1].b = 0;
    pixels[2].r = 10;
    pixels[2].g = 0;
    pixels[2].b = 0;
    pixels[3].r = 10;
    pixels[3].g = 10;
    pixels[3].b = 0;
    pixels[4].r = 0;
    pixels[4].g = 10;
    pixels[4].b = 10;

    k_sleep(K_MSEC(100));

    uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
    uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
    int err = ykb_esb_init(YKB_ESB_MODE_PTX, on_esb_callback, base_addr_0,
                           base_addr_1);
    if (err) {
        LOG_ERR("ykb_esb init failed (err %d)", err);
        return err;
    }

    static ykb_esb_data_t my_data;

    while (true) {
        struct led_rgb tmp = pixels[0];
        for (int i = 0; i < NUM_PIXELS - 1; i++) {
            pixels[i] = pixels[i + 1];
        }
        pixels[4] = tmp;

        my_data.len = strlen(ptx_str);
        memcpy(my_data.data, ptx_str, strlen(ptx_str));
        uptime = k_uptime_get();
        err = ykb_esb_send(&my_data);

        // led_strip_update_rgb(strip, pixels, NUM_PIXELS);

        k_sleep(K_MSEC(100));
    }

    return 0;
}
