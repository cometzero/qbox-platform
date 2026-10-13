-- SPDX-License-Identifier: BSD-3-Clause
-- Compose shared controls and communication in the established define sequence.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"

local system = dofile(dir .. "system.lua")
local memory = dofile(dir .. "memory.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local rse_si_mailbox = dofile(dir .. "rse_si_mailbox.lua")
local ap_si_mailbox = dofile(dir .. "ap_si_mailbox.lua")
local ap_rse_mailbox = dofile(dir .. "ap_rse_mailbox.lua")

local module = {}

function module.create(ctx)
    local system_mgmt = {}

    function system_mgmt.define(ctx, platform)
        system.define_control(ctx, platform)
        interconnect.define_si(ctx, platform)
        rse_si_mailbox.define(ctx, platform)
        memory.define_rse_si(ctx, platform)
        interconnect.define_ap(ctx, platform)
        ap_si_mailbox.define(ctx, platform)
        interconnect.define_smd(ctx, platform)
        io_peri.define(ctx, platform)
        system.define_reset(ctx, platform)
        system.define_counter(ctx, platform)
        memory.define_shared(ctx, platform)
        ap_rse_mailbox.define(ctx, platform)
        memory.define_ap_rse(ctx, platform)
    end

    return system_mgmt
end

return module
