-- Keep the public rse_cpu_pass.cpu_0 hierarchy used by CCI and reset wiring.
local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local RSE_HW = hardware.RSE_HW

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_cpu_pass == nil,
            "duplicate object: platform.rse_cpu_pass")
        platform.rse_cpu_pass = {
            moduletype = "Container";
            tlm_initiator_ports_num = 2;
            tlm_target_ports_num = 6;
            target_signals_num = RSE_HW.nvic_num_irq;
            initiator_signals_num = 0;
            initiator_socket_0 = {bind = "&rse_router.target_socket"};
            initiator_socket_1 = {bind = "&rse_router.target_socket"};
            target_socket_0 = {
                address = RSE_ADDRESS.syscounter_control_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            target_socket_1 = {
                address = RSE_ADDRESS.syscounter_read_secure;
                size = RSE_SIZE.register_window;
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
                init_svtor = RSE_ADDRESS.rom_secure;
                init_nsvtor = RSE_ADDRESS.rom_secure;
                request_origin_id = ctx.request_context.origin.rse_cpu;
                request_domain_id = ctx.request_context.domain.rse;
                requester_id = 0;
                start_powered_off = false;
                nvic = {
                    systick_cpuclk_hz = 100000000;
                    mem = {
                        address = RSE_ADDRESS.nvic;
                        size = RSE_SIZE.nvic;
                    };
                    num_irq = RSE_HW.nvic_num_irq;
                };
            };
        }
    end

    return block
end

return module
