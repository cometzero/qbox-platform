local AP_UART_REVISION = 3
local AP_REGISTER_WINDOW_SIZE = 0x00010000
-- GIC SPI constants are zero-based SPI socket indices (not architectural INTIDs).
local AP_SECURE_UART_BASE = 0x1A410000
local AP_PRIMARY_UART_BASE = 0x1A400000
local AP_PRIMARY_UART_SPI = 52
local AP_SECURE_UART_SPI = 53

local io_peri = {}

function io_peri.define(ctx, platform)
    local options = ctx.options

    assert(platform.ap_secure_uart == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_secure_uart = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            revision = AP_UART_REVISION;
            target_socket = {
                address = AP_SECURE_UART_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            irq = {bind = "&ap_gic.spi_in_" .. AP_SECURE_UART_SPI};
        }
    end

    assert(platform.ap_primary_uart == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_primary_uart = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            revision = AP_UART_REVISION;
            target_socket = {
                address = AP_PRIMARY_UART_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            irq = {bind = "&ap_gic.spi_in_" .. AP_PRIMARY_UART_SPI};
        }
    end
end

return io_peri
