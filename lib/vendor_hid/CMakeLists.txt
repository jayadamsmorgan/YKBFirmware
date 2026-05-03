zephyr_library()

if(CONFIG_USB_CONNECT_VENDOR_YKB_PROTOCOL OR CONFIG_BT_CONNECT_VENDOR_YKB_PROTOCOL)
    zephyr_library_sources(src/vendor_hid_protocol.c)
endif()
