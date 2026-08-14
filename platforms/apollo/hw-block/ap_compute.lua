local ap_compute = {}

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
local AP_ADDRESS = {
    shared_sram = 0x00000000;
    bl2_header_sram = 0x00100000;
    mhu_ns_shared_sram = 0x00180000;
    peripheral_ns_sram_tail = 0x00181000;
    flash = 0x38000000;
    trusted_nvctr = 0x32030000;
    dram1 = 0x80000000;
    ffa_mm_comm_buffer = 0xFFBF0000;
    spmc = 0xFFC00000;
    dram2 = 0x20000000000;
    gpex_pio = 0x60200000;
    gpex_mmio = 0x60300000;
    gpex_ecam = 0x43B50000;
    gpex_mmio_high = 0x400000000;
    secure_uart = 0x1A410000;
    primary_uart = 0x1A400000;
    ns_wdog_control = 0x1A420000;
    ns_wdog_refresh = 0x1A430000;
    secure_wdog_control = 0x1A460000;
    secure_wdog_refresh = 0x1A470000;
    sid = 0x1A4A0000;
    sys_timctl = 0x1A810000;
    sys_counter_secure = 0x1A820000;
    sys_counter_non_secure = 0x1A830000;
    rgic2lgic_messreg = 0x5FFF0000;
    fmu_cl0 = 0x1D000000;
    fmu_cl1 = 0x1D100000;
    fmu_cl2 = 0x1D200000;
    fmu_cl3 = 0x1D300000;
    gic_dist = 0x20800000;
    gic_redist = 0x20880000;
    gic_view0_dist = 0x20000000;
    gic_view0_redist = 0x20080000;
    gic_its = 0x20840000;
    smmu = 0x1C0000000;
    host_fmu_cl0 = 0x20000D2000000;
    host_fmu_cl1 = 0x20000D2100000;
    host_fmu_cl2 = 0x20000D2200000;
    host_fmu_cl3 = 0x20000D2300000;
    mhu_pointer_access = 0x0FFFE0000;
    hipc_alias = 0x00100000;
}
local AP_SIZE = {
    shared_sram = 0x00100000;
    sds_mem = 0x00000DC0;
    bl2_header_sram = 0x00080000;
    mhu_ns_shared_sram = 0x00001000;
    peripheral_ns_sram_tail = 0x0007F000;
    flash = 0x08000000;
    flash_sector = 0x00001000;
    trusted_nvctr = 0x00010000;
    dram1 = 0x7F000000;
    ffa_mm_comm_buffer = 0x00002000;
    spmc_local = 0x003E0000;
    dram2 = 0x80000000;
    gpex_pio = 0x00100000;
    gpex_mmio = 0x1FD00000;
    gpex_ecam = 0x10000000;
    gpex_mmio_high = 0x200000000;
    uart = 0x00010000;
    secure_wdog = 0x00010000;
    sid = 0x00010000;
    sys_timer = 0x00010000;
    rgic2lgic_messreg = 0x00010000;
    fmu_modeled = 0x00050000;
    gic_dist = 0x00010000;
    gic_redist = 0x00040000;
    gic_view0_dist = 0x00080000;
    gic_view0_redist = 0x00040000;
    gic_its = 0x00040000;
    smmu = 0x08000000;
    mhu_pointer_access = 0x00020000;
    register_window = 0x00010000;
}
local AP_IRQ = {
    arch_timer_virt = 16 + 11;
    arch_timer_secure = 16 + 13;
    arch_timer_non_secure = 16 + 14;
    arch_timer_hyp = 16 + 10;
    gic_maintenance = 25;
    pmu = 23;
    gpex = {300; 301; 302; 303};
    sys_timer_secure = 48;
    sys_timer_non_secure = 49;
    secure_wdog_ws0 = 47;
    ns_wdog_ws0 = 50;
    ns_wdog_ws1 = 51;
    primary_uart = 52;
    secure_uart = 53;
    smmu_event = 65;
}
local AP_HW = {
    max_cpus = 16;
    gpex_requester_id = 0x40;
    sds_reset_syndrome_offset = 0x00000050;
    reset_syndrome_system_reset_req = 0x00000008;
    bl2_offset = 0x00082000;
    bl2_data_offset = 0x00098000;
    bl2_stacks_offset = 0x00098F40;
    bl2_bss_offset = 0x0009A000;
    bl2_xlat_offset = 0x000A2000;
    bl2_data_elf_offset = 0x00017000;
    bl2_data_size = 0x00000F35;
    bl2_stacks_size = 0x00001000;
    bl2_bss_size = 0x00008000;
    bl2_xlat_size = 0x0000E000;
    smmu_fault_bank_count = 1;
    smmu_fault_record_count = 2;
    smmu_pamax = 52;
    smmu_sid_size = 32;
    smmu_tbu_count = 1;
    smmu_iidr = 0x720AE000;
    gic_redist_regions = 16;
    gic_active_redist_regions = AP_GIC_NUM_CPUS;
    gic_view0_redist_regions = 16;
    gic_revision = 4;
    gic_num_spi = 960;
    gic_vpeid_bits = 16;
    gic_its_svpet = 1;
    gic_its_cte_size = 2;
    arch_timer_frequency_hz = 125000000;
    arch_timer_frame_count = 2;
    fmu_bank_count = 5;
    fmu_record_count = 384;
    uart_revision = 3;
    sid_system_id = 0x0047773d;
    sid_soc_id = 0x00000000;
    sid_chip_id = 0x00000000;
    sid_pidr4 = 0x00000004;
    sid_pidr0 = 0x0000003d;
    sid_pidr1 = 0x000000b7;
    sid_pidr2 = 0x0000000b;
    sid_pidr3 = 0x00000000;
    sid_cidr0 = 0x0000000d;
    sid_cidr1 = 0x000000f0;
    sid_cidr2 = 0x00000005;
    sid_cidr3 = 0x000000b1;
}
local AP_BL2_RESET = {
    data_phys_base = AP_ADDRESS.shared_sram + AP_HW.bl2_data_offset;
    stacks_phys_base = AP_ADDRESS.shared_sram + AP_HW.bl2_stacks_offset;
    bss_phys_base = AP_ADDRESS.shared_sram + AP_HW.bl2_bss_offset;
    xlat_phys_base = AP_ADDRESS.shared_sram + AP_HW.bl2_xlat_offset;
    data_elf_offset = AP_HW.bl2_data_elf_offset;
    data_size = AP_HW.bl2_data_size;
    stacks_size = AP_HW.bl2_stacks_size;
    bss_size = AP_HW.bl2_bss_size;
    xlat_size = AP_HW.bl2_xlat_size;
}

local HOST_AP_SDS_REGION_DATA = {
    0x1007AA7A, AP_SIZE.sds_mem,
    0x01000001, 0x00000011, 0x00000000, 0x00000000,
    0x01000002, 0x00000011, 0x00000000, 0x00000000,
    0x01000003, 0x00000011, 0x00000000, 0x00000000,
    0x01000004, 0x00000011, 0x20000000, 0x00000003,
    0x01000005, 0x00000011, 0x00000000, 0x00000000,
    0x01000006, 0x00000011, 0x00000007, 0x00000000,
    0x01000009, 0x00000021, 0x00000000, 0x00000000,
    0x00000000, 0x00000000,
}

local HOST_AP_TRUSTED_NVCTR_DATA = {
    0x0000001F;
    0x000000DF;
}

local function ap_smmu_component()
    local event_irq_target = "&ap_gic.spi_in_"..AP_IRQ.smmu_event
    if getenv_bool_or("QBOX_APOLLO_FAULT_EVENT_TEST", false) then
        event_irq_target = "&ap_smmu_event_fanout.signal_in"
    end

    if smmu_backend == "systemc-mmu720ae" then
        return {
            moduletype = "smmuv3";
            pamax = AP_HW.smmu_pamax;
            sidsize = AP_HW.smmu_sid_size;
            ato = false;
            num_tbu = AP_HW.smmu_tbu_count;
            iidr = AP_HW.smmu_iidr;
            target_socket = {
                address = AP_ADDRESS.smmu;
                size = AP_SIZE.smmu;
                bind = "&system_router.initiator_socket";
            };
            dma = {bind = "&system_router.target_socket"};
            irq_eventq = {bind = event_irq_target};
        }
    end

    return {
        moduletype = "arm_smmuv3";
        args = {"&platform.ap_qemu_inst", "&platform.ap_gpex_0"};
        mem = {
            address = AP_ADDRESS.smmu;
            size = AP_SIZE.smmu;
            bind = "&system_router.initiator_socket";
        };
        irq_out_0 = {bind = event_irq_target};
        stage = "1";
    }
end

function ap_compute.define(ctx, platform)
    local pcie_irq_test_enabled =
        enable_ap_cpus and
        ctx.getenv_bool_or("QBOX_APOLLO_PCIE_IRQ_TEST", false)
    local fault_event_test_enabled =
        enable_ap_cpus and
        ctx.getenv_bool_or("QBOX_APOLLO_FAULT_EVENT_TEST", false)

    platform.ap_smmu_event_fanout = fault_event_test_enabled and {
        moduletype = "signal_fanout";
        signal_out = {
            bind = "&ap_gic.spi_in_"..AP_IRQ.smmu_event..";&ap_smmu_fault_observer.fault_in";
        };
    } or nil

    platform.ap_smmu_fault_observer = fault_event_test_enabled and {
        moduletype = "zena_fmu";
        bank_count = AP_HW.smmu_fault_bank_count;
        record_count = AP_HW.smmu_fault_record_count;
        enforce_sys_key = false;
        fault_input_enabled = true;
        fault_input_record = 1;
        fault_source = "ap_smmu_0.irq_eventq";
        fault_id = "smmuv3-eventq";
        fault_sink = "ap_gic.spi_in_"..AP_IRQ.smmu_event;
        event_log = ctx.getenv_or("QBOX_APOLLO_FAULT_EVENT_LOG", "");
        log_level = 0;
    } or nil

    platform.ap_qemu_inst_mgr = enable_ap_cpus and {
        moduletype = "QemuInstanceManager";
        construction_priority = -300;
    } or nil

    platform.ap_qemu_inst = enable_ap_cpus and {
        moduletype = "QemuInstance";
        args = {"&platform.ap_qemu_inst_mgr", "AARCH64"};
        accel = ctx.getenv_or(AP_QEMU.accel_env, AP_QEMU.accel);
        tcg_mode = ctx.getenv_or(AP_QEMU.tcg_mode_env, AP_QEMU.tcg_mode);
        sync_policy = ctx.getenv_or(
            AP_QEMU.sync_policy_env,
            AP_QEMU.sync_policy);
        time_sync_strategy = ctx.getenv_or(
            AP_QEMU.time_sync_strategy_env,
            AP_QEMU.time_sync_strategy);
        managed_start_in_reset_release = true;
        qemu_args = ap_qemu_args;
        construction_priority = -299;
    } or nil

    platform.ap_reset_gpio = enable_ap_cpus and {
        moduletype = "reset_gpio";
        args = {"&platform.ap_qemu_inst"};
        reset_out = {bind = ap_cpu_reset_bind_targets(AP_NUM_CPUS)};
        log_level = 0;
    } or nil

    platform.ap_cold_reset_fanout = enable_ap_cpus and {
        moduletype = "reset_fanout";
        reset_out = {bind = ap_cold_reset_bind_targets()};
        log_level = 0;
    } or nil

    platform.apollo_system_reset_fanout = enable_ap_cpus and {
        moduletype = "reset_fanout";
        reset_out = {bind = apollo_system_reset_bind_targets()};
        log_level = 0;
    } or nil

    platform.ap_ns_watchdog_ws1_fanout = enable_ap_cpus and {
        moduletype = "signal_fanout";
        signal_out = {
            bind = "&ap_gic.spi_in_"..AP_IRQ.ns_wdog_ws1..
                ";&host_reset_ctrl.ap_ns_watchdog_reset";
        };
    } or nil

    -- AP CPU backend and PCIe root complex

    platform.ap_global_peripheral_initiator = enable_ap_cpus and {
        moduletype = "global_peripheral_initiator";
        args = {"&platform.ap_qemu_inst", "&platform.ap_cpu_0"};
        request_origin_id = ctx.request_context.origin.ap_global_peripheral;
        request_domain_id = ctx.request_context.domain.ap;
        global_initiator = {bind = "&system_router.target_socket"};
    } or nil

    platform.ap_gpex_0 = enable_ap_cpus and {
        moduletype = "qemu_gpex";
        args = {"&platform.ap_qemu_inst"};
        request_origin_id = ctx.request_context.origin.ap_gpex;
        request_domain_id = ctx.request_context.domain.ap;
        requester_id = AP_HW.gpex_requester_id;
        bus_master = {bind = "&system_router.target_socket"};
        pio_iface = {
            address = AP_ADDRESS.gpex_pio;
            size = AP_SIZE.gpex_pio;
            bind = "&system_router.initiator_socket";
        };
        mmio_iface = {
            address = AP_ADDRESS.gpex_mmio;
            size = AP_SIZE.gpex_mmio;
            bind = "&system_router.initiator_socket";
        };
        ecam_iface = {
            address = AP_ADDRESS.gpex_ecam;
            size = AP_SIZE.gpex_ecam;
            bind = "&system_router.initiator_socket";
        };
        mmio_iface_high = {
            address = AP_ADDRESS.gpex_mmio_high;
            size = AP_SIZE.gpex_mmio_high;
            bind = "&system_router.initiator_socket";
        };
        irq_out_0 = {bind = "&ap_gic.spi_in_"..AP_IRQ.gpex[1]};
        irq_out_1 = {bind = "&ap_gic.spi_in_"..AP_IRQ.gpex[2]};
        irq_out_2 = {bind = "&ap_gic.spi_in_"..AP_IRQ.gpex[3]};
        irq_out_3 = {bind = "&ap_gic.spi_in_"..AP_IRQ.gpex[4]};
    } or nil

    platform.ap_pcie_irq_test_endpoint = pcie_irq_test_enabled and {
        moduletype = "virtio_net_pci";
        args = {"&platform.ap_qemu_inst", "&platform.ap_gpex_0"};
        addr = "01.0";
        mac = "52:54:00:12:34:56";
        netdev_str = "type=user";
    } or nil

    platform.host_ap_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.shared_sram;
            size = AP_SIZE.shared_sram;
            bind = "&system_router.initiator_socket";
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
        request_origin_id = ctx.request_context.origin.ap_loader;
        request_domain_id = ctx.request_context.domain.ap;
        request_capabilities = ctx.request_context.capability.boot_loader;
        request_secure = true;
        request_secure_valid = true;
        initiator_socket = {bind = "&system_router.target_socket"};
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
            address = (AP_ADDRESS.shared_sram + AP_HW.sds_reset_syndrome_offset);
            data = {AP_HW.reset_syndrome_system_reset_req};
        };
        log_level = 0;
    } or nil

    platform.host_ap_mhu_ns_shared_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.mhu_ns_shared_sram;
            size = AP_SIZE.mhu_ns_shared_sram;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.host_ap_peripheral_ns_sram_tail = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.peripheral_ns_sram_tail;
            size = AP_SIZE.peripheral_ns_sram_tail;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }

    platform.host_ap_bl2_header_sram = {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.bl2_header_sram;
            size = AP_SIZE.bl2_header_sram;
            bind = "&system_router.initiator_socket";
        };
        map_file = host_ap_bl2_header_sram_map_file;
        shared_memory = host_sram_shared_memory_enabled(host_ap_bl2_header_sram_map_file);
        shared_memory_prefix = "ra-aph-";
        init_mem = false;
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
        size = AP_SIZE.flash;
        sector_size = AP_SIZE.flash_sector;
        backing_file = flash_writeback and ap_flash or "";
        defer_backing_write = true;
        defer_backing_flush_interval = flash_defer_backing_flush_interval;
        stats_file = ap_flash_stats_file;
        stats_interval = flash_stats_interval;
        target_socket = {
            address = AP_ADDRESS.flash;
            size = AP_SIZE.flash;
            bind = "&system_router.initiator_socket";
        };
        load = {bin_file = ap_flash, offset = 0};
        log_level = 0;
    }

    platform.host_ap_trusted_nvctr = {
        moduletype = "gs_memory";
        read_only = true;
        target_socket = {
            address = AP_ADDRESS.trusted_nvctr;
            size = AP_SIZE.trusted_nvctr;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        load = {data = HOST_AP_TRUSTED_NVCTR_DATA, offset = 0};
        log_level = 0;
    }

    platform.host_ap_dram1 = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.dram1;
            size = AP_SIZE.dram1;
            bind = "&system_router.initiator_socket";
            priority = 10;
        };
        log_level = 0;
    } or nil

    platform.host_ap_ffa_mm_comm_buffer = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.ffa_mm_comm_buffer;
            size = AP_SIZE.ffa_mm_comm_buffer;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    } or nil

    platform.host_ap_spmc_sdram = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.spmc;
            size = AP_SIZE.spmc_local;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.host_ap_dram2 = enable_ap_cpus and {
        moduletype = "gs_memory";
        dmi_allow = host_memory_dmi;
        target_socket = {
            address = AP_ADDRESS.dram2;
            size = AP_SIZE.dram2;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_gic = enable_ap_cpus and {
        moduletype = "arm_gicv3";
        args = {"&platform.ap_qemu_inst"};
        dist_iface = {
            address = AP_ADDRESS.gic_dist;
            size = AP_SIZE.register_window;
            bind = "&system_router.initiator_socket";
        };
        num_cpus = AP_GIC_NUM_CPUS;
        redist_region = repeat_value(1, AP_HW.gic_active_redist_regions);
        has_security_extensions = true;
        has_lpi = true;
        revision = AP_HW.gic_revision;
        num_spi = AP_HW.gic_num_spi;
        has_gicv4_1 = true;
        has_direct_lpi = true;
        has_rvpeid = true;
        has_vpend_valid_dirty = true;
        vpeid_bits = AP_HW.gic_vpeid_bits;
    } or nil

    platform.ap_gic_multiview = {
        moduletype = "gicx00_multiview";
        backend_dist_base = AP_ADDRESS.gic_dist;
        backend_redist_base = AP_ADDRESS.gic_redist;
        backend_redist_stride = AP_SIZE.gic_redist;
        backend_redist_count = AP_HW.gic_active_redist_regions;
        backend_socket = {bind = "&ap_router.target_socket"};
        trace = ctx.getenv_bool_or(
            "QBOX_APOLLO_FULL_AP_GIC_MULTIVIEW_TRACE", false);
        trace_limit = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_AP_GIC_MULTIVIEW_TRACE_LIMIT", "256");
        view0_dist = {
            address = AP_ADDRESS.gic_view0_dist;
            size = AP_SIZE.gic_view0_dist;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        };
    }

    for i=0,(AP_HW.gic_view0_redist_regions-1) do
        platform.ap_gic_multiview["view0_redist_"..i] = {
            address = AP_ADDRESS.gic_view0_redist +
                (i * AP_SIZE.gic_view0_redist);
            size = AP_SIZE.gic_view0_redist;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        }
    end

    if enable_ap_cpus and
       AP_HW.gic_active_redist_regions < AP_HW.gic_redist_regions then
        platform.ap_gic_multiview.inactive_redists = {
            address = AP_ADDRESS.gic_redist +
                AP_HW.gic_active_redist_regions * AP_SIZE.gic_redist;
            size =
                (AP_HW.gic_redist_regions - AP_HW.gic_active_redist_regions) *
                AP_SIZE.gic_redist;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        }
    end

    platform.ap_gic_its = enable_ap_cpus and {
        moduletype = "arm_gicv3_its";
        args = {"&platform.ap_qemu_inst", "&platform.ap_gic"};
        has_gicv4_1 = true;
        gicv4_1_svpet = AP_HW.gic_its_svpet;
        gicv4_1_cte_size = AP_HW.gic_its_cte_size;
        mem = {
            address = AP_ADDRESS.gic_its;
            size = AP_SIZE.gic_its;
            bind = "&system_router.initiator_socket";
        };
    } or nil

    platform.ap_smmu_0 = enable_ap_cpus and ap_smmu_component() or nil

    platform.ap_smmu_lti00 = enable_ap_cpus and
        smmu_backend == "systemc-mmu720ae" and {
            moduletype = "smmuv3_tbu";
            args = {"&platform.ap_smmu_0"};
            topology_id = 0x40;
            upstream_socket = {};
            downstream_socket = {bind = "&system_router.target_socket"};
        } or nil

    platform.ap_watchdog_0 = enable_ap_cpus and {
        moduletype = "zena_watchdog";
        clock_frequency = AP_HW.arch_timer_frequency_hz;
        control = {
            address = AP_ADDRESS.ns_wdog_control;
            size = AP_SIZE.register_window;
            bind = "&system_router.initiator_socket";
        };
        refresh = {
            address = AP_ADDRESS.ns_wdog_refresh;
            size = AP_SIZE.register_window;
            bind = "&system_router.initiator_socket";
        };
        ws0 = {bind = "&ap_gic.spi_in_"..AP_IRQ.ns_wdog_ws0};
        ws1 = {bind = "&ap_ns_watchdog_ws1_fanout.signal_in"};
    } or nil

    platform.ap_secure_console_file = enable_ap_cpus and {
        moduletype = "char_backend_file";
        read_file = secure_uart_read_file;
        write_file = secure_console_log;
        poll_read = secure_uart_read_file ~= "/dev/null";
        poll_interval_ms = uart_poll_interval_ms;
        baudrate = 0;
    } or nil

    platform.ap_primary_console_file = enable_ap_cpus and {
        moduletype = "char_backend_file";
        read_file = primary_uart_read_file;
        write_file = primary_console_log;
        poll_read = primary_uart_read_file ~= "/dev/null";
        poll_interval_ms = tonumber(getenv_or(
            "QBOX_RDASPEN_PRIMARY_UART_POLL_INTERVAL_MS",
            tostring(uart_poll_interval_ms)));
        baudrate = 0;
    } or nil

    platform.ap_secure_uart = enable_ap_cpus and {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        revision = AP_HW.uart_revision;
        target_socket = {
            address = AP_ADDRESS.secure_uart;
            size = AP_SIZE.register_window;
            bind = "&system_router.initiator_socket";
        };
        irq = {bind = "&ap_gic.spi_in_"..AP_IRQ.secure_uart};
        backend_socket = {bind = "&ap_secure_console_file.biflow_socket"};
    } or nil

    platform.ap_primary_uart = enable_ap_cpus and {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        revision = AP_HW.uart_revision;
        target_socket = {
            address = AP_ADDRESS.primary_uart;
            size = AP_SIZE.register_window;
            bind = "&system_router.initiator_socket";
        };
        irq = {bind = "&ap_gic.spi_in_"..AP_IRQ.primary_uart};
        backend_socket = {bind = "&ap_primary_console_file.biflow_socket"};
    } or nil

    platform.ap_timer_mem = enable_ap_cpus and {
        moduletype = "qemu_arm_arch_timer_mmio";
        args = {"&platform.ap_qemu_inst"};
        cntfrq = AP_HW.arch_timer_frequency_hz;
        nr_frames = AP_HW.arch_timer_frame_count;
        view_size = AP_SIZE.sys_timer;
        frame_offset_0 = AP_ADDRESS.sys_counter_non_secure - AP_ADDRESS.sys_timctl;
        frame_id_0 = 0;
        frame_offset_1 = AP_ADDRESS.sys_counter_secure - AP_ADDRESS.sys_timctl;
        frame_id_1 = 1;
        mem = {
            address = AP_ADDRESS.sys_timctl;
            size = (AP_ADDRESS.sys_counter_non_secure - AP_ADDRESS.sys_timctl) + AP_SIZE.sys_timer;
            bind = "&system_router.initiator_socket";
        };
        irq = {
            -- frame 0 is the non-secure AP REFCLK frame.
            {bind = "&ap_gic.spi_in_"..AP_IRQ.sys_timer_non_secure};
            -- frame 1 is the secure AP REFCLK frame.
            {bind = "&ap_gic.spi_in_"..AP_IRQ.sys_timer_secure};
        };
    } or nil

    platform.ap_timer_counter_mirror = enable_ap_cpus and {
        moduletype = "qemu_arm_mmio_counter_mirror";
        dylib_path = "qemu_arm_counter_mirror";
        args = {
            "&platform.ap_timer_mem";
            "&platform.css_system_counter";
        };
    } or nil

    platform.ap_secure_wdog = enable_ap_cpus and {
        moduletype = "zena_watchdog";
        clock_frequency = AP_HW.arch_timer_frequency_hz;
        control = {
            address = AP_ADDRESS.secure_wdog_control;
            size = AP_SIZE.secure_wdog;
            bind = "&system_router.initiator_socket";
        };
        refresh = {
            address = AP_ADDRESS.secure_wdog_refresh;
            size = AP_SIZE.secure_wdog;
            bind = "&system_router.initiator_socket";
        };
        ws0 = {bind = "&ap_gic.spi_in_"..AP_IRQ.secure_wdog_ws0};
        ws1 = {bind = "&host_reset_ctrl.ap_s_watchdog_reset"};
        log_level = 0;
    } or nil

    platform.ap_sid = enable_ap_cpus and {
        moduletype = "host_scr";
        system_id = AP_HW.sid_system_id;
        soc_id = AP_HW.sid_soc_id;
        chip_id = AP_HW.sid_chip_id;
        pidr4 = AP_HW.sid_pidr4;
        pidr0 = AP_HW.sid_pidr0;
        pidr1 = AP_HW.sid_pidr1;
        pidr2 = AP_HW.sid_pidr2;
        pidr3 = AP_HW.sid_pidr3;
        cidr0 = AP_HW.sid_cidr0;
        cidr1 = AP_HW.sid_cidr1;
        cidr2 = AP_HW.sid_cidr2;
        cidr3 = AP_HW.sid_cidr3;
        target_socket = {
            address = AP_ADDRESS.sid;
            size = AP_SIZE.sid;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_rgic2lgic_messreg = enable_ap_cpus and {
        moduletype = "gic720ae_messreg";
        target_socket = {
            address = AP_ADDRESS.rgic2lgic_messreg;
            size = AP_SIZE.rgic2lgic_messreg;
            bind = "&system_router.initiator_socket";
        };
        window_size = AP_SIZE.rgic2lgic_messreg;
        log_level = 0;
    } or nil

    -- Models only the active 5-bank FMU register block in each 1 MiB
    -- APP aperture; unmapped gaps remain explicit coverage debt.

    platform.ap_cl0_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = AP_HW.fmu_bank_count;
        record_count = AP_HW.fmu_record_count;
        target_socket = {
            address = AP_ADDRESS.fmu_cl0;
            size = AP_SIZE.fmu_modeled;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_cl1_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = AP_HW.fmu_bank_count;
        record_count = AP_HW.fmu_record_count;
        target_socket = {
            address = AP_ADDRESS.fmu_cl1;
            size = AP_SIZE.fmu_modeled;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_cl2_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = AP_HW.fmu_bank_count;
        record_count = AP_HW.fmu_record_count;
        target_socket = {
            address = AP_ADDRESS.fmu_cl2;
            size = AP_SIZE.fmu_modeled;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    } or nil

    platform.ap_cl3_ni710ae_fmu = enable_ap_cpus and {
        moduletype = "zena_fmu";
        bank_count = AP_HW.fmu_bank_count;
        record_count = AP_HW.fmu_record_count;
        target_socket = {
            address = AP_ADDRESS.fmu_cl3;
            size = AP_SIZE.fmu_modeled;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    } or nil

if enable_ap_cpus then
    for i=0,(AP_HW.gic_active_redist_regions-1) do
        platform["ap_gic"]["redist_iface_"..i] = {
            address = AP_ADDRESS.gic_redist + (i * AP_SIZE.gic_redist);
            size = AP_SIZE.gic_redist;
            bind = "&system_router.initiator_socket";
        }
    end

    for i=0,(AP_NUM_CPUS-1) do
        local cpu = {
            moduletype = "cpu_arm_cortexA720AE";
            args = {"&platform.ap_qemu_inst"};
            mem = {bind = "&system_router.target_socket"};
            has_el3 = true;
            has_el2 = true;
            irq_timer_phys_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..AP_IRQ.arch_timer_non_secure;
            };
            irq_timer_virt_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..AP_IRQ.arch_timer_virt;
            };
            irq_timer_hyp_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..AP_IRQ.arch_timer_hyp;
            };
            irq_timer_sec_out = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..AP_IRQ.arch_timer_secure;
            };
            gicv3_maintenance_interrupt = {
                bind = "&ap_gic.ppi_in_cpu_"..i.."_"..AP_IRQ.gic_maintenance;
            };
            pmu_interrupt = {bind = "&ap_gic.ppi_in_cpu_"..i.."_"..AP_IRQ.pmu};
            psci_conduit = getenv_or("QBOX_RDASPEN_AP_PSCI_CONDUIT", "disabled");
            mp_affinity = mp_affinity(i);
            start_powered_off = i ~= 0;
            start_in_reset = true;
            reset_power_on = true;
            rvbar = (AP_ADDRESS.shared_sram + AP_HW.bl2_offset);
            cntfrq_hz = AP_HW.arch_timer_frequency_hz;
            trace_pc = ap_pc_trace;
            trace_pc_file = ap_pc_trace_file;
            trace_pc_interval = ap_pc_trace_interval;
            trace_pc_limit = ap_pc_trace_limit;
            trace_exception_state = ap_exception_trace;
            construction_priority = -200 + i;
            request_origin_id = ctx.request_context.origin.ap_cpu_base + i;
            request_domain_id = ctx.request_context.domain.ap;
            requester_id = i;
        }
        platform["ap_cpu_"..tostring(i)] = cpu
        platform["ap_cpu_counter_mirror_"..tostring(i)] = {
            moduletype = "qemu_arm_counter_mirror";
            args = {
                "&platform.ap_cpu_"..i;
                "&platform.css_system_counter";
            };
        }

        platform["ap_gic"]["irq_out_"..i] = {bind = "&ap_cpu_"..i..".irq_in"}
        platform["ap_gic"]["fiq_out_"..i] = {bind = "&ap_cpu_"..i..".fiq_in"}
        platform["ap_gic"]["virq_out_"..i] = {bind = "&ap_cpu_"..i..".virq_in"}
        platform["ap_gic"]["vfiq_out_"..i] = {bind = "&ap_cpu_"..i..".vfiq_in"}
    end
end

end

function ap_compute.enable_ap_router(ctx, platform)
    print("Apollo QVP AP router enabled...")

    local hipc_shared_base, hipc_shared_size =
        ctx.modules.si_cl1.hipc_shared_window()

    -- AP logical view
    platform.ap_router = {
        moduletype = "router";
        log_level = 0;
    }

    platform.ap_hipc_alias = {
        moduletype = "addrtr";
        mapped_base_addr = AP_ADDRESS.hipc_alias;
        target_socket = {
            address = hipc_shared_base;
            size = hipc_shared_size;
            bind = "&ap_router.initiator_socket";
            relative_addresses = false;
            priority = 0;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    platform.ap_to_system_rse_carveout_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = AP_ADDRESS.mhu_pointer_access;
        target_socket = {
            address = AP_ADDRESS.mhu_pointer_access;
            size = AP_SIZE.mhu_pointer_access;
            bind = "&ap_router.initiator_socket";
            relative_addresses = false;
            priority = 0;
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.system_to_ap_flash_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = AP_ADDRESS.flash;
        target_socket = {
            address = AP_ADDRESS.flash;
            size = AP_SIZE.flash;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    local smd_fmu_aliases = {
        { local_base = AP_ADDRESS.fmu_cl0;
          physical_base = AP_ADDRESS.host_fmu_cl0 };
        { local_base = AP_ADDRESS.fmu_cl1;
          physical_base = AP_ADDRESS.host_fmu_cl1 };
        { local_base = AP_ADDRESS.fmu_cl2;
          physical_base = AP_ADDRESS.host_fmu_cl2 };
        { local_base = AP_ADDRESS.fmu_cl3;
          physical_base = AP_ADDRESS.host_fmu_cl3 };
    }
    for i, route in ipairs(smd_fmu_aliases) do
        platform["smd_ap_cl"..(i - 1).."_ni710ae_fmu_alias"] = {
            moduletype = "addrtr";
            mapped_base_addr = route.local_base;
            target_socket = {
                address = route.physical_base;
                size = AP_SIZE.fmu_modeled;
                bind = "&smd_router.initiator_socket";
                relative_addresses = false;
            };
            initiator_socket = {bind = "&ap_router.target_socket"};
            log_level = 0;
        }
    end

    -- Host-to-AP translation and AP masters
    local function bind_ap_target(target)
        if target ~= nil then
            target.bind = "&ap_router.initiator_socket"
            target.priority = target.priority or 0
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
            "&ap_router.initiator_socket"
        platform.host_ap_atu.translation_socket.priority = 10
    end

    if platform.ap_global_peripheral_initiator ~= nil then
        platform.ap_global_peripheral_initiator.global_initiator = {
            bind = "&ap_router.target_socket";
        }
    end
    if platform.ap_bl2_reset_loader ~= nil then
        platform.ap_bl2_reset_loader.initiator_socket = {
            bind = "&ap_router.target_socket";
        }
    end
    if platform.ap_gpex_0 ~= nil then
        if smmu_backend == "systemc-mmu720ae" then
            platform.ap_gpex_0.bus_master = {
                bind = "&ap_smmu_lti00.upstream_socket";
            }
        else
            platform.ap_gpex_0.bus_master = {
                bind = "&ap_router.target_socket";
            }
        end
        bind_ap_target(platform.ap_gpex_0.pio_iface)
        bind_ap_target(platform.ap_gpex_0.mmio_iface)
        bind_ap_target(platform.ap_gpex_0.ecam_iface)
        bind_ap_target(platform.ap_gpex_0.mmio_iface_high)
    end

    -- Memory and interrupt controller windows
    bind_ap_socket(platform.host_ap_shared_sram, "target_socket")
    bind_ap_socket(platform.host_ap_mhu_ns_shared_sram, "target_socket")
    bind_ap_socket(platform.host_ap_peripheral_ns_sram_tail, "target_socket")
    bind_ap_socket(platform.host_ap_bl2_header_sram, "target_socket")
    bind_ap_socket(platform.host_ap_flash, "target_socket")
    bind_ap_socket(platform.host_ap_trusted_nvctr, "target_socket")
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
    if smmu_backend == "systemc-mmu720ae" then
        bind_ap_socket(platform.ap_smmu_0, "target_socket")
    else
        bind_ap_socket(platform.ap_smmu_0, "mem")
    end
    if smmu_backend == "systemc-mmu720ae" and platform.ap_smmu_0 ~= nil then
        platform.ap_smmu_0.dma = {
            bind = "&ap_router.target_socket";
        }
        platform.ap_smmu_lti00.downstream_socket = {
            bind = "&ap_router.target_socket";
        }
    end

    -- RoS and AP peripherals
    ctx.ros.bind_ap_view_targets(platform, bind_ap_target)
    bind_ap_socket(platform.ap_watchdog_0, "control")
    bind_ap_socket(platform.ap_watchdog_0, "refresh")
    bind_ap_socket(platform.ap_secure_uart, "target_socket")
    bind_ap_socket(platform.ap_primary_uart, "target_socket")
    bind_ap_socket(platform.ap_timer_mem, "mem")
    bind_ap_socket(platform.ap_secure_wdog, "control")
    bind_ap_socket(platform.ap_secure_wdog, "refresh")
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
            cpu.mem = {bind = "&ap_router.target_socket"}
        end
    end
end

return ap_compute
