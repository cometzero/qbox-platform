-- SPDX-License-Identifier: BSD-3-Clause
-- Assemble the domain hardware without host services.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local map = dofile(dir .. "map.lua")
local dma = dofile(dir .. "dma.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local pinctrl = dofile(dir .. "pinctrl.lua")
local address_view = dofile(dir .. "address_view.lua")

local module = {}

function module.create(ctx, runtime)
    local hardware = {
        map = map;
        dma = dma;
        io_peri = io_peri;
        pinctrl = pinctrl;
        address_view = address_view;
    }

    function hardware.define(ctx, platform)
        dma.define(ctx, platform, map)
        io_peri.define(ctx, platform, map)
        pinctrl.define(platform)
    end

    return hardware
end

return module
