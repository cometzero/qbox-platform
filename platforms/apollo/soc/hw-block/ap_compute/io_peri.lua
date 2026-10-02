local io_peri = {}

function io_peri.define(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE
    local AP_IRQ = hw.AP_IRQ
    local AP_HW = hw.AP_HW

    assert(platform.ap_secure_uart == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_secure_uart = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            revision = AP_HW.uart_revision;
            target_socket = {
                address = AP_ADDRESS.secure_uart;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            irq = {bind = "&ap_gic.spi_in_" .. AP_IRQ.secure_uart};
        }
    end

    assert(platform.ap_primary_uart == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_primary_uart = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            revision = AP_HW.uart_revision;
            target_socket = {
                address = AP_ADDRESS.primary_uart;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            irq = {bind = "&ap_gic.spi_in_" .. AP_IRQ.primary_uart};
        }
    end
end

return io_peri
