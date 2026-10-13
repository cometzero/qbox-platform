-- SPDX-License-Identifier: BSD-3-Clause
-- Assemble the domain hardware without host services.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local memory = dofile(dir .. "memory.lua")
local power = dofile(dir .. "power.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local system = dofile(dir .. "system.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local mailbox = dofile(dir .. "mailbox.lua")
local cpu = dofile(dir .. "cpu.lua")

local module = {}

function module.create(ctx, runtime)
    local hardware = {
        memory = memory.create(ctx, runtime);
        power = power.create(ctx, runtime);
        interconnect = interconnect.create(ctx, runtime);
        system = system.create(ctx, runtime);
        io_peri = io_peri.create(ctx, runtime);
        mailbox = mailbox.create(ctx, runtime);
        cpu = cpu.create(ctx, runtime);
    }

    function hardware.define_host(ctx, platform)
        hardware.memory.define_host(ctx, platform)
        hardware.power.define_host(ctx, platform)
    end
    function hardware.define(ctx, platform)
        hardware.interconnect.define(ctx, platform)
        local instance = hardware.cpu.define_qemu_instance(ctx, platform)
        hardware.memory.define(ctx, platform)
        hardware.system.define_interrupt_controller(ctx, platform, instance)
        hardware.io_peri.define(ctx, platform)
        hardware.mailbox.define(ctx, platform)
        hardware.cpu.define(ctx, platform, instance)
    end

    return hardware
end

return module
