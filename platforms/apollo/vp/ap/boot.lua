local boot = {}

local AP_BOOT = {
    sds_reset_syndrome_offset = 0x00000050;
    reset_syndrome_system_reset_req = 0x00000008;
    bl2_offset = 0x00082000;
    bl2_data_offset = 0x00098000;
    bl2_stacks_offset = 0x00098F40;
    bl2_bss_offset = 0x0009A000;
    bl2_xlat_offset = 0x000A2000;
    bl2_data_elf_offset = 0x00017000;
    bl2_data_size = 0x00000F35;
    bl2_stacks_size = 0x00001000;
    bl2_bss_size = 0x00008000;
    bl2_xlat_size = 0x0000E000;
}

function boot.define(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE

    local AP_BL2_RESET = {
        data_phys_base = AP_ADDRESS.shared_sram + AP_BOOT.bl2_data_offset;
        stacks_phys_base = AP_ADDRESS.shared_sram + AP_BOOT.bl2_stacks_offset;
        bss_phys_base = AP_ADDRESS.shared_sram + AP_BOOT.bl2_bss_offset;
        xlat_phys_base = AP_ADDRESS.shared_sram + AP_BOOT.bl2_xlat_offset;
        data_elf_offset = AP_BOOT.bl2_data_elf_offset;
        data_size = AP_BOOT.bl2_data_size;
        stacks_size = AP_BOOT.bl2_stacks_size;
        bss_size = AP_BOOT.bl2_bss_size;
        xlat_size = AP_BOOT.bl2_xlat_size;
    }

    local HOST_AP_SDS_REGION_DATA = {
        0x1007AA7A, AP_SIZE.sds_mem,
        0x01000001, 0x00000011, 0x00000000, 0x00000000,
        0x01000002, 0x00000011, 0x00000000, 0x00000000,
        0x01000003, 0x00000011, 0x00000000, 0x00000000,
        0x01000004, 0x00000011, 0x20000000, 0x00000003,
        0x01000005, 0x00000011, 0x00000000, 0x00000000,
        0x01000006, 0x00000011, 0x00000007, 0x00000000,
        0x01000009, 0x00000021, 0x00000000, 0x00000000,
        0x00000000, 0x00000000,
    }

    local HOST_AP_TRUSTED_NVCTR_DATA = {
        0x0000001F;
        0x000000DF;
    }

    assert(platform.ap_bl2_reset_loader == nil, "duplicate AP boot loader")
    if options.enable_ap_cpus then
        platform.ap_bl2_reset_loader = {
            moduletype = "loader";
            request_origin_id = ctx.request_context.origin.ap_loader;
            request_domain_id = ctx.request_context.domain.ap;
            request_capabilities = ctx.request_context.capability.boot_loader;
            request_secure = true;
            request_secure_valid = true;
            initiator_socket = {bind = "&system_router.target_socket"};
            {
                bin_file = options.AP_BL2_ELF;
                address = AP_BL2_RESET.data_phys_base;
                bin_file_offset = AP_BL2_RESET.data_elf_offset;
                bin_file_size = AP_BL2_RESET.data_size;
            };
            {
                bin_file = "/dev/zero";
                address = AP_BL2_RESET.stacks_phys_base;
                bin_file_offset = 0;
                bin_file_size = AP_BL2_RESET.stacks_size;
            };
            {
                bin_file = "/dev/zero";
                address = AP_BL2_RESET.bss_phys_base;
                bin_file_offset = 0;
                bin_file_size = AP_BL2_RESET.bss_size;
            };
            {
                bin_file = "/dev/zero";
                address = AP_BL2_RESET.xlat_phys_base;
                bin_file_offset = 0;
                bin_file_size = AP_BL2_RESET.xlat_size;
            };
            {
                address = (AP_ADDRESS.shared_sram + AP_BOOT.sds_reset_syndrome_offset);
                data = {AP_BOOT.reset_syndrome_system_reset_req};
            };
            log_level = 0;
        }
    end

    platform.host_ap_shared_sram.map_file = options.host_ap_shared_sram_map_file
    platform.host_ap_shared_sram.shared_memory = options.host_sram_shared_memory_enabled(options.host_ap_shared_sram_map_file)
    platform.host_ap_shared_sram.shared_memory_prefix = "ra-aps-"
    platform.host_ap_shared_sram.init_mem = options.host_ap_shared_sram_map_file == ""

    platform.host_ap_bl2_header_sram.map_file = options.host_ap_bl2_header_sram_map_file
    platform.host_ap_bl2_header_sram.shared_memory = options.host_sram_shared_memory_enabled(options.host_ap_bl2_header_sram_map_file)
    platform.host_ap_bl2_header_sram.shared_memory_prefix = "ra-aph-"
    platform.host_ap_bl2_header_sram.init_mem = false

    platform.host_ap_flash.trace = options.boot_flash_trace
    platform.host_ap_flash.trace_limit = options.boot_flash_trace_limit
    platform.host_ap_flash.dmi_ranges = options.ap_flash_dmi_ranges
    platform.host_ap_flash.backing_file = options.flash_writeback and options.ap_flash or ""
    platform.host_ap_flash.defer_backing_write = true
    platform.host_ap_flash.defer_backing_flush_interval = options.flash_defer_backing_flush_interval
    platform.host_ap_flash.stats_file = options.ap_flash_stats_file
    platform.host_ap_flash.stats_interval = options.flash_stats_interval
    platform.host_ap_flash.load = {bin_file = options.ap_flash, offset = 0}

    platform.host_ap_shared_sram.load = {data = HOST_AP_SDS_REGION_DATA; offset = 0}
    platform.host_ap_trusted_nvctr.load = {data = HOST_AP_TRUSTED_NVCTR_DATA; offset = 0}
    for i = 0, options.AP_NUM_CPUS - 1 do
        local cpu = platform["ap_cpu_" .. i]
        if cpu then
            cpu.rvbar = AP_ADDRESS.shared_sram + AP_BOOT.bl2_offset
        end
    end
end

return boot
