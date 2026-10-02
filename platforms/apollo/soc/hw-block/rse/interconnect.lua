-- RSE ATU and local pass routing; socket names preserve the existing address views.
local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local atu_dmi = ctx.options.atu_dmi
    local atu_trace = ctx.options.atu_trace
    local atu_trace_filter = ctx.options.atu_trace_filter
    local atu_trace_limit = ctx.options.atu_trace_limit
    local rse_local_crypto = ctx.options.rse_local_crypto
    local rse_local_peripherals = ctx.options.rse_local_peripherals
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local RSE_HW = hardware.RSE_HW
    local RSE_RUNTIME_ADDRESS = runtime.addresses

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_router == nil,
            "duplicate object: platform.rse_router")
        platform.rse_router = {
            moduletype = "router";
            broadcast_invalidation = true;
            log_level = 0;
        }
        assert(
            platform.rse_atu_regs == nil,
            "duplicate object: platform.rse_atu_regs")
        platform.rse_atu_regs = {
            moduletype = "rse_atu";
            trace = atu_trace;
            trace_limit = atu_trace_limit;
            trace_filter = atu_trace_filter;
            trace_address_min = RSE_RUNTIME_ADDRESS.atu_trace_min;
            trace_address_max = RSE_RUNTIME_ADDRESS.atu_trace_max;
            enable_dmi = atu_dmi;
            build_config = RSE_HW.atu_build_config;
            target_socket = {
                address = RSE_ADDRESS.atu_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            translation_socket = {
                address = RSE_ADDRESS.host_access_non_secure;
                size = RSE_SIZE.host_access;
                bind = "&rse_router.initiator_socket";
                relative_addresses = false;
                priority = 10;
                aliases = {
                    secure = {
                        address = RSE_ADDRESS.host_access_secure;
                        size = RSE_SIZE.host_access;
                    };
                };
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            log_level = 0;
        }
    end

    function block.define_local(ctx, platform)
        assert(
            platform.remote_main_router == nil,
            "duplicate object: platform.remote_main_router")
        if rse_local_peripherals then
            platform.remote_main_router = {
                moduletype = "router";
                broadcast_invalidation = true;
                target_socket = {
                    address = RSE_ADDRESS.remote_base;
                    size = RSE_ADDRESS.nvic;
                    bind = "&cpu_0.router.initiator_socket";
                    relative_addresses = false;
                    priority = 100;
                    aliases = {
                        post_nvic = {
                            address = RSE_ADDRESS.nvic + RSE_SIZE.nvic;
                            size = RSE_SIZE.remote_post_nvic;
                            priority = 100;
                        };
                    };
                };
                log_level = 0;
            }
        end

        assert(
            platform.remote_crypto_router == nil,
            "duplicate object: platform.remote_crypto_router")
        if rse_local_crypto then
            platform.remote_crypto_router = {
                moduletype = "router";
                target_socket = {
                    address = RSE_ADDRESS.cc3xx_secure;
                    size = RSE_SIZE.double_register_window;
                    bind = "&cpu_0.router.initiator_socket";
                    relative_addresses = false;
                };
                log_level = 0;
            }
        end

        assert(
            platform.plugin_pass == nil,
            "duplicate object: platform.plugin_pass")
        platform.plugin_pass = {
            moduletype = "LocalPass";
            tlm_initiator_ports_num = 6;
            tlm_target_ports_num = 2;
            target_signals_num = 0;
            initiator_signals_num = RSE_HW.nvic_num_irq;
            initiator_socket_0 = {bind = "&rse_lsc_counter.control"};
            initiator_socket_1 = {bind = "&rse_lsc_counter.status"};
            initiator_socket_2 = {bind = "&rse_timer_0.socket"};
            initiator_socket_3 = {bind = "&rse_timer_1.socket"};
            initiator_socket_4 = {bind = "&rse_timer_2.socket"};
            initiator_socket_5 = {bind = "&rse_timer_3.socket"};
            target_socket_0 = {
                address = RSE_ADDRESS.remote_base;
                size = RSE_ADDRESS.nvic;
                bind = rse_local_peripherals and
                    "&remote_main_router.initiator_socket" or
                    "&cpu_0.router.initiator_socket";
            };
            target_socket_1 = {
                address = RSE_ADDRESS.nvic + RSE_SIZE.nvic;
                size = RSE_SIZE.remote_post_nvic;
                bind = rse_local_peripherals and
                    "&remote_main_router.initiator_socket" or
                    "&cpu_0.router.initiator_socket";
            };
        }
    for irq = 0,(RSE_HW.nvic_num_irq-1) do
        platform.plugin_pass["initiator_signal_socket_" .. irq] = {
            bind = "&cpu_0.cpu.nvic.irq_in_" .. irq;
        }
    end
    end

    return block
end

return module
