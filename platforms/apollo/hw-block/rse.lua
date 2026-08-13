local rse = {}

local RSE_QEMU = {
    accel_env = "QBOX_RDASPEN_RSE_ACCEL";
    accel = "tcg";
    tcg_mode_env = "QBOX_RDASPEN_RSE_TCG_MODE";
    tcg_mode = "SINGLE";
    sync_policy_env = "QBOX_RDASPEN_RSE_SYNC_POLICY";
    sync_policy = "multithread-quantum";
    time_sync_strategy_env = "QBOX_RDASPEN_RSE_TIME_SYNC_STRATEGY";
    time_sync_strategy = "quantum_keeper";
}
local RSE_ADDRESS = {
    rom_secure = 0x11000000;
    itcm_non_secure = 0x00000000;
    itcm_cpu0_non_secure = 0x0A000000;
    itcm_secure = 0x10000000;
    itcm_cpu0_secure = 0x1A000000;
    dtcm_non_secure = 0x20000000;
    dtcm_cpu0_non_secure = 0x24000000;
    dtcm_secure = 0x30000000;
    dtcm_cpu0_secure = 0x34000000;
    vm0_secure = 0x31000000;
    boot_flash_secure = 0xB0000000;
    host_access_non_secure = 0x60000000;
    host_access_secure = 0x70000000;
    nsacfg_non_secure = 0x40080000;
    wdog_non_secure_control = 0x48040000;
    wdog_non_secure_refresh = 0x48041000;
    dma350_secure = 0x50002000;
    sacfg_secure = 0x50080000;
    kmu_secure = 0x5009E000;
    sam_secure = 0x5009F000;
    lcm_secure = 0x500A0000;
    cpu0_secctrl_secure = 0x50011000;
    cpu0_pwrctrl_secure = 0x50012000;
    cpu0_pwrctrl_non_secure = 0x40012000;
    cpu0_identity_secure = 0x5001F000;
    cpu0_identity_non_secure = 0x4001F000;
    sic_secure = 0x50140000;
    atu_secure = 0x50150000;
    mpc_sic_secure = 0x50151000;
    cc3xx_secure = 0x50154000;
    syscounter_control_secure = 0x5015A000;
    syscounter_read_secure = 0x5015B000;
    integrity_checker_secure = 0x5015C000;
    tram_secure = 0x5015D000;
    mhu0_sender_secure = 0x50160000;
    mhu0_receiver_secure = 0x50170000;
    mhu2_sender_secure = 0x501A0000;
    mhu2_receiver_secure = 0x501B0000;
    timer0_non_secure = 0x48000000;
    timer1_non_secure = 0x48001000;
    timer2_non_secure = 0x48002000;
    timer3_non_secure = 0x48003000;
    timer0_secure = 0x58000000;
    timer1_secure = 0x58001000;
    timer2_secure = 0x58002000;
    timer3_secure = 0x58003000;
    sysctrl_secure = 0x58021000;
    wdog_secure_control = 0x58040000;
    wdog_secure_refresh = 0x58041000;
    integ_layer_secure = 0x58100000;
    mpc_vm0_secure = 0x50083000;
    mpc_vm1_secure = 0x50084000;
    otp_wrapper_secure = 0x58111000;
    nvic = 0xE000E000;
    kmu_hw_slot_export = 0x50154400;
    remote_base = 0x00000000;
}
local RSE_SIZE = {
    rom = 0x00020000;
    itcm = 0x00008000;
    dtcm = 0x00008000;
    vm = 2 ^ rse_vmaddrwidth;
    provisioning_offset = 0x00020000;
    boot_flash = 0x04000000;
    flash_sector = 0x00001000;
    host_access = 0x10000000;
    lcm = 0x00011000;
    local_mhu_frame = 0x00010000;
    nvic = 0x00010000;
    register_window = 0x00001000;
    double_register_window = 0x00002000;
    uart_window = 0x00010000;
    remote_post_nvic = 0x00100000;
}
RSE_ADDRESS.vm1_secure = RSE_ADDRESS.vm0_secure + RSE_SIZE.vm
RSE_ADDRESS.host_uart0_non_secure =
    RSE_ADDRESS.host_access_non_secure + 0x0FF00000
RSE_ADDRESS.host_uart0_secure =
    RSE_ADDRESS.host_access_secure + 0x0FF00000
local RSE_IRQ = {
    timer0 = 3;
    timer1 = 4;
    timer2 = 5;
    timer3 = 27;
    cmu_mhu0_receiver = 41;
    cmu_mhu2_receiver = 45;
}
local RSE_HW = {
    nvic_num_irq = 160;
    watchdog_clock_hz = 32000000;
    kmu_build_config = 0x003D0005;
    kmu_hw_slot_config = 0x00D60100;
    sam_build_config = 0x00000700;
    mpc_block_config = 0x00000007;
    atu_build_config = 0x000000C5;
    integrity_checker_build_config = 0x00000109;
}

local RSE_RUNTIME_ADDRESS = {
    hotpath_memcpy = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_HOTPATH_MEMCPY_ADDR", "0x11000488"));
    hotpath_memset = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_HOTPATH_MEMSET_ADDR", "0x11000448"));
    lms_verify = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_LMS_VERIFY_ADDR", "0x11009bad"));
    bl2_boot_go_for_image_id = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOT_GO_FOR_IMAGE_ID_ADDR", "0x3101e288"));
    bl2_boot_load_image_to_sram = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOT_LOAD_IMAGE_TO_SRAM_ADDR", "0x3101e758"));
    bl2_boot_enc_load = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOT_ENC_LOAD_ADDR", "0x3101eeb6"));
    bl2_boot_enc_set_key = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOT_ENC_SET_KEY_ADDR", "0x3101ef52"));
    bl2_boot_enc_decrypt = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOT_ENC_DECRYPT_ADDR", "0x3101ef8c"));
    bl2_bootutil_img_validate = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_IMG_VALIDATE_ADDR", "0x3101f010"));
    bl2_bootutil_img_hash = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_IMG_HASH_ADDR", "0x3101f3aa"));
    bl2_bootutil_verify_sig = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_VERIFY_SIG_ADDR", "0x3101f5bc"));
    bl2_bootutil_keys = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_KEYS_ADDR", "0x31000454"));
    bl2_bootutil_key_cnt = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_KEY_CNT_ADDR", "0x3102b424"));
    bl2_fih_success = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_FIH_SUCCESS_ADDR", "0x310027dc"));
    bl2_delay_cycles = tonumber(getenv_or(
        "QBOX_RDASPEN_RSE_BL2_DELAY_CYCLES_ADDR", "0x31021aca"));
    cc3xx_trace_min = tonumber(getenv_or(
        "QBOX_RDASPEN_CC3XX_TRACE_ADDRESS_MIN", "0"));
    dma350_trace_min = tonumber(getenv_or(
        "QBOX_RDASPEN_DMA350_TRACE_ADDRESS_MIN", "0"));
    atu_trace_min = tonumber(getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MIN", "0"));
    atu_trace_max = tonumber(getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MAX", "0"));
    dma_boot = getenv_number_or(
        "QBOX_RDASPEN_RSE_DMA_BOOT_ADDR", "0x00000000");
}

local function rse_tcm_aliases(
    split_cpu0_alias, ns_address, cpu0_s_address, cpu0_ns_address, size)
    local aliases = {
        ns = {address = ns_address; size = size;};
    }

    if not split_cpu0_alias then
        aliases.cpu0_s = {address = cpu0_s_address; size = size;}
        aliases.cpu0_ns = {address = cpu0_ns_address; size = size;}
    end

    return aliases
end

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

function rse.define(ctx, platform)
    print("Apollo RSE QBox skeleton config running...")

    local rse_sys_rss_reset_targets = {
        "&rse_cpu_pass.cpu_0.cpu.reset";
        "&rse_cpu_pass.rse_nvic_cold_reset.reset";
        "&rse_cpu_pass.rse_timer_0.reset";
        "&rse_cpu_pass.rse_timer_1.reset";
        "&rse_cpu_pass.rse_timer_2.reset";
    }
    if rse_local_boot_flash and rse_flash_backend == "qemu-cfi-local" then
        rse_sys_rss_reset_targets[#rse_sys_rss_reset_targets + 1] =
            "&rse_cpu_pass.rse_pflash_cold_reset.reset"
    end

    local rse_aon_reset_targets = {
        "&rse_cpu_pass.rse_timer_3.reset";
    }
    if not rse_smd_counter_mirror then
        rse_aon_reset_targets[#rse_aon_reset_targets + 1] =
            "&rse_cpu_pass.rse_local_system_counter.reset"
    end

    platform.rse_router = {
        moduletype = "router";
        broadcast_invalidation = true;
        log_level = 0;
    }

    platform.qemu_inst_mgr = {
        moduletype = "QemuInstanceManager";
    }

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

    platform.rse_sys_rss_reset_fanout = {
        moduletype = "reset_fanout";
        reset_out = {bind = table.concat(rse_sys_rss_reset_targets, ";")};
        log_level = 0;
    }

    platform.rse_aon_reset_fanout = {
        moduletype = "reset_fanout";
        reset_out = {bind = table.concat(rse_aon_reset_targets, ";")};
        log_level = 0;
    }

    platform.rse_rom = {
        moduletype = "gs_memory";
        read_only = true;
        shared_memory = true;
        shared_memory_prefix = "rse_rom_";
        target_socket = {
            address = RSE_ADDRESS.rom_secure;
            size = RSE_SIZE.rom;
            bind = "&rse_router.initiator_socket";
        };
        load = {bin_file = rse_rom, offset = 0};
        log_level = 0;
    }

    platform.rse_itcm = {
        moduletype = "gs_memory";
        shared_memory = true;
        shared_memory_prefix = "rse_itcm_";
        dmi_allow = rse_itcm_dmi;
        target_socket = {
            address = RSE_ADDRESS.itcm_secure;
            size = RSE_SIZE.itcm;
            bind = "&rse_router.initiator_socket";
            aliases = rse_tcm_aliases(
                rse_split_cpu0_itcm_alias,
                RSE_ADDRESS.itcm_non_secure,
                RSE_ADDRESS.itcm_cpu0_secure,
                RSE_ADDRESS.itcm_cpu0_non_secure,
                RSE_SIZE.itcm);
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_itcm_cpu0 = rse_split_cpu0_itcm_alias and {
        moduletype = "gs_memory";
        shared_memory = true;
        shared_memory_prefix = "rse_itcm_cpu0_";
        dmi_allow = rse_itcm_dmi;
        target_socket = {
            address = RSE_ADDRESS.itcm_cpu0_secure;
            size = RSE_SIZE.itcm;
            bind = "&rse_router.initiator_socket";
            aliases = {
                cpu0_ns = {
                    address = RSE_ADDRESS.itcm_cpu0_non_secure;
                    size = RSE_SIZE.itcm;
                };
            };
        };
        init_mem = true;
        log_level = 0;
    } or nil

    platform.rse_dtcm = {
        moduletype = "gs_memory";
        shared_memory = true;
        shared_memory_prefix = "rse_dtcm_";
        dmi_allow = rse_dtcm_dmi;
        target_socket = {
            address = RSE_ADDRESS.dtcm_secure;
            size = RSE_SIZE.dtcm;
            bind = "&rse_router.initiator_socket";
            aliases = rse_tcm_aliases(
                rse_split_cpu0_dtcm_alias,
                RSE_ADDRESS.dtcm_non_secure,
                RSE_ADDRESS.dtcm_cpu0_secure,
                RSE_ADDRESS.dtcm_cpu0_non_secure,
                RSE_SIZE.dtcm);
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_dtcm_cpu0 = rse_split_cpu0_dtcm_alias and {
        moduletype = "gs_memory";
        shared_memory = true;
        shared_memory_prefix = "rse_dtcm_cpu0_";
        dmi_allow = rse_dtcm_dmi;
        target_socket = {
            address = RSE_ADDRESS.dtcm_cpu0_secure;
            size = RSE_SIZE.dtcm;
            bind = "&rse_router.initiator_socket";
            aliases = {
                cpu0_ns = {
                    address = RSE_ADDRESS.dtcm_cpu0_non_secure;
                    size = RSE_SIZE.dtcm;
                };
            };
        };
        init_mem = true;
        log_level = 0;
    } or nil

    platform.rse_vm0 = {
        moduletype = "gs_memory";
        shared_memory = true;
        shared_memory_prefix = "rse_vm0_";
        dmi_allow = rse_vm_dmi;
        target_socket = {
            address = RSE_ADDRESS.vm0_secure;
            size = RSE_SIZE.vm;
            bind = "&rse_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_vm1 = {
        moduletype = "gs_memory";
        shared_memory = true;
        shared_memory_prefix = "rse_vm1_";
        dmi_allow = rse_vm_dmi;
        target_socket = {
            address = RSE_ADDRESS.vm1_secure;
            size = RSE_SIZE.vm;
            bind = "&rse_router.initiator_socket";
        };
        init_mem = true;
        load = {bin_file = provisioning_bundle, offset = RSE_SIZE.provisioning_offset};
        log_level = 0;
    }

    platform.rse_boot_flash = (not rse_local_boot_flash) and {
        moduletype = "strata_flash_j3";
        trace = boot_flash_trace;
        trace_limit = boot_flash_trace_limit;
        enable_dmi = boot_flash_dmi;
        dmi_ranges = boot_flash_dmi_ranges;
        program_ff_sets_bits = true;
        program_ff_erases_sector = true;
        size = RSE_SIZE.boot_flash;
        sector_size = RSE_SIZE.flash_sector;
        backing_file = flash_writeback and rse_flash or "";
        defer_backing_write = true;
        defer_backing_flush_interval = flash_defer_backing_flush_interval;
        stats_file = rse_boot_flash_stats_file;
        stats_interval = flash_stats_interval;
        target_socket = {
            address = RSE_ADDRESS.boot_flash_secure;
            size = RSE_SIZE.boot_flash;
            bind = "&rse_router.initiator_socket";
        };
        load = {bin_file = rse_flash, offset = 0};
        log_level = 0;
    } or nil

    -- RSE local peripherals

    platform.rse_otp_wrapper = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_ADDRESS.otp_wrapper_secure;
            size = RSE_SIZE.uart_window;
            bind = "&rse_router.initiator_socket";
        };
        load = {bin_file = rse_otp, offset = 0};
        log_level = 0;
    }

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

    platform.rse_cpu0_pwrctrl_regs = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_ADDRESS.cpu0_pwrctrl_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
            aliases = {
                ns = {
                    address = RSE_ADDRESS.cpu0_pwrctrl_non_secure;
                    size = RSE_SIZE.register_window;
                };
            };
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_cpu0_identity_regs = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_ADDRESS.cpu0_identity_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
            aliases = {
                ns = {
                    address = RSE_ADDRESS.cpu0_identity_non_secure;
                    size = RSE_SIZE.register_window;
                };
            };
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_nsacfg_regs = {
        moduletype = "rse_protection_ctrl";
        target_socket = {
            address = RSE_ADDRESS.nsacfg_non_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_watchdog_ns = {
        moduletype = "zena_watchdog";
        clock_frequency = RSE_HW.watchdog_clock_hz;
        control = {
            address = RSE_ADDRESS.wdog_non_secure_control;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        refresh = {
            address = RSE_ADDRESS.wdog_non_secure_refresh;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        ws0 = {bind = "&rse_cpu_pass.target_signal_socket_1"};
        ws1 = {bind = "&rse_cpu_pass.target_signal_socket_0"};
        log_level = 0;
    }

    platform.rse_watchdog_s = {
        moduletype = "zena_watchdog";
        clock_frequency = RSE_HW.watchdog_clock_hz;
        control = {
            address = RSE_ADDRESS.wdog_secure_control;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        refresh = {
            address = RSE_ADDRESS.wdog_secure_refresh;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_dma350 = {
        moduletype = "dma350";
        trace = dma350_trace;
        trace_limit = dma350_trace_limit;
        trace_filter = dma350_trace_filter;
        trace_address_min = RSE_RUNTIME_ADDRESS.dma350_trace_min;
        target_socket = {
            address = RSE_ADDRESS.dma350_secure;
            size = RSE_SIZE.double_register_window;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&rse_router.target_socket"};
        log_level = 0;
    }

    platform.rse_sacfg_regs = {
        moduletype = "rse_protection_ctrl";
        target_socket = {
            address = RSE_ADDRESS.sacfg_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_kmu_regs = (not rse_local_crypto) and {
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
    } or nil

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

    local rse_timer_policy_masks = {1, 2, 4, 32}
    local rse_timer_secure_bases = {
        RSE_ADDRESS.timer0_secure;
        RSE_ADDRESS.timer1_secure;
        RSE_ADDRESS.timer2_secure;
        RSE_ADDRESS.timer3_secure;
    }
    local rse_timer_non_secure_bases = {
        RSE_ADDRESS.timer0_non_secure;
        RSE_ADDRESS.timer1_non_secure;
        RSE_ADDRESS.timer2_non_secure;
        RSE_ADDRESS.timer3_non_secure;
    }
    for timer=0,3 do
        platform["rse_timer_"..timer.."_ppc"] = {
            moduletype = "rse_ppc_filter";
            args = {
                "&platform.rse_sacfg_regs";
                "&platform.rse_nsacfg_regs";
            };
            policy_mask = rse_timer_policy_masks[timer + 1];
            target_socket = {
                address = rse_timer_secure_bases[timer + 1];
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
                aliases = {
                    ns = {
                        address = rse_timer_non_secure_bases[timer + 1];
                        size = RSE_SIZE.register_window;
                    };
                };
            };
            initiator_socket = {
                bind = "&rse_cpu_pass.target_socket_"..(timer + 2);
            };
            log_level = 0;
        }
    end

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

    platform.rse_atu_regs = {
        moduletype = "rse_atu";
        trace = atu_trace;
        trace_limit = atu_trace_limit;
        trace_filter = atu_trace_filter;
        trace_address_min = RSE_RUNTIME_ADDRESS.atu_trace_min;
        trace_address_max = RSE_RUNTIME_ADDRESS.atu_trace_max;
        enable_dmi = atu_dmi;
        build_config = RSE_HW.atu_build_config;
        target_socket = {
            address = RSE_ADDRESS.atu_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        translation_socket = {
            address = RSE_ADDRESS.host_access_non_secure;
            size = RSE_SIZE.host_access;
            bind = "&rse_router.initiator_socket";
            relative_addresses = false;
            priority = 10;
            aliases = {
                secure = {
                    address = RSE_ADDRESS.host_access_secure;
                    size = RSE_SIZE.host_access;
                };
            };
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.rse_sic_regs = {
        moduletype = "rse_protection_ctrl";
        target_socket = {
            address = RSE_ADDRESS.sic_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

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

    platform.rse_cc3xx = (not rse_local_crypto) and
        rse_cc3xx_component("&rse_router.initiator_socket",
                            "&rse_router.target_socket") or nil

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

    platform.rse_tram = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_ADDRESS.tram_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_mhu0_sender_s = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "rse_ap_monitor_local";
        protocol = "doorbell";
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = RSE_ADDRESS.mhu0_sender_secure;
            size = RSE_SIZE.local_mhu_frame;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.rse_mhu0_receiver_s = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "rse_ap_monitor_local";
        protocol = "doorbell";
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = RSE_ADDRESS.mhu0_receiver_secure;
            size = RSE_SIZE.local_mhu_frame;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&rse_cpu_pass.target_signal_socket_"..
            RSE_IRQ.cmu_mhu0_receiver};
        log_level = 0;
    }

    platform.rse_mhu2_sender_s = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "rse_to_ap_s";
        protocol = "doorbell-bridge";
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = RSE_ADDRESS.mhu2_sender_secure;
            size = RSE_SIZE.local_mhu_frame;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.rse_mhu2_receiver_s = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "ap_s_to_rse";
        protocol = "doorbell-bridge";
        doorbell_commit_on_notify = true;
        init_shmem = false;
        trace = mhu_trace;
        trace_limit = mhu_trace_limit;
        trace_file = mhu_trace_file;
        target_socket = {
            address = RSE_ADDRESS.mhu2_receiver_secure;
            size = RSE_SIZE.local_mhu_frame;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&rse_cpu_pass.target_signal_socket_"..
            RSE_IRQ.cmu_mhu2_receiver};
        log_level = 0;
    }

    platform.rse_sysctrl = {
        moduletype = "rse_sysctrl";
        trace = sysctrl_trace;
        trace_limit = sysctrl_trace_limit;
        reset_syndrome = rse_reset_syndrome;
        cpuwait = rse_cpuwait;
        dma_boot_en = rse_dma_boot_en;
        dma_boot_addr = RSE_RUNTIME_ADDRESS.dma_boot;
        target_socket = {
            address = RSE_ADDRESS.sysctrl_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        system_reset = {bind = "&apollo_system_reset_fanout.reset_in"};
        log_level = 0;
    }

    platform.rse_integ_layer_regs = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_ADDRESS.integ_layer_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_uart_file = {
        moduletype = "char_backend_file";
        read_file = rse_uart_read_file;
        write_file = rse_log;
        poll_read = rse_uart_read_file ~= "/dev/null";
        poll_interval_ms = uart_poll_interval_ms;
        baudrate = 0;
    }

    platform.rse_host_uart0_s = {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = {
            address = RSE_ADDRESS.host_uart0_secure;
            size = RSE_SIZE.uart_window;
            bind = "&rse_router.initiator_socket";
            aliases = {
                ns_atu_logical = {
                    address = RSE_ADDRESS.host_uart0_non_secure;
                    size = RSE_SIZE.uart_window;
                };
            };
        };
        irq = {bind = "&rse_cpu_pass.target_signal_socket_0"};
        backend_socket = {bind = "&rse_uart_file.biflow_socket"};
    }

    platform.rse_cpu_pass = {
        moduletype = "Container";
        tlm_initiator_ports_num = 2;
        tlm_target_ports_num = 6;
        target_signals_num = RSE_HW.nvic_num_irq;
        initiator_signals_num = 0;
        initiator_socket_0 = {bind = "&rse_router.target_socket"};
        initiator_socket_1 = {bind = "&rse_router.target_socket"};
        target_socket_0 = {
            address = RSE_ADDRESS.syscounter_control_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };
        target_socket_1 = {
            address = RSE_ADDRESS.syscounter_read_secure;
            size = RSE_SIZE.register_window;
            bind = "&rse_router.initiator_socket";
        };

        remote_main_router = rse_local_peripherals and {
            moduletype = "router";
            broadcast_invalidation = true;
            target_socket = {
                address = RSE_ADDRESS.remote_base;
                size = RSE_ADDRESS.nvic;
                bind = "&cpu_0.router.initiator_socket";
                relative_addresses = false;
                priority = 100;
                aliases = {
                    post_nvic = {
                        address = RSE_ADDRESS.nvic + RSE_SIZE.nvic;
                        size = RSE_SIZE.remote_post_nvic;
                        priority = 100;
                    };
                };
            };
            log_level = 0;
        } or nil,

        remote_crypto_router = rse_local_crypto and {
            moduletype = "router";
            target_socket = {
                address = RSE_ADDRESS.cc3xx_secure;
                size = RSE_SIZE.double_register_window;
                bind = "&cpu_0.router.initiator_socket";
                relative_addresses = false;
            };
            log_level = 0;
        } or nil,

        plugin_pass = {
            moduletype = "LocalPass";
            tlm_initiator_ports_num = 6;
            tlm_target_ports_num = 2;
            target_signals_num = 0;
            initiator_signals_num = RSE_HW.nvic_num_irq;
            initiator_socket_0 = {bind = "&rse_lsc_counter.control"};
            initiator_socket_1 = {bind = "&rse_lsc_counter.status"};
            initiator_socket_2 = {bind = "&rse_timer_0.socket"};
            initiator_socket_3 = {bind = "&rse_timer_1.socket"};
            initiator_socket_4 = {bind = "&rse_timer_2.socket"};
            initiator_socket_5 = {bind = "&rse_timer_3.socket"};
            target_socket_0 = {
                address = RSE_ADDRESS.remote_base;
                size = RSE_ADDRESS.nvic;
                bind = rse_local_peripherals and
                    "&remote_main_router.initiator_socket" or
                    "&cpu_0.router.initiator_socket";
            };
            target_socket_1 = {
                address = RSE_ADDRESS.nvic + RSE_SIZE.nvic;
                size = RSE_SIZE.remote_post_nvic;
                bind = rse_local_peripherals and
                    "&remote_main_router.initiator_socket" or
                    "&cpu_0.router.initiator_socket";
            };
        },

        qemu_inst_mgr = {
            moduletype = "QemuInstanceManager";
        },

        qemu_inst = {
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
        },

        rse_lsc_clock = {
            moduletype = "qemu_clock_source";
            args = {"&qemu_inst"};
            frequency_hz = rse_lsc_input_hz;
        },

        rse_local_system_counter = not rse_smd_counter_mirror and {
            moduletype = "arm_system_counter";
            input_frequency_hz = rse_lsc_input_hz;
            integer_increment = 1;
            reported_frequency_hz = rse_lsc_input_hz;
            enabled = true;
            initial_count = 0;
        } or nil,

        rse_lsc_counter = {
            moduletype = "qemu_sse_counter_mirror";
            dylib_path = "qemu_sse_counter";
            args = {
                "&qemu_inst";
                "&rse_lsc_clock";
                rse_smd_counter_mirror and
                    "&platform.css_system_counter" or
                    "&rse_local_system_counter";
            };
        },

        rse_timer_0 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_"..RSE_IRQ.timer0};
        },

        rse_timer_1 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_"..RSE_IRQ.timer1};
        },

        rse_timer_2 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_"..RSE_IRQ.timer2};
        },

        rse_timer_3 = {
            moduletype = "qemu_sse_timer";
            args = {"&qemu_inst", "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_"..RSE_IRQ.timer3};
        },

        rse_nvic_cold_reset = {
            moduletype = "qemu_device_cold_reset";
            args = {"&cpu_0.cpu.nvic"};
        },

        rse_pflash_cold_reset = rse_local_boot_flash and
            rse_flash_backend == "qemu-cfi-local" and {
            moduletype = "qemu_device_cold_reset";
            args = {"&rse_boot_flash_qemu"};
        } or nil,

        rse_boot_flash = rse_local_boot_flash and
            rse_flash_backend == "systemc-strata" and {
            moduletype = "strata_flash_j3";
            trace = boot_flash_trace;
            trace_limit = boot_flash_trace_limit;
            enable_dmi = boot_flash_dmi;
            dmi_ranges = boot_flash_dmi_ranges;
            program_ff_sets_bits = true;
            program_ff_erases_sector = true;
            size = RSE_SIZE.boot_flash;
            sector_size = RSE_SIZE.flash_sector;
            backing_file = flash_writeback and rse_flash or "";
            defer_backing_write = true;
            defer_backing_flush_interval = flash_defer_backing_flush_interval;
            stats_file = rse_boot_flash_stats_file;
            stats_interval = flash_stats_interval;
            target_socket = {
                address = RSE_ADDRESS.boot_flash_secure;
                size = RSE_SIZE.boot_flash;
                bind = "&cpu_0.router.initiator_socket";
            };
            load = {bin_file = rse_flash, offset = 0};
            log_level = 0;
        } or nil,

        rse_boot_flash_qemu = rse_local_boot_flash and
            rse_flash_backend == "qemu-cfi-local" and {
            moduletype = "pflash_cfi";
            args = {"&qemu_inst", 1};
            blkdev_str = "file="..rse_flash..
                ",format=raw,if=none,cache=writeback"..
                (flash_writeback and "" or ",snapshot=on");
            num_blocks = RSE_SIZE.boot_flash / RSE_SIZE.flash_sector;
            sector_length = RSE_SIZE.flash_sector;
            width = 1;
            device_width = 1;
            max_device_width = 4;
            id0 = 0x89;
            id1 = 0x18;
            name = "apollo-rse-boot-flash";
            local_address = RSE_ADDRESS.boot_flash_secure;
            local_priority = 10;
            local_cpu = "platform.rse_cpu_pass.cpu_0.cpu";
            program_ff_erases_sector = true;
            io_mode_only = true;
            defer_backing_write = true;
            defer_backing_flush_interval = 65536;
            defer_backing_flush_delay_ms = 25;
            target_socket = {
                address = RSE_ADDRESS.boot_flash_secure;
                size = RSE_SIZE.boot_flash;
                bind = "&cpu_0.router.initiator_socket";
            };
            log_level = 0;
        } or nil,

        rse_kmu_regs = rse_local_crypto and {
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
        } or nil,

        rse_cc3xx = rse_local_crypto and
            rse_cc3xx_component("&remote_crypto_router.initiator_socket",
                                "&remote_main_router.target_socket") or nil,

        cpu_0 = {
            moduletype = "ApolloRseCPU";
            args = {"&qemu_inst"};
            router = {
                broadcast_invalidation = true;
            };
            cpu = {
                init_svtor = RSE_ADDRESS.rom_secure;
                init_nsvtor = RSE_ADDRESS.rom_secure;
                request_origin_id = ctx.request_context.origin.rse_cpu;
                request_domain_id = ctx.request_context.domain.rse;
                requester_id = 0;
                start_powered_off = false;
                trace_pc = rse_pc_trace;
                trace_exception_state = rse_exception_trace;
                trace_pc_file = rse_pc_trace_file;
                trace_pc_interval = rse_pc_trace_interval;
                trace_pc_limit = rse_pc_trace_limit;
                hotpath_accel = rse_hotpath_accel;
                hotpath_memcpy_addr = RSE_RUNTIME_ADDRESS.hotpath_memcpy;
                hotpath_memset_addr = RSE_RUNTIME_ADDRESS.hotpath_memset;
                hotpath_max_bytes = rse_hotpath_max_bytes;
                hotpath_profile_file = rse_hotpath_profile_file;
                hotpath_profile_interval = rse_hotpath_profile_interval;
                lms_accel = rse_lms_accel;
                lms_verify_addr = RSE_RUNTIME_ADDRESS.lms_verify;
                lms_max_data_bytes = rse_lms_max_data_bytes;
                bl2_load_profile = rse_bl2_load_profile;
                bl2_boot_go_for_image_id_addr = RSE_RUNTIME_ADDRESS.bl2_boot_go_for_image_id;
                bl2_boot_load_image_to_sram_addr = RSE_RUNTIME_ADDRESS.bl2_boot_load_image_to_sram;
                bl2_boot_enc_load_addr = RSE_RUNTIME_ADDRESS.bl2_boot_enc_load;
                bl2_boot_enc_set_key_addr = RSE_RUNTIME_ADDRESS.bl2_boot_enc_set_key;
                bl2_boot_enc_decrypt_addr = RSE_RUNTIME_ADDRESS.bl2_boot_enc_decrypt;
                bl2_bootutil_img_validate_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_img_validate;
                bl2_bootutil_img_hash_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_img_hash;
                bl2_bootutil_verify_sig_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_verify_sig;
                bl2_boot_image_count = rse_bl2_boot_image_count;
                bl2_boot_state_curr_img_offset = rse_bl2_boot_state_curr_img_offset;
                bl2_boot_state_imgs_offset = rse_bl2_boot_state_imgs_offset;
                bl2_boot_state_image_stride = rse_bl2_boot_state_image_stride;
                bl2_boot_state_slot_stride = rse_bl2_boot_state_slot_stride;
                bl2_boot_state_slot_usage_offset = rse_bl2_boot_state_slot_usage_offset;
                bl2_boot_state_slot_usage_stride = rse_bl2_boot_state_slot_usage_stride;
                bl2_boot_slot_usage_img_dst_offset = rse_bl2_boot_slot_usage_img_dst_offset;
                bl2_boot_slot_usage_img_sz_offset = rse_bl2_boot_slot_usage_img_sz_offset;
                bl2_load_accel = rse_bl2_load_accel;
                bl2_load_accel_max_bytes = rse_bl2_load_accel_max_bytes;
                bl2_boot_enc_accel = rse_bl2_boot_enc_accel;
                bl2_boot_status_enckey_offset = rse_bl2_boot_status_enckey_offset;
                bl2_boot_enc_key_bytes = rse_bl2_boot_enc_key_bytes;
                bl2_boot_enc_key_stride = rse_bl2_boot_enc_key_stride;
                bl2_boot_enc_slots = rse_bl2_boot_enc_slots;
                bl2_boot_enc_max_bytes = rse_bl2_boot_enc_max_bytes;
                bl2_img_hash_accel = rse_bl2_img_hash_accel;
                bl2_img_hash_max_bytes = rse_bl2_img_hash_max_bytes;
                bl2_img_hash_max_seed_bytes = rse_bl2_img_hash_max_seed_bytes;
                bl2_verify_sig_accel = rse_bl2_verify_sig_accel;
                bl2_verify_sig_skip = rse_bl2_verify_sig_skip;
                bl2_bootutil_keys_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_keys;
                bl2_bootutil_key_cnt_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_key_cnt;
                bl2_fih_success_addr = RSE_RUNTIME_ADDRESS.bl2_fih_success;
                bl2_verify_sig_max_key_bytes = rse_bl2_verify_sig_max_key_bytes;
                bl2_verify_sig_max_sig_bytes = rse_bl2_verify_sig_max_sig_bytes;
                bl2_delay_accel = rse_bl2_delay_accel;
                bl2_delay_cycles_addr = RSE_RUNTIME_ADDRESS.bl2_delay_cycles;
                bl2_delay_max_cycles = rse_bl2_delay_max_cycles;
                bl2_delay_expected_hits = rse_bl2_delay_expected_hits;
                nvic = {
                    mem = {
                        address = RSE_ADDRESS.nvic;
                        size = RSE_SIZE.nvic;
                    };
                    num_irq = RSE_HW.nvic_num_irq;
                };
            };
        };
    }

print("rse rom:      "..rse_rom)
print("rse flash:    "..rse_flash)
print("rse flash backend: "..rse_flash_backend)
print("rse otp:      "..rse_otp)
print("ap flash:     "..ap_flash)
print("ap bl2 elf:   "..AP_BL2_ELF)
print("provisioning: "..provisioning_bundle)
print("rse log:      "..rse_log)
print("secure log:   "..secure_console_log)
print("primary log:  "..primary_console_log)
print("ap cpus:      "..tostring(AP_NUM_CPUS))
print("rse rom base: 0x"..string.format("%x", RSE_ADDRESS.rom_secure))
print("rse vmaddrwidth: "..tostring(rse_vmaddrwidth))
print("rse vm size:  0x"..string.format("%x", RSE_SIZE.vm))
print("rse SMD counter mirror: "..tostring(rse_smd_counter_mirror))
print("rse LSC input Hz: "..tostring(rse_lsc_input_hz))

for irq=0,(RSE_HW.nvic_num_irq-1) do
    platform.rse_cpu_pass.plugin_pass["initiator_signal_socket_"..irq] = {
        bind = "&cpu_0.cpu.nvic.irq_in_"..irq;
    }
end

end

return rse
