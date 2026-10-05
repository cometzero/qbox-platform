-- RoS controller ownership is separate from board populations and VP devices.
local ros = {}
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local map = dofile(dir .. "map.lua")
local dma = dofile(dir .. "dma.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local pinctrl = dofile(dir .. "pinctrl.lua")
local catalog = dofile(dir .. "catalog.lua")
local address_view = dofile(dir .. "address_view.lua")
local virtio = dofile(dir .. "../../../vp/ros/virtio.lua")
local loopback = dofile(dir .. "../../../vp/ros/loopback.lua")
local vmcu = dofile(dir .. "../../../vp/backends/vmcu.lua")

function ros.create(_ctx)
    local domain = {
        peripherals = catalog.create(map, virtio.map);
        bind_ap_view_targets = address_view.bind_ap_view_targets;
        lower_decode_priorities = address_view.lower_decode_priorities;
    }

    function domain.define(ctx, platform)
        dma.define(ctx, platform, map)
        virtio.define(ctx, platform, map)
        io_peri.define(ctx, platform, map)
        loopback.connect(ctx, platform)
        vmcu.connect(ctx, platform)
        pinctrl.define(platform)
    end

    return domain
end

return ros
