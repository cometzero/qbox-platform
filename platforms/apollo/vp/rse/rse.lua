-- RSE boot artifacts, execution tuning and host console overrides.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local acceleration = dofile(dir .. "acceleration.lua")
local qemu = dofile(dir .. "qemu.lua")
local images = dofile(dir .. "boot.lua")
local console = dofile(dir .. "console.lua")
local artifacts = dofile(dir .. "../artifacts.lua")
local module = {}

function module.configure(ctx)
    ctx.rse_acceleration = acceleration.create(ctx)
    ctx.config.rse.addresses = ctx.rse_acceleration.addresses
end

function module.apply(ctx, platform)
    qemu.configure(ctx, platform)
    platform.rse_host_uart0_s.backend_socket = {bind = "&rse_uart_file.biflow_socket"}
    console.create(ctx).define(ctx, platform)
    local container = platform.rse_cpu_pass
    artifacts.rse_boot_flash(ctx, platform)
    artifacts.rse_boot_flash(ctx, container)
    ctx.rse_acceleration.configure_cpu(ctx, container.cpu_0.cpu)
    artifacts.rse_security(ctx, platform)
    local boot = images.create(ctx)
    boot.load_images(ctx, platform)
    boot.report(ctx, platform)
end

return module
