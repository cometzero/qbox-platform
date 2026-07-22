-- Apollo QVP full-system entrypoint.
--
-- The full-system wrapper composes subsystem-owned hardware blocks and then
-- enables live Safety Island paths according to QBOX_APOLLO_FULL_SI_MODE.

print("Apollo QVP full-system QBox config running...")

local function apollo_top()
    local str = debug.getinfo(2, "S").source:sub(2)
    if str:match("(.*/)")
    then
        return str:match("(.*/)")
    else
        return "./"
    end
end

local apollo_dir = apollo_top()
local machine_contract = dofile(apollo_dir.."hw-block/machine_contract.lua")
local machine = machine_contract.load(apollo_dir.."hw-block")
local config = dofile(apollo_dir.."hw-block/config.lua")
local fabric = dofile(apollo_dir.."hw-block/fabric.lua")
local ros = dofile(apollo_dir.."hw-block/ros.lua")
local system_mgmt = dofile(apollo_dir.."hw-block/system_mgmt.lua")
local rse = dofile(apollo_dir.."hw-block/rse.lua")
local ap_compute = dofile(apollo_dir.."hw-block/ap_compute.lua")
local si_cl0 = dofile(apollo_dir.."hw-block/si_cl0.lua")
local si_cl1 = dofile(apollo_dir.."hw-block/si_cl1.lua")

local ctx = config.create(apollo_dir, machine_contract, machine)
ctx.modules = {
    rse = rse;
    ap_compute = ap_compute;
    ros = ros;
    system_mgmt = system_mgmt;
    si_cl0 = si_cl0;
    si_cl1 = si_cl1;
}
ctx.ros = ros
ctx.system_mgmt = system_mgmt
ctx.rse = rse
ctx.ap_compute = ap_compute

platform = fabric.create(ctx)
rse.define(ctx, platform)
ap_compute.define(ctx, platform)
ros.define(ctx, platform)
system_mgmt.define(ctx, platform)
ap_compute.enable_ap_router(ctx, platform)
si_cl0.define(ctx, platform)
si_cl1.define(ctx, platform)

if ctx.apollo_live_cl0 then
    si_cl0.enable(ctx, platform)
end

if ctx.apollo_live_cl1 then
    si_cl1.enable(ctx, platform)
end

local timer_snapshot_enabled = ctx.getenv_bool_or(
    "QBOX_APOLLO_TIMER_SNAPSHOT", false)
if timer_snapshot_enabled then
    assert(enable_ap_cpus and ctx.apollo_live_cl0 and ctx.apollo_live_cl1,
           "timer snapshot requires AP, live SI0, and live SI1")
    platform.apollo_timer_snapshot = {
        moduletype = "apollo_timer_snapshot";
        args = {
            "&platform.host_css_counters_timers";
            "&platform.ap_cpu_0";
            "&platform.ap_timer_mem";
            "&platform.si_cl0_cpu_0";
            "&platform.si_cl0_timer_cntbase";
            "&platform.si_cl1_cpu_0";
            "&platform.rse_cpu_pass.rse_timer_0";
            "&platform.rse_cpu_pass.rse_timer_1";
            "&platform.rse_cpu_pass.rse_timer_2";
            "&platform.rse_cpu_pass.rse_timer_3";
        };
    }
end
