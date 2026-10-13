-- SI CL1 host services; hardware composition is owned by soc/.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local qemu = dofile(dir .. "qemu.lua")
local options = dofile(dir .. "options.lua")
local boot = dofile(dir .. "boot.lua")
local console = dofile(dir .. "console.lua")
local artifacts = dofile(dir .. "../artifacts.lua")
local module = {}

function module.configure(ctx)
    ctx.config.si_cl1 = options.resolve(ctx)
end

function module.apply(ctx, platform)
    local runtime = ctx.config.si_cl1
    qemu.configure(ctx, platform)
    artifacts.si_cl1_memory(ctx, platform)
    platform.host_si_cl1_clus_ppu.power_on_load = {bind = "&si_cl1_loader.reset"}
    console.create(ctx, runtime).define(ctx, platform)
    platform.si_cl1_uart.backend_socket = {bind = "&si_cl1_console_file.biflow_socket"}
    boot.create(ctx, runtime).define_loader(ctx, platform, runtime.si_cl1_image)
end

return module
