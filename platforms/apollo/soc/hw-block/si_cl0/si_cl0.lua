-- Compose CL0 hardware and QVP services in the established enable sequence.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local hardware = dofile(dir .. "hardware.lua")
local irq_routes = dofile(dir .. "../si_common/interrupts.lua")
local memory = dofile(dir .. "memory.lua")
local power = dofile(dir .. "power.lua")
local mailbox = dofile(dir .. "mailbox.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local syscore = dofile(dir .. "syscore.lua")
local interrupts = dofile(dir .. "interrupts.lua")
local safety = dofile(dir .. "safety.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local cpu = dofile(dir .. "cpu.lua")
local qemu = dofile(dir .. "../../../vp/si/cl0-qemu.lua")
local options = dofile(dir .. "../../../vp/si/cl0-options.lua")
local boot = dofile(dir .. "../../../vp/si/cl0-boot.lua")
local console = dofile(dir .. "../../../vp/si/cl0-console.lua")
local pmic_host = dofile(dir .. "../../../vp/extensions/si-cl0-pmic-host.lua")
local vmcu = dofile(dir .. "../../../vp/extensions/si-cl0-vmcu.lua")

local module = {}

function module.create(ctx)
    local runtime = options.resolve(ctx)
    local memory = memory.create(ctx, hardware, runtime, irq_routes)
    local power = power.create(ctx, hardware, runtime, irq_routes)
    local mailbox = mailbox.create(ctx, hardware, runtime, irq_routes)
    local interconnect = interconnect.create(ctx, hardware, runtime, irq_routes)
    local syscore = syscore.create(ctx, hardware, runtime, irq_routes)
    local interrupts = interrupts.create(ctx, hardware, runtime, irq_routes)
    local safety = safety.create(ctx, hardware, runtime, irq_routes)
    local io_peri = io_peri.create(ctx, hardware, runtime, irq_routes)
    local cpu = cpu.create(ctx, hardware, runtime, irq_routes)
    local qemu = qemu.create(ctx, hardware, runtime, irq_routes)
    local boot = boot.create(ctx, hardware, runtime, irq_routes)
    local console = console.create(ctx, hardware, runtime, irq_routes)
    local pmic_host = pmic_host.create(ctx, hardware, runtime, irq_routes)
    local vmcu = vmcu.create(ctx, hardware, runtime, irq_routes)
    local si_cl0 = {
        validate_irq_routes = irq_routes.validate_irq_routes;
        irq_route = irq_routes.irq_route;
        active_irq_routes = irq_routes.active_irq_routes;
        ppi_index = irq_routes.ppi_index;
        spi_target = irq_routes.spi_target;
        define_qemu_instance = qemu.define_qemu_instance;
        define_interrupt_controller = syscore.define_interrupt_controller;
    }

    function si_cl0.define_loader_and_cpu(ctx, platform, qemu_instance, image)
        boot.define_loader(ctx, platform, image)
        cpu.define(ctx, platform, qemu_instance)
        boot.configure_cpu(ctx, platform.si_cl0_cpu_0)
    end

    function si_cl0.define(ctx, platform)
        memory.define_host(ctx, platform)
        power.define_host(ctx, platform)
    end

    function si_cl0.enable(ctx, platform)
        print("Apollo FVP live SI CL0 block enabled...")
        interconnect.define_router(ctx, platform)
        pmic_host.define(ctx, platform)
        mailbox.define(ctx, platform)
        interconnect.connect(ctx, platform)
        interconnect.define_cmn(ctx, platform)
        interrupts.define_multiview(ctx, platform)
        local instance = qemu.define_qemu_instance(ctx, platform)
        memory.define(ctx, platform)
        syscore.define(ctx, platform)
        safety.define(ctx, platform)
        interconnect.define_windows(ctx, platform)
        power.define(ctx, platform)
        syscore.define_interrupt_controller(ctx, platform, instance)
        console.define(ctx, platform)
        io_peri.define(ctx, platform)
        vmcu.define(ctx, platform)
        si_cl0.define_loader_and_cpu(ctx, platform, instance, runtime.si_cl0_image)
        print("si-cl0 image: " .. runtime.si_cl0_image)
        print("si-cl0 log:   " .. runtime.si_cl0_log)
        print("si-cl0 entry: 0x" .. string.format("%x", runtime.entry))
    end

    return si_cl0
end

return module
