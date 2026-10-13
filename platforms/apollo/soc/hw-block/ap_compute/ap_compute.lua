-- SPDX-License-Identifier: BSD-3-Clause
-- Assemble the domain hardware without host services.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local system = dofile(dir .. "system.lua")
local cpu = dofile(dir .. "cpu.lua")
local pcie = dofile(dir .. "pcie.lua")
local memory = dofile(dir .. "memory.lua")
local safety = dofile(dir .. "safety.lua")
local boot_devices = dofile(dir .. "boot.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local interconnect = dofile(dir .. "interconnect.lua")

local module = {}

function module.create(ctx, runtime)
    local hardware = {
        system = system;
        cpu = cpu;
        pcie = pcie;
        memory = memory;
        safety = safety;
        boot_devices = boot_devices;
        io_peri = io_peri;
        interconnect = interconnect;
    }

    function hardware.define(ctx, platform)
        cpu.define_instance(ctx, platform)
        system.define_cpu_reset(ctx, platform)
        system.define_system_reset(ctx, platform)
        system.define_reset(ctx, platform)
        cpu.define_initiator(ctx, platform)
        pcie.define(ctx, platform)
        memory.define(ctx, platform)
        boot_devices.define(ctx, platform)
        system.define_interrupts(ctx, platform)
        system.define_iommu(ctx, platform)
        system.define_watchdog(ctx, platform)
        io_peri.define(ctx, platform)
        system.define_timers(ctx, platform)
        system.define(ctx, platform)
        system.define_message_register(ctx, platform)
        safety.define(ctx, platform)
        cpu.define(ctx, platform)
    end
    hardware.connect = interconnect.enable_ap_router

    return hardware
end

return module
