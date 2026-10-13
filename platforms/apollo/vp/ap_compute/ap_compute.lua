-- Apply AP execution services and test-only devices to the SoC definition.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local qemu = dofile(dir .. "qemu.lua")
local testing = dofile(dir .. "testing.lua")
local boot = dofile(dir .. "boot.lua")
local console = dofile(dir .. "console.lua")
local pcie = dofile(dir .. "pcie.lua")
local module = {}

function module.configure(ctx)
    ctx.config.ap.pcie = pcie.policy(ctx)
end

function module.apply(ctx, platform)
    local policy = ctx.config.ap.pcie
    qemu.configure(ctx, platform, policy)
    testing.define_faults(ctx, platform, policy)
    testing.define_endpoints(ctx, platform, policy)
    pcie.define(ctx, platform, policy)
    testing.connect_faults(ctx, platform, policy)
    console.define(ctx, platform)
    console.connect(ctx, platform)
    boot.define(ctx, platform)
    if platform.ap_router then
        if platform.ap_global_peripheral_initiator then
            platform.ap_global_peripheral_initiator.global_initiator = {
                bind = "&ap_router.target_socket";
            }
        end
        if platform.ap_bl2_reset_loader then
            platform.ap_bl2_reset_loader.initiator_socket = {
                bind = "&ap_router.target_socket";
            }
        end
    end
end

return module
