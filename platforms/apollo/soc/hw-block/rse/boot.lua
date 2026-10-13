local RSE_BOOT_FLASH_SIZE = 0x04000000
local RSE_FLASH_SECTOR_SIZE = 0x00001000
local RSE_BOOT_FLASH_SECURE_BASE = 0xB0000000
local RSE_ROM_SECURE_BASE = 0x11000000
local RSE_ROM_SIZE = 0x00020000

-- RSE boot ROM and flash devices.
local module = {}

function module.create(ctx)
    local boot_flash_dmi = ctx.options.boot_flash_dmi
    local boot_flash_dmi_ranges = ctx.options.boot_flash_dmi_ranges
    local boot_flash_trace = ctx.options.boot_flash_trace
    local boot_flash_trace_limit = ctx.options.boot_flash_trace_limit
    local rse_flash_backend = ctx.options.rse_flash_backend
    local rse_local_boot_flash = ctx.options.rse_local_boot_flash

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_boot_flash == nil,
            "duplicate object: platform.rse_boot_flash")
        if (not rse_local_boot_flash) then
            platform.rse_boot_flash = {
                moduletype = "strata_flash_j3";
                trace = boot_flash_trace;
                trace_limit = boot_flash_trace_limit;
                enable_dmi = boot_flash_dmi;
                dmi_ranges = boot_flash_dmi_ranges;
                program_ff_sets_bits = true;
                program_ff_erases_sector = true;
                size = RSE_BOOT_FLASH_SIZE;
                sector_size = RSE_FLASH_SECTOR_SIZE;
                target_socket = {
                    address = RSE_BOOT_FLASH_SECURE_BASE;
                    size = RSE_BOOT_FLASH_SIZE;
                    bind = "&rse_router.initiator_socket";
                };
                log_level = 0;
            }
        end
    end

    function block.define_local(ctx, platform)
        assert(
            platform.rse_boot_flash == nil,
            "duplicate object: platform.rse_boot_flash")
        if rse_local_boot_flash and rse_flash_backend == "systemc-strata" then
            platform.rse_boot_flash = {
                moduletype = "strata_flash_j3";
                trace = boot_flash_trace;
                trace_limit = boot_flash_trace_limit;
                enable_dmi = boot_flash_dmi;
                dmi_ranges = boot_flash_dmi_ranges;
                program_ff_sets_bits = true;
                program_ff_erases_sector = true;
                size = RSE_BOOT_FLASH_SIZE;
                sector_size = RSE_FLASH_SECTOR_SIZE;
                target_socket = {
                    address = RSE_BOOT_FLASH_SECURE_BASE;
                    size = RSE_BOOT_FLASH_SIZE;
                    bind = "&cpu_0.router.initiator_socket";
                };
                log_level = 0;
            }
        end

        assert(
            platform.rse_boot_flash_qemu == nil,
            "duplicate object: platform.rse_boot_flash_qemu")
        if rse_local_boot_flash and rse_flash_backend == "qemu-cfi-local" then
            assert(platform.rse_pflash_cold_reset == nil, "duplicate RSE flash reset")
            platform.rse_pflash_cold_reset = {
                moduletype = "qemu_device_cold_reset";
                args = {"&rse_boot_flash_qemu"};
            }
            platform.rse_boot_flash_qemu = {
                moduletype = "pflash_cfi";
                args = {ctx.instances.rse_local, 1};
                num_blocks = RSE_BOOT_FLASH_SIZE / RSE_FLASH_SECTOR_SIZE;
                sector_length = RSE_FLASH_SECTOR_SIZE;
                width = 1;
                device_width = 1;
                max_device_width = 4;
                id0 = 0x89;
                id1 = 0x18;
                name = "apollo-rse-boot-flash";
                local_address = RSE_BOOT_FLASH_SECURE_BASE;
                local_priority = 10;
                local_cpu = "platform.rse_cpu_pass.cpu_0.cpu";
                program_ff_erases_sector = true;
                io_mode_only = true;
                target_socket = {
                    address = RSE_BOOT_FLASH_SECURE_BASE;
                    size = RSE_BOOT_FLASH_SIZE;
                    bind = "&cpu_0.router.initiator_socket";
                };
                log_level = 0;
            }
        end
    end

    function block.define_rom(ctx, platform)
        assert(
            platform.rse_rom == nil,
            "duplicate object: platform.rse_rom")
        platform.rse_rom = {
            moduletype = "gs_memory";
            read_only = true;
            shared_memory = true;
            shared_memory_prefix = "rse_rom_";
            target_socket = {
                address = RSE_ROM_SECURE_BASE;
                size = RSE_ROM_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

    end

    return block
end

return module
