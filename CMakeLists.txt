# zmk-splitkb-status/CMakeLists.txt
# Each status indicator compiles only when its Kconfig option is set.

if(CONFIG_SPLITKB_TRACKBALL_BATTERY OR CONFIG_SPLITKB_CAPS_UNDERGLOW)
    zephyr_library()
    # ZMK marks app/include PRIVATE to its app target, so the event-manager,
    # event, underglow and activity headers are not on an external module's
    # path. ZMK is a sibling west project (../zmk); add its app include dir.
    zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../zmk/app/include)
endif()

if(CONFIG_SPLITKB_TRACKBALL_BATTERY)
    zephyr_library_sources(src/trackball_battery.c)
endif()

if(CONFIG_SPLITKB_CAPS_UNDERGLOW)
    zephyr_library_sources(src/caps_underglow.c)
endif()
