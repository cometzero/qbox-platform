-- SPDX-License-Identifier: BSD-3-Clause
-- File hierarchy expresses hardware ownership; runtime CCI paths stay flat.
local soc = {}

function soc.load(ctx)
    ctx.modules = {}
    ctx.modules.ap_compute = dofile(ctx.apollo_dir.."soc/hw-block/ap_compute/ap_compute.lua").create(ctx)
    ctx.modules.ros = dofile(ctx.apollo_dir.."soc/hw-block/ros/ros.lua").create(ctx)
    ctx.modules.system_mgmt = dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/system_mgmt.lua").create(ctx)
    ctx.modules.rse = dofile(ctx.apollo_dir.."soc/hw-block/rse/rse.lua").create(ctx)
    ctx.modules.si_cl0 = dofile(ctx.apollo_dir.."soc/hw-block/si_cl0/si_cl0.lua").create(ctx)
    ctx.modules.si_cl1 = dofile(ctx.apollo_dir.."soc/hw-block/si_cl1/si_cl1.lua").create(ctx)
    ctx.ap_compute = ctx.modules.ap_compute
    ctx.ros = ctx.modules.ros
    ctx.system_mgmt = ctx.modules.system_mgmt
    ctx.rse = ctx.modules.rse
end

function soc.define_ap(ctx, platform)
    ctx.ap_compute.define(ctx, platform)
    ctx.ros.define(ctx, platform)
end

function soc.define(ctx, platform)
    dofile(ctx.apollo_dir.."soc/hw-block/fabric/fabric.lua").define(ctx, platform)
    ctx.rse.define(ctx, platform)
    soc.define_ap(ctx, platform)
    ctx.system_mgmt.define(ctx, platform)
end

function soc.connect_ap(ctx, platform)
    ctx.ap_compute.enable_ap_router(ctx, platform)
end

function soc.define_si(ctx, platform)
    ctx.modules.si_cl0.define(ctx, platform)
    ctx.modules.si_cl1.define(ctx, platform)
    ctx.modules.si_cl0.enable(ctx, platform)
end

function soc.connect_si(ctx, platform)
    ctx.modules.si_cl1.enable(ctx, platform)
end

return soc
