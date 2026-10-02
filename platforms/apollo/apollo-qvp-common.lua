-- SPDX-License-Identifier: BSD-3-Clause
-- Shared AP construction used by the firmware-free profile.
local common = {}

function common.load(apollo_dir)
    local ctx = dofile(apollo_dir.."vp/options.lua").create(apollo_dir)
    dofile(apollo_dir.."soc/apollo.lua").load(ctx)
    return ctx
end

function common.define_ap(ctx, platform)
    dofile(ctx.apollo_dir.."soc/apollo.lua").define_ap(ctx, platform)
end

function common.connect_board(ctx, platform)
    dofile(ctx.apollo_dir.."board/saturn-v.lua").connect_ap(ctx, platform)
    dofile(ctx.apollo_dir.."vp/test-wiring/loopbacks.lua").connect_ap(ctx, platform)
end

return common
