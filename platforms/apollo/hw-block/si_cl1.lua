local si_cl1 = {}
local SI_CL1_CPU_COUNT = 4
local SI_CL1_SRAM_BASE = 0x140000000
local SI_CL1_ENTRY = 0x14000647c
local SI_CL1_GICD_BASE = 0x30200000
local SI_CL1_GICR0_BASE = 0x30260000
local SI_CL1_GICR_STRIDE = 0x00020000
local SI_CL1_GICR_SIZE = 0x00020000
local SI_CL1_GICD_SIZE = 0x00010000
local SI_CL1_GIC_REDIST_REGIONS = {1; 1; 1; 1}
local SI_CL1_GIC_NUM_SPI = 128
local ARCH_TIMER_FREQUENCY_HZ = 100000000
local SI_CL1_AFFINITY_BASE = 0x00010000
local SI_CL1_AFFINITY_STRIDE = 0x00000100
local HOST_SI_CL1_SRAM_PHYS_BASE = 0x4000140000000
local HOST_SI_SRAM_WINDOW_SIZE = 0x01000000
local HOST_SI_CL1_CL_UTIL_BASE = 0x4000028800000
local HOST_SI_CL_UTIL_SIZE = 0x00800000
local HOST_SI_CLUS_PPU_OFFSET = 0x00010000
local HOST_SI_CONTROL_WINDOW_SIZE = 0x00010000
local SI_CL1_HIPC_SHARED_BASE = 0xE0130000
local SI_CL1_HIPC_SHARED_SIZE = 0x00080000
local SI_CL1_HIPC_ALIAS_BASE = 0x00100000
local SI_CL1_SRAM_SIZE = 0x00800000
local SI_CL1_UART_BASE = 0x2A410000
local SI_CL1_UART_SIZE = 0x00010000
local SI_CL1_SCMI_SHMEM_BASE = 0x48000000
local SI_CL1_SCMI_SHMEM_SIZE = 0x00001000
local SI_CL1_HIPC_PBX_BASE = 0x39000000
local SI_CL1_HIPC_MBX_BASE = 0x39040000
local SI_CL1_PFDI_PBX_BASE = 0x39200000
local SI_CL1_PFDI_MBX_BASE = 0x39240000
local SI_CL1_HIPC_MHU_SIZE = 0x00030000
local SI_CL1_PFDI_MHU_SIZE = 0x00020000
local SI_CL1_MHU_CHANNELS = 32
local SI_CL1_SCMI_MSG_SIZE_PER_CORE = 40
local SI_CL1_PFDI_MHU_CHANNEL_BASE = 2
local SI_CL1_PFDI_CORE_COUNT = 4
local SI_CL1_UART_POLL_INTERVAL_MS = 100

function si_cl1.hipc_shared_window()
    return SI_CL1_HIPC_SHARED_BASE, SI_CL1_HIPC_SHARED_SIZE
end

function si_cl1.define_qemu_instance(ctx, platform)
    platform.si_cl1_qemu_inst_mgr = {
        moduletype = "QemuInstanceManager";
    }
    platform.si_cl1_qemu_inst = {
        moduletype = "QemuInstance";
        args = {"&platform.si_cl1_qemu_inst_mgr", "AARCH64"};
        accel = ctx.getenv_or("QBOX_APOLLO_FULL_SI_CL1_ACCEL", "tcg");
        tcg_mode = ctx.getenv_or(
            "QBOX_APOLLO_FULL_SI_CL1_TCG_MODE",
            "MULTI");
        sync_policy = ctx.getenv_or(
            "QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY",
            "multithread-quantum");
        managed_start_in_reset_release = true;
        qemu_args = ctx.getenv_or("QBOX_APOLLO_FULL_SI_CL1_QEMU_ARGS", "");
    }
    return "&platform.si_cl1_qemu_inst"
end

function si_cl1.define_interrupt_controller(
    ctx,
    platform,
    qemu_instance)
    platform.si_cl1_gic = {
        moduletype = "arm_gicv3";
        args = {qemu_instance};
        dist_iface = {
            address = SI_CL1_GICD_BASE;
            size = SI_CL1_GICD_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        redist_region = SI_CL1_GIC_REDIST_REGIONS;
        num_cpus = SI_CL1_CPU_COUNT;
        num_spi = SI_CL1_GIC_NUM_SPI;
    }
end

function si_cl1.define_loader_and_cpus(ctx, platform, qemu_instance, image)
    local gic_name = "si_cl1_gic"
    local gic = platform[gic_name]
    local irq_routes = ctx.modules.si_cl0

    platform.si_cl1_loader = {
        moduletype = "loader";
        request_origin_id = ctx.request_context.origin.si_cl1_loader;
        request_domain_id = ctx.request_context.domain.si_cl1;
        request_capabilities = ctx.request_context.capability.authenticated_image;
        request_secure = true;
        request_secure_valid = true;
        initiator_socket = {bind = "&si_cl1_router.target_socket"};
        { bin_file = image, address = SI_CL1_SRAM_BASE };
    }

    for i=0,(SI_CL1_CPU_COUNT-1) do
        local gic_cpu = i
        platform["si_cl1_cpu_"..i] = {
            moduletype = "cpu_arm_cortexR82";
            args = {qemu_instance};
            mem = {bind = "&si_cl1_router.target_socket"};
            has_el2 = true;
            psci_conduit = "smc";
            start_powered_off = false;
            start_in_reset = true;
            reset_power_on = true;
            rvbar = SI_CL1_ENTRY;
            mp_affinity = SI_CL1_AFFINITY_BASE +
                (i * SI_CL1_AFFINITY_STRIDE);
            cntfrq_hz = ARCH_TIMER_FREQUENCY_HZ;
            request_origin_id = ctx.request_context.origin.si_cl1_cpu_base + i;
            request_domain_id = ctx.request_context.domain.si_cl1;
            requester_id = i;
            irq_timer_sec_out = {
                bind = "&"..gic_name..".ppi_in_cpu_"..
                    gic_cpu.."_"..
                    irq_routes.ppi_index(ctx, "si_secure_timer");
            };
            irq_timer_phys_out = {
                bind = "&"..gic_name..".ppi_in_cpu_"..
                    gic_cpu.."_"..
                    irq_routes.ppi_index(ctx, "si_physical_timer");
            };
            irq_timer_virt_out = {
                bind = "&"..gic_name..".ppi_in_cpu_"..
                    gic_cpu.."_"..
                    irq_routes.ppi_index(ctx, "si_virtual_timer");
            };
            irq_timer_hyp_out = {
                bind = "&"..gic_name..".ppi_in_cpu_"..
                    gic_cpu.."_"..
                    irq_routes.ppi_index(ctx, "si_hypervisor_timer");
            };
        }
        platform["si_cl1_cpu_counter_mirror_"..i] = {
            moduletype = "qemu_arm_counter_mirror";
            args = {
                "&platform.si_cl1_cpu_"..i;
                "&platform.css_system_counter";
            };
        }
        gic["redist_iface_"..i] = {
            address = SI_CL1_GICR0_BASE + (i * SI_CL1_GICR_STRIDE);
            size = SI_CL1_GICR_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        }
        for _, signal in ipairs({"irq", "fiq", "virq", "vfiq"}) do
            gic[signal.."_out_"..gic_cpu] = {
                bind = "&si_cl1_cpu_"..i.."."..signal.."_in";
            }
        end
    end
end

function si_cl1.finalize_reset_order(platform)
    local system_reset = platform.apollo_system_reset_fanout
    if system_reset == nil then
        return
    end

    local reset_targets = system_reset.reset_out.bind
    local cl0_count
    reset_targets, cl0_count = string.gsub(
        reset_targets,
        "&si_cl0_qemu_inst%.reset;?",
        "")
    local cl1_count
    reset_targets, cl1_count = string.gsub(
        reset_targets,
        "&si_cl1_qemu_inst%.reset;?",
        "")
    assert(
        cl0_count == 1 and cl1_count == 1,
        "split SI reset routing requires one QEMU target per cluster")

    local ordered_targets = {
        "&si_gic_multiview.reset";
        "&si_cl0_qemu_inst.reset";
        "&si_cl1_qemu_inst.reset";
        "&ap_rgic2lgic_messreg.reset";
        reset_targets;
    }
    system_reset.reset_out.bind = table.concat(ordered_targets, ";")
end

function si_cl1.define(ctx, platform)
    platform.host_si_cl1_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_si_sram_dmi;
        target_socket = {
            address = HOST_SI_CL1_SRAM_PHYS_BASE;
            size = HOST_SI_SRAM_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        map_file = host_si_cl1_sram_map_file;
        shared_memory = host_sram_shared_memory_enabled(host_si_cl1_sram_map_file);
        shared_memory_prefix = "ra-si1-";
        init_mem = host_si_cl1_sram_map_file == "";
        log_level = 0;
    }

    platform.host_si_cl1_cub = {
        moduletype = "gs_memory";
        target_socket = {
            address = HOST_SI_CL1_CL_UTIL_BASE;
            size = HOST_SI_CL_UTIL_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 20;
        };
        init_mem = true;
        log_level = 0;
    }

    platform.host_si_cl1_clus_ppu = {
        moduletype = "host_ppu";
        trace = host_ppu_trace;
        trace_limit = host_ppu_trace_limit;
        assert_power_on_load = true;
        power_on_load = {bind = "&si_cl1_loader.reset"};
        power_on_load_pulse_width_ns = 0;
        target_socket = {
            address = HOST_SI_CL1_CL_UTIL_BASE + HOST_SI_CLUS_PPU_OFFSET;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
            priority = 10;
        };
        log_level = 0;
    }

end

function si_cl1.enable(ctx, platform)
    print("Apollo FVP live SI CL1 block enabled...")

    platform.si_cl1_router = {
        moduletype = "router";
        log_level = 0;
    }

    platform.si_cl1_hipc_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = SI_CL1_HIPC_ALIAS_BASE;
        target_socket = {
            address = SI_CL1_HIPC_SHARED_BASE;
            size = SI_CL1_HIPC_SHARED_SIZE;
            bind = "&si_cl1_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    local si_cl1_image = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_IMAGE",
        ctx.apollo_root.."build/local-apollo-fvp/deploy/firmware/zephyr-demos-cl1.bin")
    local si_cl1_log = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_LOG",
        ctx.apollo_root.."build/qbox-apollo-fvp/full-system/qbox-safety-island-cl1.log")
    local si_cl1_uart_read_file = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_UART_READ_FILE",
        "/dev/null")
    local si_cl1_uart_poll_read = si_cl1_uart_read_file ~= "/dev/null"
    local mhu_trace = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_CL1_MHU_TRACE", false)
    local mhu_trace_file = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_MHU_TRACE_FILE",
        ctx.apollo_root.."build/qbox-apollo-fvp/full-system/si-cl1-mhuv3-trace.log")
    local mhu_trace_limit =
        ctx.getenv_number_or("QBOX_APOLLO_FULL_SI_CL1_MHU_TRACE_LIMIT", "4096")

    -- CL1 CPU backend
    local si_cl1_qemu_instance = si_cl1.define_qemu_instance(ctx, platform)

    -- CL1 memory
    platform.si_cl1_sram = {
        moduletype = "gs_memory";
        dmi = true;
        target_socket = {
            address = SI_CL1_SRAM_BASE;
            size = SI_CL1_SRAM_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.si_cl1_scmi_shmem = {
        moduletype = "gs_memory";
        target_socket = {
            address = SI_CL1_SCMI_SHMEM_BASE;
            size = SI_CL1_SCMI_SHMEM_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    -- CL1 interrupt controller
    si_cl1.define_interrupt_controller(
        ctx,
        platform,
        si_cl1_qemu_instance)
    local irq_routes = ctx.modules.si_cl0

    -- CL1 console
    platform.si_cl1_console_file = {
        moduletype = "char_backend_file";
        read_file = si_cl1_uart_read_file;
        write_file = si_cl1_log;
        poll_read = si_cl1_uart_poll_read;
        poll_interval_ms = SI_CL1_UART_POLL_INTERVAL_MS;
        baudrate = 0;
    }

    platform.si_cl1_uart = {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = {
            address = SI_CL1_UART_BASE;
            size = SI_CL1_UART_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        irq = {bind = irq_routes.spi_target(
            ctx, "si_cl1_uart", "View2")};
        backend_socket = {bind = "&si_cl1_console_file.biflow_socket"};
    }

    -- CL1 HIPC and PFDI MHU frames
    platform.si_cl1_hipc_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "apollo_si_cl1_to_ap";
        protocol = "doorbell-bridge";
        channel_count = SI_CL1_MHU_CHANNELS;
        trace = mhu_trace;
        trace_file = mhu_trace_file;
        trace_limit = mhu_trace_limit;
        target_socket = {
            address = SI_CL1_HIPC_PBX_BASE;
            size = SI_CL1_HIPC_MHU_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl1_router.target_socket"};
        irq = {bind = irq_routes.spi_target(
            ctx, "si_cl1_hipc_pbx", "View2")};
        log_level = 0;
    }

    platform.si_cl1_hipc_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "apollo_ap_to_si_cl1";
        protocol = "doorbell-bridge";
        channel_count = SI_CL1_MHU_CHANNELS;
        trace = mhu_trace;
        trace_file = mhu_trace_file;
        trace_limit = mhu_trace_limit;
        target_socket = {
            address = SI_CL1_HIPC_MBX_BASE;
            size = SI_CL1_HIPC_MHU_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl1_router.target_socket"};
        irq = {bind = irq_routes.spi_target(
            ctx, "si_cl1_hipc_mbx", "View2")};
        log_level = 0;
    }

    platform.si_cl1_pfdi_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "apollo_si_cl1_pfdi";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        channel_count = SI_CL1_MHU_CHANNELS;
        tx_shmem = SI_CL1_SCMI_SHMEM_BASE;
        rx_shmem = SI_CL1_SCMI_SHMEM_BASE;
        scmi_channel_stride = SI_CL1_SCMI_MSG_SIZE_PER_CORE;
        scmi_channel_base_index = SI_CL1_PFDI_MHU_CHANNEL_BASE;
        scmi_channel_count = SI_CL1_PFDI_CORE_COUNT;
        init_shmem = true;
        requester_hold_enable = true;
        trace = mhu_trace;
        trace_file = mhu_trace_file;
        trace_limit = mhu_trace_limit;
        target_socket = {
            address = SI_CL1_PFDI_PBX_BASE;
            size = SI_CL1_PFDI_MHU_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl1_router.target_socket"};
        irq = {bind = irq_routes.spi_target(
            ctx, "si_cl1_pfdi", "View2")};
        log_level = 0;
    }

    for i=0,(SI_CL1_PFDI_CORE_COUNT-1) do
        platform.si_cl1_pfdi_mhu_pbx["requester_hold_"..i] = {
            bind = "&si_cl1_cpu_"..i..".sync_hold";
        }
    end

    platform.si_cl1_pfdi_reply_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "apollo_si_cl0_pfdi_reply";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        channel_count = SI_CL1_MHU_CHANNELS;
        tx_shmem = SI_CL1_SCMI_SHMEM_BASE;
        rx_shmem = SI_CL1_SCMI_SHMEM_BASE;
        scmi_channel_stride = SI_CL1_SCMI_MSG_SIZE_PER_CORE;
        scmi_channel_base_index = SI_CL1_PFDI_MHU_CHANNEL_BASE;
        scmi_channel_count = SI_CL1_PFDI_CORE_COUNT;
        init_shmem = false;
        trace = mhu_trace;
        trace_file = mhu_trace_file;
        trace_limit = mhu_trace_limit;
        target_socket = {
            address = SI_CL1_PFDI_MBX_BASE;
            size = SI_CL1_PFDI_MHU_SIZE;
            bind = "&si_cl1_router.initiator_socket";
        };
        initiator_socket = {bind = "&si_cl1_router.target_socket"};
        log_level = 0;
    }

    si_cl1.define_loader_and_cpus(
        ctx,
        platform,
        si_cl1_qemu_instance,
        si_cl1_image)

    si_cl1.finalize_reset_order(platform)

    print("si-cl1 image: "..si_cl1_image)
    print("si-cl1 log:   "..si_cl1_log)
    print("si-cl1 entry: 0x"..string.format("%x", SI_CL1_ENTRY))
end

return si_cl1
