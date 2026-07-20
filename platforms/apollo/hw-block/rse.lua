local rse = {}

function rse.define(ctx, platform)
    print("Apollo RSE QBox skeleton config running...")

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
        tcg_mode = rse_tcg_mode;
        sync_policy = rse_sync_policy;
        qemu_args = qemu_args;
    }

    platform.rse_rom = {
        moduletype = "gs_memory";
        read_only = true;
        shared_memory = true;
        shared_memory_prefix = "rse_rom_";
        target_socket = {
            address = RSE_ROM_BASE_S;
            size = RSE_ROM_SIZE;
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
            address = RSE_ITCM_BASE_S;
            size = RSE_ITCM_SIZE;
            bind = "&rse_router.initiator_socket";
            aliases = rse_tcm_aliases(
                rse_split_cpu0_itcm_alias,
                RSE_ITCM_BASE_NS,
                RSE_ITCM_CPU0_BASE_S,
                RSE_ITCM_CPU0_BASE_NS,
                RSE_ITCM_SIZE);
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
            address = RSE_ITCM_CPU0_BASE_S;
            size = RSE_ITCM_SIZE;
            bind = "&rse_router.initiator_socket";
            aliases = {
                cpu0_ns = {
                    address = RSE_ITCM_CPU0_BASE_NS;
                    size = RSE_ITCM_SIZE;
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
            address = RSE_DTCM_BASE_S;
            size = RSE_DTCM_SIZE;
            bind = "&rse_router.initiator_socket";
            aliases = rse_tcm_aliases(
                rse_split_cpu0_dtcm_alias,
                RSE_DTCM_BASE_NS,
                RSE_DTCM_CPU0_BASE_S,
                RSE_DTCM_CPU0_BASE_NS,
                RSE_DTCM_SIZE);
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
            address = RSE_DTCM_CPU0_BASE_S;
            size = RSE_DTCM_SIZE;
            bind = "&rse_router.initiator_socket";
            aliases = {
                cpu0_ns = {
                    address = RSE_DTCM_CPU0_BASE_NS;
                    size = RSE_DTCM_SIZE;
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
            address = RSE_VM0_BASE_S;
            size = RSE_VM_SIZE;
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
            address = RSE_VM1_BASE_S;
            size = RSE_VM_SIZE;
            bind = "&rse_router.initiator_socket";
        };
        init_mem = true;
        load = {bin_file = provisioning_bundle, offset = RSE_PROVISIONING_OFFSET};
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
        size = RSE_BOOT_FLASH_SIZE;
        sector_size = 0x1000;
        backing_file = flash_writeback and rse_flash or "";
        defer_backing_write = true;
        defer_backing_flush_interval = flash_defer_backing_flush_interval;
        stats_file = rse_boot_flash_stats_file;
        stats_interval = flash_stats_interval;
        target_socket = {
            address = RSE_BOOT_FLASH_BASE_S;
            size = RSE_BOOT_FLASH_SIZE;
            bind = "&rse_router.initiator_socket";
        };
        load = {bin_file = rse_flash, offset = 0};
        log_level = 0;
    } or nil

    -- RSE local peripherals

    platform.rse_otp_wrapper = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_OTP_WRAPPER_BASE_S;
            size = 0x00010000;
            bind = "&rse_router.initiator_socket";
        };
        load = {bin_file = rse_otp, offset = 0};
        log_level = 0;
    }

    platform.rse_cpu0_secctrl_regs = {
        moduletype = "gs_memory";
        target_socket = {
            address = 0x50011000;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_cpu0_pwrctrl_regs = {
        moduletype = "gs_memory";
        target_socket = {
            address = 0x50012000;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
            aliases = {
                ns = {
                    address = 0x40012000;
                    size = 0x00001000;
                };
            };
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_cpu0_identity_regs = {
        moduletype = "gs_memory";
        target_socket = {
            address = 0x5001F000;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
            aliases = {
                ns = {
                    address = 0x4001F000;
                    size = 0x00001000;
                };
            };
        };
        init_mem = true;
        log_level = 0;
    }

    platform.rse_nsacfg_regs = {
        moduletype = "rse_protection_ctrl";
        target_socket = {
            address = RSE_NSACFG_BASE_NS;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_watchdog_ns = {
        moduletype = "zena_watchdog";
        clock_frequency = 32000000;
        control = {
            address = RSE_WDOG_NS_CONTROL_BASE;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        refresh = {
            address = RSE_WDOG_NS_REFRESH_BASE;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        ws0 = {bind = "&rse_cpu_pass.target_signal_socket_1"};
        ws1 = {bind = "&rse_cpu_pass.target_signal_socket_0"};
        log_level = 0;
    }

    platform.rse_watchdog_s = {
        moduletype = "zena_watchdog";
        clock_frequency = 32000000;
        control = {
            address = RSE_WDOG_S_CONTROL_BASE;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        refresh = {
            address = RSE_WDOG_S_REFRESH_BASE;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_dma350 = {
        moduletype = "dma350";
        trace = dma350_trace;
        trace_limit = dma350_trace_limit;
        trace_filter = dma350_trace_filter;
        trace_address_min = dma350_trace_address_min;
        target_socket = {
            address = RSE_DMA350_BASE_S;
            size = 0x00002000;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&rse_router.target_socket"};
        log_level = 0;
    }

    platform.rse_sacfg_regs = {
        moduletype = "rse_protection_ctrl";
        target_socket = {
            address = RSE_SACFG_BASE_S;
            size = 0x00001000;
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
        build_config = 0x003D0005;
        hw_slot_config = 0x00D60100;
        hw_slot_export_address = 0x50154400;
        target_socket = {
            address = RSE_KMU_BASE_S;
            size = 0x00001000;
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
        otp_size = 0x00010000;
        otp_writeback = getenv_or("QBOX_RDASPEN_RSE_OTP_WRITEBACK", "false") == "true";
        otp_lock_after_provision =
            getenv_or("QBOX_RDASPEN_RSE_OTP_LOCK_AFTER_PROVISION", "true") == "true";
        target_socket = {
            address = RSE_LCM_BASE_S;
            size = RSE_LCM_SIZE;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_sam_regs = {
        moduletype = "rse_sam";
        trace = sam_trace;
        trace_limit = sam_trace_limit;
        build_config = 0x00000700;
        target_socket = {
            address = RSE_SAM_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_mpc_vm0_regs = {
        moduletype = "rse_protection_ctrl";
        profile = 1;
        blk_max = 1;
        blk_cfg = 0x00000007;
        target_socket = {
            address = RSE_MPC_VM0_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_mpc_vm1_regs = {
        moduletype = "rse_protection_ctrl";
        profile = 1;
        blk_max = 1;
        blk_cfg = 0x00000007;
        target_socket = {
            address = RSE_MPC_VM1_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_atu_regs = {
        moduletype = "rse_atu";
        trace = atu_trace;
        trace_limit = atu_trace_limit;
        trace_filter = atu_trace_filter;
        trace_address_min = atu_trace_address_min;
        trace_address_max = atu_trace_address_max;
        enable_dmi = atu_dmi;
        build_config = 0x000000C5;
        target_socket = {
            address = RSE_ATU_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        translation_socket = {
            address = RSE_HOST_ACCESS_BASE_NS;
            size = RSE_HOST_ACCESS_SIZE;
            bind = "&rse_router.initiator_socket";
            relative_addresses = false;
            priority = 10;
            aliases = {
                secure = {
                    address = RSE_HOST_ACCESS_BASE_S;
                    size = RSE_HOST_ACCESS_SIZE;
                };
            };
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.rse_sic_regs = {
        moduletype = "rse_protection_ctrl";
        target_socket = {
            address = 0x50140000;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_mpc_sic_regs = {
        moduletype = "rse_protection_ctrl";
        profile = 1;
        blk_max = 127;
        blk_cfg = 0x00000007;
        target_socket = {
            address = 0x50151000;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_cc3xx = (not rse_local_crypto) and
        rse_cc3xx_component("&rse_router.initiator_socket",
                            "&rse_router.target_socket") or nil

    platform.rse_syscntr_cntrl_regs = {
        moduletype = "host_gtimer";
        counter_control = true;
        target_socket = {
            address = RSE_SYSCNTR_CNTRL_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_syscntr_read_regs = {
        moduletype = "host_gtimer";
        counter_read = true;
        target_socket = {
            address = RSE_SYSCNTR_READ_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_integrity_checker_regs = {
        moduletype = "rse_integrity_checker";
        trace = integrity_checker_trace;
        trace_limit = integrity_checker_trace_limit;
        build_config = 0x00000109;
        target_socket = {
            address = RSE_INTEGRITY_CHECKER_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        log_level = 0;
    }

    platform.rse_tram = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_TRAM_BASE_S;
            size = 0x00001000;
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
            address = RSE_MHU0_SENDER_BASE_S;
            size = RSE_LOCAL_MHU_FRAME_SIZE;
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
            address = RSE_MHU0_RECEIVER_BASE_S;
            size = RSE_LOCAL_MHU_FRAME_SIZE;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&rse_cpu_pass.target_signal_socket_"..
            RSE_IRQ_CMU_MHU0_RECEIVER};
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
            address = RSE_MHU2_SENDER_BASE_S;
            size = RSE_LOCAL_MHU_FRAME_SIZE;
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
            address = RSE_MHU2_RECEIVER_BASE_S;
            size = RSE_LOCAL_MHU_FRAME_SIZE;
            bind = "&rse_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&rse_cpu_pass.target_signal_socket_"..
            RSE_IRQ_CMU_MHU2_RECEIVER};
        log_level = 0;
    }

    platform.rse_sysctrl = {
        moduletype = "rse_sysctrl";
        trace = sysctrl_trace;
        trace_limit = sysctrl_trace_limit;
        reset_syndrome = rse_reset_syndrome;
        cpuwait = rse_cpuwait;
        dma_boot_en = rse_dma_boot_en;
        dma_boot_addr = rse_dma_boot_addr;
        target_socket = {
            address = RSE_SYSCTRL_BASE_S;
            size = 0x00001000;
            bind = "&rse_router.initiator_socket";
        };
        system_reset = {bind = "&apollo_system_reset_fanout.reset_in"};
        log_level = 0;
    }

    platform.rse_integ_layer_regs = {
        moduletype = "gs_memory";
        target_socket = {
            address = RSE_INTEG_LAYER_BASE_S;
            size = 0x00001000;
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
            address = RSE_HOST_UART0_BASE_S;
            size = 0x00010000;
            bind = "&rse_router.initiator_socket";
            aliases = {
                ns_atu_logical = {
                    address = RSE_HOST_UART0_BASE_NS;
                    size = 0x00010000;
                };
            };
        };
        irq = {bind = "&rse_cpu_pass.target_signal_socket_0"};
        backend_socket = {bind = "&rse_uart_file.biflow_socket"};
    }

    platform.rse_cpu_pass = {
        moduletype = "Container";
        tlm_initiator_ports_num = 2;
        tlm_target_ports_num = 0;
        target_signals_num = RSE_REMOTE_SIGNAL_COUNT;
        initiator_signals_num = 0;
        initiator_socket_0 = {bind = "&rse_router.target_socket"};
        initiator_socket_1 = {bind = "&rse_router.target_socket"};

        remote_main_router = rse_local_peripherals and {
            moduletype = "router";
            broadcast_invalidation = true;
            target_socket = {
                address = 0x00000000;
                size = RSE_NVIC_BASE;
                bind = "&cpu_0.router.initiator_socket";
                relative_addresses = false;
                priority = 100;
                aliases = {
                    post_nvic = {
                        address = RSE_NVIC_BASE + RSE_NVIC_SIZE;
                        size = 0x00100000;
                        priority = 100;
                    };
                };
            };
            log_level = 0;
        } or nil,

        remote_crypto_router = rse_local_crypto and {
            moduletype = "router";
            target_socket = {
                address = RSE_CC3XX_BASE_S;
                size = 0x00002000;
                bind = "&cpu_0.router.initiator_socket";
                relative_addresses = false;
            };
            log_level = 0;
        } or nil,

        plugin_pass = {
            moduletype = "LocalPass";
            tlm_initiator_ports_num = 0;
            tlm_target_ports_num = 2;
            target_signals_num = 0;
            initiator_signals_num = RSE_REMOTE_SIGNAL_COUNT;
            target_socket_0 = {
                address = 0x00000000;
                size = RSE_NVIC_BASE;
                bind = rse_local_peripherals and
                    "&remote_main_router.initiator_socket" or
                    "&cpu_0.router.initiator_socket";
            };
            target_socket_1 = {
                address = RSE_NVIC_BASE + RSE_NVIC_SIZE;
                size = 0x00100000;
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
            tcg_mode = rse_tcg_mode;
            sync_policy = rse_sync_policy;
            qemu_args = qemu_args;
        },

        rse_boot_flash = rse_local_boot_flash and
            rse_flash_backend == "systemc-strata" and {
            moduletype = "strata_flash_j3";
            trace = boot_flash_trace;
            trace_limit = boot_flash_trace_limit;
            enable_dmi = boot_flash_dmi;
            dmi_ranges = boot_flash_dmi_ranges;
            program_ff_sets_bits = true;
            program_ff_erases_sector = true;
            size = RSE_BOOT_FLASH_SIZE;
            sector_size = 0x1000;
            backing_file = flash_writeback and rse_flash or "";
            defer_backing_write = true;
            defer_backing_flush_interval = flash_defer_backing_flush_interval;
            stats_file = rse_boot_flash_stats_file;
            stats_interval = flash_stats_interval;
            target_socket = {
                address = RSE_BOOT_FLASH_BASE_S;
                size = RSE_BOOT_FLASH_SIZE;
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
            num_blocks = RSE_BOOT_FLASH_SIZE / 0x1000;
            sector_length = 0x1000;
            width = 1;
            device_width = 1;
            max_device_width = 4;
            id0 = 0x89;
            id1 = 0x18;
            name = "apollo-rse-boot-flash";
            local_address = RSE_BOOT_FLASH_BASE_S;
            local_priority = 10;
            local_cpu = "platform.rse_cpu_pass.cpu_0.cpu";
            program_ff_erases_sector = true;
            io_mode_only = true;
            defer_backing_write = true;
            defer_backing_flush_interval = 65536;
            defer_backing_flush_delay_ms = 25;
            target_socket = {
                address = RSE_BOOT_FLASH_BASE_S;
                size = RSE_BOOT_FLASH_SIZE;
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
            build_config = 0x003D0005;
            hw_slot_config = 0x00D60100;
            hw_slot_export_address = 0x50154400;
            target_socket = {
                address = RSE_KMU_BASE_S;
                size = 0x00001000;
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
                init_svtor = RSE_ROM_BASE_S;
                init_nsvtor = RSE_ROM_BASE_S;
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
                hotpath_memcpy_addr = rse_hotpath_memcpy_addr;
                hotpath_memset_addr = rse_hotpath_memset_addr;
                hotpath_max_bytes = rse_hotpath_max_bytes;
                hotpath_profile_file = rse_hotpath_profile_file;
                hotpath_profile_interval = rse_hotpath_profile_interval;
                lms_accel = rse_lms_accel;
                lms_verify_addr = rse_lms_verify_addr;
                lms_max_data_bytes = rse_lms_max_data_bytes;
                bl2_load_profile = rse_bl2_load_profile;
                bl2_boot_go_for_image_id_addr = rse_bl2_boot_go_for_image_id_addr;
                bl2_boot_load_image_to_sram_addr = rse_bl2_boot_load_image_to_sram_addr;
                bl2_boot_enc_load_addr = rse_bl2_boot_enc_load_addr;
                bl2_boot_enc_set_key_addr = rse_bl2_boot_enc_set_key_addr;
                bl2_boot_enc_decrypt_addr = rse_bl2_boot_enc_decrypt_addr;
                bl2_bootutil_img_validate_addr = rse_bl2_bootutil_img_validate_addr;
                bl2_bootutil_img_hash_addr = rse_bl2_bootutil_img_hash_addr;
                bl2_bootutil_verify_sig_addr = rse_bl2_bootutil_verify_sig_addr;
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
                bl2_bootutil_keys_addr = rse_bl2_bootutil_keys_addr;
                bl2_bootutil_key_cnt_addr = rse_bl2_bootutil_key_cnt_addr;
                bl2_fih_success_addr = rse_bl2_fih_success_addr;
                bl2_verify_sig_max_key_bytes = rse_bl2_verify_sig_max_key_bytes;
                bl2_verify_sig_max_sig_bytes = rse_bl2_verify_sig_max_sig_bytes;
                bl2_delay_accel = rse_bl2_delay_accel;
                bl2_delay_cycles_addr = rse_bl2_delay_cycles_addr;
                bl2_delay_max_cycles = rse_bl2_delay_max_cycles;
                bl2_delay_expected_hits = rse_bl2_delay_expected_hits;
                nvic = {
                    mem = {
                        address = RSE_NVIC_BASE;
                        size = RSE_NVIC_SIZE;
                    };
                    num_irq = RSE_NVIC_NUM_IRQ;
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
print("rse rom base: 0x"..string.format("%x", RSE_ROM_BASE_S))
print("rse vmaddrwidth: "..tostring(rse_vmaddrwidth))
print("rse vm size:  0x"..string.format("%x", RSE_VM_SIZE))

for irq=0,(RSE_NVIC_NUM_IRQ-1) do
    platform.rse_cpu_pass.plugin_pass["initiator_signal_socket_"..irq] = {
        bind = "&cpu_0.cpu.nvic.irq_in_"..irq;
    }
end

end

return rse
