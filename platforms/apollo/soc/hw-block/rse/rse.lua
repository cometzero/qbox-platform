-- SPDX-License-Identifier: BSD-3-Clause
-- Assemble the domain hardware without host services.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local cpu = dofile(dir .. "cpu.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local memory = dofile(dir .. "memory.lua")
local security = dofile(dir .. "security.lua")
local system = dofile(dir .. "system.lua")
local boot = dofile(dir .. "boot.lua")

local module = {}

function module.create(ctx, runtime)
    local hardware = {
        cpu = cpu.create(ctx);
        interconnect = interconnect.create(ctx, runtime);
        io_peri = io_peri.create(ctx, runtime);
        memory = memory.create(ctx);
        security = security.create(ctx, runtime);
        system = system.create(ctx, runtime);
        boot = boot.create(ctx);
    }

    function hardware.define(ctx, platform)
        local cpu, system = hardware.cpu, hardware.system
        hardware.interconnect.define(ctx, platform)
        cpu.define_instance(ctx, platform)
        hardware.io_peri.define(ctx, platform)
        system.define_reset(ctx, platform)
        hardware.boot.define_rom(ctx, platform)
        hardware.memory.define(ctx, platform)
        hardware.boot.define(ctx, platform)
        hardware.security.define(ctx, platform)
        cpu.define_registers(ctx, platform)
        hardware.security.define_timer_protection(ctx, platform)
        system.define(ctx, platform)
        system.define_mailbox(ctx, platform)
        cpu.define(ctx, platform)
        local container = platform.rse_cpu_pass
        hardware.interconnect.define_local(ctx, container)
        cpu.define_local_instance(ctx, container)
        system.define_local(ctx, container)
        hardware.boot.define_local(ctx, container)
        hardware.security.define_local(ctx, container)
        cpu.define_local(ctx, container)
    end

    return hardware
end

return module
