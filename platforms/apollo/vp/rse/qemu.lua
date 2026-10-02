local module = {}

function module.create(ctx, _hardware, _runtime, _interrupts)
    local qemu_args = ctx.options.qemu_args
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

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.qemu_inst_mgr == nil,
            "duplicate object: platform.qemu_inst_mgr")
        platform.qemu_inst_mgr = {
            moduletype = "QemuInstanceManager";
        }

        assert(
            platform.qemu_inst == nil,
            "duplicate object: platform.qemu_inst")
        platform.qemu_inst = {
            moduletype = "QemuInstance";
            args = {"&platform.qemu_inst_mgr", "AARCH64"};
            accel = ctx.getenv_or(RSE_QEMU.accel_env, RSE_QEMU.accel);
            tcg_mode = ctx.getenv_or(RSE_QEMU.tcg_mode_env, RSE_QEMU.tcg_mode);
            sync_policy = ctx.getenv_or(
                RSE_QEMU.sync_policy_env,
                RSE_QEMU.sync_policy);
            time_sync_strategy = ctx.getenv_or(
                RSE_QEMU.time_sync_strategy_env,
                RSE_QEMU.time_sync_strategy);
            qemu_args = qemu_args;
        }
    end

    function block.define_local(ctx, platform)
        assert(
            platform.qemu_inst_mgr == nil,
            "duplicate object: platform.qemu_inst_mgr")
        platform.qemu_inst_mgr = {
            moduletype = "QemuInstanceManager";
        }

        assert(
            platform.qemu_inst == nil,
            "duplicate object: platform.qemu_inst")
        platform.qemu_inst = {
            moduletype = "QemuInstance";
            args = {"&qemu_inst_mgr", "AARCH64"};
            accel = ctx.getenv_or(RSE_QEMU.accel_env, RSE_QEMU.accel);
            tcg_mode = ctx.getenv_or(RSE_QEMU.tcg_mode_env, RSE_QEMU.tcg_mode);
            sync_policy = ctx.getenv_or(
                RSE_QEMU.sync_policy_env,
                RSE_QEMU.sync_policy);
            time_sync_strategy = ctx.getenv_or(
                RSE_QEMU.time_sync_strategy_env,
                RSE_QEMU.time_sync_strategy);
            qemu_args = qemu_args;
        }
    end

    return block
end

return module
