-- Execution policy overrides for the SoC-owned QEMU instances.
local qemu = {}
local RSE_QEMU = {
    accel_env = "QBOX_RDASPEN_RSE_ACCEL";
    accel = "tcg";
    tcg_mode_env = "QBOX_RDASPEN_RSE_TCG_MODE";
    tcg_mode = "SINGLE";
    sync_policy_env = "QBOX_RDASPEN_RSE_SYNC_POLICY";
    sync_policy = "multithread-freerunning";
    time_sync_strategy_env = "QBOX_RDASPEN_RSE_TIME_SYNC_STRATEGY";
    time_sync_strategy = "quantum_keeper";
}

function qemu.configure(ctx, platform)
    for _, instance in ipairs({platform.qemu_inst, platform.rse_cpu_pass.qemu_inst}) do
        if instance then
            instance.accel = ctx.getenv_or(RSE_QEMU.accel_env, RSE_QEMU.accel);
            instance.tcg_mode = ctx.getenv_or(RSE_QEMU.tcg_mode_env, RSE_QEMU.tcg_mode);
            instance.sync_policy = ctx.getenv_or(
                RSE_QEMU.sync_policy_env,
                RSE_QEMU.sync_policy);
            instance.time_sync_strategy = ctx.getenv_or(
                RSE_QEMU.time_sync_strategy_env,
                RSE_QEMU.time_sync_strategy);
            instance.qemu_args = ctx.options.qemu_args;
        end
    end
end

return qemu
