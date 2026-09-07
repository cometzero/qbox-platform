-- Board wiring: I2C0 shares its EEPROM with a resettable GPIO expander.
local board = {}

function board.connect(platform)
    if not platform.ap_dw_i2c_0 then
        return
    end

    platform.board_i2c0 = {
        moduletype = "i2c_bus";
        dylib_path = "i2c-bus";
    }
    platform.ap_dw_i2c_0.i2c_socket = {bind = "&board_i2c0.target_socket"}
    platform.ap_dw_i2c_0_eeprom.i2c_socket = {bind = "&board_i2c0.initiator_socket"}
    platform.board_pca9539 = {
        moduletype = "pca9539";
        dylib_path = "pca9539";
        address = 0x74;
        i2c_socket = {bind = "&board_i2c0.initiator_socket"};
        int_n = {bind = "&host_smd_gpio.gpio_in_1"};
        gpio_out_0 = {bind = "&board_pca9539.gpio_in_1"};
        gpio_out_8 = {bind = "&board_pca9539.gpio_in_9"};
    }

    -- Pull-ups release RESET_N when PL061 GPIO0 is an input and idle INT_N.
    local gpio = platform.host_smd_gpio
    gpio.pullups = math.floor((gpio.pullups or 0) / 4) * 4 + 3
    gpio.init_inputs = math.floor((gpio.init_inputs or 0) / 4) * 4 + 3
    gpio.gpio_out_0 = {bind = "&board_pca9539.reset_n"}
end

return board
