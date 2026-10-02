local eeprom = {}

function eeprom.define(ctx, platform)
    local options = ctx.options

    for i = 0, 5 do
        local eeprom = "ap_dw_i2c_" .. i .. "_eeprom"
        assert(platform[eeprom] == nil, "duplicate Apollo object")
        if options.enable_ap_cpus then
            platform[eeprom] = {
                moduletype = "dw_i2c_eeprom";
                dylib_path = "dw-apb-i2c";
                address = 0x50;
                size = 256;
                address_width = 8;
                page_size = 8;
            }
        end
        local controller = platform["ap_dw_i2c_" .. i]
        if controller then
            assert(controller.i2c_socket == nil, "I2C controller already connected")
            controller.i2c_socket = {bind = "&" .. eeprom .. ".i2c_socket"}
        end
    end
end

return eeprom
