zephyr_library()

zephyr_library_sources(kb_settings.c)

zephyr_linker_sources(SECTIONS iterables.ld)
