zephyr_library()

zephyr_library_sources(src/kscan_common.c)

zephyr_library_sources_ifdef(CONFIG_KSCAN_ENABLES src/kscan_enables.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_CHANNELS src/kscan_channels.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_MUXES src/kscan_muxes.c)

zephyr_linker_sources(SECTIONS iterables.ld)
