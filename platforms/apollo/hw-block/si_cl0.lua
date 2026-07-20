local si_cl0 = {}

-- SCP-firmware exposes architectural GIC INTIDs, while QBox's spi_in_N
-- sockets are zero-based SPI inputs (INTID 32 is spi_in_0).
local GIC_SPI_BASE_INTID = 32
local SI_CL0_UART_INTID = 40
local SI_CL0_AP_NS_MHU_SEND_INTID = 96
local SI_CL0_AP_NS_MHU_RECV_INTID = 97
local SI_CL0_AP_SCMI_MHU_SEND_INTID = 98
local SI_CL0_AP_SCMI_MHU_RECV_INTID = 99
local SI_CL0_AP_PFDI_MHU_SEND_INTID = 102
local SI_CL0_AP_PFDI_MHU_RECV_INTID = 103
local SI_CL0_RSE_MHU_INTID = 105
local SI_CL0_CL1_MHU_INTID = 107
local SI_CL0_FMU_CRITICAL_INTID = 128
local SI_CL0_FMU_NON_CRITICAL_INTID = 129
local SI_CL0_WDOG_WS0_INTID = 37

function si_cl0.define(ctx, platform)
    platform.host_si_cl0_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_si_sram_dmi;
        target_socket = {
            address = HOST_SI_CL0_SRAM_PHYS_BASE;
            size = HOST_SI_SRAM_WINDOW_SIZE;
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
            address = HOST_SI_CL0_CL_UTIL_BASE;
            size = HOST_SI_CL_UTIL_SIZE;
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
            address = HOST_SI_CL0_CL_UTIL_BASE + HOST_SI_CLUS_PPU_OFFSET;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 10;
        };
        log_level = 0;
    }

    platform.host_si_cl0_core0_ppu = {
        moduletype = "host_ppu";
        trace = host_ppu_trace;
        trace_limit = host_ppu_trace_limit;
        assert_power_on_reset = apollo_live_cl0;
        assert_power_on_load = apollo_live_cl0;
        power_on_load = apollo_live_cl0 and {bind = "&si_cl0_loader.reset"} or nil;
        power_on_reset = apollo_live_cl0 and {bind = "&si_cl0_cpu_0.reset"} or nil;
        power_on_load_pulse_width_ns = 0;
        power_on_load_to_reset_delay_ns = 0;
        power_on_status_delay_ns = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PPU_ON_DELAY_NS", "0");
        access_latency_ns = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PPU_ACCESS_LATENCY_NS", "100");
        target_socket = {
            address = HOST_SI_CL0_CL_UTIL_BASE + HOST_SI_CORE0_PPU_OFFSET;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
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
        tx_shmem = 0x00180000;
        rx_shmem = 0x00180000;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38000000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_AP_NS_MHU_SEND_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    }

    platform.si_cl0_ap_ns_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "ap_to_si_cl0_ns";
        protocol = "doorbell-bridge";
        tx_shmem = 0x00180000;
        rx_shmem = 0x00180000;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38040000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_AP_NS_MHU_RECV_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    }

    platform.si_cl0_ap_scmi_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "si_cl0_to_ap_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38080000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_AP_SCMI_MHU_SEND_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    }

    platform.si_cl0_ap_scmi_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "ap_to_si_cl0_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x380C0000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_AP_SCMI_MHU_RECV_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    }

    platform.si_cl0_ap_pfdi_monitor_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "si_cl0_to_ap_pfdi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38380000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_AP_PFDI_MHU_SEND_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    }

    platform.si_cl0_ap_pfdi_monitor_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "ap_to_si_cl0_pfdi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x383C0000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_AP_PFDI_MHU_RECV_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    }

    platform.si_cl0_rse_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "si_cl0_to_rse";
        protocol = "doorbell-bridge";
        tx_shmem = 0x40000000;
        rx_shmem = 0x40000000;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38100000;
            size = RSE_MHU_FRAME_SIZE;
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
        tx_shmem = 0x40000000;
        rx_shmem = 0x40000000;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38140000;
            size = RSE_MHU_FRAME_SIZE;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_RSE_MHU_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    }

    platform.si_cl0_pfdi_mhu_pbx = ctx.apollo_live_cl1 and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "apollo_si_cl0_pfdi_reply";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        channel_count = 32;
        tx_shmem = 0x48000000;
        rx_shmem = 0x48000000;
        scmi_channel_stride = 40;
        scmi_channel_base_index = 2;
        scmi_channel_count = 4;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38200000;
            size = 0x00020000;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        log_level = 0;
    } or nil

    platform.si_cl0_pfdi_mhu_mbx = ctx.apollo_live_cl1 and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "apollo_si_cl1_pfdi";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        channel_count = 32;
        tx_shmem = 0x48000000;
        rx_shmem = 0x48000000;
        scmi_channel_stride = 40;
        scmi_channel_base_index = 2;
        scmi_channel_count = 4;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x38240000;
            size = 0x00020000;
            bind = "&si_cl0_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_CL1_MHU_INTID - GIC_SPI_BASE_INTID)};
        log_level = 0;
    } or nil

    platform.host_si_atu.translation_socket = {
        address = 0x80000000;
        size = 0x60340000;
        bind = "&si_cl0_router.initiator_socket";
        relative_addresses = false;
    }
    platform.host_si_atu.initiator_socket = {
        bind = "&system_router.target_socket";
    }
    platform.host_si_atu.enable_dmi = false

    platform.host_smdexp2smd_atu.translation_socket = {
        address = 0xE0340000;
        size = 0x00002000;
        bind = "&si_cl0_router.initiator_socket";
        relative_addresses = false;
    }
    platform.host_smdexp2smd_atu.initiator_socket = {
        bind = "&system_router.target_socket";
    }
    platform.host_smdexp2smd_atu.enable_dmi = false

    platform.system_to_ap_shared_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = 0x00000000;
        target_socket = {
            address = 0x00000000;
            size = 0x00200000;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    platform.system_to_ap_gic_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = 0x20000000;
        target_socket = {
            address = 0x20000000;
            size = 0x08000000;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    platform.si_cl0_rse_shared_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = HOST_RSE_SI_SSRAM_PHYS_BASE;
        target_socket = {
            address = 0x40000000;
            size = HOST_RSE_SI_SSRAM_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.si_cl0_to_si_cl1_scmi_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = 0x48000000;
        target_socket = {
            address = 0x48000000;
            size = 0x00001000;
            bind = "&si_cl0_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&si_cl1_router.target_socket"};
        log_level = 0;
    }

    local SI_CL0_SRAM_BASE = 0x120000000
    local SI_CL0_SRAM_SIZE = 0x00800000
    local SI_CL0_ENTRY = 0x120000000
    local SI_CL0_GICD_VIEW0_BASE = 0x30000000
    local SI_CL0_GICR_VIEW0_BASES = {
        0x30040000;
        0x30060000;
        0x30080000;
        0x300a0000;
        0x300c0000;
    }
    local SI_CL0_GICD_VIEW1_BASE = 0x30100000
    local SI_CL0_GICR_VIEW1_BASE = 0x30140000
    local SI_CL0_GICR_SIZE = 0x00020000
    local SI_CL1_CLUSTER_UTILITY_BUS_BASE = 0x28800000
    local SI_CL1_CLUSTER_PPU_BASE = SI_CL1_CLUSTER_UTILITY_BUS_BASE + 0x00010000
    local SI_CL1_PPU_AE_BASE = SI_CL1_CLUSTER_UTILITY_BUS_BASE + 0x00080000
    local SI_CL1_CORE_PPU0_BASE = SI_CL1_CLUSTER_UTILITY_BUS_BASE + 0x00040000
    local SI_CL1_CORE_PPU_STRIDE = 0x00100000
    local SI_CL1_CORE_PPU_COUNT = 4
    local SI_CL_PPU_SIZE = 0x00001000
    local SI_CL0_UART_BASE = 0x2a400000
    local SI_CL0_SCR_BASE = 0x2a6b0000
    local SI_CL0_SCR_SIZE = 0x00010000
    local SI_CL0_TIMER_CNTCTL_BASE = 0x2a6f0000
    local SI_CL0_TIMER_CNTCTL_SIZE = 0x00010000
    local SI_CL0_TIMER_CNT_BASE = 0x2a720000
    local SI_CL0_TIMER_CNT_SIZE = 0x00010000
    local SI_CL0_WDOG_CONTROL_BASE = 0x2a700000
    local SI_CL0_WDOG_REFRESH_BASE = 0x2a710000
    local SI_CL0_SSU_BASE = 0x2a500000
    local SI_CL0_SSU_SIZE = 0x00001000
    local SI_CL0_FMU_BASE = 0x2a510000
    local SI_CL0_FMU_SIZE = 0x00050000
    local SI_CL0_NI710AE_PRIMARY_NCI_BASE = 0x2a000000
    local SI_CL0_NI710AE_SECONDARY_NCI_BASE = 0x2a200000
    local SI_CL0_NI710AE_MHU_NCI_BASE = 0x2a300000
    local SI_CL0_NI710AE_NCI_SIZE = 0x00010000
    local SI_CL0_ATW0_CMN_BASE = 0x80000000
    local SI_CL0_ATW0_CMN_SIZE = 0x40000000
    local SI_CL0_ATW1_CLUSTER_UTILITY_BASE = 0xc0000000
    local SI_CL0_CLUSTER_UTILITY_STRIDE = 0x04000000
    local SI_CL0_AP_CLUSTER_COUNT = 4
    local SI_CL0_AP_CORE_PER_CLUSTER_COUNT = 4
    local SI_CL0_AP_CLUSTER_PPU_OFFSET = 0x01030000
    local SI_CL0_AP_CLUSTER_AE_OFFSET = 0x01050000
    local SI_CL0_AP_CORE_PPU0_OFFSET = 0x01080000
    local SI_CL0_AP_CORE_PPU_STRIDE = 0x00100000
    local SI_CL0_AP_CLUSTER_CONTROL_OFFSET = 0x02000000
    local SI_CL0_AP_CLUSTER_CONTROL_SIZE = 0x00010000
    local SI_CL0_ATW2_SMD_EXPANSION_BASE = 0xd0000000
    local SI_CL0_ATW2_SMD_EXPANSION_SIZE = 0x00020000
    local SI_CL0_PLL_BASE = SI_CL0_ATW2_SMD_EXPANSION_BASE
    local SI_CL0_PLL_SIZE = 0x00001000
    local SI_CL0_ATW3_SYSTOP_PIK_BASE = 0xd0020000
    local SI_CL0_ATW3_SYSTOP_PIK_SIZE = 0x00010000
    local SI_CL0_SYS0_PPU_BASE = 0xd0021000
    local SI_CL0_SYS0_PPU_SIZE = 0x00001000
    local SI_CL0_ATW4_SYSTEM_ID_BASE = 0xd0030000
    local SI_CL0_ATW4_SYSTEM_ID_SIZE = 0x00010000
    local SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE = 0xd0040000
    local SI_CL0_ATW5_CSS_COUNTERS_TIMERS_SIZE = 0x00030000
    local SI_CL0_REFCLK_CNTCONTROL_BASE = SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE
    local SI_CL0_REFCLK_CNTCONTROL_SIZE = 0x00010000
    local SI_CL0_REFCLK_CNTREAD_BASE =
        SI_CL0_REFCLK_CNTCONTROL_BASE + SI_CL0_REFCLK_CNTCONTROL_SIZE
    local SI_CL0_REFCLK_CNTSYNC_BASE =
        SI_CL0_REFCLK_CNTREAD_BASE + SI_CL0_REFCLK_CNTCONTROL_SIZE
    local SI_CL0_ATW5_CSS_COUNTERS_TIMERS_PROBE_BASE =
        SI_CL0_ATW5_CSS_COUNTERS_TIMERS_BASE +
        SI_CL0_ATW5_CSS_COUNTERS_TIMERS_SIZE - 4
    local SI_CL0_ATW6_AP_GIC_BASE = 0xd0770000
    local SI_CL0_ATW6_AP_GICD_MULTIVIEW_SIZE = 0x00010000
    local SI_CL0_ATW6_AP_GICR_BASE = SI_CL0_ATW6_AP_GIC_BASE + 0x00080000
    local SI_CL0_ATW6_AP_GICR_STRIDE = 0x00040000
    local SI_CL0_ATW6_AP_GICR_SIZE = 0x00020000
    local SI_CL0_ATW6_AP_GICR_COUNT = 16
    local SI_CL0_CLUSTER_UTILITY_MGI0_BASE = 0xc0200000
    local SI_CL0_CLUSTER_UTILITY_MGI_STRIDE = 0x04000000
    local SI_CL0_CLUSTER_UTILITY_MGI_SIZE = 0x00010000
    local SI_CL0_ATW16_SMCF_SMD_MGI_BASE = 0xe0230000
    local SI_CL0_ATW16_SMCF_SMD_MGI_SIZE = 0x00010000
    local SI_CL0_ATW6_AP_PERIPHERAL_SRAM_BASE = 0xe0030000
    local SI_CL0_ATW6_AP_PERIPHERAL_SRAM_SIZE = 0x00100000
    local SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_BASE = 0xe0130000
    local SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_SIZE = 0x00100000
    local SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_TAIL_BASE =
        SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_BASE +
        ctx.APOLLO_SI_CL1_HIPC_SHARED_SIZE
    local SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_TAIL_SIZE =
        SI_CL0_ATW7_AP_PERIPHERAL_NS_SRAM_SIZE -
        ctx.APOLLO_SI_CL1_HIPC_SHARED_SIZE
    local SI_CL0_ATW17_SMD_SRAM_BASE = 0xe0240000
    local SI_CL0_ATW17_SMD_SRAM_SIZE = 0x00100000
    local SI_CL0_ATW18_SMCF_SMDEXP_SRAM_BASE = 0xe0340000
    local SI_CL0_ATW18_SMCF_SMDEXP_SRAM_SIZE = 0x00002000
    local ARCH_TIMER_SEC_PPI = 16 + 13
    local ARCH_TIMER_PHYS_PPI = 16 + 4
    local ARCH_TIMER_VIRT_PPI = 16 + 11
    local ARCH_TIMER_HYP_PPI = 16 + 3

    local si_cl0_image = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_IMAGE",
        ctx.apollo_root.."build/local-apollo-fvp/deploy/firmware/si0_ramfw.bin")
    local si_cl0_log = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_LOG",
        ctx.getenv_or(
            "QBOX_RDASPEN_SCP_LOG",
            ctx.apollo_root.."build/qbox-apollo-fvp/full-live-cl0-cl1/qbox-safety-island-cl0.log"))
    local si_cl0_uart_read_file = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_UART_READ_FILE",
        "/dev/null")
    local si_cl0_uart_poll_read = si_cl0_uart_read_file ~= "/dev/null"
    local si_cl0_qemu_args = ctx.getenv_or("QBOX_APOLLO_FULL_SI_CL0_QEMU_ARGS", "")
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
            address = 0x100000000;
            size = SI_CL0_ATW0_CMN_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_gic_multiview = {
        moduletype = "gicx00_multiview";
        trace = si_gic_trace;
        trace_limit = si_gic_trace_limit;
        view0_dist_cfgid = {
            address = SI_CL0_GICD_VIEW0_BASE + 0x0000f000;
            size = 0x00000008;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        view0_dist_iviewr = {
            address = SI_CL0_GICD_VIEW0_BASE + 0x0000f600;
            size = 0x00000400;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
    }

    for i=1,4 do
        platform.si_gic_multiview["view0_redist_"..i] = {
            address = SI_CL0_GICR_VIEW0_BASES[i + 1];
            size = SI_CL0_GICR_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 20;
        }
    end

    platform.si_gic_multiview.view0_redist_0_pwrr = {
        address = SI_CL0_GICR_VIEW0_BASES[1] + 0x00000024;
        size = 0x00000004;
        bind = "&si_cl0_router.initiator_socket";
        priority = 0;
    }
    platform.si_gic_multiview.view0_redist_0_viewr = {
        address = SI_CL0_GICR_VIEW0_BASES[1] + 0x0000002c;
        size = 0x00000004;
        bind = "&si_cl0_router.initiator_socket";
        priority = 0;
    }
    platform.si_gic_multiview.view0_redist_0_flushr = {
        address = SI_CL0_GICR_VIEW0_BASES[1] + 0x00000030;
        size = 0x00000004;
        bind = "&si_cl0_router.initiator_socket";
        priority = 0;
    }

    -- CL0 CPU backend and SRAM
    platform.si_cl0_qemu_inst_mgr = {
        moduletype = "QemuInstanceManager";
    }

    platform.si_cl0_qemu_inst = {
        moduletype = "QemuInstance";
        args = {"&platform.si_cl0_qemu_inst_mgr", "AARCH64"};
        accel = ctx.getenv_or("QBOX_APOLLO_FULL_SI_CL0_ACCEL", "tcg");
        tcg_mode = ctx.getenv_or(
            "QBOX_APOLLO_FULL_SI_CL0_TCG_MODE", "MULTI");
        sync_policy = ctx.getenv_or(
            "QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY", "multithread-quantum");
        managed_start_in_reset_release = true;
        qemu_args = si_cl0_qemu_args;
    }

    platform.si_cl0_sram = {
        moduletype = "gs_memory";
        dmi = true;
        target_socket = {
            address = SI_CL0_SRAM_BASE;
            size = SI_CL0_SRAM_SIZE;
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
            address = SI_CL0_SCR_BASE;
            size = SI_CL0_SCR_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_timer_cntctl = {
        moduletype = "host_gtimer";
        target_socket = {
            address = SI_CL0_TIMER_CNTCTL_BASE;
            size = SI_CL0_TIMER_CNTCTL_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_timer_cntbase = {
        moduletype = "host_gtimer";
        counter_base = true;
        frequency = 125000000;
        counter_increment = 4096;
        target_socket = {
            address = SI_CL0_TIMER_CNT_BASE;
            size = SI_CL0_TIMER_CNT_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_watchdog = {
        moduletype = "zena_watchdog";
        clock_frequency = 125000000;
        control = {
            address = SI_CL0_WDOG_CONTROL_BASE;
            size = 0x00010000;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        refresh = {
            address = SI_CL0_WDOG_REFRESH_BASE;
            size = 0x00010000;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        ws0 = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_WDOG_WS0_INTID - GIC_SPI_BASE_INTID)};
        ws1 = {bind = "&host_reset_ctrl.si_watchdog_reset"};
        log_level = 0;
    }

    platform.si_cl0_ssu = {
        moduletype = "zena_ssu";
        target_socket = {
            address = SI_CL0_SSU_BASE;
            size = SI_CL0_SSU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        safety_status = {bind = "&host_reset_ctrl.safety_fault_reset"};
        log_level = 0;
    }

    platform.si_cl0_fmu = {
        moduletype = "zena_fmu";
        bank_count = 5;
        record_count = 384;
        fault_input_enabled = true;
        fault_input_record = 0;
        fault_source = "si_cl0_ni710ae_primary_nci.apu_fault";
        fault_id = "ni710ae-apu-permission-denied";
        fault_sink = "si_cl0_fmu.record0";
        target_socket = {
            address = SI_CL0_FMU_BASE;
            size = SI_CL0_FMU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        critical_irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_FMU_CRITICAL_INTID - GIC_SPI_BASE_INTID)};
        non_critical_irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_FMU_NON_CRITICAL_INTID - GIC_SPI_BASE_INTID)};
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
            address = SI_CL0_NI710AE_PRIMARY_NCI_BASE;
            size = SI_CL0_NI710AE_NCI_SIZE;
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
            address = SI_CL0_NI710AE_SECONDARY_NCI_BASE;
            size = SI_CL0_NI710AE_NCI_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_ni710ae_mhu_nci = {
        moduletype = "host_ni710ae_nci";
        topology = 1;
        target_socket = {
            address = SI_CL0_NI710AE_MHU_NCI_BASE;
            size = SI_CL0_NI710AE_NCI_SIZE;
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
            address = 0x20000D8000000;
            size = SI_CL0_ATW2_SMD_EXPANSION_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 10;
        };
        log_level = 0;
    }

    platform.si_cl0_pll = {
        moduletype = "host_system_pll";
        lock_mask = 0x00000001;
        target_socket = {
            address = 0x20000D8000000;
            size = SI_CL0_PLL_SIZE;
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
            address = 0x20000D8100000;
            size = SI_CL0_ATW16_SMCF_SMD_MGI_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl0_cluster_utility_window = {
        moduletype = "gs_memory";
        dmi = false;
        target_socket = {
            address = 0x140000000;
            size = 0x10000000;
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
            address = HOST_NI710AE_SYS_CTRL_PHYS_BASE;
            size = 0x00100000;
            bind = "&smd_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.si_cl0_ni710ae_smd = {
        moduletype = "gs_memory";
        dmi = false;
        target_socket = {
            address = HOST_NI710AE_SMD_PHYS_BASE;
            size = 0x00100000;
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
                address = 0x140200000 +
                    (i * SI_CL0_CLUSTER_UTILITY_MGI_STRIDE);
                size = SI_CL0_CLUSTER_UTILITY_MGI_SIZE;
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
            address = 0x20000D0400000;
            size = SI_CL0_ATW4_SYSTEM_ID_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    -- CL1 and AP power/reset control
    platform.si_cl0_sys0_ppu = {
        moduletype = "host_ppu";
        target_socket = {
            address = 0x20000D0201000;
            size = SI_CL0_SYS0_PPU_SIZE;
            bind = "&smd_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    platform.si_cl1_cluster_ppu = {
        moduletype = "host_ppu";
        initial_power_status = 0x0;
        target_socket = {
            address = SI_CL1_CLUSTER_PPU_BASE;
            size = SI_CL_PPU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        log_level = 0;
    }

    for i=0,(SI_CL1_CORE_PPU_COUNT - 1) do
        platform["si_cl1_core"..i.."_ppu"] = {
            moduletype = "host_ppu";
            initial_power_status = 0x0;
            assert_power_on_reset = ctx.apollo_live_cl1;
            power_on_reset = ctx.apollo_live_cl1 and {
                bind = "&si_cl1_cpu_"..i..".reset";
            } or nil;
            target_socket = {
                address = SI_CL1_CORE_PPU0_BASE +
                    (i * SI_CL1_CORE_PPU_STRIDE);
                size = SI_CL_PPU_SIZE;
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
            address = SI_CL1_PPU_AE_BASE;
            size = SI_CL_PPU_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        };
        init_mem = true;
        log_level = 0;
    }

    for cluster=0,(SI_CL0_AP_CLUSTER_COUNT - 1) do
        local cluster_base = 0x140000000 +
            (cluster * SI_CL0_CLUSTER_UTILITY_STRIDE)
        platform["si_cl0_ap_cluster"..cluster.."_ppu"] = {
            moduletype = "host_ppu";
            initial_power_status = 0x0;
            target_socket = {
                address = cluster_base + SI_CL0_AP_CLUSTER_PPU_OFFSET;
                size = SI_CL_PPU_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
        platform["si_cl0_ap_cluster"..cluster.."_ae"] = {
            moduletype = "gs_memory";
            dmi = false;
            target_socket = {
                address = cluster_base + SI_CL0_AP_CLUSTER_AE_OFFSET;
                size = SI_CL_PPU_SIZE;
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
                address = cluster_base + SI_CL0_AP_CLUSTER_CONTROL_OFFSET;
                size = SI_CL0_AP_CLUSTER_CONTROL_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            };
            init_mem = true;
            log_level = 0;
        }

        for core=0,(SI_CL0_AP_CORE_PER_CLUSTER_COUNT - 1) do
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
                    address = cluster_base + SI_CL0_AP_CORE_PPU0_OFFSET +
                        (core * SI_CL0_AP_CORE_PPU_STRIDE);
                    size = SI_CL_PPU_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                };
                log_level = 0;
            }
        end
    end

    -- CL0 interrupt controller and console
    platform.si_cl0_gic = {
        moduletype = "arm_gicv3";
        args = {"&platform.si_cl0_qemu_inst"};
        dist_iface = {
            address = SI_CL0_GICD_VIEW1_BASE;
            size = 0x00010000;
            bind = "&si_cl0_router.initiator_socket";
            priority = 10;
            aliases = {
                view0_functional = {
                    address = SI_CL0_GICD_VIEW0_BASE;
                    size = 0x00010000;
                };
            };
        };
        redist_region = {1};
        redist_iface_0 = {
            address = SI_CL0_GICR_VIEW1_BASE;
            size = SI_CL0_GICR_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 10;
            aliases = {
                view0_functional = {
                    address = SI_CL0_GICR_VIEW0_BASES[1];
                    size = SI_CL0_GICR_SIZE;
                };
            };
        };
        num_cpus = 1;
        num_spi = 384;
    }

    platform.si_cl0_console_file = {
        moduletype = "char_backend_file";
        read_file = si_cl0_uart_read_file;
        write_file = si_cl0_log;
        poll_read = si_cl0_uart_poll_read;
        poll_interval_ms = 100;
        baudrate = 0;
    }

    platform.si_cl0_uart = {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = {
            address = SI_CL0_UART_BASE;
            size = 0x00010000;
            bind = "&si_cl0_router.initiator_socket";
        };
        irq = {bind = "&si_cl0_gic.spi_in_"..
            (SI_CL0_UART_INTID - GIC_SPI_BASE_INTID)};
        backend_socket = {bind = "&si_cl0_console_file.biflow_socket"};
    }

    -- CL0 boot image and Cortex-R82 CPU
    platform.si_cl0_loader = {
        moduletype = "loader";
        request_origin_id = ctx.request_context.origin.si_cl0_loader;
        request_domain_id = ctx.request_context.domain.si_cl0;
        request_capabilities = ctx.request_context.capability.authenticated_image;
        request_secure = true;
        request_secure_valid = true;
        initiator_socket = {bind = "&si_cl0_router.target_socket"};
        { bin_file = si_cl0_image, address = SI_CL0_SRAM_BASE };
    }

    platform.si_cl0_cpu_0 = {
        moduletype = "cpu_arm_cortexR82";
        args = {"&platform.si_cl0_qemu_inst"};
        mem = {bind = "&si_cl0_ni710ae_primary_nci.protected_target_socket"};
        has_el2 = true;
        psci_conduit = "smc";
        start_powered_off = false;
        start_in_reset = true;
        reset_power_on = true;
        rvbar = SI_CL0_ENTRY;
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
            ctx.apollo_root.."build/qbox-apollo-fvp/full-live-cl0-cl1/si-cl0-pc-trace.log");
        trace_pc_interval = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_INTERVAL",
            "1");
        trace_pc_limit = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_LIMIT",
            "4096");
        irq_timer_sec_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..ARCH_TIMER_SEC_PPI;
        };
        irq_timer_phys_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..ARCH_TIMER_PHYS_PPI;
        };
        irq_timer_virt_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..ARCH_TIMER_VIRT_PPI;
        };
        irq_timer_hyp_out = {
            bind = "&si_cl0_gic.ppi_in_cpu_0_"..ARCH_TIMER_HYP_PPI;
        };
    }

    platform.si_cl0_gic.irq_out_0 = {bind = "&si_cl0_cpu_0.irq_in"}
    platform.si_cl0_gic.fiq_out_0 = {bind = "&si_cl0_cpu_0.fiq_in"}
    platform.si_cl0_gic.virq_out_0 = {bind = "&si_cl0_cpu_0.virq_in"}
    platform.si_cl0_gic.vfiq_out_0 = {bind = "&si_cl0_cpu_0.vfiq_in"}

    print("si-cl0 image: "..si_cl0_image)
    print("si-cl0 log:   "..si_cl0_log)
    print("si-cl0 entry: 0x"..string.format("%x", SI_CL0_ENTRY))
end

return si_cl0
