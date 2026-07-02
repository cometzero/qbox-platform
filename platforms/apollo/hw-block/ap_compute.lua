local ap_compute = {}

function ap_compute.define(ctx, platform)
    platform.ap_qemu_inst_mgr = enable_ap_cpus and {
        moduletype = "QemuInstanceManager";
        construction_priority = -300;
    } or nil

    platform.ap_qemu_inst = enable_ap_cpus and {
        moduletype = "QemuInstance";
        args = {"&platform.ap_qemu_inst_mgr", "AARCH64"};
        accel = "tcg";
        tcg_mode = "MULTI";
        sync_policy = "multithread-freerunning";
        qemu_args = ap_qemu_args;
        construction_priority = -299;
    } or nil

    platform.ap_reset_gpio = enable_ap_cpus and {
        moduletype = "reset_gpio";
        args = {"&platform.ap_qemu_inst"};
        reset_out = {bind = ap_cpu_reset_bind_targets(AP_NUM_CPUS)};
        log_level = 0;
    } or nil

    -- AP CPU backend and PCIe root complex

    platform.ap_global_peripheral_initiator = enable_ap_cpus and {
        moduletype = "global_peripheral_initiator";
        args = {"&platform.ap_qemu_inst", "&platform.ap_cpu_0"};
        global_initiator = {bind = "&host_router.target_socket"};
    } or nil

    platform.ap_gpex_0 = enable_ap_cpus and {
        moduletype = "qemu_gpex";
        args = {"&platform.ap_qemu_inst"};
        bus_master = {bind = "&host_router.target_socket"};
        pio_iface = {
            address = 0x60200000;
            size = 0x00100000;
            bind = "&host_router.initiator_socket";
        };
        mmio_iface = {
            address = 0x60300000;
            size = 0x1FD00000;
            bind = "&host_router.initiator_socket";
        };
        ecam_iface = {
            address = 0x43B50000;
            size = 0x10000000;
            bind = "&host_router.initiator_socket";
        };
        mmio_iface_high = {
            address = 0x400000000;
            size = 0x200000000;
            bind = "&host_router.initiator_socket";
        };
        irq_out_0 = {bind = "&ap_gic.spi_in_300"};
        irq_out_1 = {bind = "&ap_gic.spi_in_301"};
        irq_out_2 = {bind = "&ap_gic.spi_in_302"};
        irq_out_3 = {bind = "&ap_gic.spi_in_303"};
    } or nil

    platform.host_ap_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = HOST_AP_SHARED_SRAM_PHYS_BASE;
            size = HOST_AP_SHARED_SRAM_SIZE;
            bind = "&host_router.initiator_socket";
        };
        map_file = host_ap_shared_sram_map_file;
        shared_memory = host_sram_shared_memory_enabled(host_ap_shared_sram_map_file);
        shared_memory_prefix = "ra-aps-";
        init_mem = host_ap_shared_sram_map_file == "";
        load = {data = HOST_AP_SDS_REGION_DATA, offset = 0};
        log_level = 0;
    }

    platform.ap_bl2_reset_loader = enable_ap_cpus and {
        moduletype = "loader";
        initiator_socket = {bind = "&host_router.target_socket"};
        {
            bin_file = AP_BL2_ELF;
            address = AP_BL2_RESET.data_phys_base;
            bin_file_offset = AP_BL2_RESET.data_elf_offset;
            bin_file_size = AP_BL2_RESET.data_size;
        };
        {
            bin_file = "/dev/zero";
            address = AP_BL2_RESET.stacks_phys_base;
            bin_file_offset = 0;
            bin_file_size = AP_BL2_RESET.stacks_size;
        };
        {
            bin_file = "/dev/zero";
            address = AP_BL2_RESET.bss_phys_base;
            bin_file_offset = 0;
            bin_file_size = AP_BL2_RESET.bss_size;
        };
        {
            bin_file = "/dev/zero";
            address = AP_BL2_RESET.xlat_phys_base;
            bin_file_offset = 0;
            bin_file_size = AP_BL2_RESET.xlat_size;
        };
        {
            address = HOST_AP_SDS_RESET_SYNDROME_PHYS_BASE;
            data = {SDS_RESET_SYNDROME_SYS_RESET_REQ};
        };
        log_level = 0;
    } or nil

    platform.host_ap_mhu_ns_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = 0x00180000;
            size = 0x00001000;
            bind = "&host_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.host_ap_bl2_header_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = HOST_AP_BL2_HEADER_SRAM_PHYS_BASE;
            size = HOST_AP_BL2_HEADER_SRAM_SIZE;
            bind = "&host_router.initiator_socket";
        };
        map_file = host_ap_bl2_header_sram_map_file;
        shared_memory = host_sram_shared_memory_enabled(host_ap_bl2_header_sram_map_file);
        shared_memory_prefix = "ra-aph-";
        init_mem = host_ap_bl2_header_sram_map_file == "";
        log_level = 0;
    }

    platform.host_ap_flash = {
        moduletype = "strata_flash_j3";
        trace = boot_flash_trace;
        trace_limit = boot_flash_trace_limit;
        enable_dmi = host_memory_dmi;
        dmi_ranges = ap_flash_dmi_ranges;
        program_ff_sets_bits = true;
        program_ff_erases_sector = true;
        size = HOST_AP_FLASH_IMAGE_SIZE;
        sector_size = 0x1000;
        backing_file = flash_writeback and ap_flash or "";
        defer_backing_write = true;
        defer_backing_flush_interval = flash_defer_backing_flush_interval;
        stats_file = ap_flash_stats_file;
        stats_interval = flash_stats_interval;
        target_socket = {
            address = HOST_AP_FLASH_PHY_BASE;
            size = HOST_AP_FLASH_IMAGE_SIZE;
            bind = "&host_router.initiator_socket";
        };
        load = {bin_file = ap_flash, offset = 0};
        log_level = 0;
    }

    platform.rse_ap_fip_logical = host_sram_shared_memory and {
        moduletype = "gs_memory";
        read_only = true;
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = HOST_AP_FLASH_LOGICAL_BASE + AP_FLASH_FIP_PRIMARY_OFFSET;
            size = AP_FLASH_FIP_SIZE;
            priority = 0;
            bind = "&rse_router.initiator_socket";
        };
        load = {
            bin_file = ap_flash;
            offset = 0;
            bin_file_offset = AP_FLASH_FIP_PRIMARY_OFFSET;
            bin_file_size = AP_FLASH_FIP_SIZE;
        };
        log_level = 0;
    }

    platform.host_ap_trusted_nvctr = {
        moduletype = "gs_memory";
        read_only = true;
        target_socket = {
            address = HOST_AP_TRUSTED_NVCTR_BASE;
            size = HOST_AP_TRUSTED_NVCTR_SIZE;
            bind = "&host_router.initiator_socket";
        };
        init_mem = true;
        load = {data = HOST_AP_TRUSTED_NVCTR_DATA, offset = 0};
        log_level = 0;
    }

    platform.host_ap_dram1 = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = HOST_AP_DRAM1_BASE;
            size = HOST_AP_DRAM1_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.host_ap_ffa_mm_comm_buffer = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = 0xFFBF0000;
            size = 0x00002000;
            bind = "&host_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    } or nil

    platform.host_ap_spmc_sdram = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = HOST_AP_SPMC_BASE;
            size = HOST_AP_SPMC_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.host_ap_dram2 = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = HOST_AP_DRAM2_BASE;
            size = HOST_AP_DRAM2_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_gic = enable_ap_cpus and {
        moduletype = "arm_gicv3";
        args = {"&platform.ap_qemu_inst"};
        dist_iface = {
            address = AP_GIC_DIST_BASE;
            size = 0x00010000;
            bind = "&host_router.initiator_socket";
            aliases = {
                optee_secure_view = {
                    address = AP_GIC_LEGACY_DIST_BASE;
                    size = 0x00010000;
                };
            };
        };
        num_cpus = AP_GIC_NUM_CPUS;
        redist_region = repeat_value(1, AP_GIC_ACTIVE_REDIST_REGIONS);
        has_security_extensions = true;
        has_lpi = true;
        revision = 4;
        num_spi = 960;
        has_gicv4_1 = true;
        has_direct_lpi = true;
        has_rvpeid = true;
        has_vpend_valid_dirty = true;
        vpeid_bits = 16;
    } or nil

    platform.ap_gic_its = enable_ap_cpus and {
        moduletype = "arm_gicv3_its";
        args = {"&platform.ap_qemu_inst", "&platform.ap_gic"};
        has_gicv4_1 = true;
        gicv4_1_svpet = 1;
        gicv4_1_cte_size = 2;
        mem = {
            address = 0x20840000;
            size = 0x00040000;
            bind = "&host_router.initiator_socket";
        };
    } or nil

    platform.ap_smmu_0 = enable_ap_cpus and ap_smmu_component() or nil

    platform.ap_watchdog_0 = enable_ap_cpus and {
        moduletype = "sbsa_gwdt";
        args = {"&platform.ap_qemu_inst"};
        refresh_mem = {
            address = 0x1A420000;
            size = 0x00010000;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        control_mem = {
            address = 0x1A430000;
            size = 0x00010000;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_50"};
    } or nil

    platform.ap_secure_console_file = enable_ap_cpus and {
        moduletype = "char_backend_file";
        read_file = "/dev/null";
        write_file = secure_console_log;
        baudrate = 0;
    } or nil

    platform.ap_primary_console_file = enable_ap_cpus and {
        moduletype = "char_backend_file";
        read_file = "/dev/null";
        write_file = primary_console_log;
        baudrate = 0;
    } or nil

    platform.ap_secure_uart = enable_ap_cpus and {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = {
            address = AP_SECURE_UART_BASE;
            size = 0x00010000;
            bind = "&host_router.initiator_socket";
        };
        irq = {bind = "&ap_gic.spi_in_"..AP_SECURE_UART_IRQ};
        backend_socket = {bind = "&ap_secure_console_file.biflow_socket"};
    } or nil

    platform.ap_primary_uart = enable_ap_cpus and {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = {
            address = AP_PRIMARY_UART_BASE;
            size = 0x00010000;
            bind = "&host_router.initiator_socket";
        };
        irq = {bind = "&ap_gic.spi_in_"..AP_PRIMARY_UART_IRQ};
        backend_socket = {bind = "&ap_primary_console_file.biflow_socket"};
    } or nil

    platform.ap_timer_mem = enable_ap_cpus and {
        moduletype = "qemu_hexagon_qtimer";
        args = {"&platform.ap_qemu_inst"};
        nr_frames = 1;
        nr_views = 1;
        cnttid = 0x1;
        mem = {
            address = AP_SYS_TIMCTL_BASE;
            size = AP_SYS_TIMER_SIZE;
            bind = "&host_router.initiator_socket";
        };
        mem_view = {
            address = AP_SYS_CNT_BASE_NS;
            size = AP_SYS_TIMER_SIZE;
            bind = "&host_router.initiator_socket";
        };
        irq = {
            {bind = "&ap_gic.spi_in_"..AP_SYS_TIMER_IRQ};
        };
    } or nil

    platform.ap_secure_timer_frame = enable_ap_cpus and {
        moduletype = "gs_memory";
        target_socket = {
            address = AP_SYS_CNT_BASE_S;
            size = AP_SYS_TIMER_SIZE;
            bind = "&host_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    } or nil

    -- RD-Aspen AP BL2 refreshes the secure SBSA watchdog in panic/error paths.
    -- Keep the window mapped so watchdog access does not hide the original
    -- secure-world failure while a fuller watchdog model is still pending.

    platform.ap_secure_wdog = enable_ap_cpus and {
        moduletype = "gs_memory";
        target_socket = {
            address = AP_SECURE_WDOG_BASE;
            size = AP_SECURE_WDOG_SIZE;
            bind = "&host_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    } or nil

    platform.ap_secure_wdog_refresh = enable_ap_cpus and {
        moduletype = "gs_memory";
        target_socket = {
            address = AP_SECURE_WDOG_REFRESH_BASE;
            size = AP_SECURE_WDOG_SIZE;
            bind = "&host_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    } or nil

    platform.ap_sid = enable_ap_cpus and {
        moduletype = "host_scr";
        system_id = 0x0047773d;
        soc_id = 0x00000000;
        chip_id = 0x00000000;
        pidr4 = 0x00000004;
        pidr0 = 0x0000003d;
        pidr1 = 0x000000b7;
        pidr2 = 0x0000000b;
        pidr3 = 0x00000000;
        cidr0 = 0x0000000d;
        cidr1 = 0x000000f0;
        cidr2 = 0x00000005;
        cidr3 = 0x000000b1;
        target_socket = {
            address = AP_SID_BASE;
            size = AP_SID_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_rgic2lgic_messreg = enable_ap_cpus and {
        moduletype = "gic720ae_messreg";
        target_socket = {
            address = AP_RGIC2LGIC_MESSREG_BASE;
            size = AP_RGIC2LGIC_MESSREG_SIZE;
            bind = "&host_router.initiator_socket";
        };
        window_size = AP_RGIC2LGIC_MESSREG_SIZE;
        log_level = 0;
    } or nil

    -- Models only the active 5-bank FMU register block in each 1 MiB
    -- APP aperture; unmapped gaps remain explicit coverage debt.

    platform.ap_cl0_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = 5;
        record_count = 384;
        target_socket = {
            address = AP_CL0_NI710AE_FMU_BASE;
            size = AP_FMU_MODELED_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_cl1_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = 5;
        record_count = 384;
        target_socket = {
            address = AP_CL1_NI710AE_FMU_BASE;
            size = AP_FMU_MODELED_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_cl2_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = 5;
        record_count = 384;
        target_socket = {
            address = AP_CL2_NI710AE_FMU_BASE;
            size = AP_FMU_MODELED_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_cl3_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = 5;
        record_count = 384;
        target_socket = {
            address = AP_CL3_NI710AE_FMU_BASE;
            size = AP_FMU_MODELED_SIZE;
            bind = "&host_router.initiator_socket";
        };
        log_level = 0;
    } or nil

if enable_ap_cpus then
    for i=0,(AP_GIC_ACTIVE_REDIST_REGIONS-1) do
        platform["ap_gic"]["redist_iface_"..i] = {
            address = AP_GIC_REDIST_BASE + (i * AP_GIC_REDIST_SIZE);
            size = AP_GIC_REDIST_SIZE;
            bind = "&host_router.initiator_socket";
            aliases = {
                optee_secure_view = {
                    address = AP_GIC_LEGACY_REDIST_BASE + (i * AP_GIC_LEGACY_REDIST_SIZE);
                    size = AP_GIC_LEGACY_REDIST_SIZE;
                };
            };
        }
    end

    for i=AP_GIC_ACTIVE_REDIST_REGIONS,(AP_GIC_REDIST_REGIONS-1) do
        platform["ap_gicr_reserved_"..i] = {
            moduletype = "gs_memory";
            target_socket = {
                address = AP_GIC_REDIST_BASE + (i * AP_GIC_REDIST_SIZE);
                size = AP_GIC_REDIST_SIZE;
                bind = "&host_router.initiator_socket";
            };
            dmi_allow = false;
            log_level = 0;
        }
    end

    for i=0,(AP_NUM_CPUS-1) do
        local cpu = {
            moduletype = "cpu_arm_cortexA720AE";
            args = {"&platform.ap_qemu_inst"};
            mem = {bind = "&host_router.target_socket"};
            has_el3 = true;
            has_el2 = true;
            irq_timer_phys_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..ARCH_TIMER_NS_EL1_IRQ;
            };
            irq_timer_virt_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..ARCH_TIMER_VIRT_IRQ;
            };
            irq_timer_hyp_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..ARCH_TIMER_NS_EL2_IRQ;
            };
            irq_timer_sec_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..ARCH_TIMER_S_EL1_IRQ;
            };
            gicv3_maintenance_interrupt = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_25";
            };
            pmu_interrupt = {bind = "&ap_gic.ppi_in_cpu_"..i.."_23"};
            psci_conduit = getenv_or("QBOX_RDASPEN_AP_PSCI_CONDUIT", "disabled");
            mp_affinity = mp_affinity(i);
            start_powered_off = i ~= 0;
            start_in_reset = true;
            reset_power_on = true;
            rvbar = HOST_AP_BL2_PHYS_BASE;
            trace_pc = ap_pc_trace;
            trace_pc_file = ap_pc_trace_file;
            trace_pc_interval = ap_pc_trace_interval;
            trace_pc_limit = ap_pc_trace_limit;
            trace_exception_state = ap_exception_trace;
            construction_priority = -200 + i;
        }
        platform["ap_cpu_"..tostring(i)] = cpu

        platform["ap_gic"]["irq_out_"..i] = {bind = "&ap_cpu_"..i..".irq_in"}
        platform["ap_gic"]["fiq_out_"..i] = {bind = "&ap_cpu_"..i..".fiq_in"}
        platform["ap_gic"]["virq_out_"..i] = {bind = "&ap_cpu_"..i..".virq_in"}
        platform["ap_gic"]["vfiq_out_"..i] = {bind = "&ap_cpu_"..i..".vfiq_in"}
    end
end

end

function ap_compute.enable_ap_view_router(ctx, platform)
    if platform.ap_cpu_0 == nil then
        return
    end

    print("Apollo FVP AP logical view router enabled...")

    -- AP logical view
    platform.ap_view_router = {
        moduletype = "router";
        log_level = 0;
    }

    platform.ap_view_passthrough = {
        moduletype = "addrtr";
        mapped_base_addr = 0x0;
        target_socket = {
            address = 0x0;
            size = 0x1000000000000;
            bind = "&ap_view_router.initiator_socket";
            relative_addresses = false;
            priority = 100;
        };
        initiator_socket = {bind = "&host_router.target_socket"};
        log_level = 0;
    }

    -- Host-to-AP translation and AP masters
    local function bind_ap_target(target)
        if target ~= nil then
            target.bind = "&ap_view_router.initiator_socket"
            target.priority = 0
        end
    end

    local function bind_ap_socket(device, socket_name)
        if device ~= nil then
            bind_ap_target(device[socket_name])
        end
    end

    if platform.host_ap_atu ~= nil and
       platform.host_ap_atu.translation_socket ~= nil then
        platform.host_ap_atu.translation_socket.bind =
            "&ap_view_router.initiator_socket"
        platform.host_ap_atu.translation_socket.priority = 10
    end

    if platform.ap_global_peripheral_initiator ~= nil then
        platform.ap_global_peripheral_initiator.global_initiator = {
            bind = "&ap_view_router.target_socket";
        }
    end
    if platform.ap_gpex_0 ~= nil then
        platform.ap_gpex_0.bus_master = {
            bind = "&ap_view_router.target_socket";
        }
        bind_ap_target(platform.ap_gpex_0.pio_iface)
        bind_ap_target(platform.ap_gpex_0.mmio_iface)
        bind_ap_target(platform.ap_gpex_0.ecam_iface)
        bind_ap_target(platform.ap_gpex_0.mmio_iface_high)
    end

    -- Memory and interrupt controller windows
    bind_ap_socket(platform.host_ap_dram1, "target_socket")
    bind_ap_socket(platform.host_ap_dram2, "target_socket")
    bind_ap_socket(platform.host_ap_ffa_mm_comm_buffer, "target_socket")
    bind_ap_socket(platform.host_ap_spmc_sdram, "target_socket")
    bind_ap_socket(platform.ap_gic, "dist_iface")
    if platform.ap_gic ~= nil then
        for i=0,15 do
            bind_ap_socket(platform.ap_gic, "redist_iface_"..i)
        end
    end
    bind_ap_socket(platform.ap_gic_its, "mem")
    bind_ap_socket(platform.ap_smmu_0, "mem")

    -- RoS and AP peripherals
    ctx.ros.bind_ap_view_targets(platform, bind_ap_target)
    bind_ap_socket(platform.ap_watchdog_0, "refresh_mem")
    bind_ap_socket(platform.ap_watchdog_0, "control_mem")
    bind_ap_socket(platform.ap_secure_uart, "target_socket")
    bind_ap_socket(platform.ap_primary_uart, "target_socket")
    bind_ap_socket(platform.ap_timer_mem, "mem")
    bind_ap_socket(platform.ap_timer_mem, "mem_view")
    bind_ap_socket(platform.ap_secure_timer_frame, "target_socket")
    bind_ap_socket(platform.ap_secure_wdog, "target_socket")
    bind_ap_socket(platform.ap_secure_wdog_refresh, "target_socket")
    bind_ap_socket(platform.ap_sid, "target_socket")
    bind_ap_socket(platform.ap_rgic2lgic_messreg, "target_socket")
    bind_ap_socket(platform.ap_cl0_ni710ae_fmu, "target_socket")
    bind_ap_socket(platform.ap_cl1_ni710ae_fmu, "target_socket")
    bind_ap_socket(platform.ap_cl2_ni710ae_fmu, "target_socket")
    bind_ap_socket(platform.ap_cl3_ni710ae_fmu, "target_socket")

    -- AP CPU initiator view
    for i=0,15 do
        local cpu = platform["ap_cpu_"..i]
        if cpu ~= nil then
            cpu.mem = {bind = "&ap_view_router.target_socket"}
        end
    end
end

return ap_compute
