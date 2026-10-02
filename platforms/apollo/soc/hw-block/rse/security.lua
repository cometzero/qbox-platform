-- RSE protection and crypto register contracts for local or remote execution.
local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local cc3xx_backend = ctx.options.cc3xx_backend
    local cc3xx_stats_file = ctx.options.cc3xx_stats_file
    local cc3xx_stats_interval = ctx.options.cc3xx_stats_interval
    local cc3xx_trace = ctx.options.cc3xx_trace
    local cc3xx_trace_filter = ctx.options.cc3xx_trace_filter
    local cc3xx_trace_limit = ctx.options.cc3xx_trace_limit
    local getenv_or = ctx.options.getenv_or
    local integrity_checker_trace = ctx.options.integrity_checker_trace
    local integrity_checker_trace_limit = ctx.options.integrity_checker_trace_limit
    local kmu_trace = ctx.options.kmu_trace
    local kmu_trace_filter = ctx.options.kmu_trace_filter
    local kmu_trace_limit = ctx.options.kmu_trace_limit
    local lcm_trace = ctx.options.lcm_trace
    local lcm_trace_limit = ctx.options.lcm_trace_limit
    local rse_lcm_lcs = ctx.options.rse_lcm_lcs
    local rse_lcm_sp_enable = ctx.options.rse_lcm_sp_enable
    local rse_lcm_tp_mode = ctx.options.rse_lcm_tp_mode
    local rse_local_crypto = ctx.options.rse_local_crypto
    local rse_otp = ctx.options.rse_otp
    local sam_trace = ctx.options.sam_trace
    local sam_trace_limit = ctx.options.sam_trace_limit
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local RSE_HW = hardware.RSE_HW
    local RSE_RUNTIME_ADDRESS = runtime.addresses
    local function rse_cc3xx_component(target_bind, initiator_bind)
        local component = {
            moduletype = cc3xx_backend == "qemu-native" and
                "qemu_cc3xx" or "cc3xx";
            trace = cc3xx_trace;
            trace_limit = cc3xx_trace_limit;
            trace_skip = tonumber(getenv_or(
                "QBOX_RDASPEN_CC3XX_TRACE_SKIP", "0"));
            trace_filter = cc3xx_trace_filter;
            trace_address_min = RSE_RUNTIME_ADDRESS.cc3xx_trace_min;
            stats_file = cc3xx_stats_file;
            stats_interval = cc3xx_stats_interval;
            target_socket = {
                address = RSE_ADDRESS.cc3xx_secure;
                size = RSE_SIZE.double_register_window;
                bind = target_bind;
            };
            initiator_socket = {bind = initiator_bind};
            log_level = 0;
        }

        if cc3xx_backend == "qemu-native" then
            component.args = {"&qemu_inst"}
            component.size = RSE_SIZE.double_register_window
        end

        return component
    end

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_otp_wrapper == nil,
            "duplicate object: platform.rse_otp_wrapper")
        platform.rse_otp_wrapper = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_ADDRESS.otp_wrapper_secure;
                size = RSE_SIZE.uart_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

        assert(
            platform.rse_cpu0_secctrl_regs == nil,
            "duplicate object: platform.rse_cpu0_secctrl_regs")
        platform.rse_cpu0_secctrl_regs = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_ADDRESS.cpu0_secctrl_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
        assert(
            platform.rse_nsacfg_regs == nil,
            "duplicate object: platform.rse_nsacfg_regs")
        platform.rse_nsacfg_regs = {
            moduletype = "rse_protection_ctrl";
            target_socket = {
                address = RSE_ADDRESS.nsacfg_non_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }
        assert(
            platform.rse_sacfg_regs == nil,
            "duplicate object: platform.rse_sacfg_regs")
        platform.rse_sacfg_regs = {
            moduletype = "rse_protection_ctrl";
            target_socket = {
                address = RSE_ADDRESS.sacfg_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

        assert(
            platform.rse_kmu_regs == nil,
            "duplicate object: platform.rse_kmu_regs")
        if (not rse_local_crypto) then
            platform.rse_kmu_regs = {
                moduletype = "rse_kmu";
                trace = kmu_trace;
                trace_limit = kmu_trace_limit;
                trace_filter = kmu_trace_filter;
                otp_image = rse_otp;
                build_config = RSE_HW.kmu_build_config;
                hw_slot_config = RSE_HW.kmu_hw_slot_config;
                hw_slot_export_address = RSE_ADDRESS.kmu_hw_slot_export;
                target_socket = {
                    address = RSE_ADDRESS.kmu_secure;
                    size = RSE_SIZE.register_window;
                    bind = "&rse_router.initiator_socket";
                };
                initiator_socket = {bind = "&rse_router.target_socket"};
                log_level = 0;
            }
        end

        assert(
            platform.rse_lcm_regs == nil,
            "duplicate object: platform.rse_lcm_regs")
        platform.rse_lcm_regs = {
            moduletype = "rse_lcm";
            trace = lcm_trace;
            trace_limit = lcm_trace_limit;
            otp_image = rse_otp;
            lcs = rse_lcm_lcs;
            tp_mode = rse_lcm_tp_mode;
            sp_enable = rse_lcm_sp_enable;
            otp_size = RSE_SIZE.uart_window;
            otp_writeback = getenv_or("QBOX_RDASPEN_RSE_OTP_WRITEBACK", "false") == "true";
            otp_lock_after_provision =
                getenv_or("QBOX_RDASPEN_RSE_OTP_LOCK_AFTER_PROVISION", "true") == "true";
            target_socket = {
                address = RSE_ADDRESS.lcm_secure;
                size = RSE_SIZE.lcm;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

        assert(
            platform.rse_sam_regs == nil,
            "duplicate object: platform.rse_sam_regs")
        platform.rse_sam_regs = {
            moduletype = "rse_sam";
            trace = sam_trace;
            trace_limit = sam_trace_limit;
            build_config = RSE_HW.sam_build_config;
            target_socket = {
                address = RSE_ADDRESS.sam_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }
        assert(
            platform.rse_mpc_vm0_regs == nil,
            "duplicate object: platform.rse_mpc_vm0_regs")
        platform.rse_mpc_vm0_regs = {
            moduletype = "rse_protection_ctrl";
            profile = 1;
            blk_max = 1;
            blk_cfg = RSE_HW.mpc_block_config;
            target_socket = {
                address = RSE_ADDRESS.mpc_vm0_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

        assert(
            platform.rse_mpc_vm1_regs == nil,
            "duplicate object: platform.rse_mpc_vm1_regs")
        platform.rse_mpc_vm1_regs = {
            moduletype = "rse_protection_ctrl";
            profile = 1;
            blk_max = 1;
            blk_cfg = RSE_HW.mpc_block_config;
            target_socket = {
                address = RSE_ADDRESS.mpc_vm1_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }
        assert(
            platform.rse_sic_regs == nil,
            "duplicate object: platform.rse_sic_regs")
        platform.rse_sic_regs = {
            moduletype = "rse_protection_ctrl";
            target_socket = {
                address = RSE_ADDRESS.sic_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

        assert(
            platform.rse_mpc_sic_regs == nil,
            "duplicate object: platform.rse_mpc_sic_regs")
        platform.rse_mpc_sic_regs = {
            moduletype = "rse_protection_ctrl";
            profile = 1;
            blk_max = 127;
            blk_cfg = RSE_HW.mpc_block_config;
            target_socket = {
                address = RSE_ADDRESS.mpc_sic_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

        assert(
            platform.rse_cc3xx == nil,
            "duplicate object: platform.rse_cc3xx")
        platform.rse_cc3xx = (not rse_local_crypto) and
            rse_cc3xx_component("&rse_router.initiator_socket",
                                "&rse_router.target_socket") or nil

        assert(
            platform.rse_integrity_checker_regs == nil,
            "duplicate object: platform.rse_integrity_checker_regs")
        platform.rse_integrity_checker_regs = {
            moduletype = "rse_integrity_checker";
            trace = integrity_checker_trace;
            trace_limit = integrity_checker_trace_limit;
            build_config = RSE_HW.integrity_checker_build_config;
            target_socket = {
                address = RSE_ADDRESS.integrity_checker_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }
    end

    function block.define_local(ctx, platform)
        assert(
            platform.rse_kmu_regs == nil,
            "duplicate object: platform.rse_kmu_regs")
        if rse_local_crypto then
            platform.rse_kmu_regs = {
                moduletype = "rse_kmu";
                trace = kmu_trace;
                trace_limit = kmu_trace_limit;
                trace_filter = kmu_trace_filter;
                otp_image = rse_otp;
                build_config = RSE_HW.kmu_build_config;
                hw_slot_config = RSE_HW.kmu_hw_slot_config;
                hw_slot_export_address = RSE_ADDRESS.kmu_hw_slot_export;
                target_socket = {
                    address = RSE_ADDRESS.kmu_secure;
                    size = RSE_SIZE.register_window;
                    bind = "&cpu_0.router.initiator_socket";
                };
                initiator_socket = {bind = "&remote_crypto_router.target_socket"};
                log_level = 0;
            }
        end

        assert(
            platform.rse_cc3xx == nil,
            "duplicate object: platform.rse_cc3xx")
        platform.rse_cc3xx = rse_local_crypto and
            rse_cc3xx_component("&remote_crypto_router.initiator_socket",
                                "&remote_main_router.target_socket") or nil
    end

    return block
end

return module
