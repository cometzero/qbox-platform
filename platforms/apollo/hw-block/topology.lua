return {
    schema_version = 1;
    machine = "apollo-qvp";
    variant = "cfg2";
    scopes = {
        { name = "zena_css_architecture"; source = "doc/arm_zena_css_dev_guide/02-block-diagram-for-zena-css.md" };
        { name = "rd_aspen_cfg2"; source = "arm-zena-css/documentation/images/aspen_high_level_arch.png" };
        { name = "fvp_cfg2_extension"; source = "doc/arm_zena_css_dev_guide/08-fixed-virtual-platform.md" };
        { name = "qvp_functional_abstraction"; source = "doc/apollo-qvp-machine-architecture-ko.md" };
    };
    domains = {
        { name = "system"; owner = "rse"; scope = "zena_css_architecture" };
        { name = "ap"; owner = "ap"; scope = "zena_css_architecture" };
        { name = "smd"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "rse"; owner = "rse"; scope = "zena_css_architecture" };
        { name = "si_cl0"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "si_cl1"; owner = "si_cl1"; scope = "fvp_cfg2_extension" };
    };
    views = {
        { name = "system"; domain = "system"; width = 52; scope = "zena_css_architecture" };
        { name = "ap"; domain = "ap"; width = 52; scope = "zena_css_architecture" };
        { name = "smd"; domain = "smd"; width = 52; scope = "zena_css_architecture" };
        { name = "rse"; domain = "rse"; width = 32; scope = "zena_css_architecture" };
        { name = "si_cl0"; domain = "si_cl0"; width = 40; scope = "zena_css_architecture" };
        { name = "si_cl1"; domain = "si_cl1"; width = 40; scope = "fvp_cfg2_extension" };
    };
    routers = {
        { name = "system_router"; view = "system"; cci_path = "platform.system_router"; scope = "zena_css_architecture" };
        { name = "ap_router"; view = "ap"; cci_path = "platform.ap_router"; scope = "zena_css_architecture" };
        { name = "smd_router"; view = "smd"; cci_path = "platform.smd_router"; scope = "zena_css_architecture" };
        { name = "rse_router"; view = "rse"; cci_path = "platform.rse_router"; scope = "zena_css_architecture" };
        { name = "si_cl0_router"; view = "si_cl0"; cci_path = "platform.si_cl0_router"; scope = "zena_css_architecture" };
        { name = "si_cl1_router"; view = "si_cl1"; cci_path = "platform.si_cl1_router"; scope = "fvp_cfg2_extension" };
    };
    bridges = {
        {
            name = "ap_to_smd_atu_apu"; from = "ap"; to = "smd";
            kind = "atu_apu"; owner = "rse"; width = 52;
            reset_policy = "rse_only"; scope = "zena_css_architecture";
        };
        {
            name = "system_to_smd_nci"; from = "system"; to = "smd";
            kind = "nci_decode"; owner = "rse"; width = 52;
            reset_policy = "static_high_nibble"; scope = "zena_css_architecture";
        };
        {
            name = "rse_to_system_atu_apu"; from = "rse"; to = "system";
            kind = "atu_apu"; owner = "rse"; width = 32;
            reset_policy = "rse_only"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_to_system_atu_apu"; from = "si_cl0"; to = "system";
            kind = "atu_apu"; owner = "rse"; width = 40;
            reset_policy = "rse_only"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_ni710ae_primary_nci"; from = "si_cl0"; to = "si_cl0";
            kind = "apu"; owner = "si_cl0"; width = 40;
            reset_policy = "owner_only"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_to_rse_shared"; from = "si_cl0"; to = "system";
            kind = "static_window"; owner = "rse"; width = 40;
            reset_policy = "static_allow_list"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_to_si_cl1_scmi_apu"; from = "si_cl0"; to = "si_cl1";
            kind = "apu"; owner = "si_cl0"; width = 40;
            reset_policy = "static_allow_list"; scope = "fvp_cfg2_extension";
        };
        {
            name = "si_cl0_to_ap_hipc"; from = "si_cl0"; to = "ap";
            kind = "atu_apu"; owner = "rse"; width = 40;
            reset_policy = "deny_until_rse_programmed"; scope = "fvp_cfg2_extension";
        };
        {
            name = "si_cl1_to_ap_hipc"; from = "si_cl1"; to = "ap";
            kind = "static_window"; owner = "si_cl1"; width = 40;
            reset_policy = "static_allow_list"; scope = "fvp_cfg2_extension";
        };
    };
    qemu_instances = {
        {
            name = "ap_qemu_inst"; domain = "ap"; architecture = "AARCH64";
            cpu = "cortex-a720ae"; acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-freerunning"; ram_owner = "systemc";
        };
        {
            name = "si_cl0_qemu_inst"; domain = "si_cl0"; architecture = "AARCH64";
            cpu = "cortex-r82"; acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-quantum"; ram_owner = "systemc";
        };
        {
            name = "si_cl1_qemu_inst"; domain = "si_cl1"; architecture = "AARCH64";
            cpu = "cortex-r82"; acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-quantum"; ram_owner = "systemc";
            scope = "fvp_cfg2_extension";
        };
        {
            name = "rse_qemu_inst"; domain = "rse"; architecture = "ARM";
            cpu = "cortex-m55"; acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-freerunning"; ram_owner = "systemc";
        };
    };
    validation = {
        topology_frozen = true;
        required_socket_cardinality = 1;
        forbid_runtime_priority_mutation = true;
        forbid_broad_passthrough = true;
        migration_phase = "A4_policy_routing";
        compatibility_debt = {};
    };
}
