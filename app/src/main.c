#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define NUM_PIXELS 5

static const struct device *strip = DEVICE_DT_GET(DT_CHOSEN(ykb_led_strip));

static struct led_rgb pixels[NUM_PIXELS] __nocache __aligned(4);

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

    while (true) {
        struct led_rgb tmp = pixels[0];
        for (int i = 0; i < NUM_PIXELS - 1; i++) {
            pixels[i] = pixels[i + 1];
        }
        pixels[4] = tmp;

        led_strip_update_rgb(strip, pixels, NUM_PIXELS);
        k_sleep(K_MSEC(2000));
    }

    return 0;
}
