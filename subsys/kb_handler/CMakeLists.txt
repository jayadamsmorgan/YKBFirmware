zephyr_library()

zephyr_linker_sources(SECTIONS iterables.ld)

include(${APP_DIR}/../cmake/ykb_board_assets.cmake)
ykb_resolve_board_asset(KB_HANDLER_LAYOUT_FILE resources/kb_handler_layout.txt REQUIRED)

set(GENERATED_KB_HANDLER_LAYOUT_C
    ${CMAKE_CURRENT_BINARY_DIR}/generated_kb_handler_layout.c
)
set(GENERATED_KB_HANDLER_LAYOUT_H
    ${CMAKE_CURRENT_BINARY_DIR}/generated_kb_handler_layout.h
)

add_custom_command(
    OUTPUT
        ${GENERATED_KB_HANDLER_LAYOUT_C}
        ${GENERATED_KB_HANDLER_LAYOUT_H}
    COMMAND
        ${PYTHON_EXECUTABLE}
        ${APP_DIR}/../scripts/gen_kb_handler_layout.py
        --layout ${KB_HANDLER_LAYOUT_FILE}
        --out-c ${GENERATED_KB_HANDLER_LAYOUT_C}
        --out-h ${GENERATED_KB_HANDLER_LAYOUT_H}
    DEPENDS
        ${APP_DIR}/../scripts/gen_kb_handler_layout.py
        ${KB_HANDLER_LAYOUT_FILE}
    VERBATIM
)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
zephyr_library_sources(src/kb_handler_config.c src/kb_handler_core.c
                       src/kb_handler_transport.c)
zephyr_library_sources(${GENERATED_KB_HANDLER_LAYOUT_C})

zephyr_library_sources_ifdef(CONFIG_KB_HANDLER_SIMPLE src/kb_handler_simple.c)

zephyr_library_sources_ifdef(CONFIG_KB_HANDLER_SPLITLINK_MASTER src/kb_handler_splitlink_master.c)
zephyr_library_sources_ifdef(CONFIG_KB_HANDLER_SPLITLINK_SLAVE src/kb_handler_splitlink_slave.c)

zephyr_library_sources_ifdef(CONFIG_KB_HANDLER_SPLITLINK_HANDLER_IMPL_YKB_PROTO src/splitlink_handler/splitlink_ykb_protocol.c)
