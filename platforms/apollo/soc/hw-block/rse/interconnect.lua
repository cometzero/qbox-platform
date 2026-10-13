local RSE_ATU_BUILD_CONFIG = 0x000000C5
local RSE_ATU_SECURE_BASE = 0x50150000
local RSE_REGISTER_WINDOW_SIZE = 0x00001000
local RSE_HOST_ACCESS_NON_SECURE_BASE = 0x60000000
local RSE_HOST_ACCESS_SIZE = 0x10000000
local RSE_HOST_ACCESS_SECURE_BASE = 0x70000000
local RSE_REMOTE_BASE = 0x00000000
local RSE_NVIC_BASE = 0xE000E000
local RSE_NVIC_SIZE = 0x00021000
local RSE_REMOTE_POST_NVIC_SIZE = 0x00100000
local RSE_CC3XX_SECURE_BASE = 0x50154000
local RSE_DOUBLE_REGISTER_WINDOW_SIZE = 0x00002000
local RSE_NVIC_NUM_IRQ = 160

-- RSE ATU and local pass routing; socket names preserve the existing address views.
local module = {}

function module.create(ctx, runtime)
    local atu_dmi = ctx.options.atu_dmi
    local atu_trace = ctx.options.atu_trace
    local atu_trace_filter = ctx.options.atu_trace_filter
    local atu_trace_limit = ctx.options.atu_trace_limit
    local rse_local_crypto = ctx.options.rse_local_crypto
    local rse_local_peripherals = ctx.options.rse_local_peripherals
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
            build_config = RSE_ATU_BUILD_CONFIG;
            target_socket = {
                address = RSE_ATU_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            translation_socket = {
                address = RSE_HOST_ACCESS_NON_SECURE_BASE;
                size = RSE_HOST_ACCESS_SIZE;
                bind = "&rse_router.initiator_socket";
                relative_addresses = false;
                priority = 10;
                aliases = {
                    secure = {
                        address = RSE_HOST_ACCESS_SECURE_BASE;
                        size = RSE_HOST_ACCESS_SIZE;
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
                    address = RSE_REMOTE_BASE;
                    size = RSE_NVIC_BASE;
                    bind = "&cpu_0.router.initiator_socket";
                    relative_addresses = false;
                    priority = 100;
                    aliases = {
                        post_nvic = {
                            address = RSE_NVIC_BASE + RSE_NVIC_SIZE;
                            size = RSE_REMOTE_POST_NVIC_SIZE;
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
                    address = RSE_CC3XX_SECURE_BASE;
                    size = RSE_DOUBLE_REGISTER_WINDOW_SIZE;
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
            initiator_signals_num = RSE_NVIC_NUM_IRQ;
            initiator_socket_0 = {bind = "&rse_lsc_counter.control"};
            initiator_socket_1 = {bind = "&rse_lsc_counter.status"};
            initiator_socket_2 = {bind = "&rse_timer_0.socket"};
            initiator_socket_3 = {bind = "&rse_timer_1.socket"};
            initiator_socket_4 = {bind = "&rse_timer_2.socket"};
            initiator_socket_5 = {bind = "&rse_timer_3.socket"};
            target_socket_0 = {
                address = RSE_REMOTE_BASE;
                size = RSE_NVIC_BASE;
                bind = rse_local_peripherals and
                    "&remote_main_router.initiator_socket" or
                    "&cpu_0.router.initiator_socket";
            };
            target_socket_1 = {
                address = RSE_NVIC_BASE + RSE_NVIC_SIZE;
                size = RSE_REMOTE_POST_NVIC_SIZE;
                bind = rse_local_peripherals and
                    "&remote_main_router.initiator_socket" or
                    "&cpu_0.router.initiator_socket";
            };
        }
    for irq = 0,(RSE_NVIC_NUM_IRQ-1) do
        platform.plugin_pass["initiator_signal_socket_" .. irq] = {
            bind = "&cpu_0.cpu.nvic.irq_in_" .. irq;
        }
    end
    end

    return block
end

return module
