-- Shared AP hardware construction. Domain composition belongs to each entrypoint.
local common = {}

function common.load(apollo_dir)
    local config = dofile(apollo_dir.."hw-block/config.lua")
    local ctx = config.create(apollo_dir)
    ctx.ap_compute = dofile(apollo_dir.."hw-block/ap_compute.lua")
    ctx.ros = dofile(apollo_dir.."hw-block/ros.lua")
    return ctx
end

function common.define_ap(ctx, platform)
    ctx.ap_compute.define(ctx, platform)
    ctx.ros.define(ctx, platform)
    dofile(ctx.apollo_dir.."hw-block/pinctrl.lua").define(platform)
end

function common.connect_board(ctx, platform)
    dofile(ctx.apollo_dir.."board/pca9539.lua").connect(platform)
    dofile(ctx.apollo_dir.."board/peri0-loopback.lua").connect(platform)
end

return common
