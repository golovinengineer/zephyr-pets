/*
 * Copyright (c) 2025 Sergey Golovin <golovinengineer@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
/*#include <stdlib.h>*/

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
#include <string.h>
#include <zephyr/display/cfb.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 500

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct device *const aht20 = DEVICE_DT_GET_ONE(aosong_aht20);
static const struct device *const seg_display
    = DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(holtek_ht16k33));

static const uint8_t seg_display_font_table[] = {

    0b00111111,  // 0
    0b00000110,  // 1
    0b01011011,  // 2
    0b01001111,  // 3
    0b01100110,  // 4
    0b01101101,  // 5
    0b01111101,  // 6
    0b00000111,  // 7
    0b01111111,  // 8
    0b01101111,  // 9
    0b01000000,  // -
    0b10000000,  // .
};

static int seg_display_show(char *str) {
    int err = 0;
    uint8_t digit_cursor = 5 - strlen(str);

    for (int i = 0; i < strlen(str); i++) {
        switch (str[i]) {
        case '.': {
            switch (digit_cursor) {
            case 1:
                led_on(seg_display, 7);
                break;
            case 2:
                led_on(seg_display, 23);
                break;
            case 3:
                led_on(seg_display, 55);
                break;
            case 4:
                led_on(seg_display, 71);
                break;
            default:
                break;
            }
            break;
        }
        case '-': {
            for (int seg = 0; seg < 7; seg++) {
                if (seg_display_font_table[10] & BIT(seg)) {
                    led_on(seg_display, (digit_cursor < 2) ? (digit_cursor * 16 + seg)
                                                           : ((digit_cursor + 1) * 16 + seg));
                } else {
                    led_off(seg_display, (digit_cursor < 2) ? (digit_cursor * 16 + seg)
                                                            : ((digit_cursor + 1) * 16 + seg));
                }
            }
            digit_cursor++;
            break;
        }
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            for (int seg = 0; seg < 7; seg++) {
                if (seg_display_font_table[str[i] - 0x30] & BIT(seg)) {
                    led_on(seg_display, (digit_cursor < 2) ? (digit_cursor * 16 + seg)
                                                           : ((digit_cursor + 1) * 16 + seg));
                } else {
                    led_off(seg_display, (digit_cursor < 2) ? (digit_cursor * 16 + seg)
                                                            : ((digit_cursor + 1) * 16 + seg));
                }
            }
            digit_cursor++;
            break;
        }
        default: {
            break;
        }
        }
    }
    return err;
}

int main(void) {
    int err;
    bool led_state = true;
    uint16_t x_res;
    uint16_t y_res;
    uint16_t rows;
    uint8_t ppt;
    uint8_t font_width;
    uint8_t font_height;

    const struct device *display_dev;

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Device not ready, aborting test");
    }

    if (!device_is_ready(seg_display)) {
        LOG_ERR("device %s is not ready", seg_display->name);
    }

    if (!device_is_ready(aht20)) {
        LOG_ERR("device %s is not ready", aht20->name);
    }

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("led gpio is not ready");
    }

    err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("led gpio configuration is failed");
    }

    if (display_set_pixel_format(display_dev, PIXEL_FORMAT_MONO10) != 0) {
        if (display_set_pixel_format(display_dev, PIXEL_FORMAT_MONO01) != 0) {
            LOG_ERR("Failed to set required pixel format");
            return 0;
        }
    }

    printf("Initialized %s\n", display_dev->name);

    if (cfb_framebuffer_init(display_dev)) {
        LOG_ERR("Framebuffer initialization failed!");
        return 0;
    }

    cfb_framebuffer_clear(display_dev, true);

    display_blanking_off(display_dev);

    x_res = cfb_get_display_parameter(display_dev, CFB_DISPLAY_WIDTH);
    y_res = cfb_get_display_parameter(display_dev, CFB_DISPLAY_HEIGH);
    rows = cfb_get_display_parameter(display_dev, CFB_DISPLAY_ROWS);
    ppt = cfb_get_display_parameter(display_dev, CFB_DISPLAY_PPT);

    for (int idx = 0; idx < 42; idx++) {
        if (cfb_get_font_size(display_dev, idx, &font_width, &font_height)) {
            break;
        }
        cfb_framebuffer_set_font(display_dev, idx);
        LOG_INF("font width %d, font height %d", font_width, font_height);
    }
    LOG_INF("x_res %d, y_res %d, ppt %d, rows %d, cols %d", x_res, y_res, ppt, rows,
            cfb_get_display_parameter(display_dev, CFB_DISPLAY_COLS));
    /*cfb_framebuffer_invert(display_dev);*/
    cfb_framebuffer_set_font(display_dev, 2);
    display_set_brightness(display_dev, 100);

    cfb_set_kerning(display_dev, 3);
    while (1) {

        struct sensor_value temp, hum;

        err = sensor_sample_fetch(aht20);
        if (err == 0) {
            err = sensor_channel_get(aht20, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        }
        if (err == 0) {
            err = sensor_channel_get(aht20, SENSOR_CHAN_HUMIDITY, &hum);
        }
        if (err != 0) {
            LOG_ERR("aht20 sample fetch is failed: %d", err);
        }

        char temp_str[5];
        char oled_str[6];
        snprintf(temp_str, 5, "%3.1f", sensor_value_to_double(&temp));
        snprintf(oled_str, 5, "%3.1fC", sensor_value_to_double(&temp));
        seg_display_show(temp_str);

        err = gpio_pin_toggle_dt(&led);

        if (err < 0) {
            LOG_ERR("led gpio toggle is failed");
        }

        led_state = !led_state;

        cfb_framebuffer_clear(display_dev, false);
        if (cfb_print(display_dev, oled_str, 0, 0)) {
            LOG_ERR("Failed to print a string");
            continue;
        }

        cfb_framebuffer_finalize(display_dev);
        k_msleep(SLEEP_TIME_MS);
    }
    return 0;
}
