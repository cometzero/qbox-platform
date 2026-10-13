local AP_SHARED_SRAM_BASE = 0x00000000
local AP_MHU_NS_SHARED_SRAM_BASE = 0x00180000
local AP_PERIPHERAL_NS_SRAM_TAIL_BASE = 0x00181000
local AP_DRAM1_BASE = 0x80000000
local AP_CPER_BASE = 0xFFA00000
local AP_FFA_MM_COMM_BUFFER_BASE = 0xFFBF0000
local AP_SPMC_BASE = 0xFFC00000
local AP_DRAM2_BASE = 0x20000000000
local AP_SHARED_SRAM_SIZE = 0x00100000
local AP_MHU_NS_SHARED_SRAM_SIZE = 0x00001000
local AP_PERIPHERAL_NS_SRAM_TAIL_SIZE = 0x0007F000
local AP_DRAM1_SIZE = 0x7F000000
local AP_CPER_SIZE = 0x00100000
local AP_FFA_MM_COMM_BUFFER_SIZE = 0x00002000
local AP_SPMC_LOCAL_SIZE = 0x003E0000
local AP_DRAM2_SIZE = 0x80000000

local memory = {}

function memory.define(ctx, platform)
    local options = ctx.options

    assert(platform.host_ap_shared_sram == nil, "duplicate Apollo object")
    platform.host_ap_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = AP_SHARED_SRAM_BASE;
            size = AP_SHARED_SRAM_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_ap_mhu_ns_shared_sram == nil, "duplicate Apollo object")
    platform.host_ap_mhu_ns_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = AP_MHU_NS_SHARED_SRAM_BASE;
            size = AP_MHU_NS_SHARED_SRAM_SIZE;
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
            address = AP_PERIPHERAL_NS_SRAM_TAIL_BASE;
            size = AP_PERIPHERAL_NS_SRAM_TAIL_SIZE;
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
                address = AP_DRAM1_BASE;
                size = AP_DRAM1_SIZE;
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
                address = AP_FFA_MM_COMM_BUFFER_BASE;
                size = AP_FFA_MM_COMM_BUFFER_SIZE;
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
                address = AP_SPMC_BASE;
                size = AP_SPMC_LOCAL_SIZE;
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
                address = AP_DRAM2_BASE;
                size = AP_DRAM2_SIZE;
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
                address = AP_CPER_BASE;
                size = AP_CPER_SIZE;
                bind = "&system_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
    end
end

return memory
