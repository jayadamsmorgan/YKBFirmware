zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MUX_GPIO src/mux_gpio.c)
