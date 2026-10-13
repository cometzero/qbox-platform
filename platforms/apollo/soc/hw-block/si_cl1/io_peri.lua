local SI_CL1_UART_BASE = 0x2A410000
local SI_CL1_UART_SIZE = 0x00010000

local module = {}

function module.create(ctx, hardware, _runtime, interrupts)
    local irq_routes = interrupts

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl1_uart == nil,
            "duplicate object: platform.si_cl1_uart")
        platform.si_cl1_uart = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            target_socket = {
                address = SI_CL1_UART_BASE;
                size = SI_CL1_UART_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl1_uart", "View2")};
            backend_socket = {bind = "&si_cl1_console_file.biflow_socket"};
        }
    end

    return block
end

return module
