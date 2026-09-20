-- One TPS6594 device on the SI CL0 management I2C bus.
local board = {}

function board.connect(platform)
    if not platform.si_cl0_dw_i2c_0 then
        return
    end

    platform.board_si_i2c0 = {
        moduletype = "i2c_bus";
        dylib_path = "i2c-bus";
    }
    platform.si_cl0_dw_i2c_0.i2c_socket = {
        bind = "&board_si_i2c0.target_socket";
    }

    -- Each device occupies base through base + 4; page aliases do not overlap.
    local addresses = {0x48}
    for index, address in ipairs(addresses) do
        local name = "board_tps6594" .. (index == 1 and "" or "_" .. (index - 1))
        local irq_pin = index - 1
        platform[name] = {
            moduletype = "tps6594";
            dylib_path = "tps6594";
            address = address;
            gpio_pullups = 0x101;
            gpio_irq_mask = 0x7ff;
            i2c_socket = {bind = "&board_si_i2c0.initiator_socket"};
            int_n = {bind = "&si_cl0_pmic_gpio.gpio_in_" .. irq_pin};
            gpio_out_0 = {bind = "&" .. name .. ".gpio_in_1"};
            gpio_out_8 = {bind = "&" .. name .. ".gpio_in_9"};
        }
    end
end

return board
