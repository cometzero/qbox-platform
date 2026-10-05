-- SPDX-License-Identifier: BSD-3-Clause
-- Functional UART transport to a separate vMCU emulator process.
local vmcu = {}

function vmcu.connect(ctx, platform)
    local endpoint = ctx.options.vmcu_uart_endpoint
    if endpoint == nil then
        return
    end
    local uart2 = assert(platform.ap_dw_uart_2, "vMCU UART requires AP UART2")
    local uart3 = assert(platform.ap_dw_uart_3, "vMCU UART requires AP UART3 sink")
    assert(uart2.backend_socket == nil and uart3.backend_socket == nil,
           "vMCU UART cannot share the UART2/3 loopback or another backend")
    assert(platform.vmcu_uart_socket == nil and platform.ap_uart3_sink == nil,
           "duplicate vMCU UART backend")

    platform.vmcu_uart_socket = {
        moduletype = "char_backend_socket";
        address = endpoint;
        server = false;
        nowait = true;
        sigquit = false;
    }
    platform.ap_uart3_sink = {
        moduletype = "char_backend_file";
        -- An empty reader avoids char_backend_file injecting EOF as a byte.
        read_file = "";
        write_file = "/dev/null";
        poll_read = false;
    }
    uart2.backend_socket = {bind = "&vmcu_uart_socket.biflow_socket"}
    uart3.backend_socket = {bind = "&ap_uart3_sink.biflow_socket"}
end

return vmcu
