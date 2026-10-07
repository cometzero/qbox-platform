local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local AP_NUM_CPUS = ctx.options.AP_NUM_CPUS
    local ap_cpu_index = ctx.options.ap_cpu_index
    local enable_ap_cpus = ctx.options.enable_ap_cpus
    local host_ppu_trace = ctx.options.host_ppu_trace
    local host_ppu_trace_limit = ctx.options.host_ppu_trace_limit
    local SI_CONST = hardware

    local block = {}

    function block.define_host(ctx, platform)
        assert(
            platform.host_si_cl0_clus_ppu == nil,
            "duplicate object: platform.host_si_cl0_clus_ppu")
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

        assert(
            platform.host_si_cl0_core0_ppu == nil,
            "duplicate object: platform.host_si_cl0_core0_ppu")
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

    function block.define(ctx, platform)
        -- CL1 and AP power/reset control
        assert(
            platform.si_cl0_sys0_ppu == nil,
            "duplicate object: platform.si_cl0_sys0_ppu")
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

        assert(
            platform.si_cl1_cluster_ppu == nil,
            "duplicate object: platform.si_cl1_cluster_ppu")
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

        for i = 0,(SI_CONST.SI_CL1_CORE_PPU_COUNT - 1) do
            assert(
                platform["si_cl1_core" .. i .. "_ppu"] == nil,
                "duplicate object: platform." .. ("si_cl1_core" .. i .. "_ppu"))
            platform["si_cl1_core" .. i .. "_ppu"] = {
                moduletype = "host_ppu";
                initial_power_status = 0x0;
                assert_power_on_reset = true;
                power_on_reset = {
                    bind = "&si_cl1_cpu_" .. i .. ".reset";
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

        assert(
            platform.si_cl1_ppu_ae == nil,
            "duplicate object: platform.si_cl1_ppu_ae")
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

        for cluster = 0,(SI_CONST.SI_CL0_AP_CLUSTER_COUNT - 1) do
            local cluster_base = SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_BASE +
                (cluster * SI_CONST.SI_CL0_CLUSTER_UTILITY_STRIDE)

            assert(
                platform["si_cl0_ap_cluster" .. cluster .. "_ppu"] == nil,
                "duplicate object: platform." .. ("si_cl0_ap_cluster" .. cluster .. "_ppu"))
            platform["si_cl0_ap_cluster" .. cluster .. "_ppu"] = {
                moduletype = "host_ppu";
                initial_power_status = 0x0;
                num_active_inputs = SI_CONST.SI_CL0_AP_CORE_PER_CLUSTER_COUNT;
                trace = host_ppu_trace;
                trace_limit = host_ppu_trace_limit;
                irq = {bind = "&si_cl0_gic.spi_in_" .. (228 + 5 * cluster)};
                target_socket = {
                    address = cluster_base + SI_CONST.SI_CL0_AP_CLUSTER_PPU_OFFSET;
                    size = SI_CONST.SI_CL_PPU_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                };
                log_level = 0;
            }
            assert(
                platform["si_cl0_ap_cluster" .. cluster .. "_ae"] == nil,
                "duplicate object: platform." .. ("si_cl0_ap_cluster" .. cluster .. "_ae"))
            platform["si_cl0_ap_cluster" .. cluster .. "_ae"] = {
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

            assert(
                platform["si_cl0_ap_cluster" .. cluster .. "_control"] == nil,
                "duplicate object: platform." .. ("si_cl0_ap_cluster" .. cluster .. "_control"))
            platform["si_cl0_ap_cluster" .. cluster .. "_control"] = {
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

            for core = 0,(SI_CONST.SI_CL0_AP_CORE_PER_CLUSTER_COUNT - 1) do
                local cpu_index = ap_cpu_index(cluster, core)
                local cpu_active = enable_ap_cpus and cpu_index < AP_NUM_CPUS
                assert(
                    platform["si_cl0_ap_cluster" .. cluster .. "_core" .. core .. "_ppu"] == nil,
                    "duplicate object: platform." .. ("si_cl0_ap_cluster" .. cluster .. "_core" .. core .. "_ppu"))
                platform["si_cl0_ap_cluster" .. cluster .. "_core" .. core .. "_ppu"] = {
                    moduletype = "host_ppu";
                    initial_power_status = 0x0;
                    trace = host_ppu_trace;
                    trace_limit = host_ppu_trace_limit;
                    enable_powerdown = cpu_active;
                    irq = {bind = "&si_cl0_gic.spi_in_" .. (229 + 5 * cluster + core)};
                    devactive_out = {bind = "&si_cl0_ap_cluster" .. cluster ..
                        "_ppu.active_in_" .. core};
                    powerdown = cpu_active and {
                        bind = "&ap_cpu_" .. cpu_index .. ".powerdown_wfi_out";
                    } or nil;
                    wakeup = cpu_active and {
                        bind = "&ap_gic.wake_request_out_" .. cpu_index;
                    } or nil;
                    assert_power_on_reset = cpu_active;
                    assert_power_on_load = cpu_active and cpu_index == 0;
                    -- CPU0's load drives a +1 ps asynchronous cold-reset fanout.
                    -- Delta-only waits can publish ON before watchdog/IRQ reset.
                    -- Retain the PPU's 1 ns phases only for this cold-boot path;
                    -- secondary CPU hotplug keeps its existing delta timing.
                    power_on_load_pulse_width_ns = cpu_index == 0 and 1 or 0;
                    power_on_load_to_reset_delay_ns = cpu_index == 0 and 1 or 0;
                    power_on_load = cpu_active and cpu_index == 0 and
                        {bind = "&host_reset_ctrl.ap_power_reset"} or nil;
                    power_on_reset = cpu_active and {
                        bind = "&ap_cpu_" .. cpu_index .. ".reset";
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
    end

    return block
end

return module
