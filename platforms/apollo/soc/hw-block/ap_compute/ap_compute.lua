-- Compose AP hardware and the execution adapters in their dependency order.
local ap_compute = {}
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local vp_dir = dir .. "../../../vp/ap/"
local map = dofile(dir .. "map.lua")
local syscore = dofile(dir .. "syscore.lua")
local cpu = dofile(dir .. "cpu.lua")
local pcie = dofile(dir .. "pcie.lua")
local memory = dofile(dir .. "memory.lua")
local interrupts = dofile(dir .. "interrupts.lua")
local iommu = dofile(dir .. "iommu.lua")
local timers = dofile(dir .. "timers.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local address_view = dofile(dir .. "address_view.lua")
local execution = dofile(vp_dir .. "execution.lua")
local testing = dofile(vp_dir .. "testing.lua")
local boot = dofile(vp_dir .. "boot.lua")
local host_io = dofile(vp_dir .. "host_io.lua")
local vp_pcie = dofile(vp_dir .. "pcie.lua")

function ap_compute.create(ctx)
    local hw = map.create(ctx.options)
    local domain = {}

    function domain.define(ctx, platform)
        local policy = execution.policy(ctx)
        testing.define_faults(ctx, platform, hw, policy)
        execution.define(ctx, platform, hw, policy)
        syscore.define_reset(ctx, platform, hw, policy)
        cpu.define_initiator(ctx, platform, hw, policy)
        pcie.define(ctx, platform, hw, policy)
        testing.define_endpoints(ctx, platform, hw, policy)
        vp_pcie.define(ctx, platform, hw, policy)
        memory.define(ctx, platform, hw, policy)
        interrupts.define(ctx, platform, hw, policy)
        iommu.define(ctx, platform, hw, policy)
        timers.define_watchdog(ctx, platform, hw, policy)
        host_io.define(ctx, platform, hw, policy)
        io_peri.define(ctx, platform, hw, policy)
        host_io.connect(ctx, platform)
        timers.define(ctx, platform, hw, policy)
        syscore.define(ctx, platform, hw, policy)
        cpu.define(ctx, platform, hw, policy)
        boot.define(ctx, platform, hw, policy)
    end

    function domain.enable_ap_router(ctx, platform)
        address_view.enable_ap_router(ctx, platform, hw)
    end

    return domain
end

return ap_compute
