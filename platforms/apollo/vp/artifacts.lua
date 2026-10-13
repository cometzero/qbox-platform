-- Host files and persistence policy for SoC flash and shared SRAM.
local artifacts = {}

function artifacts.rse_boot_flash(ctx, platform)
    local options = ctx.options
    local flash = platform.rse_boot_flash
    if flash then
        flash.backing_file = options.flash_writeback and options.rse_flash or ""
        flash.defer_backing_write = true
        flash.defer_backing_flush_interval = options.flash_defer_backing_flush_interval
        flash.stats_file = options.rse_boot_flash_stats_file
        flash.stats_interval = options.flash_stats_interval
        flash.load = {bin_file = options.rse_flash, offset = 0}
    end

    local qemu_flash = platform.rse_boot_flash_qemu
    if qemu_flash then
        qemu_flash.blkdev_str = "file=" .. options.rse_flash ..
            ",format=raw,if=none,cache=writeback" ..
            (options.flash_writeback and "" or ",snapshot=on")
        qemu_flash.defer_backing_write = true
        qemu_flash.defer_backing_flush_interval = 65536
        qemu_flash.defer_backing_flush_delay_ms = 25
    end
end

function artifacts.si_cl0_memory(ctx, platform)
    local options = ctx.options
    local memory = platform.host_si_cl0_sram
    memory.map_file = options.host_si_cl0_sram_map_file
    memory.shared_memory = options.host_sram_shared_memory_enabled(memory.map_file)
    memory.shared_memory_prefix = "ra-si0-"
    memory.init_mem = memory.map_file == ""
end

function artifacts.si_cl1_memory(ctx, platform)
    local options = ctx.options
    local memory = platform.host_si_cl1_sram
    memory.map_file = options.host_si_cl1_sram_map_file
    memory.shared_memory = options.host_sram_shared_memory_enabled(memory.map_file)
    memory.shared_memory_prefix = "ra-si1-"
    memory.init_mem = memory.map_file == ""
end

function artifacts.rse_security(ctx, platform)
    local options = ctx.options
    platform.rse_lcm_regs.otp_image = options.rse_otp
    platform.rse_lcm_regs.otp_writeback = options.rse_otp_writeback
    if platform.rse_kmu_regs then
        platform.rse_kmu_regs.otp_image = options.rse_otp
    end
    if platform.rse_cpu_pass.rse_kmu_regs then
        platform.rse_cpu_pass.rse_kmu_regs.otp_image = options.rse_otp
    end
end

return artifacts
