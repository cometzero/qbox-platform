-- Four independent TPS6594 devices share I2C0 with non-overlapping page aliases.
local board = {}

function board.connect(platform)
    if not platform.board_i2c0 then
        return
    end

    -- Each device occupies base through base + 4; leave EEPROM 0x50..0x52 free.
    local addresses = {0x48, 0x58, 0x60, 0x68}
    for index, address in ipairs(addresses) do
        local name = "board_tps6594" .. (index == 1 and "" or "_" .. (index - 1))
        local irq_pin = index + 1
        platform[name] = {
            moduletype = "tps6594";
            dylib_path = "tps6594";
            address = address;
            gpio_pullups = 0x101;
            gpio_irq_mask = 0x7ff;
            i2c_socket = {bind = "&board_i2c0.initiator_socket"};
            int_n = {bind = "&host_smd_gpio.gpio_in_" .. irq_pin};
            gpio_out_0 = {bind = "&" .. name .. ".gpio_in_1"};
            gpio_out_8 = {bind = "&" .. name .. ".gpio_in_9"};
        }

        -- Keep every active-low PMIC interrupt released during initialization.
        local gpio = platform.host_smd_gpio
        local bit = 2 ^ irq_pin
        for _, property in ipairs({"pullups", "init_inputs"}) do
            local value = gpio[property] or 0
            if math.floor(value / bit) % 2 == 0 then
                gpio[property] = value + bit
            end
        end
    end
end

return board
