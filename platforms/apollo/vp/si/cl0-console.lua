local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local SI_CONST = hardware
    local si_cl0_uart_read_file = runtime.si_cl0_uart_read_file
    local si_cl0_log = runtime.si_cl0_log
    local si_cl0_uart_poll_read = runtime.si_cl0_uart_poll_read

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl0_console_file == nil,
            "duplicate object: platform.si_cl0_console_file")
        platform.si_cl0_console_file = {
            moduletype = "char_backend_file";
            read_file = si_cl0_uart_read_file;
            write_file = si_cl0_log;
            poll_read = si_cl0_uart_poll_read;
            poll_interval_ms = SI_CONST.SI_CL0_UART_POLL_INTERVAL_MS;
            baudrate = 0;
        }
    end

    return block
end

return module
