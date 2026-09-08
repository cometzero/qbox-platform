-- Fixed single-driver wiring for GPIO bank and IRQ qualification.
local board = {}

function board.connect(platform)
    local pins = platform.pinctrl_peri0
    if not pins then
        return
    end
    pins.gpio_out_12 = {bind = "&pinctrl_peri0.gpio_in_13"}
    pins.gpio_out_14 = {bind = "&pinctrl_peri0.gpio_in_15"}
    pins.gpio_out_48 = {bind = "&pinctrl_peri0.gpio_in_56"}
    pins.gpio_out_96 = {bind = "&pinctrl_peri0.gpio_in_104"}
    local peri1 = platform.pinctrl_peri1
    if peri1 then
        peri1.gpio_out_24 = {bind = "&pinctrl_peri1.gpio_in_25"}
        peri1.gpio_out_48 = {bind = "&pinctrl_peri1.gpio_in_56"}
    end
end

return board
