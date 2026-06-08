/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Caps lock indicator on the underglow strip (central half).
 *
 * Caps lock is a HID indicator the host sends to the keyboard; only the
 * central receives it, and the central owns its local underglow, so no split
 * forwarding is needed. While caps lock is on the strip shows solid white;
 * when it turns off the configured default look is restored.
 *
 * ZMK exposes no getter for the current underglow color/effect, so the off
 * transition restores the ZMK_RGB_UNDERGLOW_*_START defaults rather than
 * whatever was running before. Only the on/off state is snapshotted.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/activity.h>
#include <zmk/rgb_underglow.h>

LOG_MODULE_REGISTER(splitkb_caps_underglow, CONFIG_SPLITKB_STATUS_LOG_LEVEL);

/* HID keyboard LED output report: bit 0 Num, bit 1 Caps, bit 2 Scroll. */
#define HID_CAPS_LOCK_BIT BIT(1)

/* rgb_underglow effect order: SOLID is index 0. */
#define UNDERGLOW_EFFECT_SOLID 0

/* Solid white for the caps indication. */
#define CAPS_HUE 0
#define CAPS_SAT 0
#define CAPS_BRT 100

static bool caps_on;
static bool ug_was_on; /* underglow on/off captured when caps engaged */

static void show_caps(void)
{
    struct zmk_led_hsb white = { .h = CAPS_HUE, .s = CAPS_SAT, .b = CAPS_BRT };

    zmk_rgb_underglow_select_effect(UNDERGLOW_EFFECT_SOLID);
    zmk_rgb_underglow_set_hsb(white);
    zmk_rgb_underglow_on();
}

static void restore_underglow(void)
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

static int indicators_listener(const zmk_event_t *eh)
{
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    bool now = (ev->indicators & HID_CAPS_LOCK_BIT) != 0;
    if (now == caps_on) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    caps_on = now;

    if (caps_on) {
        bool on = false;
        zmk_rgb_underglow_get_state(&on);
        ug_was_on = on;
        show_caps();
        LOG_INF("caps lock on");
    } else {
        restore_underglow();
        LOG_INF("caps lock off");
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(splitkb_caps_indicators, indicators_listener);
ZMK_SUBSCRIPTION(splitkb_caps_indicators, zmk_hid_indicators_changed);

/* AUTO_OFF_IDLE turns the underglow off when idle. Re-assert white on wake if
 * caps lock is still on. */
static int activity_listener(const zmk_event_t *eh)
{
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->state == ZMK_ACTIVITY_ACTIVE && caps_on) {
        show_caps();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(splitkb_caps_activity, activity_listener);
ZMK_SUBSCRIPTION(splitkb_caps_activity, zmk_activity_state_changed);
