zephyr_library()

zephyr_library_sources_ifdef(
    CONFIG_LIB_YKB_ESB_IMPL_REG
    ykb_esb_reg_common.c
    ykb_esb_reg.c
)

zephyr_library_sources_ifdef(
    CONFIG_LIB_YKB_ESB_IMPL_REG_RPC_CPUNET
    ykb_esb_reg_common.c
    ykb_esb_reg_rpc_cpunet.c
)

zephyr_library_sources_ifdef(
    CONFIG_LIB_YKB_ESB_IMPL_REG_RPC_CPUAPP
    ykb_esb_reg_rpc_cpuapp.c
)
