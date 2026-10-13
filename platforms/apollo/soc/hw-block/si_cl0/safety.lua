-- SPI constants are socket indices; PPI constants are architectural INTIDs.
local SI_CL0_FMU_CRITICAL_SPI = 128 - 32
local SI_CL0_FMU_NONCRITICAL_SPI = 129 - 32
local SI_CL0_AP_RAS_CLUSTER0_SPI = 325 - 32
local SI_CL0_AP_RAS_IRQ_STRIDE = 2

local SI_CL0_FMU_BANK_COUNT = 5
local SI_CL0_FMU_RECORD_COUNT = 384
local SI_CL0_CLUSTER_UTILITY_STRIDE = 0x04000000
local SI_CL0_AP_CLUSTER_COUNT = 4
local SI_CL0_AP_CORE_PER_CLUSTER_COUNT = 4
local SI_CL0_AP_CORE_PPU_STRIDE = 0x00100000
local SI_CL0_CLUSTER_UTILITY_PHYS_BASE = 0x140000000

local SI_CL0_AP_CLUSTER_MGI_PHYS_BASE = 0x140200000
local SI_CL0_SMCF_SMD_MGI_PHYS_BASE = 0x20000D8100000
local SI_CL0_ATW16_SMCF_SMD_MGI_SIZE = 0x00010000
local SI_CL0_CLUSTER_UTILITY_MGI_SIZE = 0x00010000
local SI_CL0_CLUSTER_UTILITY_MGI_STRIDE = 0x04000000
local SI_CL0_SSU_BASE = 0x2A500000
local SI_CL0_SSU_SIZE = 0x00001000
local SI_CL0_FMU_BASE = 0x2A510000
local SI_CL0_FMU_SIZE = 0x00050000
local SI_CL0_AP_CORE_RAS_OFFSET = 0x010A0000
local SI_CL0_AP_CORE_RAS_SIZE = 0x00000040
local HOST_NI710AE_SYS_CTRL_PHYS_BASE = 0x20000D2400000
local HOST_NI710AE_SMD_PHYS_BASE = 0x20000D2600000
local HOST_NI710AE_WINDOW_SIZE = 0x00100000
local SI_CL0_GIC_FMU_BASE = 0x32020000
local SI_CL0_RSE_CL0_MHU_FMU_BASE = 0x321D0000
local SI_CL0_CL0_RSE_MHU_FMU_BASE = 0x321C0000
local SI_CL0_PC0_CL0_MHU_FMU_BASE = 0x32110000
local SI_CL0_CL0_PC0_MHU_FMU_BASE = 0x32100000
local SI_CL0_PC1_CL0_MHU_FMU_BASE = 0x32130000
local SI_CL0_CL0_PC1_MHU_FMU_BASE = 0x32120000
local SI_CL0_PC2_CL0_MHU_FMU_BASE = 0x32310000
local SI_CL0_CL0_PC2_MHU_FMU_BASE = 0x32300000
local SI_CL0_PC3_CL0_MHU_FMU_BASE = 0x32330000
local SI_CL0_CL0_PC3_MHU_FMU_BASE = 0x32320000
local SI_CL0_DEVICE_FMU_SIZE = 0x00010000

local module = {}

function module.create(ctx, _runtime)


    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl0_ssu == nil,
            "duplicate object: platform.si_cl0_ssu")
        platform.si_cl0_ssu = {
            moduletype = "zena_ssu";
            target_socket = {
                address = SI_CL0_SSU_BASE;
                size = SI_CL0_SSU_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            safety_status = {bind = "&host_reset_ctrl.safety_fault_reset"};
            log_level = 0;
        }

        assert(
            platform.si_cl0_fmu == nil,
            "duplicate object: platform.si_cl0_fmu")
        platform.si_cl0_fmu = {
            moduletype = "zena_fmu";
            bank_count = SI_CL0_FMU_BANK_COUNT;
            record_count = SI_CL0_FMU_RECORD_COUNT;
            fault_input_enabled = true;
            fault_input_record = 0;
            fault_source = "si_cl0_ni710ae_primary_nci.apu_fault";
            fault_id = "ni710ae-apu-permission-denied";
            fault_sink = "si_cl0_fmu.record0";
            target_socket = {
                address = SI_CL0_FMU_BASE;
                size = SI_CL0_FMU_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            critical_irq = {
                bind = "&si_cl0_gic.spi_in_" .. SI_CL0_FMU_CRITICAL_SPI;
            };
            non_critical_irq = {
                bind = "&si_cl0_gic.spi_in_" .. SI_CL0_FMU_NONCRITICAL_SPI;
            };
            critical_ssu = {bind = "&si_cl0_ssu.critical_in"};
            non_critical_ssu = {bind = "&si_cl0_ssu.non_critical_in"};
            log_level = 0;
        }

        local device_fmus = {
            {name = "gic"; base = SI_CL0_GIC_FMU_BASE;
             critical_record = 204; non_critical_record = 203};
            {name = "rse_cl0_mhu"; base = SI_CL0_RSE_CL0_MHU_FMU_BASE;
             critical_record = 0; non_critical_record = 1};
            {name = "cl0_rse_mhu"; base = SI_CL0_CL0_RSE_MHU_FMU_BASE;
             critical_record = 2; non_critical_record = 3};
            {name = "pc0_cl0_mhu"; base = SI_CL0_PC0_CL0_MHU_FMU_BASE;
             critical_record = 4; non_critical_record = 5};
            {name = "cl0_pc0_mhu"; base = SI_CL0_CL0_PC0_MHU_FMU_BASE;
             critical_record = 6; non_critical_record = 7};
            {name = "pc1_cl0_mhu"; base = SI_CL0_PC1_CL0_MHU_FMU_BASE;
             critical_record = 8; non_critical_record = 9};
            {name = "cl0_pc1_mhu"; base = SI_CL0_CL0_PC1_MHU_FMU_BASE;
             critical_record = 10; non_critical_record = 11};
            {name = "pc2_cl0_mhu"; base = SI_CL0_PC2_CL0_MHU_FMU_BASE;
             critical_record = 12; non_critical_record = 13};
            {name = "cl0_pc2_mhu"; base = SI_CL0_CL0_PC2_MHU_FMU_BASE;
             critical_record = 14; non_critical_record = 15};
            {name = "pc3_cl0_mhu"; base = SI_CL0_PC3_CL0_MHU_FMU_BASE;
             critical_record = 16; non_critical_record = 17};
            {name = "cl0_pc3_mhu"; base = SI_CL0_CL0_PC3_MHU_FMU_BASE;
             critical_record = 18; non_critical_record = 19};
        }
        for _, fmu in ipairs(device_fmus) do
            assert(
                platform["si_cl0_" .. fmu.name .. "_fmu"] == nil,
                "duplicate object: platform." .. ("si_cl0_" .. fmu.name .. "_fmu"))
            platform["si_cl0_" .. fmu.name .. "_fmu"] = {
                moduletype = "zena_device_fmu";
                parent_bank = 4;
                parent_critical_record = fmu.critical_record;
                parent_non_critical_record = fmu.non_critical_record;
                target_socket = {
                    address = fmu.base;
                    size = SI_CL0_DEVICE_FMU_SIZE;
                    bind = "&si_cl0_router.initiator_socket";
                    priority = 0;
                };
                fault_socket = {bind = "&si_cl0_router.target_socket"};
                log_level = 0;
            }
        end
        assert(
            platform.si_cl0_ni710ae_sys_ctrl == nil,
            "duplicate object: platform.si_cl0_ni710ae_sys_ctrl")
        platform.si_cl0_ni710ae_sys_ctrl = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0x14;
            parent_critical_record = 68;
            parent_non_critical_record = 66;
            target_socket = {
                address = HOST_NI710AE_SYS_CTRL_PHYS_BASE;
                size = HOST_NI710AE_WINDOW_SIZE;
                bind = "&smd_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.si_cl0_ni710ae_smd == nil,
            "duplicate object: platform.si_cl0_ni710ae_smd")
        platform.si_cl0_ni710ae_smd = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0x17;
            parent_critical_record = 167;
            parent_non_critical_record = 165;
            target_socket = {
                address = HOST_NI710AE_SMD_PHYS_BASE;
                size = HOST_NI710AE_WINDOW_SIZE;
                bind = "&smd_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
        for cluster = 0,(SI_CL0_AP_CLUSTER_COUNT - 1) do
            local cluster_base = SI_CL0_CLUSTER_UTILITY_PHYS_BASE +
                (cluster * SI_CL0_CLUSTER_UTILITY_STRIDE)
            local ras = {
                moduletype = "apollo_cpu_ras";
                cluster_irq = {bind = "&si_cl0_gic.spi_in_" ..
                    (SI_CL0_AP_RAS_CLUSTER0_SPI + cluster * SI_CL0_AP_RAS_IRQ_STRIDE)};
                log_level = 0;
            }
            for core = 0,(SI_CL0_AP_CORE_PER_CLUSTER_COUNT - 1) do
                ras["record_" .. core] = {
                    address = cluster_base + SI_CL0_AP_CORE_RAS_OFFSET +
                        (core * SI_CL0_AP_CORE_PPU_STRIDE);
                    size = SI_CL0_AP_CORE_RAS_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                }
            end
            assert(
                platform["si_cl0_ap_ras_cluster" .. cluster] == nil,
                "duplicate object: platform." .. ("si_cl0_ap_ras_cluster" .. cluster))
            platform["si_cl0_ap_ras_cluster" .. cluster] = ras
        end
    end

    function block.define_monitors(ctx, platform)
        assert(
            platform.si_cl0_smcf_smd_mgi == nil,
            "duplicate object: platform.si_cl0_smcf_smd_mgi")
        platform.si_cl0_smcf_smd_mgi = {
            moduletype = "host_smcf_mgi";
            group_id = 0x00000000;
            monitor_count = 1;
            data_values_per_monitor = 12;
            data_width_bits = 32;
            target_socket = {
                address = SI_CL0_SMCF_SMD_MGI_PHYS_BASE;
                size = SI_CL0_ATW16_SMCF_SMD_MGI_SIZE;
                bind = "&smd_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
        for i = 0,3 do
            assert(
                platform["si_cl0_smcf_ap_cluster_mgi_" .. i] == nil,
                "duplicate object: platform." .. ("si_cl0_smcf_ap_cluster_mgi_" .. i))
            platform["si_cl0_smcf_ap_cluster_mgi_" .. i] = {
                moduletype = "host_smcf_mgi";
                group_id = i + 1;
                monitor_count = 1;
                data_values_per_monitor = 12;
                data_width_bits = 32;
                target_socket = {
                    address = SI_CL0_AP_CLUSTER_MGI_PHYS_BASE +
                        (i * SI_CL0_CLUSTER_UTILITY_MGI_STRIDE);
                    size = SI_CL0_CLUSTER_UTILITY_MGI_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                };
                log_level = 0;
            }
        end

    end

    return block
end

return module
