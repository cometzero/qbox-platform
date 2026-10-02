local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local SI_CONST = hardware
    local si_cmn_trace = runtime.si_cmn_trace
    local si_cmn_trace_limit = runtime.si_cmn_trace_limit

    local block = {}

    function block.define_router(ctx, platform)
        assert(
            platform.si_cl0_router == nil,
            "duplicate object: platform.si_cl0_router")
        platform.si_cl0_router = {
            moduletype = "router";
            log_level = 0;
        }
    end

    function block.connect(ctx, platform)
        platform.host_si_atu.translation_socket = {
            address = SI_CONST.SI_CL0_ATU_LOGICAL_BASE;
            size = SI_CONST.SI_CL0_ATU_LOGICAL_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            relative_addresses = false;
        }
        platform.host_si_atu.initiator_socket = {
            bind = "&system_router.target_socket";
        }
        platform.host_si_atu.enable_dmi = false

        platform.host_smdexp2smd_atu.translation_socket = {
            address = SI_CONST.SI_CL0_SMDEXP_ATU_LOGICAL_BASE;
            size = SI_CONST.SI_CL0_SMDEXP_ATU_LOGICAL_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            relative_addresses = false;
        }
        platform.host_smdexp2smd_atu.initiator_socket = {
            bind = "&system_router.target_socket";
        }
        platform.host_smdexp2smd_atu.enable_dmi = false

        assert(
            platform.system_to_ap_shared_bridge == nil,
            "duplicate object: platform.system_to_ap_shared_bridge")
        platform.system_to_ap_shared_bridge = {
            moduletype = "addrtr";
            mapped_base_addr = SI_CONST.SYSTEM_AP_SHARED_BRIDGE_BASE;
            target_socket = {
                address = SI_CONST.SYSTEM_AP_SHARED_BRIDGE_BASE;
                size = SI_CONST.SYSTEM_AP_SHARED_BRIDGE_SIZE;
                bind = "&system_router.initiator_socket";
                relative_addresses = false;
            };
            initiator_socket = {bind = "&ap_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.system_to_ap_gic_bridge == nil,
            "duplicate object: platform.system_to_ap_gic_bridge")
        platform.system_to_ap_gic_bridge = {
            moduletype = "addrtr";
            mapped_base_addr = SI_CONST.SYSTEM_AP_GIC_BRIDGE_BASE;
            target_socket = {
                address = SI_CONST.SYSTEM_AP_GIC_BRIDGE_BASE;
                size = SI_CONST.SYSTEM_AP_GIC_BRIDGE_SIZE;
                bind = "&system_router.initiator_socket";
                relative_addresses = false;
            };
            initiator_socket = {bind = "&ap_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.si_cl0_rse_shared_bridge == nil,
            "duplicate object: platform.si_cl0_rse_shared_bridge")
        platform.si_cl0_rse_shared_bridge = {
            moduletype = "addrtr";
            mapped_base_addr = SI_CONST.HOST_RSE_SI_SSRAM_PHYS_BASE;
            target_socket = {
                address = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
                size = SI_CONST.HOST_RSE_SI_SSRAM_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                relative_addresses = false;
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.si_cl0_to_si_cl1_scmi_bridge == nil,
            "duplicate object: platform.si_cl0_to_si_cl1_scmi_bridge")
        platform.si_cl0_to_si_cl1_scmi_bridge = {
            moduletype = "addrtr";
            mapped_base_addr = SI_CONST.SI_CL1_SCMI_BRIDGE_BASE;
            target_socket = {
                address = SI_CONST.SI_CL1_SCMI_BRIDGE_BASE;
                size = SI_CONST.SI_CL1_SCMI_BRIDGE_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                relative_addresses = false;
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            log_level = 0;
        }
    end

    function block.define_cmn(ctx, platform)
        assert(
            platform.si_cl0_cmn_cyprus == nil,
            "duplicate object: platform.si_cl0_cmn_cyprus")
        platform.si_cl0_cmn_cyprus = {
            moduletype = "host_cmn_cyprus";
            revision = 3;
            trace = si_cmn_trace;
            trace_limit = si_cmn_trace_limit;
            target_socket = {
                address = SI_CONST.SI_CL0_CMN_PHYS_BASE;
                size = SI_CONST.SI_CL0_ATW0_CMN_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
    end

    function block.define_windows(ctx, platform)
        -- NI-710AE register windows
        assert(
            platform.si_cl0_ni710ae_primary_nci == nil,
            "duplicate object: platform.si_cl0_ni710ae_primary_nci")
        platform.si_cl0_ni710ae_primary_nci = {
            moduletype = "host_ni710ae_nci";
            topology = 4;
            protected_apu_index = 0;
            reset_owner_domain_id = ctx.request_context.domain.si_cl0;
            allow_trusted_loader = true;
            target_socket = {
                address = SI_CONST.SI_CL0_NI710AE_PRIMARY_NCI_BASE;
                size = SI_CONST.SI_CL0_NI710AE_NCI_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            initiator_socket = {bind = "&si_cl0_router.target_socket"};
            apu_fault = {bind = "&si_cl0_fmu.fault_in"};
            log_level = 0;
        }

        assert(
            platform.si_cl0_ni710ae_secondary_nci == nil,
            "duplicate object: platform.si_cl0_ni710ae_secondary_nci")
        platform.si_cl0_ni710ae_secondary_nci = {
            moduletype = "host_ni710ae_nci";
            topology = 2;
            target_socket = {
                address = SI_CONST.SI_CL0_NI710AE_SECONDARY_NCI_BASE;
                size = SI_CONST.SI_CL0_NI710AE_NCI_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }

        assert(
            platform.si_cl0_ni710ae_mhu_nci == nil,
            "duplicate object: platform.si_cl0_ni710ae_mhu_nci")
        platform.si_cl0_ni710ae_mhu_nci = {
            moduletype = "host_ni710ae_nci";
            topology = 1;
            target_socket = {
                address = SI_CONST.SI_CL0_NI710AE_MHU_NCI_BASE;
                size = SI_CONST.SI_CL0_NI710AE_NCI_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }

        -- SMD expansion and translated system windows
        assert(
            platform.si_cl0_smd_expansion_window == nil,
            "duplicate object: platform.si_cl0_smd_expansion_window")
        platform.si_cl0_smd_expansion_window = {
            moduletype = "gs_memory";
            dmi = false;
            target_socket = {
                address = SI_CONST.SI_CL0_SMD_EXPANSION_PHYS_BASE;
                size = SI_CONST.SI_CL0_ATW2_SMD_EXPANSION_SIZE;
                bind = "&smd_router.initiator_socket";
                priority = 10;
            };
            log_level = 0;
        }
        assert(
            platform.si_cl0_cluster_utility_window == nil,
            "duplicate object: platform.si_cl0_cluster_utility_window")
        platform.si_cl0_cluster_utility_window = {
            moduletype = "gs_memory";
            dmi = false;
            target_socket = {
                address = SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_BASE;
                size = SI_CONST.SI_CL0_CLUSTER_UTILITY_PHYS_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 20;
            };
            init_mem = true;
            log_level = 0;
        }
    end

    return block
end

return module
