return {
    schema_version = 1;
    irq_routes = {
        { name = "ap_timer_phys"; source = "ap_cpu.generic_timer_phys"; sink = "ap_gic.ppi"; controller = "ap_gic"; kind = "PPI"; id = 30; owner = "ap"; scope = "zena_css_architecture" };
        { name = "ap_timer_virt"; source = "ap_cpu.generic_timer_virt"; sink = "ap_gic.ppi"; controller = "ap_gic"; kind = "PPI"; id = 27; owner = "ap"; scope = "zena_css_architecture" };
        { name = "ap_refclk_secure"; source = "ap_timer_mem.frame1"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 48; owner = "smd"; scope = "zena_css_architecture" };
        { name = "ap_refclk_non_secure"; source = "ap_timer_mem.frame0"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 49; owner = "smd"; scope = "zena_css_architecture" };
        { name = "ap_watchdog"; source = "ap_watchdog_0"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 50; owner = "ap"; scope = "rd_aspen_cfg2" };
        { name = "ap_primary_uart"; source = "ap_primary_uart"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 52; owner = "ap"; scope = "zena_css_architecture" };
        { name = "ap_secure_uart"; source = "ap_secure_uart"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 53; owner = "ap"; scope = "zena_css_architecture" };
        { name = "ap_smmu_event"; source = "ap_smmu_0.event"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 65; owner = "ap"; scope = "zena_css_architecture" };
        { name = "ap_ras_ffh"; source = "ap_ras.corrected_deferred"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 89; owner = "ap"; scope = "zena_css_architecture" };
        { name = "ap_si_scmi_pbx"; source = "host_ap_si_scmi_mhu_pbx"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 112; owner = "si_cl0"; scope = "rd_aspen_cfg2" };
        { name = "ap_si_scmi_mbx"; source = "host_ap_si_scmi_mhu_mbx"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 113; owner = "si_cl0"; scope = "rd_aspen_cfg2" };
        { name = "ap_si_hipc_pbx"; source = "host_ap_si_hipc_mhu_pbx"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 120; owner = "si_cl1"; scope = "fvp_cfg2_extension" };
        { name = "ap_si_hipc_mbx"; source = "host_ap_si_hipc_mhu_mbx"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 121; owner = "si_cl1"; scope = "fvp_cfg2_extension" };
        { name = "gpex_intx_a"; source = "ap_gpex_0.intx0"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 300; owner = "ap"; scope = "rd_aspen_cfg2" };
        { name = "gpex_intx_b"; source = "ap_gpex_0.intx1"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 301; owner = "ap"; scope = "rd_aspen_cfg2" };
        { name = "gpex_intx_c"; source = "ap_gpex_0.intx2"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 302; owner = "ap"; scope = "rd_aspen_cfg2" };
        { name = "gpex_intx_d"; source = "ap_gpex_0.intx3"; sink = "ap_gic.spi"; controller = "ap_gic"; kind = "SPI"; id = 303; owner = "ap"; scope = "rd_aspen_cfg2" };
        { name = "gpex_msi"; source = "ap_gpex_0.msi"; sink = "ap_gic_its.translation"; controller = "ap_gic_its"; kind = "MSI_LPI"; id = 0; owner = "ap"; scope = "zena_css_architecture" };
        { name = "si_cl0_uart"; source = "si_cl0_uart"; sink = "si_cl0_gic.spi"; controller = "si_cl0_gic"; kind = "SPI"; id = 40; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "si_cl0_fmu_critical"; source = "si_cl0_fmu.critical"; sink = "si_cl0_gic.spi"; controller = "si_cl0_gic"; kind = "SPI"; id = 128; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "si_cl0_fmu_noncritical"; source = "si_cl0_fmu.noncritical"; sink = "si_cl0_gic.spi"; controller = "si_cl0_gic"; kind = "SPI"; id = 129; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "si_cl1_uart"; source = "si_cl1_uart"; sink = "si_cl1_gic.spi"; controller = "si_cl1_gic"; kind = "SPI"; id = 7; owner = "si_cl1"; scope = "fvp_cfg2_extension" };
        { name = "si_cl1_hipc_pbx"; source = "si_cl1_hipc_pbx"; sink = "si_cl1_gic.spi"; controller = "si_cl1_gic"; kind = "SPI"; id = 40; owner = "si_cl1"; scope = "fvp_cfg2_extension" };
        { name = "si_cl1_hipc_mbx"; source = "si_cl1_hipc_mbx"; sink = "si_cl1_gic.spi"; controller = "si_cl1_gic"; kind = "SPI"; id = 41; owner = "si_cl1"; scope = "fvp_cfg2_extension" };
        { name = "si_cl1_pfdi"; source = "si_cl1_pfdi_pbx"; sink = "si_cl1_gic.spi"; controller = "si_cl1_gic"; kind = "SPI"; id = 50; owner = "si_cl0"; scope = "fvp_cfg2_extension" };
        { name = "rse_mhu0"; source = "rse_mhu0_receiver"; sink = "rse_nvic"; controller = "rse_nvic"; kind = "IRQ"; id = 41; owner = "rse"; scope = "zena_css_architecture" };
        { name = "rse_mhu2"; source = "rse_mhu2_receiver"; sink = "rse_nvic"; controller = "rse_nvic"; kind = "IRQ"; id = 45; owner = "rse"; scope = "zena_css_architecture" };
        { name = "rse_si_mhu"; source = "host_rse_si_mhu_mbx"; sink = "rse_nvic"; controller = "rse_nvic"; kind = "IRQ"; id = 139; owner = "rse"; scope = "zena_css_architecture" };
    };
    reset_routes = {
        { name = "rse_to_ap_primary_reset"; source = "rse_ap_power_request"; sink = "ap_reset_gpio"; owner = "rse"; order = 40; scope = "zena_css_architecture" };
        { name = "si_cl0_to_ap_secondary_reset"; source = "si_cl0_power_domain_reset"; sink = "ap_cpu.reset"; owner = "si_cl0"; order = 50; scope = "zena_css_architecture" };
        { name = "si_cl0_cluster_reset"; source = "host_si_cl0_clus_ppu"; sink = "si_cl0_cpu.reset"; owner = "si_cl0"; order = 30; scope = "zena_css_architecture" };
        { name = "si_cl1_cluster_reset"; source = "host_si_cl1_clus_ppu"; sink = "si_cl1_cpu.reset"; owner = "si_cl1"; order = 30; scope = "fvp_cfg2_extension" };
    };
    fault_routes = {
        { name = "si_fmu_critical_to_ssu"; source = "si_cl0_fmu.critical"; sink = "si_cl0_ssu"; result = "escalate"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "si_ssu_to_esm"; source = "si_cl0_ssu"; sink = "external_esm"; result = "status_and_reset_policy"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "ap_ras_to_ffh"; source = "ap_ras"; sink = "tfa_ffh"; result = "spi_89_and_notification"; owner = "ap"; scope = "zena_css_architecture" };
    };
}
