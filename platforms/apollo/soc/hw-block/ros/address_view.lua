local address_view = {}
local ROS_BLOCK_DEVICE_COUNT = 4

local function visit_dwc_targets(platform, visitor)
    for i = 0, 5 do
        local device = platform["ap_dw_i2c_" .. i]
        if device ~= nil then
            visitor(device.target_socket)
        end
    end
    for i = 0, 3 do
        local ssi = platform["ap_dw_ssi_" .. i]
        local uart = platform["ap_dw_uart_" .. i]
        if ssi ~= nil then
            visitor(ssi.target_socket)
        end
        if uart ~= nil then
            visitor(uart.target_socket)
        end
    end
    for i = 0, 1 do
        local i2s = platform["ap_dw_i2s_" .. i]
        if i2s ~= nil then
            visitor(i2s.target_socket)
        end
    end
end

function address_view.bind_ap_view_targets(platform, bind_ap_target)
    if platform.dma350_1 then
        bind_ap_target(platform.dma350_1.target_socket)
        platform.dma350_1.initiator_socket = {bind = "&ap_router.target_socket"}
    end
    if platform.dma350_0 then
        bind_ap_target(platform.dma350_0.target_socket)
        platform.dma350_0.initiator_socket = {bind = "&ap_router.target_socket"}
    end
    for i = 0, (ROS_BLOCK_DEVICE_COUNT-1) do
        local virtio = platform["ap_virtioblk_" .. i]
        if virtio ~= nil and virtio.mem ~= nil then
            bind_ap_target(virtio.mem)
        end
    end

    if platform.ap_virtionet_0 ~= nil and platform.ap_virtionet_0.mem ~= nil then
        bind_ap_target(platform.ap_virtionet_0.mem)
    end
    if platform.ap_virtiorng_0 ~= nil and platform.ap_virtiorng_0.mem ~= nil then
        bind_ap_target(platform.ap_virtiorng_0.mem)
    end
    if platform.ap_rtc_0 ~= nil and platform.ap_rtc_0.mem ~= nil then
        bind_ap_target(platform.ap_rtc_0.mem)
    end
    visit_dwc_targets(platform, bind_ap_target)
    if platform.pinctrl_peri0 then
        bind_ap_target(platform.pinctrl_peri0.target_socket)
    end
    if platform.pinctrl_peri1 then
        bind_ap_target(platform.pinctrl_peri1.target_socket)
    end
end

function address_view.lower_decode_priorities(platform, lower_decode_priority, priority)
    if platform.dma350_1 then
        lower_decode_priority(platform.dma350_1.target_socket, priority)
    end
    if platform.dma350_0 then
        lower_decode_priority(platform.dma350_0.target_socket, priority)
    end
    for i = 0, (ROS_BLOCK_DEVICE_COUNT-1) do
        local virtio = platform["ap_virtioblk_" .. i]
        if virtio ~= nil and virtio.mem ~= nil then
            lower_decode_priority(virtio.mem, priority)
        end
    end

    if platform.ap_virtionet_0 ~= nil and platform.ap_virtionet_0.mem ~= nil then
        lower_decode_priority(platform.ap_virtionet_0.mem, priority)
    end
    if platform.ap_virtiorng_0 ~= nil and platform.ap_virtiorng_0.mem ~= nil then
        lower_decode_priority(platform.ap_virtiorng_0.mem, priority)
    end
    if platform.ap_rtc_0 ~= nil and platform.ap_rtc_0.mem ~= nil then
        lower_decode_priority(platform.ap_rtc_0.mem, priority)
    end
    visit_dwc_targets(platform, function(target)
        lower_decode_priority(target, priority)
    end)
    if platform.pinctrl_peri0 then
        lower_decode_priority(platform.pinctrl_peri0.target_socket, priority)
    end
    if platform.pinctrl_peri1 then
        lower_decode_priority(platform.pinctrl_peri1.target_socket, priority)
    end
end

return address_view
