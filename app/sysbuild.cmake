get_property(PM_DOMAINS GLOBAL PROPERTY PM_DOMAINS)

# Include net core image if net core is present
if(SB_CONFIG_SOC_NRF5340_CPUAPP)
  # Get net core board target
  string(REPLACE "/" ";" split_board_qualifiers "${BOARD_QUALIFIERS}")
  list(GET split_board_qualifiers 1 target_soc)
  set(board_target_cpunet "${BOARD}/${target_soc}/cpunet")
  set(target_soc)

  ExternalZephyrProject_Add(
    APPLICATION nrf5340_cpunet_mpsl
    SOURCE_DIR ${APP_DIR}/../nrf5340_cpunet_mpsl
    BOARD ${board_target_cpunet}
    BOARD_REVISION ${BOARD_REVISION}
  )

  if(NOT "CPUNET" IN_LIST PM_DOMAINS)
    list(APPEND PM_DOMAINS CPUNET)
  endif()

  set_property(GLOBAL APPEND PROPERTY PM_CPUNET_IMAGES "nrf5340_cpunet_mpsl")
  set_property(GLOBAL PROPERTY DOMAIN_APP_CPUNET "nrf5340_cpunet_mpsl")
  set(CPUNET_PM_DOMAIN_DYNAMIC_PARTITION nrf5340_cpunet_mpsl CACHE INTERNAL "")
endif()

set_property(GLOBAL PROPERTY PM_DOMAINS ${PM_DOMAINS})
