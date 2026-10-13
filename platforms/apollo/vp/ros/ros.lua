-- Virtual devices and host connections added to the physical RoS definition.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local virtio = dofile(dir .. "virtio.lua")
local loopback = dofile(dir .. "loopback.lua")
local vmcu = dofile(dir .. "../backends/vmcu.lua")
local module = {}

function module.apply(ctx, platform)
    virtio.define(ctx, platform, ctx.hardware.ros.map)
    loopback.connect(ctx, platform)
    vmcu.connect(ctx, platform)
    if platform.system_router then
        virtio.bind_ap_view_targets(platform, function(target)
            target.bind = "&ap_router.initiator_socket"
            target.priority = target.priority or 0
        end)
    end
end

return module
