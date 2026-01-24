if(CONFIG_BOARD_RAYTAC_TEST_NRF5340_CPUAPP)
  board_runner_args(jlink "--device=nrf5340_xxaa_app" "--speed=4000")
elseif(BOARD_RAYTAC_TEST_NRF5340_CPUNET)
  board_runner_args(jlink "--device=nrf5340_xxaa_net" "--speed=4000")
endif()

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
