/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Right-half battery level on the PIM447 trackball LED.
 *
 * The peripheral half knows its own battery state locally (no split
 * forwarding needed). This listens on that battery event and drives the
 * trackball's RGBW LED: off when healthy, a brief orange pulse when low,
 * a faster red pulse when critical.
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/input/pim447.h>

#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

LOG_MODULE_REGISTER(splitkb_trackball_battery, CONFIG_SPLITKB_STATUS_LOG_LEVEL);

/* Pulse shape: LED on briefly, dark for the rest of the period. */
#define PULSE_ON_MS        120
#define PERIOD_LOW_MS      3000
#define PERIOD_CRIT_MS     1500

/* RGBW values. The PIM447 LED is bright; these are deliberately modest. */
#define ORANGE_R 80
#define ORANGE_G 25
#define ORANGE_B 0
#define RED_R    90
#define RED_G    0
#define RED_B    0

/* One-shot green confirmation at boot (the driver also sets green at its own
 * init); this turns it off shortly after so the LED is dark by default. */
#define BOOT_OFF_MS 600

enum batt_mode { BATT_OK, BATT_LOW, BATT_CRIT };

static const struct device *const trackball = DEVICE_DT_GET(DT_NODELABEL(trackball));
static enum batt_mode mode = BATT_OK;
static bool led_on;

static void led_off(void)
{
    pim447_set_led(trackball, 0, 0, 0, 0);
    led_on = false;
}

static void blink_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(blink_work, blink_work_handler);

static void blink_work_handler(struct k_work *work)
{
    if (mode == BATT_OK) {
        led_off();
        return;
    }

    if (led_on) {
        led_off();
        k_work_reschedule(&blink_work,
                          K_MSEC(mode == BATT_CRIT ? PERIOD_CRIT_MS : PERIOD_LOW_MS));
    } else {
        if (mode == BATT_CRIT) {
            pim447_set_led(trackball, RED_R, RED_G, RED_B, 0);
        } else {
            pim447_set_led(trackball, ORANGE_R, ORANGE_G, ORANGE_B, 0);
        }
        led_on = true;
        k_work_reschedule(&blink_work, K_MSEC(PULSE_ON_MS));
    }
}

static int battery_listener(const zmk_event_t *eh)
{
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    enum batt_mode new_mode;
    if (ev->state_of_charge <= CONFIG_SPLITKB_TRACKBALL_BATTERY_CRIT) {
        new_mode = BATT_CRIT;
    } else if (ev->state_of_charge <= CONFIG_SPLITKB_TRACKBALL_BATTERY_LOW) {
        new_mode = BATT_LOW;
    } else {
        new_mode = BATT_OK;
    }

    if (new_mode != mode) {
        LOG_INF("battery %u%%: mode %d -> %d", ev->state_of_charge, mode, new_mode);
        mode = new_mode;
        if (mode == BATT_OK) {
            k_work_cancel_delayable(&blink_work);
            led_off();
        } else {
            /* Restart the pulse cycle immediately from the on-phase. */
            led_on = false;
            k_work_reschedule(&blink_work, K_NO_WAIT);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(splitkb_trackball_battery, battery_listener);
ZMK_SUBSCRIPTION(splitkb_trackball_battery, zmk_battery_state_changed);

static void boot_off_handler(struct k_work *work)
{
    /* Clear the boot canary unless a low-battery event already took over. */
    if (mode == BATT_OK) {
        led_off();
    }
}
static K_WORK_DELAYABLE_DEFINE(boot_off_work, boot_off_handler);

static int trackball_battery_init(void)
{
    if (!device_is_ready(trackball)) {
        LOG_ERR("trackball device not ready");
        return -ENODEV;
    }

    k_work_reschedule(&boot_off_work, K_MSEC(BOOT_OFF_MS));
    return 0;
}

SYS_INIT(trackball_battery_init, APPLICATION, 90);
