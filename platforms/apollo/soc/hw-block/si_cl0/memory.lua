local SI_CL0_SRAM_BASE = 0x120000000
local HOST_SI_CL0_CL_UTIL_BASE = 0x4000028000000

local SI_CL0_SRAM_SIZE = 0x00800000
local HOST_SI_CL_UTIL_SIZE = 0x00800000
local HOST_SI_CL0_SRAM_PHYS_BASE = 0x4000120000000
local HOST_SI_SRAM_WINDOW_SIZE = 0x01000000

local module = {}

function module.create(ctx, _runtime)
    local host_si_cl0_sram_map_file = ctx.options.host_si_cl0_sram_map_file
    local host_si_sram_dmi = ctx.options.host_si_sram_dmi
    local host_sram_shared_memory_enabled = ctx.options.host_sram_shared_memory_enabled

    local block = {}

    function block.define_host(ctx, platform)
        assert(
            platform.host_si_cl0_sram == nil,
            "duplicate object: platform.host_si_cl0_sram")
        platform.host_si_cl0_sram = {
            moduletype = "gs_memory";
            dmi_allow = host_si_sram_dmi;
            target_socket = {
                address = HOST_SI_CL0_SRAM_PHYS_BASE;
                size = HOST_SI_SRAM_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            map_file = host_si_cl0_sram_map_file;
            shared_memory = host_sram_shared_memory_enabled(host_si_cl0_sram_map_file);
            shared_memory_prefix = "ra-si0-";
            init_mem = host_si_cl0_sram_map_file == "";
            log_level = 0;
        }

        assert(
            platform.host_si_cl0_cub == nil,
            "duplicate object: platform.host_si_cl0_cub")
        platform.host_si_cl0_cub = {
            moduletype = "gs_memory";
            target_socket = {
                address = HOST_SI_CL0_CL_UTIL_BASE;
                size = HOST_SI_CL_UTIL_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 20;
            };
            init_mem = true;
            log_level = 0;
        }
    end

    function block.define(ctx, platform)
        assert(
            platform.si_cl0_sram == nil,
            "duplicate object: platform.si_cl0_sram")
        platform.si_cl0_sram = {
            moduletype = "gs_memory";
            dmi = true;
            target_socket = {
                address = SI_CL0_SRAM_BASE;
                size = SI_CL0_SRAM_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            log_level = 0;
        }
    end

    return block
end

return module
