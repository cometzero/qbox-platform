-- SPDX-License-Identifier: BSD-3-Clause
local SMD_GPIO_PHYS_BASE = 0x20000D0310000
local SMD_GPIO_SIZE = 0x00001000
local SMD_GPIO_SPI = 193 -- Zero-based GIC SPI socket index.

local io_peri = {}

function io_peri.define(ctx, platform)
    local options = ctx.options
    local smd_gpio_init_inputs = ctx.getenv_number_or(
        "QBOX_APOLLO_SMD_GPIO_INIT_INPUTS", "0")
    assert(platform.host_smd_gpio == nil, "duplicate Apollo object: host_smd_gpio")
    platform.host_smd_gpio = {
        moduletype = "qemu_pl061";
        args = {"&platform.qemu_inst"};
        init_inputs = smd_gpio_init_inputs;
        pullups = 0;
        pulldowns = 0;
        mem = {
            address = SMD_GPIO_PHYS_BASE;
            size = SMD_GPIO_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        irq = options.enable_ap_cpus and {
            bind = "&ap_gic.spi_in_"..SMD_GPIO_SPI;
        } or nil;
    }

    assert(platform.host_smd_gpio_cold_reset == nil, "duplicate Apollo object: host_smd_gpio_cold_reset")
    platform.host_smd_gpio_cold_reset = {
        moduletype = "qemu_device_cold_reset";
        args = {"&platform.host_smd_gpio"};
    }
end

return io_peri
