-- SPDX-License-Identifier: BSD-3-Clause
-- Complete Apollo hardware construction, independent of profiles and host input.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local config = dofile(dir .. "config.lua")
local reset = dofile(dir .. "hw-block/fabric/reset.lua")
local adapters = dofile(dir .. "reset-adapters.lua")
local soc = {
    rse = dofile(dir .. "hw-block/rse/rse.lua");
    ap_compute = dofile(dir .. "hw-block/ap_compute/ap_compute.lua");
    si_cl0 = dofile(dir .. "hw-block/si_cl0/si_cl0.lua");
    si_cl1 = dofile(dir .. "hw-block/si_cl1/si_cl1.lua");
    ros = dofile(dir .. "hw-block/ros/ros.lua");
    system_mgmt = dofile(dir .. "hw-block/system_mgmt/system_mgmt.lua");
    fabric = dofile(dir .. "hw-block/fabric/fabric.lua");
}

function soc.default_config()
    return config.create(dir .. "../")
end

function soc.load(ctx)
    config.validate(ctx)
    ctx.options.rse_local_peripherals =
        ctx.options.rse_local_crypto or ctx.options.rse_local_boot_flash
    reset(ctx.options)
    ctx.options.ap_system_reset_targets = ctx.options.ap_system_reset_bind_targets()
    ctx.hardware = {
        rse = soc.rse.create(ctx, ctx.config.rse);
        ap_compute = soc.ap_compute.create(ctx);
        si_cl0 = soc.si_cl0.create(ctx, ctx.config.si_cl0);
        si_cl1 = soc.si_cl1.create(ctx, ctx.config.si_cl1);
        ros = soc.ros.create(ctx);
        system_mgmt = soc.system_mgmt.create(ctx);
    }
end

function soc.define_ap(ctx, platform)
    ctx.hardware.ap_compute.define(ctx, platform)
    ctx.hardware.ros.define(ctx, platform)
end

function soc.define(ctx, platform)
    soc.fabric.define(ctx, platform)
    ctx.hardware.rse.define(ctx, platform)
    adapters.configure_rse(ctx, platform)
    soc.define_ap(ctx, platform)
    ctx.hardware.system_mgmt.define(ctx, platform)
    adapters.configure_system_mgmt(ctx, platform)
    ctx.hardware.ap_compute.connect(ctx, platform)
    ctx.hardware.si_cl0.define_host(ctx, platform)
    ctx.hardware.si_cl1.define_host(ctx, platform)
    ctx.hardware.si_cl0.define(ctx, platform)
    ctx.hardware.si_cl1.define(ctx, platform)
end

function soc.create(ctx)
    ctx = ctx or soc.default_config()
    assert(ctx.options.enable_ap_cpus, "complete Apollo SoC requires AP CPUs")
    soc.load(ctx)
    local platform = {moduletype = "Container"}
    soc.define(ctx, platform)
    return platform
end

return soc
