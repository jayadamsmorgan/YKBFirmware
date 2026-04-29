zephyr_library()

zephyr_library_sources(
    src/usb_connect.c
    src/usbd_init.c
)

zephyr_library_sources_ifdef(CONFIG_USB_CONNECT_KBD src/hid_devices/keyboard_hid.c)
zephyr_library_sources_ifdef(CONFIG_USB_CONNECT_MOUSE src/hid_devices/mouse_hid.c)
zephyr_library_sources_ifdef(CONFIG_USB_CONNECT_VENDOR src/hid_devices/vendor_hid.c)

zephyr_library_sources_ifdef(CONFIG_USB_CONNECT_VENDOR_YKB_PROTOCOL src/hid_devices/vendor_hid_ykb_protocol.c)

zephyr_linker_sources(SECTIONS iterables.ld)
