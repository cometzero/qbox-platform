-- Compose CL1 hardware, then finalize the ordered SI reset fanout.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local memory = dofile(dir .. "memory.lua")
local power = dofile(dir .. "power.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local system = dofile(dir .. "system.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local mailbox = dofile(dir .. "mailbox.lua")
local cpu = dofile(dir .. "cpu.lua")
local qemu = dofile(dir .. "../../../vp/si/cl1-qemu.lua")
local options = dofile(dir .. "../../../vp/si/cl1-options.lua")
local boot = dofile(dir .. "../../../vp/si/cl1-boot.lua")
local console = dofile(dir .. "../../../vp/si/cl1-console.lua")

local module = {}

function module.create(ctx)
    local runtime = options.resolve(ctx)

    local memory = memory.create(ctx, runtime)
    local power = power.create(ctx, runtime)
    local interconnect = interconnect.create(ctx, runtime)
    local system = system.create(ctx, runtime)
    local io_peri = io_peri.create(ctx, runtime)
    local mailbox = mailbox.create(ctx, runtime)
    local cpu = cpu.create(ctx, runtime)
    local qemu = qemu.create(ctx, runtime)
    local boot = boot.create(ctx, runtime)
    local console = console.create(ctx, runtime)
    local si_cl1 = {
        define_qemu_instance = qemu.define_qemu_instance;
        define_interrupt_controller = system.define_interrupt_controller;
        finalize_reset_order = system.finalize_reset_order;
        hipc_shared_window = interconnect.hipc_shared_window;
    }

    function si_cl1.define_loader_and_cpus(ctx, platform, qemu_instance, image)
        boot.define_loader(ctx, platform, image)
        cpu.define(ctx, platform, qemu_instance)
    end

    function si_cl1.define(ctx, platform)
        memory.define_host(ctx, platform)
        power.define_host(ctx, platform)
    end

    function si_cl1.enable(ctx, platform)
        print("Apollo FVP live SI CL1 block enabled...")
        interconnect.define(ctx, platform)
        local instance = qemu.define_qemu_instance(ctx, platform)
        memory.define(ctx, platform)
        system.define_interrupt_controller(ctx, platform, instance)
        console.define(ctx, platform)
        io_peri.define(ctx, platform)
        mailbox.define(ctx, platform)
        si_cl1.define_loader_and_cpus(ctx, platform, instance, runtime.si_cl1_image)
        system.finalize_reset_order(platform)
        print("si-cl1 image: " .. runtime.si_cl1_image)
        print("si-cl1 log:   " .. runtime.si_cl1_log)
        print("si-cl1 entry: 0x" .. string.format("%x", runtime.entry))
    end

    return si_cl1
end

return module
