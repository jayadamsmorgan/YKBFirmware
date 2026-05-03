if(NOT SB_CONFIG_YKB_BUILD_SPLIT_HALVES)
  return()
endif()

set(_side "")

if(DEFINED BOARD_QUALIFIERS AND NOT "${BOARD_QUALIFIERS}" STREQUAL "")
  string(REPLACE "/" ";" _qualifiers "${BOARD_QUALIFIERS}")
  list(LENGTH _qualifiers _qualifiers_len)
  if(_qualifiers_len GREATER 0)
    math(EXPR _last_idx "${_qualifiers_len} - 1")
    list(GET _qualifiers ${_last_idx} _last_qualifier)
    if(_last_qualifier STREQUAL "left" OR _last_qualifier STREQUAL "right")
      set(_side "${_last_qualifier}")
    endif()
  endif()
endif()

if(_side STREQUAL "left")
  set(_other_side "right")
  set(_other_image "app_right")
  set(_other_domain "RIGHT")
elseif(_side STREQUAL "right")
  set(_other_side "left")
  set(_other_image "app_left")
  set(_other_domain "LEFT")
else()
  return()
endif()

set(_other_board "${BOARD}/nrf5340/cpuapp/${_other_side}")

ExternalZephyrProject_Add(
  APPLICATION ${_other_image}
  SOURCE_DIR ${APP_DIR}
  BOARD ${_other_board}
  BOARD_REVISION ${BOARD_REVISION}
  BUILD_ONLY true
)
set_target_properties(${_other_image} PROPERTIES
  IMAGE_CONF_SCRIPT ${ZEPHYR_BASE}/share/sysbuild/image_configurations/MAIN_image_default.cmake
)
UpdateableImage_Add(APPLICATION ${_other_image})

get_property(PM_DOMAINS GLOBAL PROPERTY PM_DOMAINS)
if(NOT "${_other_domain}" IN_LIST PM_DOMAINS)
  list(APPEND PM_DOMAINS "${_other_domain}")
endif()

set_property(GLOBAL APPEND PROPERTY PM_${_other_domain}_IMAGES "${_other_image}")
set_property(GLOBAL PROPERTY DOMAIN_APP_${_other_domain} "${_other_image}")
set(${_other_domain}_PM_DOMAIN_DYNAMIC_PARTITION ${_other_image} CACHE INTERNAL "")
set_property(GLOBAL PROPERTY PM_DOMAINS ${PM_DOMAINS})

add_dependencies(${DEFAULT_IMAGE} ${_other_image})
