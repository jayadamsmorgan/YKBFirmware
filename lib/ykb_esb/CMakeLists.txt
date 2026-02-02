zephyr_library()

zephyr_library_sources_ifdef(
    CONFIG_SOC_NRF5340_CPUNET
    src/ykb_esb.c
    src/53_net/esb_53.c
)

zephyr_library_sources_ifdef(
    CONFIG_SOC_NRF5340_CPUAPP
    src/53_app/esb_53.c
)

zephyr_library_sources_ifdef(
    CONFIG_SOC_SERIES_NRF52X
    src/ykb_esb.c
)
