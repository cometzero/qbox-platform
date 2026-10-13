-- SPDX-License-Identifier: BSD-3-Clause
-- Complete model defaults; execution profiles supply host resources separately.
local config = {}

function config.create(apollo_dir)
    return {
        apollo_dir = apollo_dir;
        options = {
            enable_ap_cpus = true;
            AP_NUM_CPUS = 16;
            AP_GIC_NUM_CPUS = 16;
            ap_psci_conduit = "disabled";
            pcie_bifurcation = "none";
            smmu_backend = "systemc-mmu720ae";
            cc3xx_backend = "systemc";
            rse_flash_backend = "systemc-strata";
            rse_local_crypto = false;
            rse_local_boot_flash = false;
            rse_local_peripherals = false;
            rse_split_cpu0_dtcm_alias = false;
            rse_split_cpu0_itcm_alias = false;
            rse_smd_counter_mirror = false;
            rse_ps_proxy = false;
            rse_vmaddrwidth = 18;
            rse_lsc_input_hz = 125000000;
            rse_cpuwait = 0x0000000F;
            rse_dma_boot_en = 0x00000001;
            rse_reset_syndrome = 0x80000000;
            rse_gpio_init_inputs = {[0] = 0; [1] = 0};
            rse_lcm_lcs = 4008617381;
            rse_lcm_tp_mode = 286348714;
            rse_lcm_sp_enable = 0;
            rse_otp_lock_after_provision = true;
            smd_gpio_init_inputs = 0;
            ap_power_domain_reset_delay_ns = 1;
            si_cl0_ppu_on_delay_ns = 0;
            si_cl0_ppu_access_latency_ns = 100;
            host_memory_dmi = false;
            host_si_sram_dmi = false;
            atu_dmi = false;
            boot_flash_dmi = false;
            boot_flash_dmi_ranges = "";
            rse_itcm_dmi = false;
            rse_dtcm_dmi = false;
            rse_vm_dmi = false;
            reset_trace = false;
            ap_gic_multiview_trace = false;
            ap_gic_multiview_trace_limit = 256;
            atu_trace = false;
            atu_trace_limit = 64;
            atu_trace_filter = "all";
            atu_trace_address_min = 0;
            atu_trace_address_max = 0;
            boot_flash_trace = false;
            boot_flash_trace_limit = 64;
            cc3xx_trace = false;
            cc3xx_trace_limit = 64;
            cc3xx_trace_filter = "all";
            cc3xx_trace_skip = 0;
            cc3xx_stats_interval = 0;
            dma350_trace = false;
            dma350_trace_limit = 64;
            dma350_trace_filter = "all";
            host_ppu_trace = false;
            host_ppu_trace_limit = 64;
            integrity_checker_trace = false;
            integrity_checker_trace_limit = 64;
            kmu_trace = false;
            kmu_trace_limit = 64;
            kmu_trace_filter = "all";
            lcm_trace = false;
            lcm_trace_limit = 64;
            mhu_trace = false;
            mhu_trace_limit = 256;
            sam_trace = false;
            sam_trace_limit = 64;
            sysctrl_trace = false;
            sysctrl_trace_limit = 64;
        };
        instances = {
            ap = "&platform.ap_qemu_inst";
            rse = "&platform.qemu_inst";
            rse_local = "&qemu_inst";
        };
        request_context = {
            domain = {
                system = 0; ap = 1; smd = 2; rse = 3; si_cl0 = 4; si_cl1 = 5;
            };
            origin = {
                ap_cpu_base = 0x1000;
                ap_gpex = 0x1100;
                ap_pcie_epc = 0x1101;
                ap_global_peripheral = 0x1200;
                rse_cpu = 0x3000;
                si_cl0_cpu_base = 0x4000;
                si_cl1_cpu_base = 0x5000;
            };
        };
        config = {
            ap = {enable_cpus = true};
            rse = {
                addresses = {
                    atu_trace_min = 0; atu_trace_max = 0;
                    dma350_trace_min = 0; cc3xx_trace_min = 0;
                    dma_boot = 0;
                };
            };
            ros = {};
            system_mgmt = {};
            si_cl0 = {
                -- Unprogrammed model vector; firmware entry is a profile input.
                entry = 0;
                si_gic_trace = false; si_gic_trace_limit = 256;
                si_cmn_trace = false; si_cmn_trace_limit = 512;
            };
            si_cl1 = {
                entry = 0;
                mhu_trace = false; mhu_trace_limit = 4096;
            };
        };
    }
end

function config.validate(ctx)
    local options = ctx.options
    assert(type(options.AP_NUM_CPUS) == "number" and
           options.AP_NUM_CPUS % 1 == 0 and
           ((options.enable_ap_cpus and options.AP_NUM_CPUS >= 1 and
             options.AP_NUM_CPUS <= 16) or
            (not options.enable_ap_cpus and options.AP_NUM_CPUS == 0)),
           "Apollo AP CPU count must be an integer in 1..16, or 0 when disabled")
    assert(options.AP_GIC_NUM_CPUS ==
           (options.enable_ap_cpus and options.AP_NUM_CPUS or 1),
           "Apollo GIC CPU count must match enabled AP CPUs")
    assert(options.rse_flash_backend == "systemc-strata" or
           options.rse_flash_backend == "qemu-cfi-local",
           "unsupported RSE flash backend")
    assert(options.rse_flash_backend ~= "qemu-cfi-local" or
           options.rse_local_boot_flash,
           "qemu-cfi-local requires RSE local boot flash")
    assert(options.smmu_backend == "systemc-mmu720ae" or
           options.smmu_backend == "qemu-arm-smmuv3", "unsupported SMMU backend")
    assert(options.cc3xx_backend == "systemc" or
           options.cc3xx_backend == "qemu-native", "unsupported CC3XX backend")
end

return config
