# zmk-splitkb-status

Out-of-tree ZMK module holding the ZMK-aware status indicators for the SplitKB
keyboard. Kept separate from `zmk-driver-pim447` so the trackball driver stays
hardware-only; this module is where ZMK event logic (battery, HID indicators)
lives.

Each indicator is gated behind its own Kconfig option so a half only compiles
the code it uses.

## Indicators

### `CONFIG_SPLITKB_TRACKBALL_BATTERY` (right / peripheral)
Drives the PIM447 trackball's RGBW LED from the half's own battery state:

- healthy: off
- `<= CONFIG_SPLITKB_TRACKBALL_BATTERY_LOW`: brief orange pulse every 3s
- `<= CONFIG_SPLITKB_TRACKBALL_BATTERY_CRIT`: brief red pulse every 1.5s

Uses the local battery event (no split forwarding) and the public
`pim447_set_led()` API from `zmk-driver-pim447`.

### `CONFIG_SPLITKB_UNDERGLOW_STATUS` (left / central)
Shares the underglow strip between caps lock and the central's own battery,
priority-ordered:

- critical battery (`<= CONFIG_SPLITKB_UNDERGLOW_BATTERY_CRIT`): red pulse
- low battery (`<= CONFIG_SPLITKB_UNDERGLOW_BATTERY_LOW`): orange pulse
- caps lock: solid white
- otherwise: restore the configured `ZMK_RGB_UNDERGLOW_*_START` look

Pulsing stops on idle and re-asserts on wake. Caps lock and the local battery
are both central-side, so no split forwarding is needed.

## Consumed via west

```yaml
- name: zmk-splitkb-status
  remote: michaelcoquet
  revision: main
```
