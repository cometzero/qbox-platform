-- Compose CL1 hardware, then finalize the ordered SI reset fanout.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local hardware = dofile(dir .. "hardware.lua")
local memory = dofile(dir .. "memory.lua")
local power = dofile(dir .. "power.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local syscore = dofile(dir .. "syscore.lua")
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
    local irq_routes = assert(ctx.modules.si_cl0, "SI CL0 must provide shared IRQ routes")
    local memory = memory.create(ctx, hardware, runtime, irq_routes)
    local power = power.create(ctx, hardware, runtime, irq_routes)
    local interconnect = interconnect.create(ctx, hardware, runtime, irq_routes)
    local syscore = syscore.create(ctx, hardware, runtime, irq_routes)
    local io_peri = io_peri.create(ctx, hardware, runtime, irq_routes)
    local mailbox = mailbox.create(ctx, hardware, runtime, irq_routes)
    local cpu = cpu.create(ctx, hardware, runtime, irq_routes)
    local qemu = qemu.create(ctx, hardware, runtime, irq_routes)
    local boot = boot.create(ctx, hardware, runtime, irq_routes)
    local console = console.create(ctx, hardware, runtime, irq_routes)
    local si_cl1 = {
        define_qemu_instance = qemu.define_qemu_instance;
        define_interrupt_controller = syscore.define_interrupt_controller;
        finalize_reset_order = power.finalize_reset_order;
    }

    function si_cl1.hipc_shared_window()
        return hardware.SI_CL1_HIPC_SHARED_BASE, hardware.SI_CL1_HIPC_SHARED_SIZE
    end

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
        syscore.define_interrupt_controller(ctx, platform, instance)
        console.define(ctx, platform)
        io_peri.define(ctx, platform)
        mailbox.define(ctx, platform)
        si_cl1.define_loader_and_cpus(ctx, platform, instance, runtime.si_cl1_image)
        power.finalize_reset_order(platform)
        print("si-cl1 image: " .. runtime.si_cl1_image)
        print("si-cl1 log:   " .. runtime.si_cl1_log)
        print("si-cl1 entry: 0x" .. string.format("%x", runtime.entry))
    end

    return si_cl1
end

return module
