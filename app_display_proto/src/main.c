/*
 * Copyright (c) 2025 Sergey Golovin <golovinengineer@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void) {
    int err;
    bool led_state = true;

    const struct device *const aht20 = DEVICE_DT_GET_ONE(aosong_aht20);

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

        LOG_INF("SHT3XD: %.2f Cel ; %0.2f %%RH", sensor_value_to_double(&temp),
                sensor_value_to_double(&hum));

        err = gpio_pin_toggle_dt(&led);

        if (err < 0) {
            LOG_ERR("led gpio toggle is failed");
        }

        led_state = !led_state;
        LOG_INF("LED state: %s", led_state ? "ON" : "OFF");
        k_msleep(SLEEP_TIME_MS);
    }
    return 0;
}
