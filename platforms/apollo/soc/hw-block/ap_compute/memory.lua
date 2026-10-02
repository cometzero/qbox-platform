local memory = {}

function memory.define(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE

    assert(platform.host_ap_shared_sram == nil, "duplicate Apollo object")
    platform.host_ap_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.shared_sram;
            size = AP_SIZE.shared_sram;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_ap_mhu_ns_shared_sram == nil, "duplicate Apollo object")
    platform.host_ap_mhu_ns_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.mhu_ns_shared_sram;
            size = AP_SIZE.mhu_ns_shared_sram;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    assert(platform.host_ap_peripheral_ns_sram_tail == nil, "duplicate Apollo object")
    platform.host_ap_peripheral_ns_sram_tail = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.peripheral_ns_sram_tail;
            size = AP_SIZE.peripheral_ns_sram_tail;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    assert(platform.host_ap_bl2_header_sram == nil, "duplicate Apollo object")
    platform.host_ap_bl2_header_sram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.bl2_header_sram;
            size = AP_SIZE.bl2_header_sram;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_ap_flash == nil, "duplicate Apollo object")
    platform.host_ap_flash = {
        moduletype = "strata_flash_j3";
        enable_dmi = options.host_memory_dmi;
        program_ff_sets_bits = true;
        program_ff_erases_sector = true;
        size = AP_SIZE.flash;
        sector_size = AP_SIZE.flash_sector;
        target_socket = {
            address = AP_ADDRESS.flash;
            size = AP_SIZE.flash;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_ap_trusted_nvctr == nil, "duplicate Apollo object")
    platform.host_ap_trusted_nvctr = {
        moduletype = "gs_memory";
        read_only = true;
        target_socket = {
            address = AP_ADDRESS.trusted_nvctr;
            size = AP_SIZE.trusted_nvctr;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    assert(platform.host_ap_dram1 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.host_ap_dram1 = {
            moduletype = "gs_memory";
            dmi_allow = options.host_memory_dmi;
            target_socket = {
                address = AP_ADDRESS.dram1;
                size = AP_SIZE.dram1;
                bind = "&system_router.initiator_socket";
                priority = 10;
            };
            log_level = 0;
        }
    end

    assert(platform.host_ap_ffa_mm_comm_buffer == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.host_ap_ffa_mm_comm_buffer = {
            moduletype = "gs_memory";
            dmi_allow = options.host_memory_dmi;
            target_socket = {
                address = AP_ADDRESS.ffa_mm_comm_buffer;
                size = AP_SIZE.ffa_mm_comm_buffer;
                bind = "&system_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
    end

    assert(platform.host_ap_spmc_sdram == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.host_ap_spmc_sdram = {
            moduletype = "gs_memory";
            dmi_allow = options.host_memory_dmi;
            target_socket = {
                address = AP_ADDRESS.spmc;
                size = AP_SIZE.spmc_local;
                bind = "&system_router.initiator_socket";
            };
            log_level = 0;
        }
    end

    assert(platform.host_ap_dram2 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.host_ap_dram2 = {
            moduletype = "gs_memory";
            dmi_allow = options.host_memory_dmi;
            target_socket = {
                address = AP_ADDRESS.dram2;
                size = AP_SIZE.dram2;
                bind = "&system_router.initiator_socket";
            };
            log_level = 0;
        }
    end

    assert(platform.host_ap_cper == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.host_ap_cper = {
            moduletype = "gs_memory";
            target_socket = {
                address = AP_ADDRESS.cper;
                size = AP_SIZE.cper;
                bind = "&system_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
    end
end

return memory
