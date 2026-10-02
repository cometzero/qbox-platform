local module = {}

function module.create(ctx, _hardware, _runtime, _interrupts)
    local rse_flash_backend = ctx.options.rse_flash_backend
    local rse_local_boot_flash = ctx.options.rse_local_boot_flash
    local rse_smd_counter_mirror = ctx.options.rse_smd_counter_mirror

    local block = {}

    function block.define(ctx, platform)
        local rse_sys_rss_reset_targets = {
            "&rse_cpu_pass.cpu_0.cpu.reset";
            "&rse_cpu_pass.rse_nvic_cold_reset.reset";
            "&rse_cpu_pass.rse_timer_0.reset";
            "&rse_cpu_pass.rse_timer_1.reset";
            "&rse_cpu_pass.rse_timer_2.reset";
            "&rse_gpio_0_cold_reset.reset";
            "&rse_gpio_0.reset";
            "&rse_gpio_1_cold_reset.reset";
            "&rse_gpio_1.reset";
        }
        if rse_local_boot_flash and rse_flash_backend == "qemu-cfi-local" then
            rse_sys_rss_reset_targets[#rse_sys_rss_reset_targets + 1] =
                "&rse_cpu_pass.rse_pflash_cold_reset.reset"
        end

        local rse_aon_reset_targets = {
            "&rse_cpu_pass.rse_timer_3.reset";
        }
        if not rse_smd_counter_mirror then
            rse_aon_reset_targets[#rse_aon_reset_targets + 1] =
                "&rse_cpu_pass.rse_local_system_counter.reset"
        end
        assert(
            platform.rse_sys_rss_reset_fanout == nil,
            "duplicate object: platform.rse_sys_rss_reset_fanout")
        platform.rse_sys_rss_reset_fanout = {
            moduletype = "reset_fanout";
            reset_out = {bind = table.concat(rse_sys_rss_reset_targets, ";")};
            log_level = 0;
        }

        assert(
            platform.rse_aon_reset_fanout == nil,
            "duplicate object: platform.rse_aon_reset_fanout")
        platform.rse_aon_reset_fanout = {
            moduletype = "reset_fanout";
            reset_out = {bind = table.concat(rse_aon_reset_targets, ";")};
            log_level = 0;
        }
    end

    return block
end

return module
