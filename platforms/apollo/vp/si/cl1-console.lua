local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local SI_CL1_UART_POLL_INTERVAL_MS = hardware.SI_CL1_UART_POLL_INTERVAL_MS
    local si_cl1_uart_read_file = runtime.si_cl1_uart_read_file
    local si_cl1_log = runtime.si_cl1_log
    local si_cl1_uart_poll_read = runtime.si_cl1_uart_poll_read

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl1_console_file == nil,
            "duplicate object: platform.si_cl1_console_file")
        platform.si_cl1_console_file = {
            moduletype = "char_backend_file";
            read_file = si_cl1_uart_read_file;
            write_file = si_cl1_log;
            poll_read = si_cl1_uart_poll_read;
            poll_interval_ms = SI_CL1_UART_POLL_INTERVAL_MS;
            baudrate = 0;
        }
    end

    return block
end

return module
