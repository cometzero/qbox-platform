local si_cl1 = {}
local SI_CL1_CPU_COUNT = 4
local SI_CL1_SRAM_BASE = 0x140000000
local SI_CL1_ENTRY = 0x14000647c
local SI_CL1_GICD_BASE = 0x30200000
local SI_CL1_GICR0_BASE = 0x30260000
local SI_CL1_GICR_STRIDE = 0x00020000
local SI_CL1_GICR_SIZE = 0x00020000
local ARCH_TIMER_FREQUENCY_HZ = 100000000

function si_cl1.define_qemu_instance(ctx, platform)
    if ctx.config.si.single_gic then
        assert(
            platform.si_qemu_inst_mgr ~= nil and
                platform.si_qemu_inst ~= nil,
            "single SI QEMU lifecycle must be defined by CL0 first")
        return "&platform.si_qemu_inst"
    end

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
    if ctx.config.si.single_gic then
        assert(
            platform.si_cl0_gic ~= nil and
                platform.si_cl0_gic.args[1] == qemu_instance,
            "canonical SI GIC must be defined by CL0 first")
        return
    end

    platform.si_cl1_gic = {
        moduletype = "arm_gicv3";
        args = {qemu_instance};
        dist_iface = {
            address = SI_CL1_GICD_BASE;
            size = 0x00010000;
            bind = "&si_cl1_router.initiator_socket";
        };
        redist_region = {1, 1, 1, 1};
        num_cpus = SI_CL1_CPU_COUNT;
        num_spi = 128;
    }
end

function si_cl1.define_cpu_reset_hooks(ctx, platform)
    if not ctx.config.si.single_gic then
        return
    end
    for cpu=0,(SI_CL1_CPU_COUNT - 1) do
        platform["si_cl1_cpu_"..cpu.."_reset"] = {
            moduletype = "qemu_device_cold_reset";
            args = {"&platform.si_cl1_cpu_"..cpu};
        }
    end
end

function si_cl1.define_loader_and_cpus(ctx, platform, qemu_instance, image)
    local gic_name =
        ctx.config.si.single_gic and "si_cl0_gic" or "si_cl1_gic"
    local first_gic_cpu = ctx.config.si.single_gic and 1 or 0
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
        local gic_cpu = first_gic_cpu + i
        platform["si_cl1_cpu_"..i] = {
            moduletype = "cpu_arm_cortexR82";
            args = {qemu_instance};
            construction_priority =
                ctx.config.si.single_gic and (-199 + i) or nil;
            mem = {bind = "&si_cl1_router.target_socket"};
            has_el2 = true;
            psci_conduit = "smc";
            start_powered_off = false;
            start_in_reset = true;
            reset_power_on = true;
            rvbar = SI_CL1_ENTRY;
            mp_affinity = 0x10000 + (i * 0x100);
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
        if not ctx.config.si.single_gic then
            gic["redist_iface_"..i] = {
                address = SI_CL1_GICR0_BASE + (i * SI_CL1_GICR_STRIDE);
                size = SI_CL1_GICR_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            }
        end
        for _, signal in ipairs({"irq", "fiq", "virq", "vfiq"}) do
            if ctx.config.si.single_gic then
                gic[signal.."_out_"..gic_cpu] = {
                    bind = "&si_gic_power_bridge."..signal..
                        "_in_"..gic_cpu;
                }
                platform.si_gic_power_bridge[
                    signal.."_out_"..gic_cpu] = {
                    bind = "&si_cl1_cpu_"..i.."."..signal.."_in";
                }
            else
                gic[signal.."_out_"..gic_cpu] = {
                    bind = "&si_cl1_cpu_"..i.."."..signal.."_in";
                }
            end
        end
    end
end

function si_cl1.validate_five_pe_topology(ctx, platform)
    if not ctx.config.si.single_gic then
        return {}
    end

    local cl0_loader = platform.si_cl0_loader
    local cl1_loader = platform.si_cl1_loader
    assert(
        cl0_loader.initiator_socket.bind == "&si_cl0_router.target_socket" and
            cl0_loader[1].address == 0x120000000,
        "CL0 image must target only the CL0 address space")
    assert(
        cl1_loader.initiator_socket.bind == "&si_cl1_router.target_socket" and
            cl1_loader[1].address == SI_CL1_SRAM_BASE,
        "CL1 image must target only the CL1 address space")
    assert(
        cl0_loader[1].bin_file ~= cl1_loader[1].bin_file,
        "CL1 image must not be assigned to CPU0")

    local cpu_names = {"si_cl0_cpu_0"}
    for i=0,(SI_CL1_CPU_COUNT-1) do
        cpu_names[#cpu_names + 1] = "si_cl1_cpu_"..i
    end
    local affinities = {}
    for _, name in ipairs(cpu_names) do
        local affinity = platform[name].mp_affinity
        assert(affinities[affinity] == nil, "duplicate MPIDR: "..affinity)
        affinities[affinity] = true
    end

    local pes = {}
    local function validate_cpu(
        name,
        cluster,
        local_cpu,
        affinity,
        entry,
        router,
        image,
        reset)
        local cpu = platform[name]
        assert(cpu.moduletype == "cpu_arm_cortexR82", name.." must be Cortex-R82")
        assert(
            cpu.args[1] == "&platform.si_qemu_inst",
            name.." must use the shared SI QEMU instance")
        assert(cpu.mp_affinity == affinity, name.." has incorrect MPIDR")
        assert(cpu.rvbar == entry, name.." has incorrect reset vector")
        assert(cpu.mem.bind == router, name.." has incorrect address-space router")
        assert(
            cpu.request_domain_id == ctx.request_context.domain[cluster],
            name.." has incorrect request domain")
        assert(
            cpu.request_origin_id ==
                ctx.request_context.origin[cluster.."_cpu_base"] + local_cpu,
            name.." has incorrect request origin")
        assert(cpu.requester_id == local_cpu, name.." has incorrect requester ID")
        assert(
            platform[name.."_reset"].args[1] == reset,
            name.." has incorrect reset binding")

        local gic_cpu = #pes
        local gic = platform.si_cl0_gic
        local power_bridge = platform.si_gic_power_bridge
        for _, signal in ipairs({"irq", "fiq", "virq", "vfiq"}) do
            assert(
                gic[signal.."_out_"..gic_cpu].bind ==
                    "&si_gic_power_bridge."..signal.."_in_"..gic_cpu and
                    power_bridge[signal.."_out_"..gic_cpu].bind ==
                        "&"..name.."."..signal.."_in",
                name.." has incorrect "..signal.." binding")
        end
        local timer_bindings = {
            sec = ctx.modules.si_cl0.ppi_index(ctx, "si_secure_timer");
            phys = ctx.modules.si_cl0.ppi_index(ctx, "si_physical_timer");
            virt = ctx.modules.si_cl0.ppi_index(ctx, "si_virtual_timer");
            hyp = ctx.modules.si_cl0.ppi_index(ctx, "si_hypervisor_timer");
        }
        for timer, ppi in pairs(timer_bindings) do
            assert(
                cpu["irq_timer_"..timer.."_out"].bind ==
                    "&si_cl0_gic.ppi_in_cpu_"..gic_cpu.."_"..ppi,
                name.." has incorrect "..timer.." timer binding")
        end

        pes[#pes + 1] = {
            pe = #pes;
            name = name;
            cluster = cluster;
            mp_affinity = affinity;
            image = image;
            router = router;
            reset = reset;
            rvbar = entry;
        }
    end

    validate_cpu(
        "si_cl0_cpu_0",
        "si_cl0",
        0,
        0x0,
        0x120000000,
        "&si_cl0_ni710ae_primary_nci.protected_target_socket",
        cl0_loader[1].bin_file,
        "&platform.si_cl0_cpu_0")
    for i=0,(SI_CL1_CPU_COUNT-1) do
        validate_cpu(
            "si_cl1_cpu_"..i,
            "si_cl1",
            i,
            0x10000 + (i * 0x100),
            SI_CL1_ENTRY,
            "&si_cl1_router.target_socket",
            cl1_loader[1].bin_file,
            "&platform.si_cl1_cpu_"..i)
    end
    assert(#pes == 5, "single SI QEMU instance requires exactly five PEs")
    return pes
end

function si_cl1.validate_canonical_gic(platform)
    if platform.si_qemu_inst == nil then
        return {}
    end

    local gic_count = 0
    for name, module in pairs(platform) do
        if name:match("^si_.*gic$") and
            module.moduletype == "arm_gicv3" then
            gic_count = gic_count + 1
        end
    end
    assert(
        gic_count == 1 and platform.si_cl1_gic == nil,
        "canonical SI GIC realize error: exactly one SI GIC is required")

    local gic = platform.si_cl0_gic
    assert(
        gic ~= nil and gic.args[1] == "&platform.si_qemu_inst",
        "canonical SI GIC realize error: GIC must use si_qemu_inst")
    assert(
        #gic.redist_region == 2 and
            gic.redist_region[1] == 1 and
            gic.redist_region[2] == 4,
        "canonical SI GIC realize error: redistributor regions must be {1,4}")
    local redistributor_count =
        gic.redist_region[1] + gic.redist_region[2]
    assert(
        redistributor_count == 5 and gic.num_cpus == 5,
        "canonical SI GIC realize error: five CPU interfaces are required")
    assert(
        gic.num_spi == 960,
        "canonical SI GIC realize error: 960 normal SPIs are required")

    local multiview = platform.si_gic_multiview
    assert(
        multiview ~= nil and
            multiview.backend_socket.bind ==
                "&si_gic_power_bridge.target_socket" and
            multiview.backend_redist_count == 5,
        "canonical SI GIC realize error: multiview must use one backend socket")
    local power_bridge = platform.si_gic_power_bridge
    assert(
        power_bridge ~= nil and
            power_bridge.moduletype == "gic720ae_power_bridge" and
            power_bridge.redistributor_count == 5 and
            power_bridge.backend_socket.bind ==
                "&si_cl0_router.target_socket",
        "canonical SI GIC realize error: power bridge is miswired")
    assert(
        multiview.view1_dist.bind ==
            "&si_cl0_router.initiator_socket" and
            multiview.view2_dist.bind ==
                "&si_cl1_router.initiator_socket",
        "canonical SI GIC realize error: view frontends are misrouted")
    local multiview_shadow_count = 0
    for name, _ in pairs(multiview) do
        if name:match("^pending") or name:match("^active") or
            name:match("^enable") or name:match("^priority") then
            multiview_shadow_count = multiview_shadow_count + 1
        end
    end
    assert(
        multiview_shadow_count == 0,
        "canonical SI GIC realize error: multiview state clone is forbidden")

    local cpu_names = {"si_cl0_cpu_0"}
    for cpu=0,(SI_CL1_CPU_COUNT - 1) do
        cpu_names[#cpu_names + 1] = "si_cl1_cpu_"..cpu
    end
    for cpu, name in ipairs(cpu_names) do
        local gic_cpu = cpu - 1
        assert(
            platform[name].args[1] == gic.args[1],
            "canonical SI GIC realize error: "..name..
                " is outside the GIC QEMU instance")
        for _, signal in ipairs({"irq", "fiq", "virq", "vfiq"}) do
            assert(
                gic[signal.."_out_"..gic_cpu].bind ==
                    "&si_gic_power_bridge."..signal.."_in_"..gic_cpu and
                    power_bridge[signal.."_out_"..gic_cpu].bind ==
                        "&"..name.."."..signal.."_in",
                "canonical SI GIC realize error: "..name..
                    " has an invalid "..signal.." CPU interface")
        end
    end

    local forbidden_symbol_count = platform.si_cl1_gic == nil and 0 or 1
    for name, _ in pairs(gic) do
        if name:match("^pending") or name:match("^active") or
            name:match("^enable") or name:match("^priority") then
            forbidden_symbol_count = forbidden_symbol_count + 1
        end
    end
    assert(
        forbidden_symbol_count == 0,
        "canonical SI GIC realize error: SystemC shadow state is forbidden")

    return {
        gic_count = gic_count;
        redistributor_count = redistributor_count;
        cpu_interface_count = gic.num_cpus;
        normal_spi_count = gic.num_spi;
        forbidden_symbol_count = forbidden_symbol_count;
        multiview_backend_socket_count = 1;
        multiview_shadow_count = multiview_shadow_count;
    }
end

function si_cl1.finalize_single_instance(ctx, platform)
    if ctx.config.si.single_gic then
        assert(
            platform.si_qemu_inst.tcg_mode == ctx.config.si.tcg_mode,
            "shared SI QEMU TCG policy changed after configuration")
        assert(
            platform.si_qemu_inst.sync_policy == ctx.config.si.sync_policy,
            "shared SI QEMU sync policy changed after configuration")
        assert(
            platform.si_cl0_cpu_0_reset.args[1] ==
                "&platform.si_cl0_cpu_0",
            "si_cl0_cpu_0_reset must target only si_cl0_cpu_0")
        for cpu=0,(SI_CL1_CPU_COUNT - 1) do
            local hook = "si_cl1_cpu_"..cpu.."_reset"
            local target = "&platform.si_cl1_cpu_"..cpu
            assert(
                platform[hook].args[1] == target,
                hook.." must target only si_cl1_cpu_"..cpu)
        end
    end

    local system_reset = platform.apollo_system_reset_fanout
    if system_reset == nil then
        return
    end
    local reset_targets = system_reset.reset_out.bind
    local ordered_targets = {"&si_gic_multiview.reset"}
    if ctx.config.si.single_gic then
        ordered_targets[#ordered_targets + 1] =
            "&si_gic_power_bridge.reset"
        local cl1_targets = {}
        for cpu=0,(SI_CL1_CPU_COUNT - 1) do
            cl1_targets[#cl1_targets + 1] =
                "&si_cl1_cpu_"..cpu.."_reset.reset"
        end
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
            "single SI reset routing requires one legacy reset target per cluster")
        ordered_targets[#ordered_targets + 1] = "&si_gic_reset.reset"
        ordered_targets[#ordered_targets + 1] =
            "&ap_rgic2lgic_messreg.reset"
        ordered_targets[#ordered_targets + 1] =
            "&si_cl0_cpu_0_reset.reset"
        ordered_targets[#ordered_targets + 1] =
            table.concat(cl1_targets, ";")
    else
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
        ordered_targets[#ordered_targets + 1] = "&si_cl0_qemu_inst.reset"
        ordered_targets[#ordered_targets + 1] = "&si_cl1_qemu_inst.reset"
        ordered_targets[#ordered_targets + 1] =
            "&ap_rgic2lgic_messreg.reset"
    end
    ordered_targets[#ordered_targets + 1] = reset_targets
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
        mapped_base_addr = 0x00100000;
        target_socket = {
            address = ctx.APOLLO_SI_CL1_HIPC_SHARED_BASE;
            size = ctx.APOLLO_SI_CL1_HIPC_SHARED_SIZE;
            bind = "&si_cl1_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    local SI_CL1_SRAM_SIZE = 0x00800000
    local SI_CL1_UART_BASE = 0x2a410000
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
        poll_interval_ms = 100;
        baudrate = 0;
    }

    platform.si_cl1_uart = {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = {
            address = SI_CL1_UART_BASE;
            size = 0x00010000;
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
        scmi_channel_count = 4;
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

    for i=0,3 do
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
        scmi_channel_count = 4;
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

    si_cl1.define_cpu_reset_hooks(ctx, platform)
    si_cl1.validate_five_pe_topology(ctx, platform)
    si_cl1.validate_canonical_gic(platform)
    si_cl1.finalize_single_instance(ctx, platform)

    print("si-cl1 image: "..si_cl1_image)
    print("si-cl1 log:   "..si_cl1_log)
    print("si-cl1 entry: 0x"..string.format("%x", SI_CL1_ENTRY))
end

return si_cl1
