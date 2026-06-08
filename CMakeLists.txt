# zmk-splitkb-status/CMakeLists.txt
# Each status indicator compiles only when its Kconfig option is set.

if(CONFIG_SPLITKB_TRACKBALL_BATTERY OR CONFIG_SPLITKB_CAPS_UNDERGLOW)
    zephyr_library()
endif()

if(CONFIG_SPLITKB_TRACKBALL_BATTERY)
    zephyr_library_sources(src/trackball_battery.c)
endif()

if(CONFIG_SPLITKB_CAPS_UNDERGLOW)
    zephyr_library_sources(src/caps_underglow.c)
endif()
