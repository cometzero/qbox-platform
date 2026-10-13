-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx)
    local apollo_dir = ctx.apollo_dir
    local composition = dofile(apollo_dir.."vp/composition.lua")
    local qvp = dofile(apollo_dir.."vp/qvp.lua")
    local board = dofile(apollo_dir.."board/saturn-v.lua")
    local loops = dofile(apollo_dir.."vp/test-wiring/loopbacks.lua")
    composition.load(ctx)

    -- Preserve construction and reset-release order across domains.
    local result = qvp.create()
    composition.define(ctx, result)
    board.connect_ap(ctx, result)
    loops.connect_ap(ctx, result)
    dofile(apollo_dir.."vp/backends/audio.lua").connect(result)
    board.connect_si(ctx, result)
    loops.connect_pmic(result)
    dofile(apollo_dir.."vp/diagnostics.lua").configure(ctx, result)
    dofile(apollo_dir.."vp/debug.lua")(ctx, result)

    return result
end
