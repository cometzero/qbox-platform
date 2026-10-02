-- SPDX-License-Identifier: BSD-3-Clause
-- Explicit compatibility fixture. These wires are not a verified Saturn-V schematic.
local wiring = {}

function wiring.connect_ap(ctx, platform)
    if not platform.ap_dw_i2c_0 then return end
    local expander = assert(platform.board_pca9539, "loopback requires PCA9539")
    expander.gpio_out_0 = {bind = "&board_pca9539.gpio_in_1"}
    expander.gpio_out_8 = {bind = "&board_pca9539.gpio_in_9"}
    dofile(ctx.apollo_dir.."vp/test-wiring/peri-loopback.lua").connect(platform)
end

function wiring.connect_pmic(platform)
    local pmic = assert(platform.board_tps6594, "loopback requires TPS6594")
    pmic.gpio_out_0 = {bind = "&board_tps6594.gpio_in_1"}
    pmic.gpio_out_8 = {bind = "&board_tps6594.gpio_in_9"}
end

return wiring
