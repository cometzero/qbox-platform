local host_io = {}

function host_io.define(ctx, platform)
    local options = ctx.options

    assert(platform.ap_secure_console_file == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_secure_console_file = {
            moduletype = "char_backend_file";
            read_file = options.secure_uart_read_file;
            write_file = options.secure_console_log;
            poll_read = options.secure_uart_read_file ~= "/dev/null";
            poll_interval_ms = options.uart_poll_interval_ms;
            baudrate = 0;
        }
    end

    assert(platform.ap_primary_console_file == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_primary_console_file = {
            moduletype = "char_backend_file";
            read_file = options.primary_uart_read_file;
            write_file = options.primary_console_log;
            poll_read = options.primary_uart_read_file ~= "/dev/null";
            poll_interval_ms = tonumber(options.getenv_or(
                "QBOX_RDASPEN_PRIMARY_UART_POLL_INTERVAL_MS",
                tostring(options.uart_poll_interval_ms)));
            baudrate = 0;
        }
    end
end

function host_io.connect(_ctx, platform)
    if platform.ap_secure_uart then
        platform.ap_secure_uart.backend_socket = {bind = "&ap_secure_console_file.biflow_socket"}
        platform.ap_primary_uart.backend_socket = {bind = "&ap_primary_console_file.biflow_socket"}
    end
end

return host_io
