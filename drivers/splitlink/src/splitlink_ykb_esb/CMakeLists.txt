zephyr_library()

zephyr_library_sources_ifdef(CONFIG_SPLITLINK_YKB_ESB_PRX prx.c)
zephyr_library_sources_ifdef(CONFIG_SPLITLINK_YKB_ESB_PTX ptx.c)

zephyr_linker_sources(SECTIONS iterables.ld)
