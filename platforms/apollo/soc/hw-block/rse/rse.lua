-- Compose RSE hardware with its existing local QEMU execution container.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local hardware = dofile(dir .. "hardware.lua")
local cpu = dofile(dir .. "cpu.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local reset = dofile(dir .. "reset.lua")
local memory = dofile(dir .. "memory.lua")
local security = dofile(dir .. "security.lua")
local syscore = dofile(dir .. "syscore.lua")
local mailbox = dofile(dir .. "mailbox.lua")
local acceleration = dofile(dir .. "../../../vp/rse/acceleration.lua")
local qemu = dofile(dir .. "../../../vp/rse/qemu.lua")
local boot = dofile(dir .. "../../../vp/rse/boot.lua")
local console = dofile(dir .. "../../../vp/rse/console.lua")

local module = {}

function module.create(ctx)
    local hw = hardware.create(ctx)
    local runtime = acceleration.create(ctx)
    local cpu = cpu.create(ctx, hw, runtime)
    local interconnect = interconnect.create(ctx, hw, runtime)
    local io_peri = io_peri.create(ctx, hw, runtime)
    local reset = reset.create(ctx, hw, runtime)
    local memory = memory.create(ctx, hw, runtime)
    local security = security.create(ctx, hw, runtime)
    local syscore = syscore.create(ctx, hw, runtime)
    local mailbox = mailbox.create(ctx, hw, runtime)
    local qemu = qemu.create(ctx, hw, runtime)
    local boot = boot.create(ctx, hw, runtime)
    local console = console.create(ctx, hw, runtime)
    local rse = {}

    function rse.define(ctx, platform)
        print("Apollo RSE QBox skeleton config running...")
        interconnect.define(ctx, platform)
        qemu.define(ctx, platform)
        io_peri.define(ctx, platform)
        reset.define(ctx, platform)
        memory.define(ctx, platform)
        boot.define(ctx, platform)
        security.define(ctx, platform)
        syscore.define(ctx, platform)
        mailbox.define(ctx, platform)
        console.define(ctx, platform)
        cpu.define(ctx, platform)
        -- Preserve the local QEMU execution container and every CCI path.
        local container = platform.rse_cpu_pass
        interconnect.define_local(ctx, container)
        qemu.define_local(ctx, container)
        syscore.define_local(ctx, container)
        boot.define_local(ctx, container)
        security.define_local(ctx, container)
        cpu.define_local(ctx, container)
        runtime.configure_cpu(ctx, container.cpu_0.cpu)
        boot.load_images(ctx, platform)
        boot.report(ctx, platform)
    end

    return rse
end

return module
