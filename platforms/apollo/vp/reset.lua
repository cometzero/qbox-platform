-- VP reset participants augment the hardware reset sequence in place.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local configure_hardware = dofile(dir .. "../soc/hw-block/fabric/reset.lua")

return function(ctx, platform)
    local options = {}
    for key, value in pairs(ctx.options) do options[key] = value end
    configure_hardware(options)
    local hardware_cold_reset = options.ap_cold_reset_bind_targets
    local hardware_system_reset = options.apollo_system_reset_bind_targets

    function options.ap_cold_reset_bind_targets()
        local targets = hardware_cold_reset()
        targets = targets:gsub("&host_ap_bl2_header_sram%.reset", "&ap_bl2_reset_loader.reset;%0")
        if options.runtime_injection_enabled then
            targets = targets .. ";&ap_i2c5_irq_fault.reset"
        end
        return targets
    end

    function options.apollo_system_reset_bind_targets()
        local targets = hardware_system_reset()
        targets = targets:gsub("&si_cl0_qemu_inst%.reset", table.concat({
            "%0";
            "&si_cl0_dw_i2c_0.reset";
            "&si_cl0_pmic_gpio_cold_reset.reset";
            "&si_cl0_pmic_gpio.reset";
            "&si_cl0_vmcu_uart.reset";
            "&si_cl0_vmcu_gpio_cold_reset.reset";
            "&si_cl0_vmcu_gpio.reset";
        }, ";"))
        if options.runtime_injection_enabled then
            targets = targets .. ";&apollo_runtime_injection.reset"
        end
        return targets
    end
    if platform.ap_cold_reset_fanout then
        platform.ap_cold_reset_fanout.reset_out.bind = options.ap_cold_reset_bind_targets()
    end
    if platform.apollo_system_reset_fanout then
        platform.apollo_system_reset_fanout.reset_out.bind = options.apollo_system_reset_bind_targets()
    end
    if platform.host_ap_si_scmi_mhu_pbx then
        platform.host_ap_si_scmi_mhu_pbx.system_reset.bind = options.ap_system_reset_bind_targets()
    end

end
