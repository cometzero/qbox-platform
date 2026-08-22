local system_mgmt = {}

local HOST_SI_PIK_PHYS_BASE = 0x400002A600000
local HOST_SI_SCR_PHYS_BASE = 0x400002A6B0000
local HOST_SI_ATU_PHYS_BASE = 0x4000031000000
local HOST_SI_CONTROL_WINDOW_SIZE = 0x00010000
local HOST_RSE_SI_MHU_PHYS_BASE = 0x400003C000000
local RSE_MHU_FRAME_SIZE = 0x00020000
local RSE_IRQ_SI_CL0_RSE_CMU_MHU_RECEIVER = 139
local HOST_RSE_SI_SSRAM_PHYS_BASE = 0x4000040000000
local HOST_RSE_SI_SSRAM_SIZE = 0x00040000
local HOST_AP_SHARED_SRAM_PHYS_BASE = 0x00000000
local HOST_AP_SDS_MEM_SIZE = 0x00000DC0
local HOST_AP_SCMI_PAYLOAD_BASE =
    HOST_AP_SHARED_SRAM_PHYS_BASE + HOST_AP_SDS_MEM_SIZE
local HOST_AP_NS_SCMI_TX_SHMEM_BASE = 0x00180000
local HOST_AP_NS_SCMI_RX_SHMEM_BASE = 0x00180100
local HOST_AP_SI_NS_SCMI_MHU_PBX_PHYS_BASE = 0x400003B000000
local HOST_AP_SI_NS_SCMI_MHU_MBX_PHYS_BASE = 0x400003B040000
local HOST_AP_SI_SCMI_MHU_PBX_PHYS_BASE = 0x400003B080000
local HOST_AP_SI_SCMI_MHU_MBX_PHYS_BASE = 0x400003B0C0000
local HOST_AP_SI_CL1_MHU_PBX_PHYS_BASE = 0x400003B100000
local HOST_AP_SI_CL1_MHU_MBX_PHYS_BASE = 0x400003B140000
local HOST_AP_SI_PFDI_MONITOR_MHU_PBX_PHYS_BASE = 0x400003B380000
local HOST_AP_SI_PFDI_MONITOR_MHU_MBX_PHYS_BASE = 0x400003B3C0000
local HOST_AP_SI_MHU_FRAME_SIZE = 0x00030000
local HOST_AP_SCMI_PFDI_MONITOR_OFFSET = 0x00000100
local HOST_AP_SCMI_PFDI_MONITOR_BASE =
    HOST_AP_SCMI_PAYLOAD_BASE + HOST_AP_SCMI_PFDI_MONITOR_OFFSET
local HOST_AP_SCMI_PFDI_MONITOR_STRIDE = 40
local HOST_AP_SCMI_PFDI_MONITOR_CHANNELS = 16
local AP_SI_NS_MHU_PBX_IRQ = 112
local AP_SI_NS_MHU_MBX_IRQ = 113
local AP_SI_SCMI_MHU_PBX_IRQ = 114
local AP_SI_SCMI_MHU_MBX_IRQ = 115
local AP_SI_PFDI_MHU_PBX_IRQ = 118
local AP_SI_PFDI_MHU_MBX_IRQ = 119
local AP_SI_CL1_MHU_PBX_IRQ = 120
local AP_SI_CL1_MHU_MBX_IRQ = 121
local AP_SI_CL1_MHU_CHANNEL_COUNT = 32
local HOST_AP_ATU_PHYS_BASE = 0x20000D0080000
local HOST_AP_ATU_LOGICAL_BASE = 0x40000000
local HOST_AP_ATU_LOGICAL_SIZE = 0x00800000
local HOST_SMDEXP2SMD_ATU_PHYS_BASE = 0x20000D0070000
local HOST_CSS_RGM_PHYS_BASE = 0x20000D0010000
local HOST_SYSTOP_PIK_PHYS_BASE = 0x20000D0200000
local HOST_CSS_COUNTERS_TIMERS_PHYS_BASE = 0x20000D0100000
local HOST_CSS_COUNTER_CONTROL_OFFSET = 0x00000000
local HOST_CSS_COUNTER_READ_OFFSET = 0x00010000
local HOST_CSS_COUNTER_SYNC_OFFSET = 0x00020000
local HOST_CSS_COUNTER_FRAME_SIZE = 0x00010000
local SYSTEM_RESET_REGISTER_SIZE = 0x00010000
local HOST_SMD_SHARED_SRAM_PHYS_BASE = 0x2000060000000
local HOST_SMD_SHARED_SRAM_SIZE = 0x00100000
local HOST_AP_RSE_MHU_PHYS_BASE = 0x300001B600000
local MHU_V3_FRAME_SIZE = 0x00030000
local HOST_AP_MHU_POINTER_ACCESS_PHYS_BASE = 0x0FFFE0000
local HOST_AP_MHU_POINTER_ACCESS_SIZE = 0x00020000
local HOST_AP_RSE_MAILBOX_PHYS_BASE = 0xFFFFC000
local SYSTEM_ATU_BUILD_CONFIG = 0x000000C5
local SYSTEM_COUNTER_FREQUENCY_HZ = 125000000
local SYSTEM_COUNTER_INTEGER_INCREMENT = 1
local SYSTEM_POWER_RESET_DELAY_NS = 1
local SYSTEM_POWER_RESET_PULSE_WIDTH_NS = 1
local atu_trace_address_min = tonumber(getenv_or(
    "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MIN", "0"))
local atu_trace_address_max = tonumber(getenv_or(
    "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MAX", "0"))

system_mgmt.ownership = {
    reset = {
        "reset_gpio";
        "reset_fanout";
        "zena_reset_ctrl";
        "host_ppu";
        "mhu320ae";
    };
    shared_memory = {
        "host_rse_si_ssram";
        "host_smd_shared_sram";
    };
    messaging = {
        "host_ap_rse_mhu_pbx";
        "host_ap_rse_mhu_mbx";
        "host_rse_si_mhu_pbx";
        "host_rse_si_mhu_mbx";
        "host_ap_si_pfdi_monitor_mhu_pbx";
        "host_ap_si_pfdi_monitor_mhu_mbx";
        "si_cl1_pfdi_mhu_pbx";
    };
    translation = {
        "rse_atu_regs";
        "host_si_atu";
        "host_ap_atu";
        "host_smdexp2smd_atu";
    };
    safety_control = {
        "host_smcf_mgi";
        "si_cl0_ssu";
        "si_cl0_fmu";
        "host_scr";
        "host_system_pll";
        "host_gtimer";
        "host_css_counters_timers";
        "host_css_counters_timers_read";
        "host_css_counters_timers_sync";
    };
}

function system_mgmt.define(ctx, platform)
    platform.host_si_pik = {
        moduletype = "host_ppu";
        trace = host_ppu_trace;
        trace_limit = host_ppu_trace_limit;
        target_socket = {
            address = HOST_SI_PIK_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_si_scr = {
        moduletype = "host_scr";
        cl1_present = true;
        target_socket = {
            address = HOST_SI_SCR_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_si_atu = {
        moduletype = "rse_atu";
        trace = atu_trace;
        trace_limit = atu_trace_limit;
        trace_filter = atu_trace_filter;
        trace_address_min = atu_trace_address_min;
        trace_address_max = atu_trace_address_max;
        build_config = SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = HOST_SI_ATU_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_rse_si_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "rse_to_si_cl0";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_RSE_SI_SSRAM_PHYS_BASE;
        rx_shmem = HOST_RSE_SI_SSRAM_PHYS_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_RSE_SI_MHU_PHYS_BASE;
            size = RSE_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.host_rse_si_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_rse";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_RSE_SI_SSRAM_PHYS_BASE;
        rx_shmem = HOST_RSE_SI_SSRAM_PHYS_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_RSE_SI_MHU_PHYS_BASE + RSE_MHU_FRAME_SIZE;
            size = RSE_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&rse_cpu_pass.target_signal_socket_"..
            RSE_IRQ_SI_CL0_RSE_CMU_MHU_RECEIVER};
        log_level = 0;
    }

    platform.host_rse_si_ssram = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = HOST_RSE_SI_SSRAM_PHYS_BASE;
            size = HOST_RSE_SI_SSRAM_SIZE;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.host_ap_atu = {
        moduletype = "rse_atu";
        trace = atu_trace;
        trace_limit = atu_trace_limit;
        trace_filter = atu_trace_filter;
        trace_address_min = atu_trace_address_min;
        trace_address_max = atu_trace_address_max;
        enable_dmi = enable_ap_cpus and atu_dmi;
        build_config = SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = HOST_AP_ATU_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        translation_socket = enable_ap_cpus and {
            address = HOST_AP_ATU_LOGICAL_BASE;
            size = HOST_AP_ATU_LOGICAL_SIZE;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
            priority = 10;
        } or nil;
        initiator_socket = enable_ap_cpus and {bind = "&system_router.target_socket"} or nil;
        log_level = 0;
    }

    platform.host_ap_si_ns_scmi_mhu_pbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "ap_to_si_cl0_ns";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_NS_SCMI_TX_SHMEM_BASE;
        rx_shmem = HOST_AP_NS_SCMI_RX_SHMEM_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_NS_SCMI_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_NS_MHU_PBX_IRQ};
        log_level = 0;
    } or nil

    platform.host_ap_si_ns_scmi_mhu_mbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_ap_ns";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_NS_SCMI_TX_SHMEM_BASE;
        rx_shmem = HOST_AP_NS_SCMI_RX_SHMEM_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_NS_SCMI_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_NS_MHU_MBX_IRQ};
        log_level = 0;
    } or nil

    platform.host_ap_si_scmi_mhu_pbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "ap_to_si_cl0_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        power_domain_reset_count = AP_NUM_CPUS;
        performance_domain_count = math.floor((AP_NUM_CPUS + 3) / 4);
        power_domain_reset_delay_ns = ap_power_domain_reset_delay_ns;
        power_domain_reset_assert_on_power_off = false;
        power_domain_reset_pulse_on_power_on = true;
        system_power_reset_delay_ns = SYSTEM_POWER_RESET_DELAY_NS;
        system_power_reset_pulse_width_ns = SYSTEM_POWER_RESET_PULSE_WIDTH_NS;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_SCMI_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_SCMI_MHU_PBX_IRQ};
        system_reset = {bind = ap_system_reset_bind_targets()};
        log_level = 0;
    } or nil

    platform.host_ap_si_scmi_mhu_mbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_ap_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_SCMI_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_SCMI_MHU_MBX_IRQ};
        log_level = 0;
    } or nil

    platform.host_ap_si_cl1_mhu_pbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "apollo_ap_to_si_cl1";
        protocol = "doorbell-bridge";
        channel_count = AP_SI_CL1_MHU_CHANNEL_COUNT;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_CL1_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_CL1_MHU_PBX_IRQ};
        log_level = 0;
    } or nil

    platform.host_ap_si_cl1_mhu_mbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "apollo_si_cl1_to_ap";
        protocol = "doorbell-bridge";
        channel_count = AP_SI_CL1_MHU_CHANNEL_COUNT;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_CL1_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_CL1_MHU_MBX_IRQ};
        log_level = 0;
    } or nil

    platform.host_ap_si_pfdi_monitor_mhu_pbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "ap_to_si_cl0_pfdi";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        tx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        scmi_channel_stride = HOST_AP_SCMI_PFDI_MONITOR_STRIDE;
        scmi_channel_count = HOST_AP_SCMI_PFDI_MONITOR_CHANNELS;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_PFDI_MONITOR_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_PFDI_MHU_PBX_IRQ};
        log_level = 0;
    } or nil

    platform.host_ap_si_pfdi_monitor_mhu_mbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_ap_pfdi";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        tx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        scmi_channel_stride = HOST_AP_SCMI_PFDI_MONITOR_STRIDE;
        scmi_channel_count = HOST_AP_SCMI_PFDI_MONITOR_CHANNELS;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_PFDI_MONITOR_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_PFDI_MHU_MBX_IRQ};
        log_level = 0;
    } or nil

    platform.host_smdexp2smd_atu = {
        moduletype = "rse_atu";
        trace = atu_trace;
        trace_limit = atu_trace_limit;
        trace_filter = atu_trace_filter;
        trace_address_min = atu_trace_address_min;
        trace_address_max = atu_trace_address_max;
        build_config = SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = HOST_SMDEXP2SMD_ATU_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_reset_ctrl = {
        moduletype = "zena_reset_ctrl";
        rgm = {
            address = HOST_CSS_RGM_PHYS_BASE;
            size = SYSTEM_RESET_REGISTER_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        pik = {
            address = HOST_SYSTOP_PIK_PHYS_BASE;
            size = SYSTEM_RESET_REGISTER_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        ap_reset = {bind = "&ap_cold_reset_fanout.reset_in"};
        log_level = 0;
    }

    platform.css_system_counter = {
        moduletype = "arm_system_counter";
        input_frequency_hz = SYSTEM_COUNTER_FREQUENCY_HZ;
        integer_increment = SYSTEM_COUNTER_INTEGER_INCREMENT;
        reported_frequency_hz = SYSTEM_COUNTER_FREQUENCY_HZ;
        construction_priority = -298;
    }

    platform.host_css_counters_timers = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        counter_control = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE +
                HOST_CSS_COUNTER_CONTROL_OFFSET;
            size = HOST_CSS_COUNTER_FRAME_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_css_counters_timers_read = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        counter_read = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + HOST_CSS_COUNTER_READ_OFFSET;
            size = HOST_CSS_COUNTER_FRAME_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_css_counters_timers_sync = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        sync_frame = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + HOST_CSS_COUNTER_SYNC_OFFSET;
            size = HOST_CSS_COUNTER_FRAME_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_smd_shared_sram = {
        moduletype = "gs_memory";
        target_socket = {
            address = HOST_SMD_SHARED_SRAM_PHYS_BASE;
            size = HOST_SMD_SHARED_SRAM_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    local ap_rse_ps_proxy = getenv_bool_or("QBOX_RDASPEN_RSE_PS_PROXY", false)
    local ap_rse_mhu_protocol = ap_rse_ps_proxy and "rse-ps-proxy" or "doorbell-bridge"
    local ap_rse_mhu_pbx_pair = ap_rse_ps_proxy and "ap_rse_ps_proxy" or "ap_s_to_rse"
    local ap_rse_mhu_mbx_pair = ap_rse_ps_proxy and "ap_rse_ps_proxy" or "rse_to_ap_s"

    platform.host_ap_rse_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = ap_rse_mhu_pbx_pair;
        protocol = ap_rse_mhu_protocol;
        tx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        rx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_RSE_MHU_PHYS_BASE;
            size = MHU_V3_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.host_ap_rse_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = ap_rse_mhu_mbx_pair;
        protocol = ap_rse_mhu_protocol;
        doorbell_commit_on_notify = not ap_rse_ps_proxy;
        tx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        rx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = HOST_AP_RSE_MHU_PHYS_BASE + MHU_V3_FRAME_SIZE;
            size = MHU_V3_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.host_ap_rse_mailbox = {
        moduletype = "gs_memory";
        dmi_allow = false;
        target_socket = {
            address = HOST_AP_MHU_POINTER_ACCESS_PHYS_BASE;
            size = HOST_AP_MHU_POINTER_ACCESS_SIZE;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

end

return system_mgmt
