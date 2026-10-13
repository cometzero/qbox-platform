-- Execution policy overrides for the SoC-owned QEMU instances.
local qemu = {}
local AP_QEMU = {
    accel_env = "QBOX_APOLLO_FULL_AP_ACCEL";
    accel = "tcg";
    tcg_mode_env = "QBOX_APOLLO_FULL_AP_TCG_MODE";
    tcg_mode = "MULTI";
    sync_policy_env = "QBOX_APOLLO_FULL_AP_SYNC_POLICY";
    sync_policy = "multithread-freerunning";
    time_sync_strategy_env = "QBOX_APOLLO_FULL_AP_TIME_SYNC_STRATEGY";
    time_sync_strategy = "quantum_keeper";
}

function qemu.configure(ctx, platform, policy)
    local instance = platform.ap_qemu_inst
    if instance then
        instance.accel = ctx.getenv_or(AP_QEMU.accel_env, AP_QEMU.accel);
        instance.tcg_mode = ctx.getenv_or(AP_QEMU.tcg_mode_env, AP_QEMU.tcg_mode);
        instance.sync_policy = ctx.getenv_or(
            AP_QEMU.sync_policy_env,
            AP_QEMU.sync_policy);
        instance.time_sync_strategy = ctx.getenv_or(
            AP_QEMU.time_sync_strategy_env,
            AP_QEMU.time_sync_strategy);
        instance.qemu_args = policy.pci_qemu_args;
    end
end

return qemu
