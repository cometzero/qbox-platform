-- SI CL0 host services; hardware composition is owned by soc/.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local qemu = dofile(dir .. "qemu.lua")
local options = dofile(dir .. "options.lua")
local boot = dofile(dir .. "boot.lua")
local console = dofile(dir .. "console.lua")
local artifacts = dofile(dir .. "../artifacts.lua")
local pmic = dofile(dir .. "../extensions/si-cl0-pmic-host.lua")
local vmcu = dofile(dir .. "../extensions/si-cl0-vmcu.lua")
local module = {}

function module.configure(ctx)
    ctx.config.si_cl0 = options.resolve(ctx)
end

function module.apply(ctx, platform)
    local runtime = ctx.config.si_cl0
    qemu.configure(ctx, platform)
    artifacts.si_cl0_memory(ctx, platform)
    platform.host_si_cl0_core0_ppu.power_on_load = {bind = "&si_cl0_loader.reset"}
    console.create(ctx, runtime).define(ctx, platform)
    platform.si_cl0_uart.backend_socket = {bind = "&si_cl0_console_file.biflow_socket"}
    boot.create(ctx, runtime).define_loader(ctx, platform, runtime.si_cl0_image)
    pmic.create(ctx, runtime).define(ctx, platform)
    vmcu.create(ctx, runtime).define(ctx, platform)
    boot.create(ctx, runtime).configure_cpu(ctx, platform.si_cl0_cpu_0)
end

return module
