-- SPDX-License-Identifier: BSD-3-Clause
-- Apollo SoC + QBox execution policy + Saturn-V board population.
local apollo_dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local ctx = dofile(apollo_dir.."vp/options.lua").create(apollo_dir)
local soc = dofile(apollo_dir.."soc/apollo.lua")
local qvp = dofile(apollo_dir.."vp/qvp.lua")
local board = dofile(apollo_dir.."board/saturn-v.lua")
local loops = dofile(apollo_dir.."vp/test-wiring/loopbacks.lua")
soc.load(ctx)

-- Preserve construction and reset-release order across domains.
local result = qvp.create()
soc.define(ctx, result)
board.connect_ap(ctx, result)
loops.connect_ap(ctx, result)
soc.connect_ap(ctx, result)
dofile(apollo_dir.."vp/backends/audio.lua").connect(result)
soc.define_si(ctx, result)
board.connect_si(ctx, result)
loops.connect_pmic(result)
soc.connect_si(ctx, result)
dofile(apollo_dir.."vp/debug.lua")(ctx, result)

-- QBox exports this one root table into CCI only after composition succeeds.
platform = result
