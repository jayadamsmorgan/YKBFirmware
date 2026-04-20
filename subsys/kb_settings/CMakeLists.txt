zephyr_library()

zephyr_library_sources(src/kb_settings.c)

zephyr_linker_sources(SECTIONS iterables.ld)
