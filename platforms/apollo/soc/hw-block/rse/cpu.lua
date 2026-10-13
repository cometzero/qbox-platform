local RSE_NVIC_NUM_IRQ = 160
local RSE_SYSCOUNTER_CONTROL_SECURE_BASE = 0x5015A000
local RSE_REGISTER_WINDOW_SIZE = 0x00001000
local RSE_SYSCOUNTER_READ_SECURE_BASE = 0x5015B000
local RSE_ROM_SECURE_BASE = 0x11000000
local RSE_NVIC_BASE = 0xE000E000
local RSE_NVIC_SIZE = 0x00021000
local RSE_CPU0_PWRCTRL_SECURE_BASE = 0x50012000
local RSE_CPU0_PWRCTRL_NON_SECURE_BASE = 0x40012000
local RSE_CPU0_IDENTITY_SECURE_BASE = 0x5001F000
local RSE_CPU0_IDENTITY_NON_SECURE_BASE = 0x4001F000

-- Keep the public rse_cpu_pass.cpu_0 hierarchy used by CCI and reset wiring.
local module = {}

function module.create(ctx)
    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_cpu_pass == nil,
            "duplicate object: platform.rse_cpu_pass")
        platform.rse_cpu_pass = {
            moduletype = "Container";
            tlm_initiator_ports_num = 2;
            tlm_target_ports_num = 6;
            target_signals_num = RSE_NVIC_NUM_IRQ;
            initiator_signals_num = 0;
            initiator_socket_0 = {bind = "&rse_router.target_socket"};
            initiator_socket_1 = {bind = "&rse_router.target_socket"};
            target_socket_0 = {
                address = RSE_SYSCOUNTER_CONTROL_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            target_socket_1 = {
                address = RSE_SYSCOUNTER_READ_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
        }
    end

    function block.define_local(ctx, platform)
        assert(
            platform.cpu_0 == nil,
            "duplicate object: platform.cpu_0")
        platform.cpu_0 = {
            moduletype = "ApolloRseCPU";
            args = {"&qemu_inst"};
            router = {
                broadcast_invalidation = true;
            };
            cpu = {
                init_svtor = RSE_ROM_SECURE_BASE;
                init_nsvtor = RSE_ROM_SECURE_BASE;
                request_origin_id = ctx.request_context.origin.rse_cpu;
                request_domain_id = ctx.request_context.domain.rse;
                requester_id = 0;
                start_powered_off = false;
                nvic = {
                    systick_cpuclk_hz = 100000000;
                    mem = {
                        address = RSE_NVIC_BASE;
                        size = RSE_NVIC_SIZE;
                    };
                    num_irq = RSE_NVIC_NUM_IRQ;
                };
            };
        }
        assert(
            platform.rse_nvic_cold_reset == nil,
            "duplicate object: platform.rse_nvic_cold_reset")
        platform.rse_nvic_cold_reset = {
            moduletype = "qemu_device_cold_reset";
            args = {"&cpu_0.cpu.nvic"};
        }
    end

    function block.define_registers(ctx, platform)
        assert(
            platform.rse_cpu0_pwrctrl_regs == nil,
            "duplicate object: platform.rse_cpu0_pwrctrl_regs")
        platform.rse_cpu0_pwrctrl_regs = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_CPU0_PWRCTRL_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
                aliases = {
                    ns = {
                        address = RSE_CPU0_PWRCTRL_NON_SECURE_BASE;
                        size = RSE_REGISTER_WINDOW_SIZE;
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
                address = RSE_CPU0_IDENTITY_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
                aliases = {
                    ns = {
                        address = RSE_CPU0_IDENTITY_NON_SECURE_BASE;
                        size = RSE_REGISTER_WINDOW_SIZE;
                    };
                };
            };
            init_mem = true;
            log_level = 0;
        }
    end

    return block
end

return module
