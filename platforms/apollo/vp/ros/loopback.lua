local loopback = {}

function loopback.connect(ctx, platform)
    if not ctx.options.enable_ap_cpus then
        return
    end
    for i = 0, 3 do
        local uart = "ap_dw_uart_" .. i
        if i == 0 or (i == 2 and not ctx.options.vmcu_uart_endpoint) then
            platform[uart].backend_socket = {
                bind = "&ap_dw_uart_" .. (i + 1) .. ".backend_socket";
            }
        end
    end
    if platform.ap_dw_i2s_0 then
        platform.ap_dw_i2s_0.audio_socket = {
            bind = "&ap_dw_i2s_1.audio_socket";
        }
    end
end

return loopback
