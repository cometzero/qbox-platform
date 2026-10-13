local module = {}

function module.create(ctx, _runtime)
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

    local block = {}

    function block.define_qemu_instance(ctx, platform)
        assert(
            platform.si_cl1_qemu_inst_mgr == nil,
            "duplicate object: platform.si_cl1_qemu_inst_mgr")
        platform.si_cl1_qemu_inst_mgr = {
            moduletype = "QemuInstanceManager";
        }
        assert(
            platform.si_cl1_qemu_inst == nil,
            "duplicate object: platform.si_cl1_qemu_inst")
        platform.si_cl1_qemu_inst = {
            moduletype = "QemuInstance";
            args = {"&platform.si_cl1_qemu_inst_mgr", "AARCH64"};
            accel = ctx.getenv_or(SI_CL1_QEMU.accel_env, SI_CL1_QEMU.accel);
            tcg_mode = ctx.getenv_or(
                SI_CL1_QEMU.tcg_mode_env,
                SI_CL1_QEMU.tcg_mode);
            sync_policy = ctx.getenv_or(
                SI_CL1_QEMU.sync_policy_env,
                SI_CL1_QEMU.sync_policy);
            time_sync_strategy = ctx.getenv_or(
                SI_CL1_QEMU.time_sync_strategy_env,
                SI_CL1_QEMU.time_sync_strategy);
            managed_start_in_reset_release = true;
            qemu_args = ctx.getenv_or("QBOX_APOLLO_FULL_SI_CL1_QEMU_ARGS", "");
        }
        return "&platform.si_cl1_qemu_inst"
    end

    return block
end

return module
