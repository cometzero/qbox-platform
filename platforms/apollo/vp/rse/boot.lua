-- RSE image files and selected local/native boot-flash execution backend.
local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local AP_BL2_ELF = ctx.options.AP_BL2_ELF
    local AP_NUM_CPUS = ctx.options.AP_NUM_CPUS
    local ap_flash = ctx.options.ap_flash
    local boot_flash_dmi = ctx.options.boot_flash_dmi
    local boot_flash_dmi_ranges = ctx.options.boot_flash_dmi_ranges
    local boot_flash_trace = ctx.options.boot_flash_trace
    local boot_flash_trace_limit = ctx.options.boot_flash_trace_limit
    local flash_defer_backing_flush_interval = ctx.options.flash_defer_backing_flush_interval
    local flash_stats_interval = ctx.options.flash_stats_interval
    local flash_writeback = ctx.options.flash_writeback
    local primary_console_log = ctx.options.primary_console_log
    local provisioning_bundle = ctx.options.provisioning_bundle
    local rse_boot_flash_stats_file = ctx.options.rse_boot_flash_stats_file
    local rse_flash = ctx.options.rse_flash
    local rse_flash_backend = ctx.options.rse_flash_backend
    local rse_local_boot_flash = ctx.options.rse_local_boot_flash
    local rse_log = ctx.options.rse_log
    local rse_lsc_input_hz = ctx.options.rse_lsc_input_hz
    local rse_otp = ctx.options.rse_otp
    local rse_rom = ctx.options.rse_rom
    local rse_smd_counter_mirror = ctx.options.rse_smd_counter_mirror
    local rse_vmaddrwidth = ctx.options.rse_vmaddrwidth
    local secure_console_log = ctx.options.secure_console_log
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local RSE_IRQ = hardware.RSE_IRQ
    local RSE_HW = hardware.RSE_HW

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
                size = RSE_SIZE.boot_flash;
                sector_size = RSE_SIZE.flash_sector;
                backing_file = flash_writeback and rse_flash or "";
                defer_backing_write = true;
                defer_backing_flush_interval = flash_defer_backing_flush_interval;
                stats_file = rse_boot_flash_stats_file;
                stats_interval = flash_stats_interval;
                target_socket = {
                    address = RSE_ADDRESS.boot_flash_secure;
                    size = RSE_SIZE.boot_flash;
                    bind = "&rse_router.initiator_socket";
                };
                load = {bin_file = rse_flash, offset = 0};
                log_level = 0;
            }
        end
    end

    function block.define_local(ctx, platform)
        assert(
            platform.rse_pflash_cold_reset == nil,
            "duplicate object: platform.rse_pflash_cold_reset")
        if rse_local_boot_flash and rse_flash_backend == "qemu-cfi-local" then
            platform.rse_pflash_cold_reset = {
                moduletype = "qemu_device_cold_reset";
                args = {"&rse_boot_flash_qemu"};
            }
        end

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
                size = RSE_SIZE.boot_flash;
                sector_size = RSE_SIZE.flash_sector;
                backing_file = flash_writeback and rse_flash or "";
                defer_backing_write = true;
                defer_backing_flush_interval = flash_defer_backing_flush_interval;
                stats_file = rse_boot_flash_stats_file;
                stats_interval = flash_stats_interval;
                target_socket = {
                    address = RSE_ADDRESS.boot_flash_secure;
                    size = RSE_SIZE.boot_flash;
                    bind = "&cpu_0.router.initiator_socket";
                };
                load = {bin_file = rse_flash, offset = 0};
                log_level = 0;
            }
        end

        assert(
            platform.rse_boot_flash_qemu == nil,
            "duplicate object: platform.rse_boot_flash_qemu")
        if rse_local_boot_flash and rse_flash_backend == "qemu-cfi-local" then
            platform.rse_boot_flash_qemu = {
                moduletype = "pflash_cfi";
                args = {"&qemu_inst", 1};
                blkdev_str = "file=" .. rse_flash ..
                    ",format=raw,if=none,cache=writeback" ..
                    (flash_writeback and "" or ",snapshot=on");
                num_blocks = RSE_SIZE.boot_flash / RSE_SIZE.flash_sector;
                sector_length = RSE_SIZE.flash_sector;
                width = 1;
                device_width = 1;
                max_device_width = 4;
                id0 = 0x89;
                id1 = 0x18;
                name = "apollo-rse-boot-flash";
                local_address = RSE_ADDRESS.boot_flash_secure;
                local_priority = 10;
                local_cpu = "platform.rse_cpu_pass.cpu_0.cpu";
                program_ff_erases_sector = true;
                io_mode_only = true;
                defer_backing_write = true;
                defer_backing_flush_interval = 65536;
                defer_backing_flush_delay_ms = 25;
                target_socket = {
                    address = RSE_ADDRESS.boot_flash_secure;
                    size = RSE_SIZE.boot_flash;
                    bind = "&cpu_0.router.initiator_socket";
                };
                log_level = 0;
            }
        end
    end

    function block.load_images(ctx, platform)
        platform.rse_rom.load = {bin_file = rse_rom; offset = 0;}
        platform.rse_vm1.load = {bin_file = provisioning_bundle; offset = RSE_SIZE.provisioning_offset;}
        platform.rse_otp_wrapper.load = {bin_file = rse_otp; offset = 0;}
    end

    function block.report(ctx, platform)
        print("rse rom:      " .. rse_rom)
        print("rse flash:    " .. rse_flash)
        print("rse flash backend: " .. rse_flash_backend)
        print("rse otp:      " .. rse_otp)
        print("ap flash:     " .. ap_flash)
        print("ap bl2 elf:   " .. AP_BL2_ELF)
        print("provisioning: " .. provisioning_bundle)
        print("rse log:      " .. rse_log)
        print("secure log:   " .. secure_console_log)
        print("primary log:  " .. primary_console_log)
        print("ap cpus:      " .. tostring(AP_NUM_CPUS))
        print("rse rom base: 0x" .. string.format("%x", RSE_ADDRESS.rom_secure))
        print("rse vmaddrwidth: " .. tostring(rse_vmaddrwidth))
        print("rse vm size:  0x" .. string.format("%x", RSE_SIZE.vm))
        print("rse SMD counter mirror: " .. tostring(rse_smd_counter_mirror))
        print("rse LSC input Hz: " .. tostring(rse_lsc_input_hz))
    end

    return block
end

return module
