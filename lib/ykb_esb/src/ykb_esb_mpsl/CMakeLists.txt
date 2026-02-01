zephyr_library()

zephyr_library_sources_ifdef(
    CONFIG_LIB_YKB_ESB_IMPL_MPSL
    ykb_esb_mpsl_common.c
    ykb_esb_mpsl.c
)

zephyr_library_sources_ifdef(
    CONFIG_LIB_YKB_ESB_IMPL_MPSL_RPC_CPUNET
    ykb_esb_mpsl_common.c
    ykb_esb_mpsl_rpc_cpunet.c
)

zephyr_library_sources_ifdef(
    CONFIG_LIB_YKB_ESB_IMPL_MPSL_RPC_CPUAPP
    ykb_esb_mpsl_rpc_cpuapp.c
)
