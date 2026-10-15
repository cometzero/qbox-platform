-- SPDX-License-Identifier: BSD-3-Clause
-- Run with the compiled Lua runner or `lua TEST.lua platforms/apollo/`.
local apollo_dir = apollo_dir or assert(arg[1], "Apollo source directory required")
local original_getenv = os.getenv

local function evaluate(entry, endpoint, enable_ap, safety, gpio, spi)
    local environment = {
        QBOX_RDASPEN_ENABLE_AP_CPUS = enable_ap == false and "false" or "true";
        QBOX_APOLLO_VMCU_UART_ENDPOINT = endpoint;
        QBOX_APOLLO_VMCU_SAFETY_ENDPOINT = safety;
        QBOX_APOLLO_VMCU_GPIO_ENDPOINT = gpio;
        QBOX_APOLLO_VMCU_SPI_ENDPOINT = spi;
        QBOX_LINUX_BOOT_STUB = "fixture-boot-stub.bin";
        QBOX_LINUX_KERNEL = "fixture-kernel.bin";
        QBOX_LINUX_DTB = "fixture.dtb";
    }
    os.getenv = function(name) return environment[name] end
    platform = nil
    local ok, err = pcall(dofile, apollo_dir..entry)
    os.getenv = original_getenv
    return ok, err, platform
end

local function equal(left, right, path)
    assert(type(left) == type(right), "type changed: "..path)
    if type(left) ~= "table" then
        assert(left == right, "value changed: "..path)
        return
    end
    for key, value in pairs(left) do equal(value, right[key], path.."."..key) end
    for key in pairs(right) do
        assert(left[key] ~= nil, "unexpected key: "..path.."."..key)
    end
end

for _, entry in ipairs({"apollo-qvp-saturn-v.lua", "apollo-qvp-linux.lua"}) do
    local ok, err, baseline = evaluate(entry)
    assert(ok, err)
    assert(baseline.ap_dw_uart_0.backend_socket.bind == "&ap_dw_uart_1.backend_socket")
    assert(baseline.ap_dw_uart_2.backend_socket.bind == "&ap_dw_uart_3.backend_socket")
    assert(baseline.ap_dw_uart_3.backend_socket == nil)
    assert(baseline.vmcu_uart_socket == nil)
    assert(baseline.ap_uart3_sink == nil)

    local enabled
    ok, err, enabled = evaluate(entry, "127.0.0.1:44001")
    assert(ok, err)
    assert(enabled.ap_dw_uart_2.target_socket.address == 0x301c0000)
    assert(enabled.ap_dw_uart_2.irq.bind == "&ap_gic.spi_in_332")
    assert(enabled.ap_dw_uart_2.backend_socket.bind == "&vmcu_uart_socket.biflow_socket")
    assert(enabled.ap_dw_uart_3.backend_socket.bind == "&ap_uart3_sink.biflow_socket")
    equal(enabled.vmcu_uart_socket, {
        moduletype = "char_backend_socket";
        address = "127.0.0.1:44001";
        server = false; nowait = true; sigquit = false;
    }, "vmcu_uart_socket")
    equal(enabled.ap_uart3_sink, {
        moduletype = "char_backend_file";
        read_file = ""; write_file = "/dev/null"; poll_read = false;
    }, "ap_uart3_sink")

    -- Every other effective descriptor field, including UART0/1 DMA, is stable.
    enabled.ap_dw_uart_2.backend_socket = baseline.ap_dw_uart_2.backend_socket
    enabled.ap_dw_uart_3.backend_socket = nil
    enabled.vmcu_uart_socket = nil
    enabled.ap_uart3_sink = nil
    equal(baseline, enabled, "platform")
end

for _, endpoint in ipairs({"", "localhost:44001", "0.0.0.0:44001",
    "127.0.0.1", "127.0.0.1:0", "127.0.0.1:65536", "127.0.0.1:-1",
    "127.0.0.1:1.5", "127.0.0.1:4e3", "127.0.0.1:44001oops"}) do
    local ok, err, result = evaluate("apollo-qvp-saturn-v.lua", endpoint)
    assert(not ok and tostring(err):find("QBOX_APOLLO_VMCU_UART_ENDPOINT", 1, true))
    assert(result == nil, "invalid endpoint must fail before platform export")
end

local ok, err = evaluate("apollo-qvp-saturn-v.lua", "127.0.0.1:44001", false)
assert(not ok and tostring(err):find("requires QBOX_RDASPEN_ENABLE_AP_CPUS=true", 1, true))

local backend = dofile(apollo_dir.."vp/backends/vmcu.lua")
ok, err = pcall(backend.connect, {options = {vmcu_uart_endpoint = "127.0.0.1:44001"}}, {
    ap_dw_uart_2 = {backend_socket = {bind = "&ap_dw_uart_3.backend_socket"}};
    ap_dw_uart_3 = {};
})
assert(not ok and tostring(err):find("cannot share", 1, true))

local baseline, enabled
ok, err, baseline = evaluate("apollo-qvp-saturn-v.lua")
assert(ok, err)
assert(baseline.si_cl0_vmcu_uart and baseline.si_cl0_vmcu_gpio)
assert(baseline.si_cl0_vmcu_uart.backend_socket.bind == "&vmcu_safety_sink.biflow_socket")
assert(baseline.vmcu_safety_sink.read_file == "" and baseline.vmcu_safety_sink.poll_read == false)
assert(baseline.vmcu_gpio_bridge == nil and baseline.vmcu_gpio_socket == nil)
assert(baseline.si_cl0_vmcu_gpio.gpio_out_0 == nil)
ok, err, enabled = evaluate("apollo-qvp-saturn-v.lua", "127.0.0.1:44001", true,
                            "127.0.0.1:44002", "127.0.0.1:44003")
assert(ok, err)
assert(enabled.si_cl0_vmcu_uart.target_socket.address == 0x2a820000)
assert(enabled.si_cl0_vmcu_uart.target_socket.bind == "&si_cl0_router.initiator_socket")
assert(enabled.si_cl0_vmcu_uart.irq.bind == "&si_cl0_gic.spi_in_9")
assert(enabled.si_cl0_vmcu_uart.backend_socket.bind == "&vmcu_safety_socket.biflow_socket")
assert(enabled.vmcu_safety_socket.address == "127.0.0.1:44002")
assert(enabled.si_cl0_vmcu_gpio.mem.address == 0x2a830000)
assert(enabled.si_cl0_vmcu_gpio.irq.bind == "&si_cl0_gic.spi_in_10")
assert(enabled.si_cl0_vmcu_gpio.init_inputs == 0x0c)
assert(enabled.si_cl0_vmcu_gpio.pullups == 0x0c)
assert(enabled.vmcu_gpio_bridge.reset_request.bind == "&host_reset_ctrl.vmcu_reset")
assert(enabled.vmcu_gpio_bridge.gpio_out_3.bind == "&si_cl0_vmcu_gpio.gpio_in_3")
assert(enabled.vmcu_gpio_bridge.gpio_out_4.bind == "&si_cl0_vmcu_gpio.gpio_in_4")
for pin = 0,2 do
    assert(enabled.si_cl0_vmcu_gpio["gpio_out_"..pin].bind ==
           "&vmcu_gpio_bridge.gpio_in_"..pin)
end
equal(baseline.si_cl0_uart, enabled.si_cl0_uart, "si_cl0_console")
equal(baseline.si_cl0_dw_i2c_0, enabled.si_cl0_dw_i2c_0, "pmic_i2c")
equal(baseline.si_cl0_pmic_gpio, enabled.si_cl0_pmic_gpio, "pmic_fault_gpio")
equal(baseline.host_reset_ctrl, enabled.host_reset_ctrl, "existing_reset_arbiter")
local reset = enabled.apollo_system_reset_fanout.reset_out.bind
assert(reset:find("&si_cl0_vmcu_uart.reset", 1, true) <
       reset:find("&si_cl0_qemu_inst.reset", 1, true))
assert(reset:find("&si_cl0_vmcu_gpio_cold_reset.reset", 1, true))
assert(not enabled.ap_cold_reset_fanout.reset_out.bind:find("vmcu", 1, true))

for _, invalid in ipairs({"", "0.0.0.0:44002", "127.0.0.1:0", "127.0.0.1:65536"}) do
    ok, err = evaluate("apollo-qvp-saturn-v.lua", nil, true, invalid)
    assert(not ok and tostring(err):find("QBOX_APOLLO_VMCU_SAFETY_ENDPOINT", 1, true))
    ok, err = evaluate("apollo-qvp-saturn-v.lua", nil, true, nil, invalid)
    assert(not ok and tostring(err):find("QBOX_APOLLO_VMCU_GPIO_ENDPOINT", 1, true))
end
ok, err = evaluate("apollo-qvp-saturn-v.lua", nil, true, "127.0.0.1:44002", "127.0.0.1:44002")
assert(not ok and tostring(err):find("endpoints must be distinct", 1, true))
-- Safety ownership does not depend on the AP management UART being connected.
ok, err, enabled = evaluate("apollo-qvp-saturn-v.lua", nil, false, "127.0.0.1:44002")
assert(ok, err)
assert(enabled.si_cl0_vmcu_uart and not enabled.vmcu_uart_socket)
print("PASS: vMCU UART config, full/AP-only bindings, unchanged default and invalid options")
print("PASS: SI safety UART, independent GPIO, reset ordering and PMIC preservation")

ok, err, enabled = evaluate("apollo-qvp-saturn-v.lua", nil, true, nil,
                            "127.0.0.1:44003", "127.0.0.1:44004")
assert(ok, err)
assert(enabled.si_cl0_vmcu_spi.target_socket.address == 0x2a840000)
assert(enabled.si_cl0_vmcu_spi.irq.bind == "&si_cl0_gic.spi_in_11")
assert(enabled.si_cl0_vmcu_spi.connected == true)
assert(baseline.si_cl0_vmcu_spi.connected == false)
assert(enabled.vmcu_gpio_bridge.soc_mask == 0x27)
assert(enabled.si_cl0_vmcu_gpio.gpio_out_5.bind == "&vmcu_gpio_bridge.gpio_in_5")
assert(enabled.apollo_system_reset_fanout.reset_out.bind:find("&si_cl0_vmcu_spi.reset", 1, true))
assert(not enabled.ap_cold_reset_fanout.reset_out.bind:find("vmcu_spi", 1, true))
ok, err = evaluate("apollo-qvp-saturn-v.lua", nil, true, nil, "127.0.0.1:44004", "127.0.0.1:44004")
assert(not ok and tostring(err):find("endpoints must be distinct", 1, true))
print("PASS: SPI QVP slave mapping, request pin, endpoint uniqueness and AP reset independence")
