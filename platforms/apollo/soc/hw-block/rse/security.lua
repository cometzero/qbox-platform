local RSE_CC3XX_SECURE_BASE = 0x50154000
local RSE_DOUBLE_REGISTER_WINDOW_SIZE = 0x00002000
local RSE_OTP_WRAPPER_SECURE_BASE = 0x58111000
local RSE_UART_WINDOW_SIZE = 0x00010000
local RSE_CPU0_SECCTRL_SECURE_BASE = 0x50011000
local RSE_REGISTER_WINDOW_SIZE = 0x00001000
local RSE_NSACFG_NON_SECURE_BASE = 0x40080000
local RSE_SACFG_SECURE_BASE = 0x50080000
local RSE_KMU_BUILD_CONFIG = 0x003D0005
local RSE_KMU_HW_SLOT_CONFIG = 0x00D60100
local RSE_KMU_HW_SLOT_EXPORT_BASE = 0x50154400
local RSE_KMU_SECURE_BASE = 0x5009E000
local RSE_LCM_SECURE_BASE = 0x500A0000
local RSE_LCM_SIZE = 0x00011000
local RSE_SAM_BUILD_CONFIG = 0x00000700
local RSE_SAM_SECURE_BASE = 0x5009F000
local RSE_MPC_BLOCK_CONFIG = 0x00000007
local RSE_MPC_VM0_SECURE_BASE = 0x50083000
local RSE_MPC_VM1_SECURE_BASE = 0x50084000
local RSE_SIC_SECURE_BASE = 0x50140000
local RSE_MPC_SIC_SECURE_BASE = 0x50151000
local RSE_INTEGRITY_CHECKER_BUILD_CONFIG = 0x00000109
local RSE_INTEGRITY_CHECKER_SECURE_BASE = 0x5015C000
local RSE_TIMER0_SECURE_BASE = 0x58000000
local RSE_TIMER1_SECURE_BASE = 0x58001000
local RSE_TIMER2_SECURE_BASE = 0x58002000
local RSE_TIMER3_SECURE_BASE = 0x58003000
local RSE_TIMER0_NON_SECURE_BASE = 0x48000000
local RSE_TIMER1_NON_SECURE_BASE = 0x48001000
local RSE_TIMER2_NON_SECURE_BASE = 0x48002000
local RSE_TIMER3_NON_SECURE_BASE = 0x48003000

-- RSE protection and crypto register contracts for local or remote execution.
local module = {}

function module.create(ctx, runtime)
    local cc3xx_backend = ctx.options.cc3xx_backend
    local cc3xx_stats_interval = ctx.options.cc3xx_stats_interval
    local cc3xx_trace = ctx.options.cc3xx_trace
    local cc3xx_trace_filter = ctx.options.cc3xx_trace_filter
    local cc3xx_trace_limit = ctx.options.cc3xx_trace_limit
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
    local sam_trace = ctx.options.sam_trace
    local sam_trace_limit = ctx.options.sam_trace_limit
    local RSE_RUNTIME_ADDRESS = runtime.addresses
    local function rse_cc3xx_component(target_bind, initiator_bind)
        local component = {
            moduletype = cc3xx_backend == "qemu-native" and
                "qemu_cc3xx" or "cc3xx";
            trace = cc3xx_trace;
            trace_limit = cc3xx_trace_limit;
            trace_skip = ctx.options.cc3xx_trace_skip;
            trace_filter = cc3xx_trace_filter;
            trace_address_min = RSE_RUNTIME_ADDRESS.cc3xx_trace_min;
            stats_interval = cc3xx_stats_interval;
            target_socket = {
                address = RSE_CC3XX_SECURE_BASE;
                size = RSE_DOUBLE_REGISTER_WINDOW_SIZE;
                bind = target_bind;
            };
            initiator_socket = {bind = initiator_bind};
            log_level = 0;
        }

        if cc3xx_backend == "qemu-native" then
            component.args = {ctx.instances.rse_local}
            component.size = RSE_DOUBLE_REGISTER_WINDOW_SIZE
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
                address = RSE_OTP_WRAPPER_SECURE_BASE;
                size = RSE_UART_WINDOW_SIZE;
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
                address = RSE_CPU0_SECCTRL_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
                address = RSE_NSACFG_NON_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
                address = RSE_SACFG_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
                build_config = RSE_KMU_BUILD_CONFIG;
                hw_slot_config = RSE_KMU_HW_SLOT_CONFIG;
                hw_slot_export_address = RSE_KMU_HW_SLOT_EXPORT_BASE;
                target_socket = {
                    address = RSE_KMU_SECURE_BASE;
                    size = RSE_REGISTER_WINDOW_SIZE;
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
            lcs = rse_lcm_lcs;
            tp_mode = rse_lcm_tp_mode;
            sp_enable = rse_lcm_sp_enable;
            otp_size = RSE_UART_WINDOW_SIZE;
            otp_lock_after_provision =
                ctx.options.rse_otp_lock_after_provision;
            target_socket = {
                address = RSE_LCM_SECURE_BASE;
                size = RSE_LCM_SIZE;
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
            build_config = RSE_SAM_BUILD_CONFIG;
            target_socket = {
                address = RSE_SAM_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
            blk_cfg = RSE_MPC_BLOCK_CONFIG;
            target_socket = {
                address = RSE_MPC_VM0_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
            blk_cfg = RSE_MPC_BLOCK_CONFIG;
            target_socket = {
                address = RSE_MPC_VM1_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
                address = RSE_SIC_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
            blk_cfg = RSE_MPC_BLOCK_CONFIG;
            target_socket = {
                address = RSE_MPC_SIC_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
            build_config = RSE_INTEGRITY_CHECKER_BUILD_CONFIG;
            target_socket = {
                address = RSE_INTEGRITY_CHECKER_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
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
                build_config = RSE_KMU_BUILD_CONFIG;
                hw_slot_config = RSE_KMU_HW_SLOT_CONFIG;
                hw_slot_export_address = RSE_KMU_HW_SLOT_EXPORT_BASE;
                target_socket = {
                    address = RSE_KMU_SECURE_BASE;
                    size = RSE_REGISTER_WINDOW_SIZE;
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

    function block.define_timer_protection(ctx, platform)
        local rse_timer_policy_masks = {1, 2, 4, 32}
        local rse_timer_secure_bases = {
            RSE_TIMER0_SECURE_BASE;
            RSE_TIMER1_SECURE_BASE;
            RSE_TIMER2_SECURE_BASE;
            RSE_TIMER3_SECURE_BASE;
        }
        local rse_timer_non_secure_bases = {
            RSE_TIMER0_NON_SECURE_BASE;
            RSE_TIMER1_NON_SECURE_BASE;
            RSE_TIMER2_NON_SECURE_BASE;
            RSE_TIMER3_NON_SECURE_BASE;
        }
        for timer = 0,3 do
            assert(
                platform["rse_timer_" .. timer .. "_ppc"] == nil,
                "duplicate object: platform." .. ("rse_timer_" .. timer .. "_ppc"))
            platform["rse_timer_" .. timer .. "_ppc"] = {
                moduletype = "rse_ppc_filter";
                args = {
                    "&platform.rse_sacfg_regs";
                    "&platform.rse_nsacfg_regs";
                };
                policy_mask = rse_timer_policy_masks[timer + 1];
                target_socket = {
                    address = rse_timer_secure_bases[timer + 1];
                    size = RSE_REGISTER_WINDOW_SIZE;
                    bind = "&rse_router.initiator_socket";
                    aliases = {
                        ns = {
                            address = rse_timer_non_secure_bases[timer + 1];
                            size = RSE_REGISTER_WINDOW_SIZE;
                        };
                    };
                };
                initiator_socket = {
                    bind = "&rse_cpu_pass.target_socket_" .. (timer + 2);
                };
                log_level = 0;
            }
        end
    end

    return block
end

return module
