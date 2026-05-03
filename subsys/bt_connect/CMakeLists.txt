zephyr_library()

zephyr_library_sources(src/bt_connect.c)
zephyr_library_sources_ifdef(CONFIG_BT_CONNECT_KBD src/hid_devices/keyboard_hid.c)
zephyr_library_sources_ifdef(CONFIG_BT_CONNECT_MOUSE src/hid_devices/mouse_hid.c)
zephyr_library_sources_ifdef(CONFIG_BT_CONNECT_VENDOR src/hid_devices/vendor_hid.c)

zephyr_linker_sources(SECTIONS iterables.ld)
