local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local dma350_trace = ctx.options.dma350_trace
    local dma350_trace_filter = ctx.options.dma350_trace_filter
    local dma350_trace_limit = ctx.options.dma350_trace_limit
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local RSE_IRQ = hardware.RSE_IRQ
    local RSE_RUNTIME_ADDRESS = runtime.addresses

    local block = {}

    function block.define(ctx, platform)
        local rse_gpio_secure_bases = {
            RSE_ADDRESS.gpio0_secure;
            RSE_ADDRESS.gpio1_secure;
        }
        local rse_gpio_non_secure_bases = {
            RSE_ADDRESS.gpio0_non_secure;
            RSE_ADDRESS.gpio1_non_secure;
        }
        for gpio = 0,1 do
            local gpio_name = "rse_gpio_" .. gpio
            assert(
                platform[gpio_name] == nil,
                "duplicate object: platform." .. (gpio_name))
            platform[gpio_name] = {
                moduletype = "qemu_pl061";
                args = {"&platform.qemu_inst"};
                init_inputs = ctx.getenv_number_or(
                    "QBOX_APOLLO_RSE_GPIO" .. gpio .. "_INIT_INPUTS", "0");
                pullups = 0;
                pulldowns = 0;
                irq = {bind = "&rse_gpio_irq_or.signal_in_" .. gpio};
            }
            assert(
                platform[gpio_name .. "_cold_reset"] == nil,
                "duplicate object: platform." .. (gpio_name .. "_cold_reset"))
            platform[gpio_name .. "_cold_reset"] = {
                moduletype = "qemu_device_cold_reset";
                args = {"&platform." .. gpio_name};
            }
            assert(
                platform[gpio_name .. "_ppc"] == nil,
                "duplicate object: platform." .. (gpio_name .. "_ppc"))
            platform[gpio_name .. "_ppc"] = {
                moduletype = "rse_ppc_filter";
                args = {
                    "&platform.rse_sacfg_regs";
                    "&platform.rse_nsacfg_regs";
                };
                policy_mask = 2 ^ gpio;
                ppc_register_offset = 0x10;
                target_socket = {
                    address = rse_gpio_secure_bases[gpio + 1];
                    size = RSE_SIZE.register_window;
                    bind = "&rse_router.initiator_socket";
                    aliases = {
                        ns = {
                            address = rse_gpio_non_secure_bases[gpio + 1];
                            size = RSE_SIZE.register_window;
                        };
                    };
                };
                initiator_socket = {bind = "&" .. gpio_name .. ".mem"};
                log_level = 0;
            }
        end

        assert(
            platform.rse_gpio_irq_or == nil,
            "duplicate object: platform.rse_gpio_irq_or")
        platform.rse_gpio_irq_or = {
            moduletype = "signal_or";
            num_inputs = 2;
            signal_out = {
                bind = "&rse_cpu_pass.target_signal_socket_" ..
                    RSE_IRQ.gpio_combined;
            };
        }
        assert(
            platform.rse_dma350 == nil,
            "duplicate object: platform.rse_dma350")
        platform.rse_dma350 = {
            moduletype = "dma350";
            trace = dma350_trace;
            trace_limit = dma350_trace_limit;
            trace_filter = dma350_trace_filter;
            trace_address_min = RSE_RUNTIME_ADDRESS.dma350_trace_min;
            target_socket = {
                address = RSE_ADDRESS.dma350_secure;
                size = RSE_SIZE.double_register_window;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&rse_router.target_socket"};
            log_level = 0;
        }
        assert(
            platform.rse_host_uart0_s == nil,
            "duplicate object: platform.rse_host_uart0_s")
        platform.rse_host_uart0_s = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            target_socket = {
                address = RSE_ADDRESS.host_uart0_secure;
                size = RSE_SIZE.uart_window;
                bind = "&rse_router.initiator_socket";
                aliases = {
                    ns_atu_logical = {
                        address = RSE_ADDRESS.host_uart0_non_secure;
                        size = RSE_SIZE.uart_window;
                    };
                };
            };
            irq = {bind = "&rse_cpu_pass.target_signal_socket_0"};
            backend_socket = {bind = "&rse_uart_file.biflow_socket"};
        }
    end

    return block
end

return module
