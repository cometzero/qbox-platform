local module = {}

function module.create(ctx, _hardware, _runtime, _interrupts)
    local rse_log = ctx.options.rse_log
    local rse_uart_read_file = ctx.options.rse_uart_read_file
    local uart_poll_interval_ms = ctx.options.uart_poll_interval_ms

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_uart_file == nil,
            "duplicate object: platform.rse_uart_file")
        platform.rse_uart_file = {
            moduletype = "char_backend_file";
            read_file = rse_uart_read_file;
            write_file = rse_log;
            poll_read = rse_uart_read_file ~= "/dev/null";
            poll_interval_ms = uart_poll_interval_ms;
            baudrate = 0;
        }
    end

    return block
end

return module
