-- Compose RSE hardware with its existing local QEMU execution container.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local cpu = dofile(dir .. "cpu.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local memory = dofile(dir .. "memory.lua")
local security = dofile(dir .. "security.lua")
local system = dofile(dir .. "system.lua")
local boot = dofile(dir .. "boot.lua")
local acceleration = dofile(dir .. "../../../vp/rse/acceleration.lua")
local qemu = dofile(dir .. "../../../vp/rse/qemu.lua")
local images = dofile(dir .. "../../../vp/rse/boot.lua")
local console = dofile(dir .. "../../../vp/rse/console.lua")

local module = {}

function module.create(ctx)
    local runtime = acceleration.create(ctx)
    local cpu = cpu.create(ctx)
    local interconnect = interconnect.create(ctx, runtime)
    local io_peri = io_peri.create(ctx, runtime)
    local memory = memory.create(ctx)
    local security = security.create(ctx, runtime)
    local system = system.create(ctx, runtime)
    local images = images.create(ctx)
    local qemu = qemu.create(ctx)
    local boot = boot.create(ctx)
    local console = console.create(ctx)
    local rse = {}

    function rse.define(ctx, platform)
        print("Apollo RSE QBox skeleton config running...")
        interconnect.define(ctx, platform)
        qemu.define(ctx, platform)
        io_peri.define(ctx, platform)
        system.define_reset(ctx, platform)
        boot.define_rom(ctx, platform)
        memory.define(ctx, platform)
        boot.define(ctx, platform)
        security.define(ctx, platform)
        cpu.define_registers(ctx, platform)
        security.define_timer_protection(ctx, platform)
        system.define(ctx, platform)
        system.define_mailbox(ctx, platform)
        console.define(ctx, platform)
        cpu.define(ctx, platform)
        -- Preserve the local QEMU execution container and every CCI path.
        local container = platform.rse_cpu_pass
        interconnect.define_local(ctx, container)
        qemu.define_local(ctx, container)
        system.define_local(ctx, container)
        boot.define_local(ctx, container)
        security.define_local(ctx, container)
        cpu.define_local(ctx, container)
        runtime.configure_cpu(ctx, container.cpu_0.cpu)
        images.load_images(ctx, platform)
        images.report(ctx, platform)
    end

    return rse
end

return module
