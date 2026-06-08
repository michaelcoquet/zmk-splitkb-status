# zmk-splitkb-status/CMakeLists.txt
# Each status indicator compiles only when its Kconfig option is set.

if(CONFIG_SPLITKB_TRACKBALL_BATTERY)
    zephyr_library()
    zephyr_library_sources(src/trackball_battery.c)
endif()
