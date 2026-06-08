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

## Consumed via west

```yaml
- name: zmk-splitkb-status
  remote: michaelcoquet
  revision: main
```
