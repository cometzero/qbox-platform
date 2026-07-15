return {
    schema_version = 1;
    initiators = {
        { name = "ap_cpu"; domain = "ap"; security = "from_qemu_memtxattrs"; requester = "cpu_index"; access_kinds = { "b_transport"; "transport_dbg"; "dmi" } };
        { name = "ap_loader"; domain = "ap"; security = "trusted_secure"; capability = "boot_loader"; access_kinds = { "transport_dbg" } };
        { name = "gpex_dma"; domain = "ap"; security = "non_secure"; requester = 0; stream_id = 0; access_kinds = { "b_transport"; "dmi" } };
        { name = "rse_cpu"; domain = "rse"; security = "secure"; requester = 0; access_kinds = { "b_transport"; "transport_dbg"; "dmi" } };
        { name = "rse_loader"; domain = "rse"; security = "trusted_secure"; capability = "authenticated_image_loader"; access_kinds = { "transport_dbg" } };
        { name = "si_cl0_cpu"; domain = "si_cl0"; security = "secure"; requester = 0; access_kinds = { "b_transport"; "transport_dbg"; "dmi" } };
        { name = "si_cl0_loader"; domain = "si_cl0"; security = "trusted_secure"; capability = "rse_authenticated_loader"; access_kinds = { "transport_dbg" } };
        { name = "si_cl1_cpu"; domain = "si_cl1"; security = "secure"; requester = "cpu_index"; access_kinds = { "b_transport"; "transport_dbg"; "dmi" }; scope = "fvp_cfg2_extension" };
        { name = "si_cl1_loader"; domain = "si_cl1"; security = "trusted_secure"; capability = "rse_authenticated_loader"; access_kinds = { "transport_dbg" }; scope = "fvp_cfg2_extension" };
    };
    routes = {
        { name = "ap_cpu_local"; initiator = "ap_cpu"; from = "ap"; to = "ap"; target = "ap_router"; response = "tlm_to_memtx" };
        { name = "pcie_dma_to_smmu"; initiator = "gpex_dma"; from = "ap"; to = "ap"; target = "ap_smmu_0"; path = { "ap_router"; "ap_smmu_0"; "ap_router" }; requester_required = true; stream_id_required = true; fault_signal = "ap_smmu_event" };
        { name = "ap_to_smd"; initiator = "ap_cpu"; from = "ap"; to = "smd"; target = "smd_router"; bridge = "ap_to_smd_atu_apu"; reset_access = "deny"; programmed_access = "allow_list" };
        { name = "rse_to_system"; initiator = "rse_cpu"; from = "rse"; to = "system"; target = "system_router"; bridge = "rse_to_system_atu_apu"; reset_access = "allow"; programmed_access = "allow_list" };
        { name = "si_cl0_to_system"; initiator = "si_cl0_cpu"; from = "si_cl0"; to = "system"; target = "system_router"; bridge = "si_cl0_to_system_atu_apu"; reset_access = "deny"; programmed_access = "allow_list" };
        { name = "si_cl0_to_si_cl1_scmi"; initiator = "si_cl0_cpu"; from = "si_cl0"; to = "si_cl1"; target = "si_cl1_scmi_shmem"; bridge = "si_cl0_to_si_cl1_scmi_apu"; reset_access = "allow"; programmed_access = "allow_list"; scope = "fvp_cfg2_extension" };
        { name = "si_cl0_to_hipc"; initiator = "si_cl0_cpu"; from = "si_cl0"; to = "ap"; target = "host_ap_bl2_header_sram"; bridge = "si_cl0_to_ap_hipc"; reset_access = "deny"; programmed_access = "allow_list"; scope = "fvp_cfg2_extension" };
        { name = "si_cl1_to_system"; initiator = "si_cl1_cpu"; from = "si_cl1"; to = "system"; target = "system_router"; bridge = "si_cl1_to_system_atu_apu"; reset_access = "deny"; programmed_access = "allow_list"; scope = "fvp_cfg2_extension" };
        { name = "si_cl1_to_hipc"; initiator = "si_cl1_cpu"; from = "si_cl1"; to = "ap"; target = "host_ap_bl2_header_sram"; bridge = "si_cl1_to_ap_hipc"; reset_access = "deny"; programmed_access = "allow_list"; scope = "fvp_cfg2_extension" };
    };
    policies = {
        { access_kind = "b_transport"; identity_required = true; policy = "normal" };
        { access_kind = "transport_dbg"; identity_required = true; policy = "trusted_capability_only" };
        { access_kind = "dmi"; identity_required = true; policy = "deny_on_policy_or_translation_window"; invalidate_on = { "atu_reprogram"; "apu_reprogram"; "reset"; "backing_remap" } };
        { access_kind = "qemu_direct"; identity_required = true; policy = "same_as_b_transport" };
        { access_kind = "qemu_reentrant"; identity_required = true; policy = "same_as_b_transport" };
    };
    responses = {
        { tlm = "TLM_OK_RESPONSE"; memtx = "MemTxOK"; guest = "success"; side_effect = true };
        { tlm = "TLM_ADDRESS_ERROR_RESPONSE"; memtx = "MemTxDecodeError"; guest = "data_or_prefetch_abort"; side_effect = false };
        { tlm = "TLM_GENERIC_ERROR_RESPONSE"; memtx = "MemTxError"; guest = "external_abort"; side_effect = false };
    };
}
