local module = {}

function module.create(ctx, hardware, _runtime, interrupts)
    local SI_CONST = hardware
    local irq_routes = interrupts

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl0_ssu == nil,
            "duplicate object: platform.si_cl0_ssu")
        platform.si_cl0_ssu = {
            moduletype = "zena_ssu";
            target_socket = {
                address = SI_CONST.SI_CL0_SSU_BASE;
                size = SI_CONST.SI_CL0_SSU_SIZE;
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
            bank_count = SI_CONST.SI_CL0_FMU_BANK_COUNT;
            record_count = SI_CONST.SI_CL0_FMU_RECORD_COUNT;
            fault_input_enabled = true;
            fault_input_record = 0;
            fault_source = "si_cl0_ni710ae_primary_nci.apu_fault";
            fault_id = "ni710ae-apu-permission-denied";
            fault_sink = "si_cl0_fmu.record0";
            target_socket = {
                address = SI_CONST.SI_CL0_FMU_BASE;
                size = SI_CONST.SI_CL0_FMU_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            critical_irq = {
                bind = irq_routes.spi_target(
                    ctx, "si_cl0_fmu_critical", "View1");
            };
            non_critical_irq = {
                bind = irq_routes.spi_target(
                    ctx, "si_cl0_fmu_noncritical", "View1");
            };
            critical_ssu = {bind = "&si_cl0_ssu.critical_in"};
            non_critical_ssu = {bind = "&si_cl0_ssu.non_critical_in"};
            log_level = 0;
        }

        local device_fmus = {
            {name = "gic"; base = SI_CONST.SI_CL0_GIC_FMU_BASE;
             critical_record = 204; non_critical_record = 203};
            {name = "rse_cl0_mhu"; base = SI_CONST.SI_CL0_RSE_CL0_MHU_FMU_BASE;
             critical_record = 0; non_critical_record = 1};
            {name = "cl0_rse_mhu"; base = SI_CONST.SI_CL0_CL0_RSE_MHU_FMU_BASE;
             critical_record = 2; non_critical_record = 3};
            {name = "pc0_cl0_mhu"; base = SI_CONST.SI_CL0_PC0_CL0_MHU_FMU_BASE;
             critical_record = 4; non_critical_record = 5};
            {name = "cl0_pc0_mhu"; base = SI_CONST.SI_CL0_CL0_PC0_MHU_FMU_BASE;
             critical_record = 6; non_critical_record = 7};
            {name = "pc1_cl0_mhu"; base = SI_CONST.SI_CL0_PC1_CL0_MHU_FMU_BASE;
             critical_record = 8; non_critical_record = 9};
            {name = "cl0_pc1_mhu"; base = SI_CONST.SI_CL0_CL0_PC1_MHU_FMU_BASE;
             critical_record = 10; non_critical_record = 11};
            {name = "pc2_cl0_mhu"; base = SI_CONST.SI_CL0_PC2_CL0_MHU_FMU_BASE;
             critical_record = 12; non_critical_record = 13};
            {name = "cl0_pc2_mhu"; base = SI_CONST.SI_CL0_CL0_PC2_MHU_FMU_BASE;
             critical_record = 14; non_critical_record = 15};
            {name = "pc3_cl0_mhu"; base = SI_CONST.SI_CL0_PC3_CL0_MHU_FMU_BASE;
             critical_record = 16; non_critical_record = 17};
            {name = "cl0_pc3_mhu"; base = SI_CONST.SI_CL0_CL0_PC3_MHU_FMU_BASE;
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
                    size = SI_CONST.SI_CL0_DEVICE_FMU_SIZE;
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
                address = SI_CONST.HOST_NI710AE_SYS_CTRL_PHYS_BASE;
                size = SI_CONST.HOST_NI710AE_WINDOW_SIZE;
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
                address = SI_CONST.HOST_NI710AE_SMD_PHYS_BASE;
                size = SI_CONST.HOST_NI710AE_WINDOW_SIZE;
                bind = "&smd_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
        for cluster = 0,(SI_CONST.SI_CL0_AP_CLUSTER_COUNT - 1) do
            local cluster_base = SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_BASE +
                (cluster * SI_CONST.SI_CL0_CLUSTER_UTILITY_STRIDE)
            local ras = {
                moduletype = "apollo_cpu_ras";
                cluster_irq = {bind = irq_routes.spi_target(
                    ctx, "si_cl0_ap_ras_cluster" .. cluster, "View1")};
                log_level = 0;
            }
            for core = 0,(SI_CONST.SI_CL0_AP_CORE_PER_CLUSTER_COUNT - 1) do
                ras["record_" .. core] = {
                    address = cluster_base + SI_CONST.SI_CL0_AP_CORE_RAS_OFFSET +
                        (core * SI_CONST.SI_CL0_AP_CORE_PPU_STRIDE);
                    size = SI_CONST.SI_CL0_AP_CORE_RAS_SIZE;
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

    return block
end

return module
