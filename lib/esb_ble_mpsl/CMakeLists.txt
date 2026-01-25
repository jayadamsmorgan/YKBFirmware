zephyr_library()

zephyr_library_sources_ifdef(
    CONFIG_LIB_ESB_BLE_MPSL_NRF53X_CPUNET
    src/53/53_net/hci_rpmsg_module.c
    src/53/53_net/esb_53_net.c
    src/app_esb.c
    src/timeslot_handler.c
)

zephyr_library_sources_ifdef(
    CONFIG_LIB_ESB_BLE_MPSL_NRF53X_CPUAPP
    src/app_bt_lbs.c
    src/53/53_app/esb_53_app.c
)

zephyr_library_sources_ifdef(
    CONFIG_LIB_ESB_BLE_MPSL_NRF52X
    src/app_bt_lbs.c
    src/app_esb.c
    src/timeslot_handler.c
)
