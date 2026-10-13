-- SPI constants are socket indices; PPI constants are architectural INTIDs.
local SI_CL0_UART_SPI = 40 - 32

local SI_CL0_UART_BASE = 0x2A400000
local SI_CL0_UART_SIZE = 0x00010000

local module = {}

function module.create(ctx, _runtime)


    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl0_uart == nil,
            "duplicate object: platform.si_cl0_uart")
        platform.si_cl0_uart = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            target_socket = {
                address = SI_CL0_UART_BASE;
                size = SI_CL0_UART_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            irq = {bind = "&si_cl0_gic.spi_in_" .. SI_CL0_UART_SPI};
            backend_socket = {bind = "&si_cl0_console_file.biflow_socket"};
        }
    end

    return block
end

return module
