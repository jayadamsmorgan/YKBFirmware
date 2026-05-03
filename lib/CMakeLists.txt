add_subdirectory_ifdef(CONFIG_LIB_YKB_ESB ykb_esb)
add_subdirectory_ifdef(CONFIG_LIB_YKB_TIMESLOT ykb_timeslot)

if(CONFIG_USB_CONNECT_VENDOR_YKB_PROTOCOL OR CONFIG_BT_CONNECT_VENDOR_YKB_PROTOCOL)
    add_subdirectory(vendor_hid)
endif()
