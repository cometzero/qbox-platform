-- Execution policy overrides for the SoC-owned QEMU instances.
local qemu = {}
local SI_CL1_QEMU = {
    accel_env = "QBOX_APOLLO_FULL_SI_CL1_ACCEL";
    accel = "tcg";
    tcg_mode_env = "QBOX_APOLLO_FULL_SI_CL1_TCG_MODE";
    tcg_mode = "MULTI";
    sync_policy_env = "QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY";
    sync_policy = "multithread-freerunning";
    time_sync_strategy_env = "QBOX_APOLLO_FULL_SI_CL1_TIME_SYNC_STRATEGY";
    time_sync_strategy = "quantum_keeper";
}

function qemu.configure(ctx, platform)
    local instance = platform.si_cl1_qemu_inst
    if instance then
        instance.accel = ctx.getenv_or(SI_CL1_QEMU.accel_env, SI_CL1_QEMU.accel);
        instance.tcg_mode = ctx.getenv_or(
            SI_CL1_QEMU.tcg_mode_env,
            SI_CL1_QEMU.tcg_mode);
        instance.sync_policy = ctx.getenv_or(
            SI_CL1_QEMU.sync_policy_env,
            SI_CL1_QEMU.sync_policy);
        instance.time_sync_strategy = ctx.getenv_or(
            SI_CL1_QEMU.time_sync_strategy_env,
            SI_CL1_QEMU.time_sync_strategy);
        instance.qemu_args = ctx.getenv_or("QBOX_APOLLO_FULL_SI_CL1_QEMU_ARGS", "");
    end
end

return qemu
