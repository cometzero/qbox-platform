-- SPI constants are socket indices; PPI constants are architectural INTIDs.
local SI_CL0_VMCU_UART_SPI = 41 - 32
local SI_CL0_VMCU_GPIO_SPI = 42 - 32

-- SPDX-License-Identifier: BSD-3-Clause
-- QVP-only external safety channel and board wires; not RD-Aspen assignments.
local module = {}

local function socket(endpoint)
    return {moduletype = "char_backend_socket"; address = endpoint;
            server = false; nowait = true; sigquit = false}
end

function module.create(ctx, _runtime)
    local block = {}
    function block.define(ctx, platform)
        assert(platform.si_cl0_vmcu_uart == nil, "duplicate vMCU safety UART")
        local uart_backend
        if ctx.options.vmcu_safety_endpoint then
            platform.vmcu_safety_socket = socket(ctx.options.vmcu_safety_endpoint)
            uart_backend = "&vmcu_safety_socket.biflow_socket"
        else
            platform.vmcu_safety_sink = {
                moduletype = "char_backend_file";
                read_file = ""; write_file = "/dev/null"; poll_read = false;
            }
            uart_backend = "&vmcu_safety_sink.biflow_socket"
        end
        -- SCP always accesses these registers, including --no-vmcu boots.
        platform.si_cl0_vmcu_uart = {
            moduletype = "Pl011";
            dylib_path = "uart-pl011";
            target_socket = {
                address = 0x2A820000; size = 0x10000;
                bind = "&si_cl0_router.initiator_socket";
            };
            irq = {bind = "&si_cl0_gic.spi_in_" .. SI_CL0_VMCU_UART_SPI};
            backend_socket = {bind = uart_backend};
        }
        assert(platform.si_cl0_vmcu_gpio == nil, "duplicate vMCU board GPIO")
        platform.si_cl0_vmcu_gpio = {
            moduletype = "qemu_pl061";
            args = {"&platform.si_cl0_qemu_inst"};
            -- RESET_N pull-up and inactive IST_DONE_N board default.
            init_inputs = 0x0C; pullups = 0x0C; pulldowns = 0x13;
            mem = {address = 0x2A830000; size = 0x10000;
                   bind = "&si_cl0_router.initiator_socket"};
            irq = {bind = "&si_cl0_gic.spi_in_" .. SI_CL0_VMCU_GPIO_SPI};
        }
        platform.si_cl0_vmcu_gpio_cold_reset = {
            moduletype = "qemu_device_cold_reset";
            args = {"&platform.si_cl0_vmcu_gpio"};
        }
        if ctx.options.vmcu_gpio_endpoint then
            platform.vmcu_gpio_socket = socket(ctx.options.vmcu_gpio_endpoint)
            platform.vmcu_gpio_bridge = {
                moduletype = "vmcu_gpio_bridge";
                backend_socket = {bind = "&vmcu_gpio_socket.biflow_socket"};
                gpio_out_3 = {bind = "&si_cl0_vmcu_gpio.gpio_in_3"};
                gpio_out_4 = {bind = "&si_cl0_vmcu_gpio.gpio_in_4"};
                reset_request = {bind = "&host_reset_ctrl.vmcu_reset"};
            }
            for pin = 0,2 do
                platform.si_cl0_vmcu_gpio["gpio_out_"..pin] = {
                    bind = "&vmcu_gpio_bridge.gpio_in_"..pin}
            end
        end
    end
    return block
end

return module
