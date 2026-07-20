local system_mgmt = {}

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
        build_config = 0x000000C5;
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
        build_config = 0x000000C5;
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
        pair = ctx.apollo_live_cl0 and "ap_to_si_cl0_ns" or "ap_si_ns_scmi";
        protocol = ctx.apollo_live_cl0 and "doorbell-bridge" or "scmi";
        tx_shmem = 0x00180000;
        rx_shmem = 0x00180100;
        init_shmem = not ctx.apollo_live_cl0;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x400003B000000;
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
        pair = ctx.apollo_live_cl0 and "si_cl0_to_ap_ns" or "ap_si_ns_scmi";
        protocol = ctx.apollo_live_cl0 and "doorbell-bridge" or "scmi";
        tx_shmem = 0x00180000;
        rx_shmem = 0x00180100;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x400003B040000;
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
        pair = ctx.apollo_live_cl0 and "ap_to_si_cl0_scmi" or "ap_si_scmi";
        protocol = ctx.apollo_live_cl0 and "doorbell-bridge" or "scmi";
        tx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = not ctx.apollo_live_cl0;
        power_domain_reset_count = AP_NUM_CPUS;
        power_domain_reset_delay_ns = ap_power_domain_reset_delay_ns;
        power_domain_reset_assert_on_power_off = false;
        power_domain_reset_pulse_on_power_on = true;
        system_power_reset_delay_ns = 1;
        system_power_reset_pulse_width_ns = 1;
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

    if platform.host_ap_si_scmi_mhu_pbx ~= nil and
       not ctx.apollo_live_cl0 then
        for i=1,(AP_NUM_CPUS-1) do
            platform.host_ap_si_scmi_mhu_pbx["power_domain_reset_"..i] = {
                bind = "&ap_cpu_"..i..".reset";
            }
        end
    end

    platform.host_ap_si_scmi_mhu_mbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = ctx.apollo_live_cl0 and "si_cl0_to_ap_scmi" or "ap_si_scmi";
        protocol = ctx.apollo_live_cl0 and "doorbell-bridge" or "scmi";
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
        pair = ctx.apollo_live_cl1 and "apollo_ap_to_si_cl1" or "ap_si_cl1";
        protocol = ctx.apollo_live_cl1 and "doorbell-bridge" or "doorbell";
        channel_count = 32;
        doorbell_ack_trigger_channel = not ctx.apollo_live_cl1 and 0 or nil;
        doorbell_ack_trigger_value = not ctx.apollo_live_cl1 and 0x8 or nil;
        doorbell_ack_channel = not ctx.apollo_live_cl1 and 0 or nil;
        doorbell_ack_value = not ctx.apollo_live_cl1 and 0x4 or nil;
        -- si_cl1_rproc_rsctbl@0x00100000 from the AP DTS HIPC layout.
        doorbell_ack_seed_address =
            not ctx.apollo_live_cl1 and 0x00100000 or nil;
        doorbell_ack_seed_words = not ctx.apollo_live_cl1 and {
            0x00000001, 0x00000001, 0x00000000, 0x00000000,
            0x00000014, 0x00000003, 0x00000007, 0x00000000,
            0x00000001, 0x00000000, 0x00000000, 0x00000200,
            0xffffffff, 0x00000010, 0x00000020, 0x00000000,
            0x00000000, 0xffffffff, 0x00000010, 0x00000020,
            0x00000001, 0x00000000,
        } or nil;
        rpmsg_ns_enable = not ctx.apollo_live_cl1;
        rpmsg_ns_name = "ethsi1";
        rpmsg_ns_remote_addr = 0x400;
        -- si_cl1_vdev0vring0@0x00120000 from the AP DTS HIPC layout.
        rpmsg_ns_vring_address = 0x00120000;
        rpmsg_ns_vring_num = 0x20;
        rpmsg_ns_vring_align = 0x10;
        rpmsg_ns_signal_channel = 0;
        rpmsg_ns_signal_value = 0x1;
        rpmsg_ns_signal_delay_ns = 1000000;
        rpmsg_ns_poll_period_ns = 100000;
        rpmsg_ns_max_polls = 10000;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x400003B100000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_120"};
        log_level = 0;
    } or nil

    platform.host_ap_si_cl1_mhu_mbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = ctx.apollo_live_cl1 and "apollo_si_cl1_to_ap" or "ap_si_cl1";
        protocol = ctx.apollo_live_cl1 and "doorbell-bridge" or "doorbell";
        channel_count = 32;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = 0x400003B140000;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_121"};
        log_level = 0;
    } or nil

    platform.host_ap_si_pfdi_monitor_mhu_pbx = enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = ctx.apollo_live_cl0 and "ap_to_si_cl0_pfdi" or "ap_si_pfdi_monitor";
        protocol = ctx.apollo_live_cl0 and "doorbell-bridge" or "scmi";
        scmi_transport = "pfdi-monitor";
        tx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        scmi_channel_stride = HOST_AP_SCMI_PFDI_MONITOR_STRIDE;
        scmi_channel_count = HOST_AP_SCMI_PFDI_MONITOR_CHANNELS;
        init_shmem = not ctx.apollo_live_cl0;
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
        pair = ctx.apollo_live_cl0 and "si_cl0_to_ap_pfdi" or "ap_si_pfdi_monitor";
        protocol = ctx.apollo_live_cl0 and "doorbell-bridge" or "scmi";
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
        build_config = 0x000000C5;
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
            size = 0x00001000;
            bind = "&smd_router.initiator_socket";
        };
        pik = {
            address = HOST_SYSTOP_PIK_PHYS_BASE;
            size = 0x00001000;
            bind = "&smd_router.initiator_socket";
        };
        ap_reset = {bind = "&ap_cold_reset_fanout.reset_in"};
        log_level = 0;
    }

    platform.host_css_counters_timers = {
        moduletype = "host_gtimer";
        counter_control = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE;
            size = 0x00010000;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_css_counters_timers_read = {
        moduletype = "host_gtimer";
        counter_read = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + 0x00010000;
            size = 0x00010000;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.host_css_counters_timers_sync = {
        moduletype = "host_gtimer";
        sync_frame = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + 0x00020000;
            size = 0x00010000;
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
