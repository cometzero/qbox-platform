local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local host_si_cl1_sram_map_file = ctx.options.host_si_cl1_sram_map_file
    local host_si_sram_dmi = ctx.options.host_si_sram_dmi
    local host_sram_shared_memory_enabled = ctx.options.host_sram_shared_memory_enabled
    local SI_CL1_SRAM_BASE = hardware.SI_CL1_SRAM_BASE
    local HOST_SI_CL1_SRAM_PHYS_BASE = hardware.HOST_SI_CL1_SRAM_PHYS_BASE
    local HOST_SI_SRAM_WINDOW_SIZE = hardware.HOST_SI_SRAM_WINDOW_SIZE
    local HOST_SI_CL1_CL_UTIL_BASE = hardware.HOST_SI_CL1_CL_UTIL_BASE
    local HOST_SI_CL_UTIL_SIZE = hardware.HOST_SI_CL_UTIL_SIZE
    local SI_CL1_SRAM_SIZE = hardware.SI_CL1_SRAM_SIZE
    local SI_CL1_SCMI_SHMEM_BASE = hardware.SI_CL1_SCMI_SHMEM_BASE
    local SI_CL1_SCMI_SHMEM_SIZE = hardware.SI_CL1_SCMI_SHMEM_SIZE

    local block = {}

    function block.define_host(ctx, platform)
        assert(
            platform.host_si_cl1_sram == nil,
            "duplicate object: platform.host_si_cl1_sram")
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

        assert(
            platform.host_si_cl1_cub == nil,
            "duplicate object: platform.host_si_cl1_cub")
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
    end

    function block.define(ctx, platform)
        assert(
            platform.si_cl1_sram == nil,
            "duplicate object: platform.si_cl1_sram")
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

        assert(
            platform.si_cl1_scmi_shmem == nil,
            "duplicate object: platform.si_cl1_scmi_shmem")
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
    end

    return block
end

return module
