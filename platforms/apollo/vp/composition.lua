-- Apply the QVP profile to the complete SoC model.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local soc = dofile(dir .. "../soc/apollo.lua")
local rse = dofile(dir .. "rse/rse.lua")
local ap = dofile(dir .. "ap_compute/ap_compute.lua")
local cl0 = dofile(dir .. "si_cl0/si_cl0.lua")
local cl1 = dofile(dir .. "si_cl1/si_cl1.lua")
local ros = dofile(dir .. "ros/ros.lua")
local reset = dofile(dir .. "reset.lua")
local si_reset = dofile(dir .. "si_cl1/reset.lua")
local composition = {}

function composition.load(ctx)
    ap.configure(ctx)
    rse.configure(ctx)
    cl0.configure(ctx)
    cl1.configure(ctx)
    soc.load(ctx)
end

function composition.define_ap(ctx, platform)
    soc.define_ap(ctx, platform)
    reset(ctx, platform)
    ap.apply(ctx, platform)
    ros.apply(ctx, platform)
end

function composition.define(ctx, platform)
    soc.define(ctx, platform)
    reset(ctx, platform)
    rse.apply(ctx, platform)
    ap.apply(ctx, platform)
    ros.apply(ctx, platform)
    cl0.apply(ctx, platform)
    cl1.apply(ctx, platform)
    si_reset.finalize_reset_order(platform)
end

return composition
