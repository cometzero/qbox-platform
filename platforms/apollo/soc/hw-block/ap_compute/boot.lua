local AP_FLASH_SIZE = 0x08000000
local AP_FLASH_BASE = 0x38000000
local AP_BL2_HEADER_SRAM_BASE = 0x00100000
local AP_TRUSTED_NVCTR_BASE = 0x32030000
local AP_BL2_HEADER_SRAM_SIZE = 0x00080000
local AP_FLASH_SECTOR_SIZE = 0x00001000
local AP_TRUSTED_NVCTR_SIZE = 0x00010000

local boot = {}

function boot.define(ctx, platform)
    local options = ctx.options
    assert(platform.host_ap_bl2_header_sram == nil, "duplicate Apollo object")
    platform.host_ap_bl2_header_sram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = AP_BL2_HEADER_SRAM_BASE;
            size = AP_BL2_HEADER_SRAM_SIZE;
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
        size = AP_FLASH_SIZE;
        sector_size = AP_FLASH_SECTOR_SIZE;
        target_socket = {
            address = AP_FLASH_BASE;
            size = AP_FLASH_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_ap_trusted_nvctr == nil, "duplicate Apollo object")
    platform.host_ap_trusted_nvctr = {
        moduletype = "gs_memory";
        read_only = true;
        target_socket = {
            address = AP_TRUSTED_NVCTR_BASE;
            size = AP_TRUSTED_NVCTR_SIZE;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

end

return boot
