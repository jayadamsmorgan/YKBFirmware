if(CONFIG_BOARD_DACTYL_V1_NRF5340_CPUAPP_LEFT OR CONFIG_BOARD_DACTYL_V1_NRF5340_CPUAPP_RIGHT)
  board_runner_args(jlink "--device=nrf5340_xxaa_app" "--speed=4000")
elseif(BOARD_DACTYL_V1_NRF5340_CPUNET)
  board_runner_args(jlink "--device=nrf5340_xxaa_net" "--speed=4000")
endif()

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
