return {
    schema_version = 1;
    owners = {
        { resource = "image_authentication"; owner = "rse"; scope = "zena_css_architecture" };
        { resource = "system_atu_apu"; owner = "rse"; scope = "zena_css_architecture" };
        { resource = "si_atu_scr_readback"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { resource = "cmn_gic_peripheral_init"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { resource = "ap_primary_release"; owner = "rse"; scope = "zena_css_architecture" };
        { resource = "ap_secondary_release"; owner = "tfa"; scope = "zena_css_architecture" };
        { resource = "si_cl1_release"; owner = "rse"; scope = "fvp_cfg2_extension" };
    };
    sequence = {
        { id = "rse_bl1_start"; actor = "rse"; action = "start_secure_boot"; order = 10 };
        { id = "rse_auth_images"; actor = "rse"; action = "authenticate_and_measure"; after = "rse_bl1_start"; order = 20 };
        { id = "rse_program_atu_apu"; actor = "rse"; action = "program_lock_and_readback"; after = "rse_auth_images"; order = 30 };
        { id = "rse_release_si_cl0"; actor = "rse"; action = "release"; after = "rse_program_atu_apu"; order = 40 };
        { id = "si_cl0_verify_rse_config"; actor = "si_cl0"; action = "readback_atu_and_scr"; after = "rse_release_si_cl0"; failure = "hold_ap_in_reset"; order = 50 };
        { id = "si_cl0_init_system"; actor = "si_cl0"; action = "init_cmn_gic_peripherals"; after = "si_cl0_verify_rse_config"; order = 60 };
        { id = "rse_scp_boot_confirm"; actor = "rse"; action = "scmi_boot_confirmation"; after = "si_cl0_init_system"; order = 70 };
        { id = "rse_release_ap_primary"; actor = "rse"; action = "scmi_power_on"; after = "rse_scp_boot_confirm"; order = 80 };
        { id = "tfa_release_ap_secondary"; actor = "tfa"; action = "psci_scmi_power_on"; after = "rse_release_ap_primary"; order = 90 };
        { id = "rse_release_si_cl1"; actor = "rse"; action = "release_authenticated_image"; after = "rse_program_atu_apu"; order = 45; scope = "fvp_cfg2_extension" };
    };
    reset_defaults = {
        cross_domain_access = "rse_only";
        ap_primary = "reset_asserted";
        ap_secondary = "reset_asserted_powered_off";
        si_cl0 = "reset_asserted";
        si_cl1 = "reset_asserted";
        atu_apu = "default_deny_unlocked";
    };
}
