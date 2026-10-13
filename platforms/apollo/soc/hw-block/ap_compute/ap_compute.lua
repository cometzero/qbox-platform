-- Compose AP hardware and the execution adapters in their dependency order.
local ap_compute = {}
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local vp_dir = dir .. "../../../vp/ap/"
local system = dofile(dir .. "system.lua")
local cpu = dofile(dir .. "cpu.lua")
local pcie = dofile(dir .. "pcie.lua")
local memory = dofile(dir .. "memory.lua")
local safety = dofile(dir .. "safety.lua")
local boot_devices = dofile(dir .. "boot.lua")
local io_peri = dofile(dir .. "io_peri.lua")
local interconnect = dofile(dir .. "interconnect.lua")
local execution = dofile(vp_dir .. "execution.lua")
local testing = dofile(vp_dir .. "testing.lua")
local boot = dofile(vp_dir .. "boot.lua")
local host_io = dofile(vp_dir .. "host_io.lua")
local vp_pcie = dofile(vp_dir .. "pcie.lua")

function ap_compute.create(ctx)
    local domain = {}

    function domain.define(ctx, platform)
        local policy = execution.policy(ctx)
        testing.define_faults(ctx, platform, policy)
        execution.define(ctx, platform, policy)
        cpu.define_reset(ctx, platform)
        system.define_reset(ctx, platform)
        interconnect.define_initiator(ctx, platform)
        pcie.define(ctx, platform, policy)
        testing.define_endpoints(ctx, platform, policy)
        vp_pcie.define(ctx, platform, policy)
        memory.define(ctx, platform)
        boot_devices.define(ctx, platform)
        system.define_interrupts(ctx, platform)
        system.define_iommu(ctx, platform, policy)
        system.define_watchdog(ctx, platform)
        host_io.define(ctx, platform)
        io_peri.define(ctx, platform)
        host_io.connect(ctx, platform)
        system.define_timers(ctx, platform)
        system.define(ctx, platform)
        system.define_message_register(ctx, platform)
        safety.define(ctx, platform)
        cpu.define(ctx, platform)
        boot.define(ctx, platform)
    end

    function domain.enable_ap_router(ctx, platform)
        interconnect.enable_ap_router(ctx, platform)
    end

    return domain
end

return ap_compute
