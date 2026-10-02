local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local rse_cpuwait = ctx.options.rse_cpuwait
    local rse_dma_boot_en = ctx.options.rse_dma_boot_en
    local rse_lsc_input_hz = ctx.options.rse_lsc_input_hz
    local rse_reset_syndrome = ctx.options.rse_reset_syndrome
    local rse_smd_counter_mirror = ctx.options.rse_smd_counter_mirror
    local sysctrl_trace = ctx.options.sysctrl_trace
    local sysctrl_trace_limit = ctx.options.sysctrl_trace_limit
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local RSE_IRQ = hardware.RSE_IRQ
    local RSE_HW = hardware.RSE_HW
    local RSE_RUNTIME_ADDRESS = runtime.addresses

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_cpu0_pwrctrl_regs == nil,
            "duplicate object: platform.rse_cpu0_pwrctrl_regs")
        platform.rse_cpu0_pwrctrl_regs = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_ADDRESS.cpu0_pwrctrl_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
                aliases = {
                    ns = {
                        address = RSE_ADDRESS.cpu0_pwrctrl_non_secure;
                        size = RSE_SIZE.register_window;
                    };
                };
            };
            init_mem = true;
            log_level = 0;
        }

        assert(
            platform.rse_cpu0_identity_regs == nil,
            "duplicate object: platform.rse_cpu0_identity_regs")
        platform.rse_cpu0_identity_regs = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_ADDRESS.cpu0_identity_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
                aliases = {
                    ns = {
                        address = RSE_ADDRESS.cpu0_identity_non_secure;
                        size = RSE_SIZE.register_window;
                    };
                };
            };
            init_mem = true;
            log_level = 0;
        }
        assert(
            platform.rse_watchdog_ns == nil,
            "duplicate object: platform.rse_watchdog_ns")
        platform.rse_watchdog_ns = {
            moduletype = "zena_watchdog";
            clock_frequency = RSE_HW.watchdog_clock_hz;
            control = {
                address = RSE_ADDRESS.wdog_non_secure_control;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            refresh = {
                address = RSE_ADDRESS.wdog_non_secure_refresh;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            ws0 = {bind = "&rse_cpu_pass.target_signal_socket_1"};
            ws1 = {bind = "&rse_cpu_pass.target_signal_socket_0"};
            log_level = 0;
        }

        assert(
            platform.rse_watchdog_s == nil,
            "duplicate object: platform.rse_watchdog_s")
        platform.rse_watchdog_s = {
            moduletype = "zena_watchdog";
            clock_frequency = RSE_HW.watchdog_clock_hz;
            control = {
                address = RSE_ADDRESS.wdog_secure_control;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            refresh = {
                address = RSE_ADDRESS.wdog_secure_refresh;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }
        local rse_timer_policy_masks = {1, 2, 4, 32}
        local rse_timer_secure_bases = {
            RSE_ADDRESS.timer0_secure;
            RSE_ADDRESS.timer1_secure;
            RSE_ADDRESS.timer2_secure;
            RSE_ADDRESS.timer3_secure;
        }
        local rse_timer_non_secure_bases = {
            RSE_ADDRESS.timer0_non_secure;
            RSE_ADDRESS.timer1_non_secure;
            RSE_ADDRESS.timer2_non_secure;
            RSE_ADDRESS.timer3_non_secure;
        }
        for timer = 0,3 do
            assert(
                platform["rse_timer_" .. timer .. "_ppc"] == nil,
                "duplicate object: platform." .. ("rse_timer_" .. timer .. "_ppc"))
            platform["rse_timer_" .. timer .. "_ppc"] = {
                moduletype = "rse_ppc_filter";
                args = {
                    "&platform.rse_sacfg_regs";
                    "&platform.rse_nsacfg_regs";
                };
                policy_mask = rse_timer_policy_masks[timer + 1];
                target_socket = {
                    address = rse_timer_secure_bases[timer + 1];
                    size = RSE_SIZE.register_window;
                    bind = "&rse_router.initiator_socket";
                    aliases = {
                        ns = {
                            address = rse_timer_non_secure_bases[timer + 1];
                            size = RSE_SIZE.register_window;
                        };
                    };
                };
                initiator_socket = {
                    bind = "&rse_cpu_pass.target_socket_" .. (timer + 2);
                };
                log_level = 0;
            }
        end
        assert(
            platform.rse_sysctrl == nil,
            "duplicate object: platform.rse_sysctrl")
        platform.rse_sysctrl = {
            moduletype = "rse_sysctrl";
            trace = sysctrl_trace;
            trace_limit = sysctrl_trace_limit;
            reset_syndrome = rse_reset_syndrome;
            cpuwait = rse_cpuwait;
            dma_boot_en = rse_dma_boot_en;
            dma_boot_addr = RSE_RUNTIME_ADDRESS.dma_boot;
            target_socket = {
                address = RSE_ADDRESS.sysctrl_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            system_reset = {bind = "&apollo_system_reset_fanout.reset_in"};
            log_level = 0;
        }

        assert(
            platform.rse_integ_layer_regs == nil,
            "duplicate object: platform.rse_integ_layer_regs")
        platform.rse_integ_layer_regs = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_ADDRESS.integ_layer_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
    end

    function block.define_local(ctx, platform)
        assert(
            platform.rse_lsc_clock == nil,
            "duplicate object: platform.rse_lsc_clock")
        platform.rse_lsc_clock = {
            moduletype = "qemu_clock_source";
            args = {"&qemu_inst"};
            frequency_hz = rse_lsc_input_hz;
        }

        assert(
            platform.rse_local_system_counter == nil,
            "duplicate object: platform.rse_local_system_counter")
        if not rse_smd_counter_mirror then
            platform.rse_local_system_counter = {
                moduletype = "arm_system_counter";
                input_frequency_hz = rse_lsc_input_hz;
                integer_increment = 1;
                reported_frequency_hz = rse_lsc_input_hz;
                enabled = true;
                initial_count = 0;
            }
        end

        assert(
            platform.rse_lsc_counter == nil,
            "duplicate object: platform.rse_lsc_counter")
        platform.rse_lsc_counter = {
            moduletype = "qemu_sse_counter_mirror";
            dylib_path = "qemu_sse_counter";
            args = {
                "&qemu_inst";
                "&rse_lsc_clock";
                rse_smd_counter_mirror and
                    "&platform.css_system_counter" or
                    "&rse_local_system_counter";
            };
        }

        assert(
            platform.rse_timer_0 == nil,
            "duplicate object: platform.rse_timer_0")
        platform.rse_timer_0 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_IRQ.timer0};
        }

        assert(
            platform.rse_timer_1 == nil,
            "duplicate object: platform.rse_timer_1")
        platform.rse_timer_1 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_IRQ.timer1};
        }

        assert(
            platform.rse_timer_2 == nil,
            "duplicate object: platform.rse_timer_2")
        platform.rse_timer_2 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_IRQ.timer2};
        }

        assert(
            platform.rse_timer_3 == nil,
            "duplicate object: platform.rse_timer_3")
        platform.rse_timer_3 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_IRQ.timer3};
        }

        assert(
            platform.rse_nvic_cold_reset == nil,
            "duplicate object: platform.rse_nvic_cold_reset")
        platform.rse_nvic_cold_reset = {
            moduletype = "qemu_device_cold_reset";
            args = {"&cpu_0.cpu.nvic"};
        }
    end

    return block
end

return module
