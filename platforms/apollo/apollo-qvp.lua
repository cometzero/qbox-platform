-- Apollo QVP full-system entrypoint.
--
-- The full-system wrapper composes subsystem-owned hardware blocks, including
-- the real Safety Island CL0 and CL1 firmware domains.

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
local config = dofile(apollo_dir.."hw-block/config.lua")
local fabric = dofile(apollo_dir.."hw-block/fabric.lua")
local ros = dofile(apollo_dir.."hw-block/ros.lua")
local system_mgmt = dofile(apollo_dir.."hw-block/system_mgmt.lua")
local rse = dofile(apollo_dir.."hw-block/rse.lua")
local ap_compute = dofile(apollo_dir.."hw-block/ap_compute.lua")
local si_cl0 = dofile(apollo_dir.."hw-block/si_cl0.lua")
local si_cl1 = dofile(apollo_dir.."hw-block/si_cl1.lua")
local board = dofile(apollo_dir.."board/pca9539.lua")
local pmic = dofile(apollo_dir.."board/tps6594.lua")

local ctx = config.create(apollo_dir)
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
board.connect(platform)
pmic.connect(platform)
ap_compute.enable_ap_router(ctx, platform)
si_cl0.define(ctx, platform)
si_cl1.define(ctx, platform)

si_cl0.enable(ctx, platform)
si_cl1.enable(ctx, platform)

if ctx.config.runtime_injection.enabled then
    platform.apollo_runtime_injection = {
        moduletype = "apollo_runtime_injection";
        args = {
            "&platform.si_gic_multiview";
            "&platform.si_cl0_gic";
            "&platform.si_cl1_gic";
            "&platform.si_cl0_ssu";
            "&platform.css_system_counter";
            "&platform.host_smd_gpio";
            "&platform.rse_gpio_0";
            "&platform.rse_gpio_1";
        };
    }
end

if ctx.config.monitor.enabled then
    platform.qbox_monitor = {
        moduletype = "monitor";
        bind_address = ctx.config.monitor.bind_address;
        server_port = ctx.config.monitor.port;
        runtime_mutation = ctx.config.runtime_injection.enabled;
        injection_service = ctx.config.runtime_injection.enabled and
            "platform.apollo_runtime_injection" or "";
        use_html_presentation = true;
    }
    print("QBox monitor dashboard: http://127.0.0.1:"..
          ctx.config.monitor.port.."/")
end

if enable_ap_cpus then
    platform.apollo_timer_snapshot = {
        moduletype = "apollo_timer_snapshot";
        args = {
            "&platform.host_css_counters_timers_read";
            "&platform.ap_cpu_counter_mirror_0";
            "&platform.ap_timer_counter_mirror";
            "&platform.si_cl0_cpu_counter_mirror";
            "&platform.si_cl0_timer_cntbase";
            "&platform.si_cl1_cpu_counter_mirror_0";
            "&platform.rse_cpu_pass.rse_lsc_counter";
            "&platform.rse_cpu_pass.rse_timer_0";
            "&platform.rse_cpu_pass.rse_timer_1";
            "&platform.rse_cpu_pass.rse_timer_2";
            "&platform.rse_cpu_pass.rse_timer_3";
        };
    }
end
