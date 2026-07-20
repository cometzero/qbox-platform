local architecture = "zena_css_architecture"
local cfg2 = "rd_aspen_cfg2"
local cl1 = "fvp_cfg2_extension"
local guide = "doc/arm_zena_css_dev_guide/09-programmers-model-for-zena-css.md"

return {
    schema_version = 1;
    ranges = {
        { name = "ap_shared_sram"; base = 0x00000000; size = 0x00100000; view = "ap"; target = "host_ap_shared_sram"; owner = "smd"; access = "rw"; backing = "ap-shared-sram"; scope = cfg2; source = guide };
        { name = "ap_bl2_header_sram"; base = 0x00100000; size = 0x00080000; view = "ap"; target = "host_ap_bl2_header_sram"; owner = "rse"; access = "rw"; backing = "ap-si-cl1-hipc"; reset_policy = "preserve_on_ap_reset"; scope = cfg2; source = guide };
        { name = "ap_mhu_ns_shared_sram"; base = 0x00180000; size = 0x00001000; view = "ap"; target = "host_ap_mhu_ns_shared_sram"; owner = "smd"; access = "rw"; backing = "ap-mhu-ns"; reset_policy = "preserve_on_ap_reset"; scope = cfg2; source = guide };
        { name = "ap_primary_uart"; base = 0x1A400000; size = 0x00010000; view = "ap"; target = "ap_primary_uart"; owner = "ap"; access = "rw"; scope = architecture; source = guide };
        { name = "ap_secure_uart"; base = 0x1A410000; size = 0x00010000; view = "ap"; target = "ap_secure_uart"; owner = "ap"; access = "secure_rw"; scope = architecture; source = guide };
        { name = "ap_watchdog_control"; base = 0x1A420000; size = 0x00010000; view = "ap"; target = "ap_watchdog_0.control"; owner = "ap"; access = "rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "ap_watchdog_refresh"; base = 0x1A430000; size = 0x00010000; view = "ap"; target = "ap_watchdog_0.refresh"; owner = "ap"; access = "rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "ap_secure_watchdog_control"; base = 0x1A460000; size = 0x00010000; view = "ap"; target = "ap_secure_wdog.control"; owner = "ap"; access = "secure_rw"; fidelity = "two_stage_functional"; scope = cfg2; source = guide };
        { name = "ap_secure_watchdog_refresh"; base = 0x1A470000; size = 0x00010000; view = "ap"; target = "ap_secure_wdog.refresh"; owner = "ap"; access = "secure_rw"; fidelity = "two_stage_functional"; scope = cfg2; source = guide };
        { name = "ap_system_id"; base = 0x1A4A0000; size = 0x00010000; view = "ap"; target = "ap_sid"; owner = "smd"; access = "ro"; scope = architecture; source = guide };
        { name = "ap_refclk_timer"; base = 0x1A810000; size = 0x00030000; view = "ap"; target = "ap_timer_mem"; owner = "smd"; access = "rw"; scope = architecture; source = guide };
        { name = "ap_fmu_cl0"; base = 0x1D000000; size = 0x00050000; view = "ap"; target = "ap_cl0_ni710ae_fmu"; owner = "ap"; access = "rw"; fidelity = "functional_subset"; scope = cfg2; source = guide };
        { name = "ap_fmu_cl1"; base = 0x1D100000; size = 0x00050000; view = "ap"; target = "ap_cl1_ni710ae_fmu"; owner = "ap"; access = "rw"; fidelity = "functional_subset"; scope = cfg2; source = guide };
        { name = "ap_fmu_cl2"; base = 0x1D200000; size = 0x00050000; view = "ap"; target = "ap_cl2_ni710ae_fmu"; owner = "ap"; access = "rw"; fidelity = "functional_subset"; scope = cfg2; source = guide };
        { name = "ap_fmu_cl3"; base = 0x1D300000; size = 0x00050000; view = "ap"; target = "ap_cl3_ni710ae_fmu"; owner = "ap"; access = "rw"; fidelity = "functional_subset"; scope = cfg2; source = guide };
        { name = "ap_gic_view0_dist"; base = 0x20000000; size = 0x00080000; view = "ap"; target = "ap_gic_multiview"; owner = "ap"; access = "secure_rw"; fidelity = "control_plane"; scope = cfg2; source = guide };
        { name = "ap_gic_view0_redist"; base = 0x20080000; size = 0x00400000; view = "ap"; target = "ap_gic_multiview"; owner = "ap"; access = "secure_rw"; fidelity = "control_plane"; scope = cfg2; source = guide };
        { name = "ap_gic_dist"; base = 0x20800000; size = 0x00010000; view = "ap"; target = "ap_gic"; owner = "ap"; access = "rw"; scope = architecture; source = guide };
        { name = "ap_gic_its"; base = 0x20840000; size = 0x00040000; view = "ap"; target = "ap_gic_its"; owner = "ap"; access = "rw"; scope = architecture; source = guide };
        { name = "ap_gic_redist"; base = 0x20880000; size = 0x00400000; view = "ap"; target = "ap_gic"; owner = "ap"; access = "rw"; scope = architecture; source = guide };
        { name = "ap_trusted_nvctr"; base = 0x32030000; size = 0x00010000; view = "ap"; target = "host_ap_trusted_nvctr"; owner = "rse"; access = "ro"; scope = cfg2; source = guide };
        { name = "ap_flash"; base = 0x38000000; size = 0x08000000; view = "ap"; target = "host_ap_flash"; owner = "rse"; access = "rw"; backing = "ap-flash"; scope = cfg2; source = guide };
        { name = "ap_to_smd_atu"; base = 0x40000000; size = 0x00800000; view = "ap"; target = "host_ap_atu"; owner = "rse"; access = "policy"; bridge = "ap_to_smd_atu_apu"; scope = architecture; source = guide };
        { name = "ap_rse_mhu_pbx"; base = 0x40680000; size = 0x00030000; view = "ap"; target = "host_ap_rse_mhu_pbx"; owner = "rse"; access = "secure_rw"; bridge = "ap_to_smd_atu_apu"; alias_of = "ap_to_smd_atu"; priority = 0; reason = "AP secure MHU has a dedicated target inside the programmed ATU aperture"; scope = cfg2; source = guide };
        { name = "ap_rse_mhu_mbx"; base = 0x406B0000; size = 0x00030000; view = "ap"; target = "host_ap_rse_mhu_mbx"; owner = "rse"; access = "secure_rw"; bridge = "ap_to_smd_atu_apu"; alias_of = "ap_to_smd_atu"; priority = 0; reason = "AP secure MHU has a dedicated target inside the programmed ATU aperture"; scope = cfg2; source = guide };
        { name = "ap_pcie_ecam"; base = 0x43B50000; size = 0x10000000; view = "ap"; target = "ap_gpex_0.ecam_iface"; owner = "ap"; access = "rw"; scope = cfg2; source = guide };
        { name = "ap_rgic2lgic"; base = 0x5FFF0000; size = 0x00010000; view = "ap"; target = "ap_rgic2lgic_messreg"; owner = "ap"; access = "rw"; scope = cfg2; source = guide };
        { name = "ap_pcie_pio"; base = 0x60200000; size = 0x00100000; view = "ap"; target = "ap_gpex_0.pio_iface"; owner = "ap"; access = "rw"; scope = cfg2; source = guide };
        { name = "ap_pcie_mmio"; base = 0x60300000; size = 0x1FD00000; view = "ap"; target = "ap_gpex_0.mmio_iface"; owner = "ap"; access = "rw"; scope = cfg2; source = guide };
        { name = "ap_dram_low"; base = 0x80000000; size = 0x7F000000; view = "ap"; target = "host_ap_dram1"; owner = "ap"; access = "rw"; backing = "ap-dram-low"; scope = cfg2; source = guide };
        { name = "ap_hipc_shared"; base = 0xE0130000; size = 0x00080000; view = "ap"; target = "ap_hipc_alias"; owner = "si_cl1"; access = "rw"; backing = "ap-si-cl1-hipc"; alias_of = "ap_dram_low"; priority = 0; reason = "HIPC shared memory is a dedicated alias inside the low DRAM aperture"; scope = cl1; source = "arm-zena-css/documentation/design/hipc.rst" };
        { name = "ap_ffa_buffer"; base = 0xFFBF0000; size = 0x00002000; view = "ap"; target = "host_ap_ffa_mm_comm_buffer"; owner = "optee"; access = "rw"; backing = "ffa-buffer"; scope = cfg2; source = guide };
        { name = "ap_spmc_sdram"; base = 0xFFC00000; size = 0x003E0000; view = "ap"; target = "host_ap_spmc_sdram"; owner = "optee"; access = "secure_rw"; backing = "spmc-sdram"; scope = cfg2; source = guide };
        { name = "ap_rse_carveout"; base = 0xFFFE0000; size = 0x00020000; view = "ap"; target = "ap_to_system_rse_carveout_bridge"; owner = "rse"; access = "secure_rw"; backing = "ap-rse-carveout"; scope = cfg2; source = guide };
        { name = "ap_smmu"; base = 0x1C0000000; size = 0x08000000; view = "ap"; target = "ap_smmu_0"; owner = "ap"; access = "rw"; scope = architecture; source = guide };
        { name = "ap_pcie_mmio_high"; base = 0x400000000; size = 0x200000000; view = "ap"; target = "ap_gpex_0.mmio_iface_high"; owner = "ap"; access = "rw"; scope = cfg2; source = guide };
        { name = "ap_dram_high"; base = 0x20000000000; size = 0x80000000; view = "ap"; target = "host_ap_dram2"; owner = "ap"; access = "rw"; backing = "ap-dram-high"; scope = cfg2; source = guide };

        { name = "smd_shared_sram"; base = 0x00000000; size = 0x00100000; view = "smd"; target = "host_ap_shared_sram"; owner = "smd"; access = "rw"; backing = "ap-shared-sram"; scope = architecture; source = guide };
        { name = "system_smd_shared_sram"; base = 0x2000060000000; size = 0x00100000; view = "system"; target = "host_smd_shared_sram"; owner = "smd"; access = "rw"; backing = "smd-shared-sram"; scope = architecture; source = guide };
        { name = "system_smdexp_atu"; base = 0x20000D0070000; size = 0x00010000; view = "system"; target = "host_smdexp2smd_atu"; owner = "rse"; access = "policy"; bridge = "system_to_smd_nci"; scope = architecture; source = guide };
        { name = "system_ap_atu"; base = 0x20000D0080000; size = 0x00010000; view = "system"; target = "host_ap_atu"; owner = "rse"; access = "policy"; bridge = "system_to_smd_nci"; scope = architecture; source = guide };
        { name = "system_css_rgm"; base = 0x20000D0010000; size = 0x00001000; view = "system"; target = "host_reset_ctrl.rgm"; owner = "smd"; access = "rw"; fidelity = "functional_subset"; bridge = "system_to_smd_nci"; scope = architecture; source = guide };
        { name = "system_ap_flash"; base = 0x38000000; size = 0x08000000; view = "system"; target = "system_to_ap_flash_bridge"; owner = "rse"; access = "rw"; backing = "ap-flash"; alias_of = "ap_flash"; priority = 0; reason = "RSE ATU reaches the AP flash through its architected system-physical address"; scope = cfg2; source = guide };
        { name = "system_ap_mhu_pointer_data"; base = 0xFFFE0000; size = 0x0001C000; view = "system"; target = "host_ap_rse_mailbox"; owner = "rse"; access = "secure_rw"; backing = "ap-rse-carveout"; backing_size = 0x00020000; alias_of = "ap_rse_carveout"; priority = 0; reason = "RSE SFCP pointer access and AP SE-Proxy use one shared carveout backing"; scope = cfg2; source = guide };
        { name = "system_ap_rse_mailbox"; base = 0xFFFFC000; size = 0x00004000; view = "system"; target = "host_ap_rse_mailbox"; owner = "rse"; access = "secure_rw"; backing = "ap-rse-carveout"; backing_size = 0x00020000; alias_of = "ap_rse_carveout"; priority = 0; reason = "The MHU outband mailbox is the final 16 KiB of the AP-RSE carveout"; scope = cfg2; source = guide };
        { name = "system_css_counters"; base = 0x20000D0100000; size = 0x00030000; view = "system"; target = "host_css_gtimer"; owner = "smd"; access = "rw"; bridge = "system_to_smd_nci"; scope = architecture; source = guide };
        { name = "system_systop_pik"; base = 0x20000D0200000; size = 0x00001000; view = "system"; target = "host_reset_ctrl.pik"; owner = "si_cl0"; access = "rw"; fidelity = "functional_subset"; bridge = "system_to_smd_nci"; scope = architecture; source = guide };
        { name = "system_systop_ppu"; base = 0x20000D0201000; size = 0x00001000; view = "system"; target = "si_cl0_sys0_ppu"; owner = "si_cl0"; access = "rw"; fidelity = "functional_subset"; bridge = "system_to_smd_nci"; scope = architecture; source = guide };
        { name = "system_ap_rse_mhu"; base = 0x300001B600000; size = 0x00060000; view = "system"; target = "host_ap_rse_mhu"; owner = "rse"; access = "secure_rw"; scope = cfg2; source = guide };
        { name = "system_si_cl0_util"; base = 0x4000028000000; size = 0x00800000; view = "system"; target = "host_si_cl0_cub"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "system_si_cl1_util"; base = 0x4000028800000; size = 0x00800000; view = "system"; target = "host_si_cl1_cub"; owner = "si_cl1"; access = "rw"; scope = cl1; source = guide };
        { name = "system_si_pik"; base = 0x400002A600000; size = 0x00010000; view = "system"; target = "host_si_pik"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "system_si_scr"; base = 0x400002A6B0000; size = 0x00010000; view = "system"; target = "host_si_scr"; owner = "rse"; access = "rw"; scope = architecture; source = guide };
        { name = "system_si_atu"; base = 0x4000031000000; size = 0x00010000; view = "system"; target = "host_si_atu"; owner = "rse"; access = "policy"; scope = architecture; source = guide };
        { name = "system_ap_si_mhu"; base = 0x400003B000000; size = 0x00400000; view = "system"; target = "host_ap_si_mhu"; owner = "si_cl0"; access = "rw"; scope = cfg2; source = guide };
        { name = "system_rse_si_mhu"; base = 0x400003C000000; size = 0x01000000; view = "system"; target = "host_rse_si_mhu"; owner = "rse"; access = "secure_rw"; scope = architecture; source = guide };
        { name = "system_rse_si_ssram"; base = 0x4000040000000; size = 0x00040000; view = "system"; target = "host_rse_si_ssram"; owner = "rse"; access = "rw"; backing = "rse-si-ssram"; scope = architecture; source = guide };
        { name = "system_si_cl0_sram"; base = 0x4000120000000; size = 0x01000000; view = "system"; target = "host_si_cl0_sram"; owner = "si_cl0"; access = "rw"; backing = "si-cl0-sram"; backing_size = 0x00800000; scope = architecture; source = guide };
        { name = "system_si_cl1_sram"; base = 0x4000140000000; size = 0x01000000; view = "system"; target = "host_si_cl1_sram"; owner = "si_cl1"; access = "rw"; backing = "si-cl1-sram"; backing_size = 0x00800000; scope = cl1; source = guide };

        { name = "rse_itcm_ns"; base = 0x00000000; size = 0x00008000; view = "rse"; target = "rse_itcm"; owner = "rse"; access = "rw"; backing = "rse-itcm"; scope = architecture; source = guide };
        { name = "rse_rom_s"; base = 0x11000000; size = 0x00020000; view = "rse"; target = "rse_rom"; owner = "rse"; access = "secure_ro"; backing = "rse-rom"; scope = architecture; source = guide };
        { name = "rse_dtcm_ns"; base = 0x20000000; size = 0x00008000; view = "rse"; target = "rse_dtcm"; owner = "rse"; access = "rw"; backing = "rse-dtcm"; scope = architecture; source = guide };
        { name = "rse_vm0_s"; base = 0x31000000; size = 0x00040000; view = "rse"; target = "rse_vm0"; owner = "rse"; access = "secure_rw"; backing = "rse-vm0"; scope = architecture; source = guide };
        { name = "rse_nsacfg"; base = 0x40080000; size = 0x00001000; view = "rse"; target = "rse_nsacfg"; owner = "rse"; access = "rw"; scope = architecture; source = guide };
        { name = "rse_watchdog_ns_control"; base = 0x48040000; size = 0x00001000; view = "rse"; target = "rse_watchdog_ns.control"; owner = "rse"; access = "rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "rse_watchdog_ns_refresh"; base = 0x48041000; size = 0x00001000; view = "rse"; target = "rse_watchdog_ns.refresh"; owner = "rse"; access = "rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "rse_dma350"; base = 0x50002000; size = 0x00001000; view = "rse"; target = "rse_dma350"; owner = "rse"; access = "secure_rw"; scope = architecture; source = guide };
        { name = "rse_atu"; base = 0x50150000; size = 0x00001000; view = "rse"; target = "rse_atu"; owner = "rse"; access = "secure_rw"; bridge = "rse_to_system_atu_apu"; scope = architecture; source = guide };
        { name = "rse_cc3xx"; base = 0x50154000; size = 0x00002000; view = "rse"; target = "rse_cc3xx"; owner = "rse"; access = "secure_rw"; scope = architecture; source = guide };
        { name = "rse_watchdog_s_control"; base = 0x58040000; size = 0x00001000; view = "rse"; target = "rse_watchdog_s.control"; owner = "rse"; access = "secure_rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "rse_watchdog_s_refresh"; base = 0x58041000; size = 0x00001000; view = "rse"; target = "rse_watchdog_s.refresh"; owner = "rse"; access = "secure_rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "rse_mhu0_sender"; base = 0x50160000; size = 0x00010000; view = "rse"; target = "rse_mhu0_sender"; owner = "rse"; access = "secure_rw"; scope = architecture; source = guide };
        { name = "rse_mhu0_receiver"; base = 0x50170000; size = 0x00010000; view = "rse"; target = "rse_mhu0_receiver"; owner = "rse"; access = "secure_rw"; scope = architecture; source = guide };
        { name = "rse_host_ns"; base = 0x60000000; size = 0x10000000; view = "rse"; target = "rse_host_access"; owner = "rse"; access = "rw"; bridge = "rse_to_system_atu_apu"; scope = architecture; source = guide };
        { name = "rse_host_s"; base = 0x70000000; size = 0x10000000; view = "rse"; target = "rse_host_access"; owner = "rse"; access = "secure_rw"; bridge = "rse_to_system_atu_apu"; scope = architecture; source = guide };
        { name = "rse_boot_flash"; base = 0xB0000000; size = 0x04000000; view = "rse"; target = "rse_flash"; owner = "rse"; access = "secure_rw"; backing = "rse-flash"; scope = architecture; source = guide };
        { name = "rse_nvic"; base = 0xE000E000; size = 0x00010000; view = "rse"; target = "rse_nvic"; owner = "rse"; access = "secure_rw"; scope = architecture; source = guide };

        { name = "si_cl0_nci_primary"; base = 0x2A000000; size = 0x00010000; view = "si_cl0"; target = "si_cl0_nci_primary"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "si_cl0_uart"; base = 0x2A400000; size = 0x00010000; view = "si_cl0"; target = "si_cl0_uart"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "si_cl0_ssu"; base = 0x2A500000; size = 0x00001000; view = "si_cl0"; target = "si_cl0_ssu"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "si_cl0_fmu"; base = 0x2A510000; size = 0x00050000; view = "si_cl0"; target = "si_cl0_fmu"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "si_cl0_scr"; base = 0x2A6B0000; size = 0x00010000; view = "si_cl0"; target = "si_cl0_scr"; owner = "rse"; access = "rw"; scope = architecture; source = guide };
        { name = "si_cl0_refclk_control"; base = 0x2A6F0000; size = 0x00010000; view = "si_cl0"; target = "si_cl0_refclk_control"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "si_cl0_watchdog_control"; base = 0x2A700000; size = 0x00010000; view = "si_cl0"; target = "si_cl0_watchdog.control"; owner = "si_cl0"; access = "rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "si_cl0_watchdog_refresh"; base = 0x2A710000; size = 0x00010000; view = "si_cl0"; target = "si_cl0_watchdog.refresh"; owner = "si_cl0"; access = "rw"; fidelity = "two_stage_functional"; scope = architecture; source = guide };
        { name = "si_cl0_refclk_count"; base = 0x2A720000; size = 0x00010000; view = "si_cl0"; target = "si_cl0_refclk_count"; owner = "si_cl0"; access = "ro"; scope = architecture; source = guide };
        { name = "si_cl0_gic_view0"; base = 0x30000000; size = 0x000E0000; view = "si_cl0"; target = "si_cl0_gic"; owner = "si_cl0"; access = "rw"; scope = architecture; source = guide };
        { name = "si_cl0_gic_view1"; base = 0x30100000; size = 0x00060000; view = "si_cl0"; target = "si_gic_view1"; owner = "si_cl1"; access = "policy"; scope = cl1; source = guide };
        { name = "si_cl0_rse_shared"; base = 0x40000000; size = 0x00040000; view = "si_cl0"; target = "host_rse_si_ssram"; owner = "rse"; access = "rw"; backing = "rse-si-ssram"; bridge = "si_cl0_to_rse_shared"; scope = architecture; source = guide };
        { name = "si_cl0_si_cl1_scmi_shmem"; base = 0x48000000; size = 0x00001000; view = "si_cl0"; target = "si_cl1_scmi_shmem"; owner = "si_cl0"; access = "rw"; backing = "si-cl1-scmi"; bridge = "si_cl0_to_si_cl1_scmi_apu"; scope = cl1; source = guide };
        { name = "si_cl0_cmn_atw"; base = 0x80000000; size = 0x40000000; view = "si_cl0"; target = "si_cl0_cmn"; owner = "si_cl0"; access = "policy"; bridge = "si_cl0_to_system_atu_apu"; scope = architecture; source = guide };
        { name = "si_cl0_cluster_utility_atw"; base = 0xC0000000; size = 0x10000000; view = "si_cl0"; target = "si_cl0_cluster_utility"; owner = "si_cl0"; access = "policy"; bridge = "si_cl0_to_system_atu_apu"; scope = architecture; source = guide };
        { name = "si_cl0_smd_atw"; base = 0xD0000000; size = 0x08000000; view = "si_cl0"; target = "si_cl0_smd_windows"; owner = "rse"; access = "policy"; bridge = "si_cl0_to_system_atu_apu"; scope = architecture; source = guide };
        { name = "si_cl0_ap_shared_atw"; base = 0xE0030000; size = 0x00100000; view = "si_cl0"; target = "host_ap_shared_sram"; owner = "ap"; access = "rw"; backing = "ap-shared-sram"; bridge = "si_cl0_to_system_atu_apu"; scope = architecture; source = guide };
        { name = "si_cl0_hipc_atw"; base = 0xE0130000; size = 0x00080000; view = "si_cl0"; target = "host_ap_bl2_header_sram"; owner = "si_cl1"; access = "rw"; backing = "ap-si-cl1-hipc"; bridge = "si_cl0_to_ap_hipc"; scope = cl1; source = guide };
        { name = "si_cl0_local_sram"; base = 0x120000000; size = 0x00800000; view = "si_cl0"; target = "si_cl0_sram"; owner = "si_cl0"; access = "rw"; backing = "si-cl0-sram"; scope = architecture; source = guide };

        { name = "si_cl1_uart"; base = 0x2A410000; size = 0x00010000; view = "si_cl1"; target = "si_cl1_uart"; owner = "si_cl1"; access = "rw"; scope = cl1; source = guide };
        { name = "si_cl1_gic"; base = 0x30200000; size = 0x00010000; view = "si_cl1"; target = "si_cl1_gic"; owner = "si_cl1"; access = "rw"; scope = cl1; source = guide };
        { name = "si_cl1_gic_redist"; base = 0x30260000; size = 0x00080000; view = "si_cl1"; target = "si_cl1_gic"; owner = "si_cl1"; access = "rw"; scope = cl1; source = guide };
        { name = "si_cl1_hipc_pbx"; base = 0x39000000; size = 0x00030000; view = "si_cl1"; target = "si_cl1_hipc_pbx"; owner = "si_cl1"; access = "rw"; scope = cl1; source = guide };
        { name = "si_cl1_hipc_mbx"; base = 0x39040000; size = 0x00030000; view = "si_cl1"; target = "si_cl1_hipc_mbx"; owner = "si_cl1"; access = "rw"; scope = cl1; source = guide };
        { name = "si_cl1_pfdi_pbx"; base = 0x39200000; size = 0x00020000; view = "si_cl1"; target = "si_cl1_pfdi_pbx"; owner = "si_cl0"; access = "rw"; scope = cl1; source = guide };
        { name = "si_cl1_scmi_shmem"; base = 0x48000000; size = 0x00001000; view = "si_cl1"; target = "si_cl1_scmi_shmem"; owner = "si_cl0"; access = "rw"; backing = "si-cl1-scmi"; scope = cl1; source = guide };
        { name = "si_cl1_hipc_shared"; base = 0xE0130000; size = 0x00080000; view = "si_cl1"; target = "host_ap_bl2_header_sram"; owner = "si_cl1"; access = "rw"; backing = "ap-si-cl1-hipc"; bridge = "si_cl1_to_ap_hipc"; scope = cl1; source = "arm-zena-css/documentation/design/hipc.rst" };
        { name = "si_cl1_local_sram"; base = 0x140000000; size = 0x00800000; view = "si_cl1"; target = "si_cl1_sram"; owner = "si_cl1"; access = "rw"; backing = "si-cl1-sram"; scope = cl1; source = guide };
    };
}
