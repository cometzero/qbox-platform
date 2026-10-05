-- SPDX-License-Identifier: BSD-3-Clause
-- The external vMCU endpoint is opt-in and local to this host.
local function endpoint_option(name)
    local endpoint = os.getenv(name)
    if endpoint == nil then return nil end
    local port = endpoint:match("^127%.0%.0%.1:(%d+)$")
    port = tonumber(port)
    assert(port and port >= 1 and port <= 65535 and port % 1 == 0,
           name.." must be 127.0.0.1:PORT (1..65535)")
    return "127.0.0.1:"..tostring(port)
end

return function(options)
    options.vmcu_uart_endpoint = endpoint_option("QBOX_APOLLO_VMCU_UART_ENDPOINT")
    options.vmcu_safety_endpoint = endpoint_option("QBOX_APOLLO_VMCU_SAFETY_ENDPOINT")
    options.vmcu_gpio_endpoint = endpoint_option("QBOX_APOLLO_VMCU_GPIO_ENDPOINT")
    assert(not options.vmcu_uart_endpoint or options.enable_ap_cpus,
           "QBOX_APOLLO_VMCU_UART_ENDPOINT requires QBOX_RDASPEN_ENABLE_AP_CPUS=true")
    local used = {}
    for _, endpoint in ipairs({options.vmcu_uart_endpoint or false,
        options.vmcu_safety_endpoint or false, options.vmcu_gpio_endpoint or false}) do
        if endpoint then
            assert(not used[endpoint], "vMCU endpoints must be distinct")
            used[endpoint] = true
        end
    end
end
