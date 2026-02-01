get_property(PM_DOMAINS GLOBAL PROPERTY PM_DOMAINS)

set(_ykb_app_src "${APP_DIR}")

# Determine which side the *main* image is, from BOARD_QUALIFIERS
set(_side "")
if(DEFINED BOARD_QUALIFIERS AND NOT "${BOARD_QUALIFIERS}" STREQUAL "")
  string(REPLACE "/" ";" _q_list "${BOARD_QUALIFIERS}")
  list(LENGTH _q_list _q_len)
  if(_q_len GREATER 0)
    math(EXPR _last_idx "${_q_len} - 1")
    list(GET _q_list ${_last_idx} _q_last)
    if(_q_last STREQUAL "left" OR _q_last STREQUAL "right")
      set(_side "${_q_last}")
    endif()
  endif()
endif()

# Only for split boards (i.e., when we are invoked on /left or /right)
if(_side STREQUAL "left")
  set(_need_other "right")
elseif(_side STREQUAL "right")
  set(_need_other "left")
else()
  set(_need_other "")
endif()

# Add only the *other* half as an extra image.
if(_need_other STREQUAL "left")
  set(_other_board "${BOARD}/nrf5340/cpuapp/left")

  ExternalZephyrProject_Add(
    APPLICATION ykb_dactyl_v1_left
    SOURCE_DIR  ${_ykb_app_src}
    BOARD       ${_other_board}
    BOARD_REVISION ${BOARD_REVISION}
  )

  if(NOT "LEFT" IN_LIST PM_DOMAINS)
    list(APPEND PM_DOMAINS LEFT)
  endif()
  set_property(GLOBAL APPEND PROPERTY PM_LEFT_IMAGES "ykb_dactyl_v1_left")
  set_property(GLOBAL PROPERTY DOMAIN_APP_LEFT "ykb_dactyl_v1_left")
  set(LEFT_PM_DOMAIN_DYNAMIC_PARTITION ykb_dactyl_v1_left CACHE INTERNAL "")

elseif(_need_other STREQUAL "right")
  set(_other_board "${BOARD}/nrf5340/cpuapp/right")

  ExternalZephyrProject_Add(
    APPLICATION ykb_dactyl_v1_right
    SOURCE_DIR  ${_ykb_app_src}
    BOARD       ${_other_board}
    BOARD_REVISION ${BOARD_REVISION}
  )

  if(NOT "RIGHT" IN_LIST PM_DOMAINS)
    list(APPEND PM_DOMAINS RIGHT)
  endif()
  set_property(GLOBAL APPEND PROPERTY PM_RIGHT_IMAGES "ykb_dactyl_v1_right")
  set_property(GLOBAL PROPERTY DOMAIN_APP_RIGHT "ykb_dactyl_v1_right")
  set(RIGHT_PM_DOMAIN_DYNAMIC_PARTITION ykb_dactyl_v1_right CACHE INTERNAL "")

endif()

# Net core image
if(SB_CONFIG_SOC_NRF5340_CPUAPP)
  set(board_target_cpunet "${BOARD}/nrf5340/cpunet")

  ExternalZephyrProject_Add(
    APPLICATION cpunet_mpsl
    SOURCE_DIR ${APP_DIR}/../cpunet_mpsl
    BOARD ${board_target_cpunet}
    BOARD_REVISION ${BOARD_REVISION}
  )

  if(NOT "CPUNET" IN_LIST PM_DOMAINS)
    list(APPEND PM_DOMAINS CPUNET)
  endif()

  set_property(GLOBAL APPEND PROPERTY PM_CPUNET_IMAGES "cpunet_mpsl")
  set_property(GLOBAL PROPERTY DOMAIN_APP_CPUNET "cpunet_mpsl")
  set(CPUNET_PM_DOMAIN_DYNAMIC_PARTITION cpunet_mpsl CACHE INTERNAL "")
endif()

set_property(GLOBAL PROPERTY PM_DOMAINS ${PM_DOMAINS})

