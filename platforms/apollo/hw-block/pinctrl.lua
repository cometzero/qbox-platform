-- HSOC PERI0 (56 pins) and PERI1 (36 pins) in the AP domain.
local pinctrl = {}
local PINCTRL_BASE = 0x301e0000
local PINCTRL_SIZE = 0x10000
local PINCTRL_SPI_BASE = 334
local PINCTRL_PERI1_BASE = 0x301f0000
local PINCTRL_PERI1_SPI_BASE = 348

function pinctrl.define(platform)
    if not platform.ap_dw_i2c_0 then
        return
    end
    platform.pinctrl_peri0 = {
        moduletype = "hsoc_gpio";
        bank_sizes = "8,8,8,8,8,8,1,1,1,1,1,1,1,1";
        peripheral_routes = "0:0:0:2;1:0:2:2;2:0:4:2;3:0:6:2;4:1:0:2;5:1:2:2;6:2:0:4;7:2:4:4;10:4:0:2;11:4:2:2;14:3:0:4;15:3:4:4";
        target_socket = {
            address = PINCTRL_BASE;
            size = PINCTRL_SIZE;
            bind = "&host_router.initiator_socket";
        };
    }
    local device = platform.pinctrl_peri0
    for bank=0,13 do
        device["irq_"..bank] = {bind = "&ap_gic.spi_in_"..(PINCTRL_SPI_BASE + bank)}
    end
    for i=0,5 do
        device["peripheral_enable_"..i] = {bind = "&ap_dw_i2c_"..i..".pinmux_enable"}
    end
    for i=0,1 do
        device["peripheral_enable_"..(6 + i)] = {bind = "&ap_dw_ssi_"..i..".pinmux_enable"}
        device["peripheral_enable_"..(10 + i)] = {bind = "&ap_dw_uart_"..i..".pinmux_enable"}
    end
    for i=0,1 do
        device["peripheral_enable_"..(14 + i)] = {bind = "&ap_dw_i2s_"..i..".pinmux_enable"}
    end

    platform.pinctrl_peri1 = {
        moduletype = "hsoc_gpio";
        bank_sizes = "8,8,8,8,1,1,1,1";
        peripheral_routes = "8:0:0:4;9:0:4:4;12:1:0:2;13:1:2:2";
        target_socket = {
            address = PINCTRL_PERI1_BASE;
            size = PINCTRL_SIZE;
            bind = "&host_router.initiator_socket";
        };
    }
    local peri1 = platform.pinctrl_peri1
    for bank=0,7 do
        peri1["irq_"..bank] = {bind = "&ap_gic.spi_in_"..(PINCTRL_PERI1_SPI_BASE + bank)}
    end
    for i=2,3 do
        peri1["peripheral_enable_"..(6 + i)] = {bind = "&ap_dw_ssi_"..i..".pinmux_enable"}
        peri1["peripheral_enable_"..(10 + i)] = {bind = "&ap_dw_uart_"..i..".pinmux_enable"}
    end
end

return pinctrl
