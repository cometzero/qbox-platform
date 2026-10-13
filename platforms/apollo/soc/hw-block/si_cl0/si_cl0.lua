-- SPDX-License-Identifier: BSD-3-Clause
-- Assemble the domain hardware without host services.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local memory = dofile(dir .. "memory.lua")
local power = dofile(dir .. "power.lua")
local mailbox = dofile(dir .. "mailbox.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local system = dofile(dir .. "system.lua")
local safety = dofile(dir .. "safety.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local cpu = dofile(dir .. "cpu.lua")

local module = {}

function module.create(ctx, runtime)
    local hardware = {
        memory = memory.create(ctx, runtime);
        power = power.create(ctx, runtime);
        mailbox = mailbox.create(ctx, runtime);
        interconnect = interconnect.create(ctx, runtime);
        system = system.create(ctx, runtime);
        safety = safety.create(ctx, runtime);
        io_peri = io_peri.create(ctx, runtime);
        cpu = cpu.create(ctx, runtime);
    }

    function hardware.define_host(ctx, platform)
        hardware.memory.define_host(ctx, platform)
        hardware.power.define_host(ctx, platform)
    end
    function hardware.define(ctx, platform)
        hardware.interconnect.define_router(ctx, platform)
        hardware.mailbox.define(ctx, platform)
        hardware.interconnect.connect(ctx, platform)
        hardware.interconnect.define_cmn(ctx, platform)
        hardware.system.define_multiview(ctx, platform)
        local instance = hardware.cpu.define_qemu_instance(ctx, platform)
        hardware.memory.define(ctx, platform)
        hardware.system.define(ctx, platform)
        hardware.safety.define_monitors(ctx, platform)
        hardware.safety.define(ctx, platform)
        hardware.interconnect.define_windows(ctx, platform)
        hardware.power.define(ctx, platform)
        hardware.system.define_interrupt_controller(ctx, platform, instance)
        hardware.io_peri.define(ctx, platform)
        hardware.cpu.define(ctx, platform, instance)
    end

    return hardware
end

return module
