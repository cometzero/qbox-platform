local si_cl0 = {}

local SI_CL0_QEMU = {
    accel_env = "QBOX_APOLLO_FULL_SI_CL0_ACCEL";
    accel = "tcg";
    tcg_mode_env = "QBOX_APOLLO_FULL_SI_CL0_TCG_MODE";
    tcg_mode = "SINGLE";
    sync_policy_env = "QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY";
    sync_policy = "multithread-freerunning";
    time_sync_strategy_env = "QBOX_APOLLO_FULL_SI_CL0_TIME_SYNC_STRATEGY";
    time_sync_strategy = "quantum_keeper";
}
local SI_CONST = {}
SI_CONST.GIC_SPI_BASE_INTID = 32
SI_CONST.GIC_SPI_LIMIT_INTID = 992
SI_CONST.SI_PE_COUNT = 5
SI_CONST.SI_CL0_SRAM_BASE = 0x120000000
SI_CONST.SI_CL0_ENTRY = 0x120000000
SI_CONST.SI_CL0_GICD_VIEW0_BASE = 0x30000000
SI_CONST.SI_CL0_GICR_VIEW0_BASES = {
    0x30040000;
    0x30060000;
    0x30080000;
    0x300a0000;
    0x300c0000;
}
SI_CONST.SI_CL0_GICD_VIEW1_BASE = 0x30100000
SI_CONST.SI_CL0_GICR_VIEW1_BASE = 0x30140000
SI_CONST.SI_GICD_SIZE = 0x00010000
SI_CONST.SI_GICR_SIZE = 0x00020000
SI_CONST.SI_GIC_BACKEND_DIST_BASE = 0x30f00000
SI_CONST.SI_GIC_BACKEND_REDIST_BASE = 0x30f40000
SI_CONST.SI_CL0_SRAM_SIZE = 0x00800000
SI_CONST.HOST_SI_CL0_CL_UTIL_BASE = 0x4000028000000
SI_CONST.HOST_SI_CL_UTIL_SIZE = 0x00800000
SI_CONST.HOST_SI_CLUS_PPU_OFFSET = 0x00010000
SI_CONST.HOST_SI_CORE0_PPU_OFFSET = 0x00040000
SI_CONST.HOST_SI_CL0_SRAM_PHYS_BASE = 0x4000120000000
SI_CONST.HOST_SI_SRAM_WINDOW_SIZE = 0x01000000
SI_CONST.HOST_SI_CONTROL_WINDOW_SIZE = 0x00010000
SI_CONST.HOST_AP_SHARED_SRAM_PHYS_BASE = 0x00000000
SI_CONST.HOST_AP_SDS_MEM_SIZE = 0x00000DC0
SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE =
    SI_CONST.HOST_AP_SHARED_SRAM_PHYS_BASE + SI_CONST.HOST_AP_SDS_MEM_SIZE
SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_OFFSET = 0x00000100
SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE =
    SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE + SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_OFFSET
SI_CONST.HOST_AP_NS_MHU_SHMEM_BASE = 0x00180000
SI_CONST.HOST_AP_SI_MHU_FRAME_SIZE = 0x00030000
SI_CONST.RSE_MHU_FRAME_SIZE = 0x00020000
SI_CONST.HOST_RSE_SI_SSRAM_PHYS_BASE = 0x4000040000000
SI_CONST.HOST_RSE_SI_SSRAM_SIZE = 0x00040000
SI_CONST.SI_CL0_AP_NS_MHU_PBX_BASE = 0x38000000
SI_CONST.SI_CL0_AP_NS_MHU_MBX_BASE = 0x38040000
SI_CONST.SI_CL0_AP_SCMI_MHU_PBX_BASE = 0x38080000
SI_CONST.SI_CL0_AP_SCMI_MHU_MBX_BASE = 0x380C0000
SI_CONST.SI_CL0_AP_PFDI_MHU_PBX_BASE = 0x38380000
SI_CONST.SI_CL0_AP_PFDI_MHU_MBX_BASE = 0x383C0000
SI_CONST.SI_CL0_RSE_MHU_PBX_BASE = 0x38100000
SI_CONST.SI_CL0_RSE_MHU_MBX_BASE = 0x38140000
SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE = 0x40000000
SI_CONST.SI_CL0_PFDI_MHU_PBX_BASE = 0x38200000
SI_CONST.SI_CL0_PFDI_MHU_MBX_BASE = 0x38240000
SI_CONST.SI_CL0_PFDI_MHU_SIZE = 0x00020000
SI_CONST.SI_CL0_PFDI_MHU_CHANNELS = 32
SI_CONST.SI_CL0_PFDI_SHMEM_BASE = 0x48000000
SI_CONST.SI_CL0_PFDI_CHANNEL_STRIDE = 40
SI_CONST.SI_CL0_PFDI_CHANNEL_BASE = 2
SI_CONST.SI_CL0_PFDI_CHANNEL_COUNT = 4
SI_CONST.SI_CL0_ATU_LOGICAL_BASE = 0x80000000
SI_CONST.SI_CL0_ATU_LOGICAL_SIZE = 0x60340000
SI_CONST.SI_CL0_SMDEXP_ATU_LOGICAL_BASE = 0xE0340000
SI_CONST.SI_CL0_SMDEXP_ATU_LOGICAL_SIZE = 0x00002000
SI_CONST.SYSTEM_AP_SHARED_BRIDGE_BASE = 0x00000000
SI_CONST.SYSTEM_AP_SHARED_BRIDGE_SIZE = 0x00200000
SI_CONST.SYSTEM_AP_GIC_BRIDGE_BASE = 0x20000000
SI_CONST.SYSTEM_AP_GIC_BRIDGE_SIZE = 0x08000000
SI_CONST.SI_CL1_SCMI_BRIDGE_BASE = 0x48000000
SI_CONST.SI_CL1_SCMI_BRIDGE_SIZE = 0x00001000
SI_CONST.SI_CL1_HIPC_SHARED_SIZE = 0x00080000
SI_CONST.SI_CL1_CLUSTER_UTILITY_BUS_BASE = 0x28800000
SI_CONST.SI_CL1_CLUSTER_PPU_OFFSET = 0x00010000
SI_CONST.SI_CL1_CLUSTER_PPU_BASE =
    SI_CONST.SI_CL1_CLUSTER_UTILITY_BUS_BASE + SI_CONST.SI_CL1_CLUSTER_PPU_OFFSET
SI_CONST.SI_CL1_PPU_AE_OFFSET = 0x00080000
SI_CONST.SI_CL1_PPU_AE_BASE =
    SI_CONST.SI_CL1_CLUSTER_UTILITY_BUS_BASE + SI_CONST.SI_CL1_PPU_AE_OFFSET
SI_CONST.SI_CL1_CORE_PPU0_OFFSET = 0x00040000
SI_CONST.SI_CL1_CORE_PPU0_BASE =
    SI_CONST.SI_CL1_CLUSTER_UTILITY_BUS_BASE + SI_CONST.SI_CL1_CORE_PPU0_OFFSET
SI_CONST.SI_CL1_CORE_PPU_STRIDE = 0x00100000
SI_CONST.SI_CL1_CORE_PPU_COUNT = 4
SI_CONST.SI_CL_PPU_SIZE = 0x00001000
SI_CONST.SI_CL0_UART_BASE = 0x2A400000
SI_CONST.SI_CL0_UART_SIZE = 0x00010000
SI_CONST.SI_CL0_SCR_BASE = 0x2A6B0000
SI_CONST.SI_CL0_SCR_SIZE = 0x00010000
SI_CONST.SI_CL0_TIMER_CNTCTL_BASE = 0x2A6F0000
SI_CONST.SI_CL0_TIMER_CNTCTL_SIZE = 0x00010000
SI_CONST.SI_CL0_TIMER_CNT_BASE = 0x2A720000
SI_CONST.SI_CL0_TIMER_CNT_SIZE = 0x00010000
SI_CONST.SI_CL0_WDOG_CONTROL_BASE = 0x2A700000
SI_CONST.SI_CL0_WDOG_REFRESH_BASE = 0x2A710000
SI_CONST.SI_CL0_WDOG_SIZE = 0x00010000
SI_CONST.SI_CL0_WDOG_CLOCK_HZ = 125000000
SI_CONST.SI_CL0_SSU_BASE = 0x2A500000
SI_CONST.SI_CL0_SSU_SIZE = 0x00001000
SI_CONST.SI_CL0_FMU_BASE = 0x2A510000
SI_CONST.SI_CL0_FMU_SIZE = 0x00050000
SI_CONST.SI_CL0_FMU_BANK_COUNT = 5
SI_CONST.SI_CL0_FMU_RECORD_COUNT = 384
SI_CONST.SI_CL0_NI710AE_PRIMARY_NCI_BASE = 0x2A000000
SI_CONST.SI_CL0_NI710AE_SECONDARY_NCI_BASE = 0x2A200000
SI_CONST.SI_CL0_NI710AE_MHU_NCI_BASE = 0x2A300000
SI_CONST.SI_CL0_NI710AE_NCI_SIZE = 0x00010000
SI_CONST.SI_CL0_ATW0_CMN_BASE = 0x80000000
SI_CONST.SI_CL0_ATW0_CMN_SIZE = 0x40000000
SI_CONST.SI_CL0_ATW1_CLUSTER_UTILITY_BASE = 0xC0000000
SI_CONST.SI_CL0_CLUSTER_UTILITY_STRIDE = 0x04000000
SI_CONST.SI_CL0_AP_CLUSTER_COUNT = 4
SI_CONST.SI_CL0_AP_CORE_PER_CLUSTER_COUNT = 4
SI_CONST.SI_CL0_AP_CLUSTER_PPU_OFFSET = 0x01030000
SI_CONST.SI_CL0_AP_CLUSTER_AE_OFFSET = 0x01050000
SI_CONST.SI_CL0_AP_CORE_RAS_OFFSET = 0x010A0000
SI_CONST.SI_CL0_AP_CORE_RAS_SIZE = 0x00000040
SI_CONST.SI_CL0_AP_CORE_PPU0_OFFSET = 0x01080000
SI_CONST.SI_CL0_AP_CORE_PPU_STRIDE = 0x00100000
SI_CONST.SI_CL0_AP_CLUSTER_CONTROL_OFFSET = 0x02000000
SI_CONST.SI_CL0_AP_CLUSTER_CONTROL_SIZE = 0x00010000
SI_CONST.SI_CL0_ATW2_SMD_EXPANSION_BASE = 0xD0000000
SI_CONST.SI_CL0_ATW2_SMD_EXPANSION_SIZE = 0x00020000
SI_CONST.SI_CL0_PLL_BASE = SI_CONST.SI_CL0_ATW2_SMD_EXPANSION_BASE
SI_CONST.SI_CL0_PLL_SIZE = 0x00001000
SI_CONST.SI_CL0_ATW3_SYSTOP_PIK_BASE = 0xD0020000
SI_CONST.SI_CL0_ATW3_SYSTOP_PIK_SIZE = 0x00010000
SI_CONST.SI_CL0_SYS0_PPU_BASE = 0xD0021000
SI_CONST.SI_CL0_SYS0_PPU_SIZE = 0x00001000
SI_CONST.SI_CL0_ATW4_SYSTEM_ID_BASE = 0xD0030000
SI_CONST.SI_CL0_ATW4_SYSTEM_ID_SIZE = 0x00010000
SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE = 0xD0040000
SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_SIZE = 0x00030000
SI_CONST.SI_CL0_REFCLK_CNTCONTROL_BASE = SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE
SI_CONST.SI_CL0_REFCLK_CNTCONTROL_SIZE = 0x00010000
SI_CONST.SI_CL0_REFCLK_CNTREAD_OFFSET = 0x00010000
SI_CONST.SI_CL0_REFCLK_CNTREAD_BASE =
    SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE + SI_CONST.SI_CL0_REFCLK_CNTREAD_OFFSET
SI_CONST.SI_CL0_REFCLK_CNTSYNC_OFFSET = 0x00020000
SI_CONST.SI_CL0_REFCLK_CNTSYNC_BASE =
    SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE + SI_CONST.SI_CL0_REFCLK_CNTSYNC_OFFSET
SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_PROBE_BASE =
    SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE +
    SI_CONST.SI_CL0_ATW5_CSS_COUNTERS_TIMERS_SIZE
SI_CONST.SI_CL0_ATW6_AP_GIC_BASE = 0xD0770000
SI_CONST.SI_CL0_ATW6_AP_GICD_MULTIVIEW_SIZE = 0x00010000
SI_CONST.SI_CL0_ATW6_AP_GICR_OFFSET = 0x00080000
SI_CONST.SI_CL0_ATW6_AP_GICR_BASE =
    SI_CONST.SI_CL0_ATW6_AP_GIC_BASE + SI_CONST.SI_CL0_ATW6_AP_GICR_OFFSET
SI_CONST.SI_CL0_ATW6_AP_GICR_STRIDE = 0x00040000
SI_CONST.SI_CL0_ATW6_AP_GICR_SIZE = 0x00020000
SI_CONST.SI_CL0_ATW6_AP_GICR_COUNT = 16
SI_CONST.SI_CL0_CLUSTER_UTILITY_MGI0_BASE = 0xC0200000
SI_CONST.SI_CL0_CLUSTER_UTILITY_MGI_STRIDE = 0x04000000
SI_CONST.SI_CL0_CLUSTER_UTILITY_MGI_SIZE = 0x00010000
SI_CONST.SI_CL0_ATW16_SMCF_SMD_MGI_BASE = 0xE0230000
SI_CONST.SI_CL0_ATW16_SMCF_SMD_MGI_SIZE = 0x00010000
SI_CONST.SI_CL0_ATW6_AP_PERIPHERAL_SRAM_BASE = 0xE0030000
SI_CONST.SI_CL0_ATW6_AP_PERIPHERAL_SRAM_SIZE = 0x00100000
SI_CONST.SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_BASE = 0xE0130000
SI_CONST.SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_SIZE = 0x00100000
SI_CONST.SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_TAIL_BASE =
    SI_CONST.SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_BASE + SI_CONST.SI_CL1_HIPC_SHARED_SIZE
SI_CONST.SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_TAIL_SIZE =
    SI_CONST.SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_SIZE - SI_CONST.SI_CL1_HIPC_SHARED_SIZE
SI_CONST.SI_CL0_ATW17_SMD_SRAM_BASE = 0xE0240000
SI_CONST.SI_CL0_ATW17_SMD_SRAM_SIZE = 0x00100000
SI_CONST.SI_CL0_ATW18_SMCF_SMDEXP_SRAM_BASE = 0xE0340000
SI_CONST.SI_CL0_ATW18_SMCF_SMDEXP_SRAM_SIZE = 0x00002000
SI_CONST.SI_CL0_CMN_PHYS_BASE = 0x100000000
SI_CONST.SI_CL0_SMD_EXPANSION_PHYS_BASE = 0x20000D8000000
SI_CONST.SI_CL0_SMCF_SMD_MGI_PHYS_BASE = 0x20000D8100000
SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_BASE = 0x140000000
SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_SIZE = 0x10000000
SI_CONST.HOST_NI710AE_SYS_CTRL_PHYS_BASE = 0x20000D2400000
SI_CONST.HOST_NI710AE_SMD_PHYS_BASE = 0x20000D2600000
SI_CONST.HOST_NI710AE_WINDOW_SIZE = 0x00100000
SI_CONST.SI_CL0_AP_CLUSTER_MGI_PHYS_BASE = 0x140200000
SI_CONST.SI_CL0_SYSTEM_ID_PHYS_BASE = 0x20000D0400000
SI_CONST.SI_CL0_SYS0_PPU_PHYS_BASE = 0x20000D0201000
SI_CONST.SI_GIC_BACKEND_REDIST_COUNT = 0
SI_CONST.SI_GIC_VIEW1_REDIST_FIRST = 0
SI_CONST.SI_GIC_VIEW2_REDIST_FIRST = 1
SI_CONST.SI_GIC_SPI_COUNT = 960
SI_CONST.SI_GIC_DIST_CFGID_OFFSET = 0x0000F000
SI_CONST.SI_GIC_DIST_CFGID_SIZE = 0x00000008
SI_CONST.SI_GIC_DIST_IVIEWR_OFFSET = 0x0000F600
SI_CONST.SI_GIC_DIST_IVIEWR_SIZE = 0x00000400
SI_CONST.SI_GIC_REDIST_PWRR_OFFSET = 0x00000024
SI_CONST.SI_GIC_REDIST_VIEWR_OFFSET = 0x0000002C
SI_CONST.SI_GIC_REDIST_FLUSHR_OFFSET = 0x00000030
SI_CONST.SI_GIC_REDIST_REGISTER_SIZE = 0x00000004
SI_CONST.SI_CL0_UART_POLL_INTERVAL_MS = 100
SI_CONST.SI_IRQ = {
    sgi_directed = 5;
    sgi_broadcast = 6;
    hypervisor_timer = 19;
    physical_timer = 20;
    virtual_timer = 27;
    secure_timer = 29;
    cl0_system_timer = 34;
    cl0_watchdog_ws0 = 37;
    cl1_uart = 39;
    cl0_uart = 40;
    cl1_hipc_pbx = 72;
    cl1_hipc_mbx = 73;
    cl1_pfdi = 82;
    cl0_ap_ns_mhu_pbx = 96;
    cl0_ap_ns_mhu_mbx = 97;
    cl0_ap_scmi_mhu_pbx = 98;
    cl0_ap_scmi_mhu_mbx = 99;
    cl0_ap_pfdi_mhu_pbx = 102;
    cl0_ap_pfdi_mhu_mbx = 103;
    cl0_rse_mhu = 105;
    cl0_cl1_mhu = 107;
    cl0_fmu_critical = 128;
    cl0_fmu_noncritical = 129;
    cl0_ap_ras_cluster0 = 325;
    cl0_ap_ras_cluster1 = 327;
    cl0_ap_ras_cluster2 = 329;
    cl0_ap_ras_cluster3 = 331;
}
local VALID_TRIGGER = {edge = true; level = true}
local VALID_POLARITY = {
    high = true; low = true; positive = true; negative = true; none = true
}
local VALID_TARGET_SEMANTICS = {
    shared = true; per_cpu = true; directed = true; broadcast = true
}

local function irq_route_definition(
    name, source, kind, intid, owner_view, target_semantics, target_pes)
    local private_interrupt = kind == "SGI" or kind == "PPI"
    return {
        name = name;
        source = source;
        controller = owner_view == "View1" and
            "si_cl0_gic" or "si_cl1_gic";
        kind = kind;
        architectural_intid = intid;
        socket_class = kind == "SGI" and "sgi" or
            (kind == "PPI" and "ppi" or "normal_spi");
        socket_index = private_interrupt and intid or
            intid - SI_CONST.GIC_SPI_BASE_INTID;
        trigger = kind == "SGI" and "edge" or "level";
        polarity = kind == "SGI" and "none" or
            (kind == "PPI" and "low" or "high");
        owner_view = owner_view;
        target_semantics = target_semantics;
        target_pes = target_pes;
    }
end

local SI_ACTIVE_ROUTES = {
    irq_route_definition(
        "si_sgi_directed", "si_cpu.icc_sgi1r_el1", "SGI", SI_CONST.SI_IRQ.sgi_directed,
        "View1", "directed", {4});
    irq_route_definition(
        "si_sgi_broadcast", "si_cpu.icc_sgi1r_el1_irm", "SGI", SI_CONST.SI_IRQ.sgi_broadcast,
        "View1", "broadcast", {1, 2, 3, 4});
    irq_route_definition(
        "si_physical_timer", "si_cpu.generic_timer_phys", "PPI", SI_CONST.SI_IRQ.physical_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_hypervisor_timer", "si_cpu.generic_timer_hyp", "PPI", SI_CONST.SI_IRQ.hypervisor_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_virtual_timer", "si_cpu.generic_timer_virt", "PPI", SI_CONST.SI_IRQ.virtual_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_secure_timer", "si_cpu.generic_timer_sec", "PPI", SI_CONST.SI_IRQ.secure_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_cl0_system_timer", "si_cl0_timer_cntbase.irq", "SPI", SI_CONST.SI_IRQ.cl0_system_timer,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_watchdog_ws0", "si_cl0_watchdog.ws0", "SPI", SI_CONST.SI_IRQ.cl0_watchdog_ws0,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl1_uart", "si_cl1_uart.irq", "SPI", SI_CONST.SI_IRQ.cl1_uart,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl0_uart", "si_cl0_uart.irq", "SPI", SI_CONST.SI_IRQ.cl0_uart,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl1_hipc_pbx", "si_cl1_hipc_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl1_hipc_pbx,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl1_hipc_mbx", "si_cl1_hipc_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl1_hipc_mbx,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl1_pfdi", "si_cl1_pfdi_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl1_pfdi,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl0_ap_ns_mhu_pbx", "si_cl0_ap_ns_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_ns_mhu_pbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ns_mhu_mbx", "si_cl0_ap_ns_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_ns_mhu_mbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_scmi_mhu_pbx", "si_cl0_ap_scmi_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_scmi_mhu_pbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_scmi_mhu_mbx", "si_cl0_ap_scmi_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_scmi_mhu_mbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_pfdi_mhu_pbx", "si_cl0_ap_pfdi_monitor_mhu_pbx.irq",
        "SPI", SI_CONST.SI_IRQ.cl0_ap_pfdi_mhu_pbx, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_pfdi_mhu_mbx", "si_cl0_ap_pfdi_monitor_mhu_mbx.irq",
        "SPI", SI_CONST.SI_IRQ.cl0_ap_pfdi_mhu_mbx, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_rse_mhu", "si_cl0_rse_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_rse_mhu,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_cl1_mhu", "si_cl0_cl1_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_cl1_mhu,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_fmu_critical", "si_cl0_fmu.critical_irq", "SPI", SI_CONST.SI_IRQ.cl0_fmu_critical,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_fmu_noncritical", "si_cl0_fmu.non_critical_irq", "SPI", SI_CONST.SI_IRQ.cl0_fmu_noncritical,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster0", "si_cl0_ap_ras_cluster0.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster0, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster1", "si_cl0_ap_ras_cluster1.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster1, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster2", "si_cl0_ap_ras_cluster2.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster2, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster3", "si_cl0_ap_ras_cluster3.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster3, "View1", "shared", {0});
}

function si_cl0.validate_irq_routes(routes)
    assert(type(routes) == "table", "SI IRQ routes must be a table")
    local names = {}
    local bindings = {}
    local owners = {}
    for _, route in ipairs(routes) do
        assert(
            type(route.name) == "string" and route.name ~= "",
            "SI IRQ route requires a name")
        assert(names[route.name] == nil, "duplicate SI IRQ route: "..route.name)
        names[route.name] = true
        assert(
            type(route.source) == "string" and route.source ~= "",
            "invalid SI IRQ source: "..route.name)
        assert(
            type(route.controller) == "string" and
                route.controller ~= "",
            "invalid SI IRQ controller: "..route.name)
        assert(
            route.owner_view == "View1" or route.owner_view == "View2",
            "invalid SI IRQ owner view: "..route.name)
        assert(
            VALID_TRIGGER[route.trigger] == true,
            "invalid SI IRQ trigger: "..route.name)
        assert(
            VALID_POLARITY[route.polarity] == true,
            "invalid SI IRQ polarity: "..route.name)
        assert(
            VALID_TARGET_SEMANTICS[route.target_semantics] == true,
            "invalid SI IRQ target semantics: "..route.name)
        assert(
            type(route.target_pes) == "table" and #route.target_pes > 0,
            "invalid SI IRQ targets: "..route.name)
        local targets = {}
        for _, pe in ipairs(route.target_pes) do
            assert(
                type(pe) == "number" and pe >= 0 and pe < SI_CONST.SI_PE_COUNT and
                    pe % 1 == 0 and targets[pe] == nil,
                "invalid SI IRQ target PE: "..route.name)
            targets[pe] = true
        end
        if route.kind == "SPI" then
            assert(
                route.socket_class == "normal_spi" and
                    type(route.architectural_intid) == "number" and
                    route.architectural_intid >= SI_CONST.GIC_SPI_BASE_INTID and
                    route.architectural_intid < SI_CONST.GIC_SPI_LIMIT_INTID,
                "invalid normal SPI route: "..route.name)
            assert(
                route.socket_index ==
                    route.architectural_intid - SI_CONST.GIC_SPI_BASE_INTID,
                "normal SPI socket must equal architectural INTID - 32: "..
                    route.name)
        elseif route.kind == "PPI" then
            assert(
                route.socket_class == "ppi" and
                    route.architectural_intid >= 16 and
                    route.architectural_intid < SI_CONST.GIC_SPI_BASE_INTID and
                    route.socket_index == route.architectural_intid and
                    route.target_semantics == "per_cpu",
                "invalid PPI route: "..route.name)
        elseif route.kind == "SGI" then
            assert(
                route.socket_class == "sgi" and
                    route.architectural_intid >= 0 and
                    route.architectural_intid < 16 and
                    route.socket_index == route.architectural_intid and
                    (route.target_semantics == "directed" or
                        route.target_semantics == "broadcast"),
                "invalid SGI route: "..route.name)
        else
            error(
                "ESPI/EPPI and unknown classes cannot use normal SI routes: "..
                    route.name)
        end
        local binding = route.source.."|"..route.controller.."|"..
            route.socket_index
        assert(bindings[binding] == nil, "duplicate SI IRQ binding: "..route.name)
        bindings[binding] = true
        local owner = route.controller.."|"..route.owner_view.."|"..
            route.architectural_intid
        assert(owners[owner] == nil, "duplicate SI IRQ owner: "..route.name)
        owners[owner] = true
    end
    return names
end

function si_cl0.irq_route(routes, name)
    si_cl0.validate_irq_routes(routes)
    for _, route in ipairs(routes) do
        if route.name == name then
            return route
        end
    end
    error("missing active SI IRQ route: "..name)
end

function si_cl0.active_irq_routes()
    return SI_ACTIVE_ROUTES
end

local function active_route(name)
    return si_cl0.irq_route(SI_ACTIVE_ROUTES, name)
end

function si_cl0.ppi_index(ctx, name)
    local route = active_route(name)
    assert(
        route.kind == "PPI" and route.socket_class == "ppi",
        "active SI route is not a PPI: "..name)
    return route.socket_index
end

function si_cl0.spi_target(ctx, name, expected_view)
    local route = active_route(name)
    assert(
        route.kind == "SPI" and route.socket_class == "normal_spi",
        "active SI route is not a normal SPI: "..name)
    assert(
        route.owner_view == expected_view,
        "active SI route owner mismatch: "..name)
    return "&"..route.controller..".spi_in_"..route.socket_index
end

function si_cl0.define_qemu_instance(ctx, platform)
    platform.si_cl0_qemu_inst_mgr = {
        moduletype = "QemuInstanceManager";
    }
    platform.si_cl0_qemu_inst = {
        moduletype = "QemuInstance";
        args = {"&platform.si_cl0_qemu_inst_mgr", "AARCH64"};
        accel = ctx.getenv_or(SI_CL0_QEMU.accel_env, SI_CL0_QEMU.accel);
        tcg_mode = ctx.getenv_or(
            SI_CL0_QEMU.tcg_mode_env,
            SI_CL0_QEMU.tcg_mode);
        sync_policy = ctx.getenv_or(
            SI_CL0_QEMU.sync_policy_env,
            SI_CL0_QEMU.sync_policy);
        time_sync_strategy = ctx.getenv_or(
            SI_CL0_QEMU.time_sync_strategy_env,
            SI_CL0_QEMU.time_sync_strategy);
        managed_start_in_reset_release = true;
        qemu_args = ctx.getenv_or("QBOX_APOLLO_FULL_SI_CL0_QEMU_ARGS", "");
    }
    return "&platform.si_cl0_qemu_inst"
end

function si_cl0.define_interrupt_controller(
    ctx,
    platform,
    qemu_instance)
    local gic = {
        moduletype = "arm_gicv3";
        args = {qemu_instance};
        dist_iface = {
            address = SI_CONST.SI_CL0_GICD_VIEW1_BASE;
            size = SI_CONST.SI_GICD_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 10;
            aliases = {
                view0_functional = {
                    address = SI_CONST.SI_CL0_GICD_VIEW0_BASE;
                    size = SI_CONST.SI_GICD_SIZE;
                };
            };
        };
        redist_iface_0 = {
            address = SI_CONST.SI_CL0_GICR_VIEW1_BASE;
            size = SI_CONST.SI_GICR_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 10;
            aliases = {
                view0_functional = {
                    address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1];
                    size = SI_CONST.SI_GICR_SIZE;
                };
            };
        };
        redist_region = {1};
        num_cpus = 1;
        num_spi = 384;
    }
    platform.si_cl0_gic = gic
end

function si_cl0.define_loader_and_cpu(ctx, platform, qemu_instance, image)
    platform.si_cl0_loader = {
        moduletype = "loader";
        request_origin_id = ctx.request_context.origin.si_cl0_loader;
        request_domain_id = ctx.request_context.domain.si_cl0;
        request_capabilities = ctx.request_context.capability.authenticated_image;
        request_secure = true;
        request_secure_valid = true;
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        { bin_file = image, address = SI_CONST.SI_CL0_SRAM_BASE };
    }

    platform.si_cl0_cpu_0 = {
        moduletype = "cpu_arm_cortexR82";
        args = {qemu_instance};
        mem = {bind = "&si_cl0_ni710ae_primary_nci.protected_target_socket"};
        has_el2 = true;
        psci_conduit = "smc";
        start_powered_off = false;
        start_in_reset = true;
        reset_power_on = true;
        rvbar = SI_CONST.SI_CL0_ENTRY;
        cntfrq_hz = 125000000;
        mp_affinity = 0x0;
        request_origin_id = ctx.request_context.origin.si_cl0_cpu_base;
        request_domain_id = ctx.request_context.domain.si_cl0;
        requester_id = 0;
        request_secure = true;
        request_secure_valid = true;
        trace_pc = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_CL0_PC_TRACE", false);
        trace_exception_state = ctx.getenv_bool_or(
            "QBOX_APOLLO_FULL_SI_CL0_EXCEPTION_TRACE",
            false);
        trace_pc_file = ctx.getenv_or(
            "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_FILE",
            ctx.apollo_root.."build/qbox-apollo-fvp/full-system/si-cl0-pc-trace.log");
        trace_pc_interval = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_INTERVAL",
            "1");
        trace_pc_limit = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_LIMIT",
            "4096");
        irq_timer_sec_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..
                si_cl0.ppi_index(ctx, "si_secure_timer");
        };
        irq_timer_phys_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..
                si_cl0.ppi_index(ctx, "si_physical_timer");
        };
        irq_timer_virt_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..
                si_cl0.ppi_index(ctx, "si_virtual_timer");
        };
        irq_timer_hyp_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..
                si_cl0.ppi_index(ctx, "si_hypervisor_timer");
        };
    }

    platform.si_cl0_cpu_counter_mirror = {
        moduletype = "qemu_arm_counter_mirror";
        args = {
            "&platform.si_cl0_cpu_0";
            "&platform.css_system_counter";
        };
    }

    platform.si_cl0_gic.irq_out_0 = {bind = "&si_cl0_cpu_0.irq_in"}
    platform.si_cl0_gic.fiq_out_0 = {bind = "&si_cl0_cpu_0.fiq_in"}
    platform.si_cl0_gic.virq_out_0 = {bind = "&si_cl0_cpu_0.virq_in"}
    platform.si_cl0_gic.vfiq_out_0 = {bind = "&si_cl0_cpu_0.vfiq_in"}
end

function si_cl0.define(ctx, platform)
    platform.host_si_cl0_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_si_sram_dmi;
        target_socket = {
            address = SI_CONST.HOST_SI_CL0_SRAM_PHYS_BASE;
            size = SI_CONST.HOST_SI_SRAM_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        map_file = host_si_cl0_sram_map_file;
        shared_memory = host_sram_shared_memory_enabled(host_si_cl0_sram_map_file);
        shared_memory_prefix = "ra-si0-";
        init_mem = host_si_cl0_sram_map_file == "";
        log_level = 0;
    }

    platform.host_si_cl0_cub = {
        moduletype = "gs_memory";
        target_socket = {
            address = SI_CONST.HOST_SI_CL0_CL_UTIL_BASE;
            size = SI_CONST.HOST_SI_CL_UTIL_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 20;
        };
        init_mem = true;
        log_level = 0;
    }

    platform.host_si_cl0_clus_ppu = {
        moduletype = "host_ppu";
        trace = host_ppu_trace;
        trace_limit = host_ppu_trace_limit;
        target_socket = {
            address = SI_CONST.HOST_SI_CL0_CL_UTIL_BASE + SI_CONST.HOST_SI_CLUS_PPU_OFFSET;
            size = SI_CONST.HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 10;
        };
        log_level = 0;
    }

    platform.host_si_cl0_core0_ppu = {
        moduletype = "host_ppu";
        trace = host_ppu_trace;
        trace_limit = host_ppu_trace_limit;
        assert_power_on_reset = true;
        assert_power_on_load = true;
        power_on_load = {bind = "&si_cl0_loader.reset"};
        power_on_reset = {bind = "&si_cl0_cpu_0.reset"};
        power_on_load_pulse_width_ns = 0;
        power_on_load_to_reset_delay_ns = 0;
        power_on_status_delay_ns = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PPU_ON_DELAY_NS", "0");
        access_latency_ns = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PPU_ACCESS_LATENCY_NS", "100");
        target_socket = {
            address = SI_CONST.HOST_SI_CL0_CL_UTIL_BASE + SI_CONST.HOST_SI_CORE0_PPU_OFFSET;
            size = SI_CONST.HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 10;
        };
        log_level = 0;
    }

end

function si_cl0.enable(ctx, platform)
    print("Apollo FVP live SI CL0 block enabled...")

    platform.si_cl0_router = {
        moduletype = "router";
        log_level = 0;
    }

    platform.si_cl0_ap_ns_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "si_cl0_to_ap_ns";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.HOST_AP_NS_MHU_SHMEM_BASE;
        rx_shmem = SI_CONST.HOST_AP_NS_MHU_SHMEM_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_AP_NS_MHU_PBX_BASE;
            size = SI_CONST.HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_ap_ns_mhu_pbx", "View1")};
        log_level = 0;
    }

    platform.si_cl0_ap_ns_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "ap_to_si_cl0_ns";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.HOST_AP_NS_MHU_SHMEM_BASE;
        rx_shmem = SI_CONST.HOST_AP_NS_MHU_SHMEM_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_AP_NS_MHU_MBX_BASE;
            size = SI_CONST.HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_ap_ns_mhu_mbx", "View1")};
        log_level = 0;
    }

    platform.si_cl0_ap_scmi_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "si_cl0_to_ap_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_AP_SCMI_MHU_PBX_BASE;
            size = SI_CONST.HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_ap_scmi_mhu_pbx", "View1")};
        log_level = 0;
    }

    platform.si_cl0_ap_scmi_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "ap_to_si_cl0_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_AP_SCMI_MHU_MBX_BASE;
            size = SI_CONST.HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_ap_scmi_mhu_mbx", "View1")};
        log_level = 0;
    }

    platform.si_cl0_ap_pfdi_monitor_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "si_cl0_to_ap_pfdi";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_AP_PFDI_MHU_PBX_BASE;
            size = SI_CONST.HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_ap_pfdi_mhu_pbx", "View1")};
        log_level = 0;
    }

    platform.si_cl0_ap_pfdi_monitor_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "ap_to_si_cl0_pfdi";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_AP_PFDI_MHU_MBX_BASE;
            size = SI_CONST.HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_ap_pfdi_mhu_mbx", "View1")};
        log_level = 0;
    }

    platform.si_cl0_rse_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "si_cl0_to_rse";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
        rx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_RSE_MHU_PBX_BASE;
            size = SI_CONST.RSE_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        log_level = 0;
    }

    platform.si_cl0_rse_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "rse_to_si_cl0";
        protocol = "doorbell-bridge";
        tx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
        rx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_RSE_MHU_MBX_BASE;
            size = SI_CONST.RSE_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_rse_mhu", "View1")};
        log_level = 0;
    }

    platform.si_cl0_pfdi_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "apollo_si_cl0_pfdi_reply";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        channel_count = SI_CONST.SI_CL0_PFDI_MHU_CHANNELS;
        tx_shmem = SI_CONST.SI_CL0_PFDI_SHMEM_BASE;
        rx_shmem = SI_CONST.SI_CL0_PFDI_SHMEM_BASE;
        scmi_channel_stride = SI_CONST.SI_CL0_PFDI_CHANNEL_STRIDE;
        scmi_channel_base_index = SI_CONST.SI_CL0_PFDI_CHANNEL_BASE;
        scmi_channel_count = SI_CONST.SI_CL0_PFDI_CHANNEL_COUNT;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_PFDI_MHU_PBX_BASE;
            size = SI_CONST.SI_CL0_PFDI_MHU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        log_level = 0;
    }

    platform.si_cl0_pfdi_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "apollo_si_cl1_pfdi";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        channel_count = SI_CONST.SI_CL0_PFDI_MHU_CHANNELS;
        tx_shmem = SI_CONST.SI_CL0_PFDI_SHMEM_BASE;
        rx_shmem = SI_CONST.SI_CL0_PFDI_SHMEM_BASE;
        scmi_channel_stride = SI_CONST.SI_CL0_PFDI_CHANNEL_STRIDE;
        scmi_channel_base_index = SI_CONST.SI_CL0_PFDI_CHANNEL_BASE;
        scmi_channel_count = SI_CONST.SI_CL0_PFDI_CHANNEL_COUNT;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = SI_CONST.SI_CL0_PFDI_MHU_MBX_BASE;
            size = SI_CONST.SI_CL0_PFDI_MHU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_cl1_mhu", "View1")};
        log_level = 0;
    }

    platform.host_si_atu.translation_socket = {
        address = SI_CONST.SI_CL0_ATU_LOGICAL_BASE;
        size = SI_CONST.SI_CL0_ATU_LOGICAL_SIZE;
        bind = "&si_cl0_router.initiator_socket";
        relative_addresses = false;
    }
    platform.host_si_atu.initiator_socket = {
        bind = "&system_router.target_socket";
    }
    platform.host_si_atu.enable_dmi = false

    platform.host_smdexp2smd_atu.translation_socket = {
        address = SI_CONST.SI_CL0_SMDEXP_ATU_LOGICAL_BASE;
        size = SI_CONST.SI_CL0_SMDEXP_ATU_LOGICAL_SIZE;
        bind = "&si_cl0_router.initiator_socket";
        relative_addresses = false;
    }
    platform.host_smdexp2smd_atu.initiator_socket = {
        bind = "&system_router.target_socket";
    }
    platform.host_smdexp2smd_atu.enable_dmi = false

    platform.system_to_ap_shared_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = SI_CONST.SYSTEM_AP_SHARED_BRIDGE_BASE;
        target_socket = {
            address = SI_CONST.SYSTEM_AP_SHARED_BRIDGE_BASE;
            size = SI_CONST.SYSTEM_AP_SHARED_BRIDGE_SIZE;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    platform.system_to_ap_gic_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = SI_CONST.SYSTEM_AP_GIC_BRIDGE_BASE;
        target_socket = {
            address = SI_CONST.SYSTEM_AP_GIC_BRIDGE_BASE;
            size = SI_CONST.SYSTEM_AP_GIC_BRIDGE_SIZE;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    platform.si_cl0_rse_shared_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = SI_CONST.HOST_RSE_SI_SSRAM_PHYS_BASE;
        target_socket = {
            address = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
            size = SI_CONST.HOST_RSE_SI_SSRAM_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.si_cl0_to_si_cl1_scmi_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = SI_CONST.SI_CL1_SCMI_BRIDGE_BASE;
        target_socket = {
            address = SI_CONST.SI_CL1_SCMI_BRIDGE_BASE;
            size = SI_CONST.SI_CL1_SCMI_BRIDGE_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&si_cl1_router.target_socket"};
        log_level = 0;
    }

    local si_cl0_image = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_IMAGE",
        ctx.apollo_root.."build/local-apollo-fvp/deploy/firmware/si0_ramfw.bin")
    local si_cl0_log = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_LOG",
        ctx.getenv_or(
            "QBOX_RDASPEN_SCP_LOG",
            ctx.apollo_root.."build/qbox-apollo-fvp/full-system/qbox-safety-island-cl0.log"))
    local si_cl0_uart_read_file = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_UART_READ_FILE",
        "/dev/null")
    local si_cl0_uart_poll_read = si_cl0_uart_read_file ~= "/dev/null"
    local si_gic_trace = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_GIC_MULTIVIEW_TRACE", false)
    local si_gic_trace_limit =
        ctx.getenv_number_or("QBOX_APOLLO_FULL_SI_GIC_MULTIVIEW_TRACE_LIMIT", "256")
    local si_cmn_trace = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_CMN_TRACE", false)
    local si_cmn_trace_limit =
        ctx.getenv_number_or("QBOX_APOLLO_FULL_SI_CMN_TRACE_LIMIT", "512")

    -- CL0 interconnect and GIC multiviews
    platform.si_cl0_cmn_cyprus = {
        moduletype = "host_cmn_cyprus";
        revision = 3;
        trace = si_cmn_trace;
        trace_limit = si_cmn_trace_limit;
        target_socket = {
            address = SI_CONST.SI_CL0_CMN_PHYS_BASE;
            size = SI_CONST.SI_CL0_ATW0_CMN_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_gic_multiview = {
        moduletype = "gicx00_multiview";
        trace = si_gic_trace;
        trace_limit = si_gic_trace_limit;
        backend_dist_base = SI_CONST.SI_GIC_BACKEND_DIST_BASE;
        backend_redist_base = SI_CONST.SI_GIC_BACKEND_REDIST_BASE;
        backend_redist_stride = SI_CONST.SI_GICR_SIZE;
        backend_redist_count = SI_CONST.SI_GIC_BACKEND_REDIST_COUNT;
        view_redist_stride = SI_CONST.SI_GICR_SIZE;
        view1_redist_first = SI_CONST.SI_GIC_VIEW1_REDIST_FIRST;
        view2_redist_first = SI_CONST.SI_GIC_VIEW2_REDIST_FIRST;
        spi_count = SI_CONST.SI_GIC_SPI_COUNT;
        view0_dist_cfgid = {
            address = SI_CONST.SI_CL0_GICD_VIEW0_BASE + SI_CONST.SI_GIC_DIST_CFGID_OFFSET;
            size = SI_CONST.SI_GIC_DIST_CFGID_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        view0_dist_iviewr = {
            address = SI_CONST.SI_CL0_GICD_VIEW0_BASE + SI_CONST.SI_GIC_DIST_IVIEWR_OFFSET;
            size = SI_CONST.SI_GIC_DIST_IVIEWR_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
    }

    for i=1,4 do
        platform.si_gic_multiview["view0_redist_"..i] = {
            address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[i + 1];
            size = SI_CONST.SI_GICR_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 20;
        }
    end

    platform.si_gic_multiview.view0_redist_0_pwrr = {
        address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1] + SI_CONST.SI_GIC_REDIST_PWRR_OFFSET;
        size = SI_CONST.SI_GIC_REDIST_REGISTER_SIZE;
        bind = "&si_cl0_router.initiator_socket";
        priority = 0;
    }
    platform.si_gic_multiview.view0_redist_0_viewr = {
        address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1] + SI_CONST.SI_GIC_REDIST_VIEWR_OFFSET;
        size = SI_CONST.SI_GIC_REDIST_REGISTER_SIZE;
        bind = "&si_cl0_router.initiator_socket";
        priority = 0;
    }
    platform.si_gic_multiview.view0_redist_0_flushr = {
        address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1] + SI_CONST.SI_GIC_REDIST_FLUSHR_OFFSET;
        size = SI_CONST.SI_GIC_REDIST_REGISTER_SIZE;
        bind = "&si_cl0_router.initiator_socket";
        priority = 0;
    }

    -- CL0 CPU backend and SRAM
    local si_cl0_qemu_instance = si_cl0.define_qemu_instance(ctx, platform)

    platform.si_cl0_sram = {
        moduletype = "gs_memory";
        dmi = true;
        target_socket = {
            address = SI_CONST.SI_CL0_SRAM_BASE;
            size = SI_CONST.SI_CL0_SRAM_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        log_level = 0;
    }

    -- CL0 local control and safety IP
    platform.si_cl0_scr = {
        moduletype = "host_scr";
        cl1_present = true;
        cl0_config_0 = 0x01201717;
        cl0_config_1 = 0x01100002;
        cl0_config_2 = 0x00000034;
        cl0_c0_config_0 = 0x01000001;
        cl0_c0_config_1 = 0x01001000;
        cl0_c0_config_2 = 0x01200000;
        cl0_c0_config_3 = 0x00000000;
        target_socket = {
            address = SI_CONST.SI_CL0_SCR_BASE;
            size = SI_CONST.SI_CL0_SCR_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_timer_cntctl = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        target_socket = {
            address = SI_CONST.SI_CL0_TIMER_CNTCTL_BASE;
            size = SI_CONST.SI_CL0_TIMER_CNTCTL_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_timer_cntbase = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        counter_base = true;
        irq = {bind = si_cl0.spi_target(
            ctx, "si_cl0_system_timer", "View1")};
        target_socket = {
            address = SI_CONST.SI_CL0_TIMER_CNT_BASE;
            size = SI_CONST.SI_CL0_TIMER_CNT_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_watchdog = {
        moduletype = "zena_watchdog";
        clock_frequency = SI_CONST.SI_CL0_WDOG_CLOCK_HZ;
        control = {
            address = SI_CONST.SI_CL0_WDOG_CONTROL_BASE;
            size = SI_CONST.SI_CL0_WDOG_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        refresh = {
            address = SI_CONST.SI_CL0_WDOG_REFRESH_BASE;
            size = SI_CONST.SI_CL0_WDOG_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        ws0 = {bind = si_cl0.spi_target(
            ctx, "si_cl0_watchdog_ws0", "View1")};
        ws1 = {bind = "&host_reset_ctrl.si_watchdog_reset"};
        log_level = 0;
    }

    platform.si_cl0_ssu = {
        moduletype = "zena_ssu";
        target_socket = {
            address = SI_CONST.SI_CL0_SSU_BASE;
            size = SI_CONST.SI_CL0_SSU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        safety_status = {bind = "&host_reset_ctrl.safety_fault_reset"};
        log_level = 0;
    }

    platform.si_cl0_fmu = {
        moduletype = "zena_fmu";
        bank_count = SI_CONST.SI_CL0_FMU_BANK_COUNT;
        record_count = SI_CONST.SI_CL0_FMU_RECORD_COUNT;
        fault_input_enabled = true;
        fault_input_record = 0;
        fault_source = "si_cl0_ni710ae_primary_nci.apu_fault";
        fault_id = "ni710ae-apu-permission-denied";
        fault_sink = "si_cl0_fmu.record0";
        target_socket = {
            address = SI_CONST.SI_CL0_FMU_BASE;
            size = SI_CONST.SI_CL0_FMU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        critical_irq = {
            bind = si_cl0.spi_target(
                ctx, "si_cl0_fmu_critical", "View1");
        };
        non_critical_irq = {
            bind = si_cl0.spi_target(
                ctx, "si_cl0_fmu_noncritical", "View1");
        };
        critical_ssu = {bind = "&si_cl0_ssu.critical_in"};
        non_critical_ssu = {bind = "&si_cl0_ssu.non_critical_in"};
        log_level = 0;
    }

    -- NI-710AE register windows
    platform.si_cl0_ni710ae_primary_nci = {
        moduletype = "host_ni710ae_nci";
        topology = 4;
        protected_apu_index = 0;
        reset_owner_domain_id = ctx.request_context.domain.si_cl0;
        allow_trusted_loader = true;
        target_socket = {
            address = SI_CONST.SI_CL0_NI710AE_PRIMARY_NCI_BASE;
            size = SI_CONST.SI_CL0_NI710AE_NCI_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        apu_fault = {bind = "&si_cl0_fmu.fault_in"};
        log_level = 0;
    }

    platform.si_cl0_ni710ae_secondary_nci = {
        moduletype = "host_ni710ae_nci";
        topology = 2;
        target_socket = {
            address = SI_CONST.SI_CL0_NI710AE_SECONDARY_NCI_BASE;
            size = SI_CONST.SI_CL0_NI710AE_NCI_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_ni710ae_mhu_nci = {
        moduletype = "host_ni710ae_nci";
        topology = 1;
        target_socket = {
            address = SI_CONST.SI_CL0_NI710AE_MHU_NCI_BASE;
            size = SI_CONST.SI_CL0_NI710AE_NCI_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    -- SMD expansion and translated system windows
    platform.si_cl0_smd_expansion_window = {
        moduletype = "gs_memory";
        dmi = false;
        target_socket = {
            address = SI_CONST.SI_CL0_SMD_EXPANSION_PHYS_BASE;
            size = SI_CONST.SI_CL0_ATW2_SMD_EXPANSION_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 10;
        };
        log_level = 0;
    }

    platform.si_cl0_pll = {
        moduletype = "host_system_pll";
        lock_mask = 0x00000001;
        target_socket = {
            address = SI_CONST.SI_CL0_SMD_EXPANSION_PHYS_BASE;
            size = SI_CONST.SI_CL0_PLL_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_smcf_smd_mgi = {
        moduletype = "host_smcf_mgi";
        group_id = 0x00000000;
        monitor_count = 1;
        data_values_per_monitor = 12;
        data_width_bits = 32;
        target_socket = {
            address = SI_CONST.SI_CL0_SMCF_SMD_MGI_PHYS_BASE;
            size = SI_CONST.SI_CL0_ATW16_SMCF_SMD_MGI_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_cluster_utility_window = {
        moduletype = "gs_memory";
        dmi = false;
        target_socket = {
            address = SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_BASE;
            size = SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 20;
        };
        init_mem = true;
        log_level = 0;
    }

    platform.si_cl0_ni710ae_sys_ctrl = {
        moduletype = "gs_memory";
        dmi = false;
        target_socket = {
            address = SI_CONST.HOST_NI710AE_SYS_CTRL_PHYS_BASE;
            size = SI_CONST.HOST_NI710AE_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.si_cl0_ni710ae_smd = {
        moduletype = "gs_memory";
        dmi = false;
        target_socket = {
            address = SI_CONST.HOST_NI710AE_SMD_PHYS_BASE;
            size = SI_CONST.HOST_NI710AE_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    for i=0,3 do
        platform["si_cl0_smcf_ap_cluster_mgi_"..i] = {
            moduletype = "host_smcf_mgi";
            group_id = i + 1;
            monitor_count = 1;
            data_values_per_monitor = 12;
            data_width_bits = 32;
            target_socket = {
                address = SI_CONST.SI_CL0_AP_CLUSTER_MGI_PHYS_BASE +
                    (i * SI_CONST.SI_CL0_CLUSTER_UTILITY_MGI_STRIDE);
                size = SI_CONST.SI_CL0_CLUSTER_UTILITY_MGI_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
    end

    platform.si_cl0_system_id = {
        moduletype = "host_scr";
        system_id = 0x0000073c;
        soc_id = 0x0000073c;
        pidr4 = 0x00000004;
        pidr0 = 0x0000003c;
        pidr1 = 0x000000b7;
        pidr2 = 0x0000000b;
        pidr3 = 0x00000000;
        cidr0 = 0x0000000d;
        cidr1 = 0x000000f0;
        cidr2 = 0x00000005;
        cidr3 = 0x000000b1;
        target_socket = {
            address = SI_CONST.SI_CL0_SYSTEM_ID_PHYS_BASE;
            size = SI_CONST.SI_CL0_ATW4_SYSTEM_ID_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    -- CL1 and AP power/reset control
    platform.si_cl0_sys0_ppu = {
        moduletype = "host_ppu";
        target_socket = {
            address = SI_CONST.SI_CL0_SYS0_PPU_PHYS_BASE;
            size = SI_CONST.SI_CL0_SYS0_PPU_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl1_cluster_ppu = {
        moduletype = "host_ppu";
        initial_power_status = 0x0;
        target_socket = {
            address = SI_CONST.SI_CL1_CLUSTER_PPU_BASE;
            size = SI_CONST.SI_CL_PPU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    for i=0,(SI_CONST.SI_CL1_CORE_PPU_COUNT - 1) do
        platform["si_cl1_core"..i.."_ppu"] = {
            moduletype = "host_ppu";
            initial_power_status = 0x0;
            assert_power_on_reset = true;
            power_on_reset = {
                bind = "&si_cl1_cpu_"..i..".reset";
            };
            target_socket = {
                address = SI_CONST.SI_CL1_CORE_PPU0_BASE +
                    (i * SI_CONST.SI_CL1_CORE_PPU_STRIDE);
                size = SI_CONST.SI_CL_PPU_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
    end

    platform.si_cl1_ppu_ae = {
        moduletype = "gs_memory";
        dmi = false;
        target_socket = {
            address = SI_CONST.SI_CL1_PPU_AE_BASE;
            size = SI_CONST.SI_CL_PPU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        init_mem = true;
        log_level = 0;
    }

    for cluster=0,(SI_CONST.SI_CL0_AP_CLUSTER_COUNT - 1) do
        local cluster_base = SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_BASE +
            (cluster * SI_CONST.SI_CL0_CLUSTER_UTILITY_STRIDE)
        local ras = {
            moduletype = "apollo_cpu_ras";
            cluster_irq = {bind = si_cl0.spi_target(
                ctx, "si_cl0_ap_ras_cluster"..cluster, "View1")};
            log_level = 0;
        }
        for core=0,(SI_CONST.SI_CL0_AP_CORE_PER_CLUSTER_COUNT - 1) do
            ras["record_"..core] = {
                address = cluster_base + SI_CONST.SI_CL0_AP_CORE_RAS_OFFSET +
                    (core * SI_CONST.SI_CL0_AP_CORE_PPU_STRIDE);
                size = SI_CONST.SI_CL0_AP_CORE_RAS_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            }
        end
        platform["si_cl0_ap_ras_cluster"..cluster] = ras

        platform["si_cl0_ap_cluster"..cluster.."_ppu"] = {
            moduletype = "host_ppu";
            initial_power_status = 0x0;
            target_socket = {
                address = cluster_base + SI_CONST.SI_CL0_AP_CLUSTER_PPU_OFFSET;
                size = SI_CONST.SI_CL_PPU_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
        platform["si_cl0_ap_cluster"..cluster.."_ae"] = {
            moduletype = "gs_memory";
            dmi = false;
            target_socket = {
                address = cluster_base + SI_CONST.SI_CL0_AP_CLUSTER_AE_OFFSET;
                size = SI_CONST.SI_CL_PPU_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            };
            init_mem = true;
            log_level = 0;
        }

        platform["si_cl0_ap_cluster"..cluster.."_control"] = {
            moduletype = "gs_memory";
            dmi = false;
            target_socket = {
                address = cluster_base + SI_CONST.SI_CL0_AP_CLUSTER_CONTROL_OFFSET;
                size = SI_CONST.SI_CL0_AP_CLUSTER_CONTROL_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            };
            init_mem = true;
            log_level = 0;
        }

        for core=0,(SI_CONST.SI_CL0_AP_CORE_PER_CLUSTER_COUNT - 1) do
            local cpu_index = ap_cpu_index(cluster, core)
            local cpu_active = enable_ap_cpus and cpu_index < AP_NUM_CPUS
            platform["si_cl0_ap_cluster"..cluster.."_core"..core.."_ppu"] = {
                moduletype = "host_ppu";
                initial_power_status = 0x0;
                trace = host_ppu_trace;
                trace_limit = host_ppu_trace_limit;
                assert_power_on_reset = cpu_active;
                assert_power_on_load = cpu_active and cpu_index == 0;
                power_on_load_pulse_width_ns = 0;
                power_on_load_to_reset_delay_ns = 0;
                power_on_load = cpu_active and cpu_index == 0 and
                    {bind = "&host_reset_ctrl.ap_power_reset"} or nil;
                power_on_reset = cpu_active and {
                    bind = "&ap_cpu_"..cpu_index..".reset";
                } or nil;
                target_socket = {
                    address = cluster_base + SI_CONST.SI_CL0_AP_CORE_PPU0_OFFSET +
                        (core * SI_CONST.SI_CL0_AP_CORE_PPU_STRIDE);
                    size = SI_CONST.SI_CL_PPU_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                };
                log_level = 0;
            }
        end
    end

    -- CL0 interrupt controller and console
    si_cl0.define_interrupt_controller(
        ctx,
        platform,
        si_cl0_qemu_instance)

    platform.si_cl0_console_file = {
        moduletype = "char_backend_file";
        read_file = si_cl0_uart_read_file;
        write_file = si_cl0_log;
        poll_read = si_cl0_uart_poll_read;
        poll_interval_ms = SI_CONST.SI_CL0_UART_POLL_INTERVAL_MS;
        baudrate = 0;
    }

    platform.si_cl0_uart = {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = {
            address = SI_CONST.SI_CL0_UART_BASE;
            size = SI_CONST.SI_CL0_UART_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        irq = {bind = si_cl0.spi_target(ctx, "si_cl0_uart", "View1")};
        backend_socket = {bind = "&si_cl0_console_file.biflow_socket"};
    }

    si_cl0.define_loader_and_cpu(
        ctx,
        platform,
        si_cl0_qemu_instance,
        si_cl0_image)


    print("si-cl0 image: "..si_cl0_image)
    print("si-cl0 log:   "..si_cl0_log)
    print("si-cl0 entry: 0x"..string.format("%x", SI_CONST.SI_CL0_ENTRY))
end

return si_cl0
