zephyr_library()

zephyr_library_sources(
    src/usb_connect.c
    src/usbd_init.c

    src/hid_devices/keyboard_hid.c
    src/hid_devices/mouse_hid.c
    src/hid_devices/vendor_hid.c
)

zephyr_linker_sources(SECTIONS iterables.ld)
