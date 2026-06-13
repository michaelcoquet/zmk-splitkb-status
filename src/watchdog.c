/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Hardware watchdog auto-recovery.
 *
 * Arms the SoC watchdog and feeds it from a periodic work item on the system
 * work queue. If that queue stops running (a hang or deadlock - the
 * intermittent total lockup that otherwise needs a manual reset), the watchdog
 * resets this half after the timeout instead of leaving it dead.
 *
 * Paused while the CPU is in deep sleep, so normal idle does not trip it; it
 * only counts while the firmware is meant to be running. This is a mitigation
 * (auto-recover), not a fix for the underlying hang.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(splitkb_watchdog, CONFIG_SPLITKB_STATUS_LOG_LEVEL);

#define WDT_TIMEOUT_MS 10000
#define WDT_FEED_MS     3000

static const struct device *const wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));
static int wdt_channel;

static void feed_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(feed_work, feed_work_handler);

static void feed_work_handler(struct k_work *work)
{
    wdt_feed(wdt, wdt_channel);
    k_work_reschedule(&feed_work, K_MSEC(WDT_FEED_MS));
}

static int watchdog_init(void)
{
    if (!device_is_ready(wdt)) {
        LOG_ERR("watchdog device not ready");
        return -ENODEV;
    }

    struct wdt_timeout_cfg cfg = {
        .flags = WDT_FLAG_RESET_SOC,
        .window = { .min = 0U, .max = WDT_TIMEOUT_MS },
        .callback = NULL,
    };

    wdt_channel = wdt_install_timeout(wdt, &cfg);
    if (wdt_channel < 0) {
        LOG_ERR("wdt_install_timeout failed: %d", wdt_channel);
        return wdt_channel;
    }

    int ret = wdt_setup(wdt, WDT_OPT_PAUSE_IN_SLEEP | WDT_OPT_PAUSE_HALTED_BY_DBG);
    if (ret < 0) {
        LOG_ERR("wdt_setup failed: %d", ret);
        return ret;
    }

    /* Feed from the system work queue so a queue hang stops the feed. */
    k_work_reschedule(&feed_work, K_MSEC(WDT_FEED_MS));
    LOG_INF("watchdog armed (%dms timeout, fed every %dms)", WDT_TIMEOUT_MS, WDT_FEED_MS);
    return 0;
}

SYS_INIT(watchdog_init, APPLICATION, 99);
