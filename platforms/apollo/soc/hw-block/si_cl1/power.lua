local HOST_SI_CL1_CL_UTIL_BASE = 0x4000028800000

local HOST_SI_CLUS_PPU_OFFSET = 0x00010000
local HOST_SI_CONTROL_WINDOW_SIZE = 0x00010000

local module = {}

function module.create(ctx, _runtime)
    local host_ppu_trace = ctx.options.host_ppu_trace
    local host_ppu_trace_limit = ctx.options.host_ppu_trace_limit

    local block = {}

    function block.define_host(ctx, platform)
        assert(
            platform.host_si_cl1_clus_ppu == nil,
            "duplicate object: platform.host_si_cl1_clus_ppu")
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

    return block
end

return module
