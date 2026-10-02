-- SPDX-License-Identifier: BSD-3-Clause
-- Host models policy, evaluated once before hardware construction.
return function(options, apollo_dir)
    options.cc3xx_trace = options.getenv_or("QBOX_RDASPEN_CC3XX_TRACE", "false") == "true"
    options.cc3xx_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_CC3XX_TRACE_LIMIT", "64"))
    options.cc3xx_trace_filter = options.getenv_or("QBOX_RDASPEN_CC3XX_TRACE_FILTER", "all")
    options.cc3xx_stats_file = options.getenv_or("QBOX_RDASPEN_CC3XX_STATS_FILE", "")
    options.cc3xx_stats_interval = tonumber(options.getenv_or("QBOX_RDASPEN_CC3XX_STATS_INTERVAL", "0"))
    options.cc3xx_backend = options.getenv_or("QBOX_RDASPEN_CC3XX_BACKEND", "systemc")
    assert(options.cc3xx_backend == "systemc" or options.cc3xx_backend == "qemu-native",
           "QBOX_RDASPEN_CC3XX_BACKEND must be systemc or qemu-native")
    options.smmu_backend = options.getenv_or("QBOX_RDASPEN_SMMU_BACKEND", "systemc-mmu720ae")
    assert(options.smmu_backend == "qemu-arm-smmuv3" or options.smmu_backend == "systemc-mmu720ae",
           "QBOX_RDASPEN_SMMU_BACKEND must be qemu-arm-smmuv3 or systemc-mmu720ae")
    options.dma350_trace = options.getenv_or("QBOX_RDASPEN_DMA350_TRACE", "false") == "true"
    options.dma350_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_DMA350_TRACE_LIMIT", "64"))
    options.dma350_trace_filter = options.getenv_or("QBOX_RDASPEN_DMA350_TRACE_FILTER", "all")
    options.sysctrl_trace = options.getenv_or("QBOX_RDASPEN_SYSCTRL_TRACE", "false") == "true"
    options.sysctrl_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_SYSCTRL_TRACE_LIMIT", "64"))
    options.lcm_trace = options.getenv_or("QBOX_RDASPEN_LCM_TRACE", "false") == "true"
    options.lcm_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_LCM_TRACE_LIMIT", "64"))
    options.rse_lcm_lcs = options.getenv_number_or("QBOX_RDASPEN_RSE_LCM_LCS", "4008617381")
    options.rse_lcm_tp_mode = options.getenv_number_or("QBOX_RDASPEN_RSE_LCM_TP_MODE", "286348714")
    options.rse_lcm_sp_enable = options.getenv_number_or("QBOX_RDASPEN_RSE_LCM_SP_ENABLE", "0")
    options.boot_flash_trace = options.getenv_or("QBOX_RDASPEN_BOOT_FLASH_TRACE", "false") == "true"
    options.boot_flash_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_BOOT_FLASH_TRACE_LIMIT", "64"))
    options.boot_flash_dmi = options.getenv_or("QBOX_RDASPEN_BOOT_FLASH_DMI", "false") == "true"
    options.boot_flash_dmi_ranges = options.getenv_or("QBOX_RDASPEN_BOOT_FLASH_DMI_RANGES", "")
    options.host_memory_dmi = options.getenv_or("QBOX_RDASPEN_HOST_MEMORY_DMI", "false") == "true"
    options.host_si_sram_dmi = options.getenv_or("QBOX_RDASPEN_HOST_SI_SRAM_DMI", "false") == "true"
    options.ap_flash_dmi_ranges = options.getenv_or("QBOX_RDASPEN_AP_FLASH_DMI_RANGES", "")
    options.rse_boot_flash_stats_file = options.getenv_or(
        "QBOX_RDASPEN_RSE_BOOT_FLASH_STATS_FILE",
        "")
    options.ap_flash_stats_file = options.getenv_or("QBOX_RDASPEN_AP_FLASH_STATS_FILE", "")
    options.flash_stats_interval = tonumber(options.getenv_or("QBOX_RDASPEN_FLASH_STATS_INTERVAL", "0"))
    options.atu_trace = options.getenv_or("QBOX_RDASPEN_ATU_TRACE", "false") == "true"
    options.atu_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_ATU_TRACE_LIMIT", "64"))
    options.atu_trace_filter = options.getenv_or("QBOX_RDASPEN_ATU_TRACE_FILTER", "all")
    options.atu_dmi = options.getenv_or("QBOX_RDASPEN_ATU_DMI", "false") == "true"
    options.kmu_trace = options.getenv_or("QBOX_RDASPEN_KMU_TRACE", "false") == "true"
    options.kmu_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_KMU_TRACE_LIMIT", "64"))
    options.kmu_trace_filter = options.getenv_or("QBOX_RDASPEN_KMU_TRACE_FILTER", "all")
    options.integrity_checker_trace = options.getenv_or("QBOX_RDASPEN_INTEGRITY_CHECKER_TRACE", "false") == "true"
    options.integrity_checker_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_INTEGRITY_CHECKER_TRACE_LIMIT", "64"))
    options.sam_trace = options.getenv_or("QBOX_RDASPEN_SAM_TRACE", "false") == "true"
    options.sam_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_SAM_TRACE_LIMIT", "64"))
    options.host_ppu_trace = options.getenv_or("QBOX_RDASPEN_HOST_PPU_TRACE", "false") == "true"
    options.host_ppu_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_HOST_PPU_TRACE_LIMIT", "64"))
    options.mhu_trace = options.getenv_or("QBOX_RDASPEN_MHU_TRACE", "false") == "true"
    options.mhu_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_MHU_TRACE_LIMIT", "256"))
    options.mhu_trace_file = options.getenv_or(
        "QBOX_RDASPEN_MHU_TRACE_FILE",
        options.root.."build/qbox-fvp-rd-aspen/mhuv3-trace.log")
    options.ap_power_domain_reset_delay_ns = tonumber(
        options.getenv_or("QBOX_RDASPEN_AP_POWER_DOMAIN_RESET_DELAY_NS", "1"))
    options.rse_local_crypto = options.getenv_or("QBOX_RDASPEN_RSE_LOCAL_CRYPTO", "true") == "true"
    options.rse_local_boot_flash = options.getenv_or("QBOX_RDASPEN_RSE_LOCAL_BOOT_FLASH", "true") == "true"
    options.rse_flash_backend = options.getenv_or(
        "QBOX_RDASPEN_RSE_FLASH_BACKEND",
        "qemu-cfi-local")
    assert(options.rse_flash_backend == "systemc-strata" or
           options.rse_flash_backend == "qemu-cfi-local",
           "QBOX_RDASPEN_RSE_FLASH_BACKEND must be systemc-strata or qemu-cfi-local")
    assert(options.rse_flash_backend ~= "qemu-cfi-local" or options.rse_local_boot_flash,
           "qemu-cfi-local requires QBOX_RDASPEN_RSE_LOCAL_BOOT_FLASH=true")
    options.rse_local_peripherals = options.rse_local_crypto or options.rse_local_boot_flash
    options.rse_split_cpu0_dtcm_alias = options.getenv_or(
        "QBOX_RDASPEN_RSE_SPLIT_CPU0_DTCM_ALIAS",
        "false") == "true"
    options.rse_split_cpu0_itcm_alias = options.getenv_or(
        "QBOX_RDASPEN_RSE_SPLIT_CPU0_ITCM_ALIAS",
        "false") == "true"
    options.rse_itcm_dmi = options.getenv_or("QBOX_RDASPEN_RSE_ITCM_DMI", "true") == "true"
    options.rse_dtcm_dmi = options.getenv_or("QBOX_RDASPEN_RSE_DTCM_DMI", "true") == "true"
    options.rse_vm_dmi = options.getenv_or("QBOX_RDASPEN_RSE_VM_DMI", "true") == "true"
    options.rse_vmaddrwidth = options.getenv_number_or(
        "QBOX_RDASPEN_RSE_VMADDRWIDTH",
        "18")
    assert(options.rse_vmaddrwidth >= 18 and options.rse_vmaddrwidth <= 31,
           "QBOX_RDASPEN_RSE_VMADDRWIDTH must be in range 18..31")
    options.rse_reset_syndrome = options.getenv_number_or(
        "QBOX_RDASPEN_RSE_RESET_SYNDROME",
        "0x80000000")
    options.rse_smd_counter_mirror = options.getenv_bool_or(
        "QBOX_APOLLO_RSE_SMD_COUNTER_MIRROR",
        true)
    options.rse_lsc_input_hz = options.rse_smd_counter_mirror and 125000000 or
        options.getenv_number_or("QBOX_APOLLO_RSE_LSC_INPUT_HZ", "125000000")
    assert(options.rse_lsc_input_hz > 0,
           "QBOX_APOLLO_RSE_LSC_INPUT_HZ must be greater than zero")
    options.rse_cpuwait = options.getenv_number_or(
        "QBOX_RDASPEN_RSE_CPUWAIT",
        "0x0000000F")
    options.rse_dma_boot_en = options.getenv_number_or(
        "QBOX_RDASPEN_RSE_DMA_BOOT_EN",
        "0x00000001")
end
