-- Connect SoC reset signals to the QEMU device reset interface.
local adapters = {}

function adapters.configure_rse(ctx, platform)
    for gpio = 0,1 do
        local gpio_name = "rse_gpio_" .. gpio
        assert(
            platform[gpio_name .. "_cold_reset"] == nil,
            "duplicate object: platform." .. (gpio_name .. "_cold_reset"))
        platform[gpio_name .. "_cold_reset"] = {
            moduletype = "qemu_device_cold_reset";
            args = {"&platform." .. gpio_name};
        }
    end

    local cpu_pass = platform.rse_cpu_pass
    assert(
        cpu_pass.rse_nvic_cold_reset == nil,
        "duplicate object: platform.rse_nvic_cold_reset")
    cpu_pass.rse_nvic_cold_reset = {
        moduletype = "qemu_device_cold_reset";
        args = {"&cpu_0.cpu.nvic"};
    }
end

function adapters.configure_system_mgmt(ctx, platform)
    assert(platform.host_smd_gpio_cold_reset == nil, "duplicate Apollo object: host_smd_gpio_cold_reset")
    platform.host_smd_gpio_cold_reset = {
        moduletype = "qemu_device_cold_reset";
        args = {"&platform.host_smd_gpio"};
    }
end

return adapters
