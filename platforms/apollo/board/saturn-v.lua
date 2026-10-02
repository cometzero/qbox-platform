-- SPDX-License-Identifier: BSD-3-Clause
-- Board population and buses; loopback/test routing belongs to vp/test-wiring.
-- Existing QVP population is preserved. Saturn-V schematic parity is UNVERIFIED.
local board = {}

function board.connect_ap(ctx, platform)
    dofile(ctx.apollo_dir.."board/hw-block/eeprom.lua").define(ctx, platform)
    if ctx.options.enable_ap_cpus then
        dofile(ctx.apollo_dir.."board/hw-block/pca9539.lua").connect(platform)
        dofile(ctx.apollo_dir.."board/hw-block/validate.lua").i2c_bus(platform, "board_i2c0")
    end
end

function board.connect_si(ctx, platform)
    dofile(ctx.apollo_dir.."board/hw-block/tps6594.lua").connect(platform)
    dofile(ctx.apollo_dir.."board/hw-block/validate.lua").i2c_bus(platform, "board_si_i2c0")
end

return board
