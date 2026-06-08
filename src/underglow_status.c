/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Status indicators on the underglow strip (central half).
 *
 * One strip serves two sources, priority-ordered:
 *
 *   critical battery (red pulse)
 *     > low battery (orange pulse)
 *     > caps lock (solid white)
 *     > nothing (restore the configured default look)
 *
 * Caps lock (a HID indicator) and the central's own battery are both local to
 * the central, so no split forwarding is needed. The strip is only borrowed
 * while there is something to show; otherwise it is handed back to whatever
 * the user had. Pulsing is gated on the active state: it stops on idle and
 * re-asserts on wake (mirrors how caps lock survives idle).
 *
 * ZMK exposes no getter for the live underglow color/effect, so the hand-back
 * restores the ZMK_RGB_UNDERGLOW_*_START defaults, not prior runtime state.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/activity.h>
#include <zmk/rgb_underglow.h>

LOG_MODULE_REGISTER(splitkb_underglow_status, CONFIG_SPLITKB_STATUS_LOG_LEVEL);

/* HID keyboard LED output report: bit 0 Num, bit 1 Caps, bit 2 Scroll. */
#define HID_CAPS_LOCK_BIT BIT(1)

/* rgb_underglow effect order: SOLID is index 0. */
#define UNDERGLOW_EFFECT_SOLID 0

/* Pulse timing, matching the trackball battery indicator. */
#define PULSE_ON_MS    120
#define PERIOD_LOW_MS  3000
#define PERIOD_CRIT_MS 1500

/* Colors (HSB: h 0-359, s/b 0-100). */
#define WHITE_H  0
#define WHITE_S  0
#define WHITE_B  100
#define ORANGE_H 30
#define ORANGE_S 100
#define ORANGE_B 100
#define RED_H    0
#define RED_S    100
#define RED_B    100

enum batt_mode { BATT_OK, BATT_LOW, BATT_CRIT };

static bool caps_on;
static enum batt_mode batt = BATT_OK;
static bool is_active = true;
static bool overriding;     /* true while we are driving the strip */
static bool ug_was_on;      /* user's on/off state captured when we took over */
static bool pulse_phase_on;

static void set_solid(uint16_t h, uint8_t s, uint8_t b)
{
    struct zmk_led_hsb c = { .h = h, .s = s, .b = b };

    zmk_rgb_underglow_select_effect(UNDERGLOW_EFFECT_SOLID);
    zmk_rgb_underglow_set_hsb(c);
    zmk_rgb_underglow_on();
}

static void restore_default(void)
{
    struct zmk_led_hsb def = {
        .h = CONFIG_ZMK_RGB_UNDERGLOW_HUE_START,
        .s = CONFIG_ZMK_RGB_UNDERGLOW_SAT_START,
        .b = CONFIG_ZMK_RGB_UNDERGLOW_BRT_START,
    };

    zmk_rgb_underglow_select_effect(CONFIG_ZMK_RGB_UNDERGLOW_EFF_START);
    zmk_rgb_underglow_set_hsb(def);

    if (ug_was_on) {
        zmk_rgb_underglow_on();
    } else {
        zmk_rgb_underglow_off();
    }
}

static void blink_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(blink_work, blink_handler);

static void blink_handler(struct k_work *work)
{
    if (!is_active || (batt != BATT_LOW && batt != BATT_CRIT)) {
        return;
    }

    if (pulse_phase_on) {
        /* Brightness 0 keeps the strip powered (no ext-power thrash) and is
         * smoother than toggling the underglow on/off each pulse. */
        set_solid(0, 0, 0);
        pulse_phase_on = false;
        k_work_reschedule(&blink_work,
                          K_MSEC(batt == BATT_CRIT ? PERIOD_CRIT_MS : PERIOD_LOW_MS));
    } else {
        if (batt == BATT_CRIT) {
            set_solid(RED_H, RED_S, RED_B);
        } else {
            set_solid(ORANGE_H, ORANGE_S, ORANGE_B);
        }
        pulse_phase_on = true;
        k_work_reschedule(&blink_work, K_MSEC(PULSE_ON_MS));
    }
}

static void take_over(void)
{
    if (!overriding) {
        bool on = false;
        zmk_rgb_underglow_get_state(&on);
        ug_was_on = on;
        overriding = true;
    }
}

static void apply_state(void)
{
    /* On idle, stop pulsing and let ZMK's auto-off own the strip. Keep the
     * override flag so wake re-asserts whatever is current. */
    if (!is_active) {
        k_work_cancel_delayable(&blink_work);
        return;
    }

    if (batt == BATT_LOW || batt == BATT_CRIT) {
        take_over();
        pulse_phase_on = false;
        k_work_reschedule(&blink_work, K_NO_WAIT);
    } else if (caps_on) {
        take_over();
        k_work_cancel_delayable(&blink_work);
        set_solid(WHITE_H, WHITE_S, WHITE_B);
    } else {
        k_work_cancel_delayable(&blink_work);
        if (overriding) {
            restore_default();
            overriding = false;
        }
    }
}

static int indicators_listener(const zmk_event_t *eh)
{
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    bool now = (ev->indicators & HID_CAPS_LOCK_BIT) != 0;
    if (now != caps_on) {
        caps_on = now;
        LOG_INF("caps lock %s", caps_on ? "on" : "off");
        apply_state();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(splitkb_underglow_indicators, indicators_listener);
ZMK_SUBSCRIPTION(splitkb_underglow_indicators, zmk_hid_indicators_changed);

static int battery_listener(const zmk_event_t *eh)
{
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    enum batt_mode new_mode;
    if (ev->state_of_charge <= CONFIG_SPLITKB_UNDERGLOW_BATTERY_CRIT) {
        new_mode = BATT_CRIT;
    } else if (ev->state_of_charge <= CONFIG_SPLITKB_UNDERGLOW_BATTERY_LOW) {
        new_mode = BATT_LOW;
    } else {
        new_mode = BATT_OK;
    }

    if (new_mode != batt) {
        LOG_INF("battery %u%%: mode %d -> %d", ev->state_of_charge, batt, new_mode);
        batt = new_mode;
        apply_state();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(splitkb_underglow_battery, battery_listener);
ZMK_SUBSCRIPTION(splitkb_underglow_battery, zmk_battery_state_changed);

static int activity_listener(const zmk_event_t *eh)
{
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    bool active = (ev->state == ZMK_ACTIVITY_ACTIVE);
    if (active != is_active) {
        is_active = active;
        apply_state();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(splitkb_underglow_activity, activity_listener);
ZMK_SUBSCRIPTION(splitkb_underglow_activity, zmk_activity_state_changed);
