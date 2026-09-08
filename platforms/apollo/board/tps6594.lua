-- Board wiring: TPS6594 uses the existing I2C0 segment at address 0x48.
local board = {}

function board.connect(platform)
    if not platform.board_i2c0 then
        return
    end

    platform.board_tps6594 = {
        moduletype = "tps6594";
        dylib_path = "tps6594";
        address = 0x48;
        gpio_pullups = 0x101;
        gpio_irq_mask = 0x7ff;
        i2c_socket = {bind = "&board_i2c0.initiator_socket"};
        int_n = {bind = "&host_smd_gpio.gpio_in_2"};
        gpio_out_0 = {bind = "&board_tps6594.gpio_in_1"};
        gpio_out_8 = {bind = "&board_tps6594.gpio_in_9"};
    }

    -- Keep the active-low PMIC interrupt released during initialization.
    local gpio = platform.host_smd_gpio
    local pullups = gpio.pullups or 0
    local inputs = gpio.init_inputs or 0
    gpio.pullups = math.floor(pullups / 8) * 8 + pullups % 4 + 4
    gpio.init_inputs = math.floor(inputs / 8) * 8 + inputs % 4 + 4
end

return board
