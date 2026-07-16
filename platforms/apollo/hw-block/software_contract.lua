return {
    schema_version = 1;
    actors = {
        "rse_bl1"; "rse_bl2"; "rse_runtime"; "scp_si0"; "tfa_bl2";
        "tfa_bl31"; "optee"; "uboot"; "linux"; "zephyr_si1"; "device_tree";
    };
    interfaces = {
        { name = "rse_scp_scmi"; protocol = "SCMI"; producer = "rse_runtime"; consumer = "scp_si0"; transport = "MHUv3"; purpose = "boot_confirmation_and_ap_primary_power"; scope = "zena_css_architecture" };
        { name = "tfa_scp_scmi"; protocol = "SCMI"; producer = "tfa_bl31"; consumer = "scp_si0"; transport = "MHUv3"; shmem_backing = "ap-mhu-ns"; reset_policy = "preserve_on_ap_reset"; purpose = "secondary_cpu_system_power_reset"; scope = "zena_css_architecture" };
        { name = "linux_psci"; protocol = "PSCI"; producer = "linux"; consumer = "tfa_bl31"; transport = "SMC"; purpose = "cpu_idle_frequency_power"; scope = "zena_css_architecture" };
        { name = "ap_pfdi"; protocol = "PFDI"; protocol_id = 0x90; producer = "tfa_bl31"; consumer = "scp_si0"; transport = "SCMI_MHUv3"; channel_count = 16; channel_stride = 40; error_policy = "bounded_timeout"; scope = "rd_aspen_cfg2" };
        { name = "si_cl1_pfdi"; protocol = "PFDI"; protocol_id = 0x90; producer = "zephyr_si1"; consumer = "scp_si0"; transport = "SCMI_MHUv3"; channel_base = 2; channel_stride = 40; error_policy = "bounded_timeout"; scope = "fvp_cfg2_extension" };
        { name = "hipc"; protocol = "RPMsg"; producer = "zephyr_si1"; consumer = "linux"; transport = "MHUv3_shared_memory"; remoteproc_state = "detached"; scope = "fvp_cfg2_extension" };
        { name = "ffa"; protocol = "FF-A"; producer = "optee"; consumer = "linux"; transport = "SMC_shared_memory"; error_policy = "deny_invalid_descriptor"; scope = "rd_aspen_cfg2" };
        { name = "boot_dtb"; protocol = "FDT"; producer = "tfa_bl2"; consumer = "uboot"; secondary_consumer = "linux"; source = "fip"; systemready_version = "3.1"; scope = "rd_aspen_cfg2" };
        { name = "ras_ffh"; protocol = "FFH"; producer = "tfa_bl31"; consumer = "linux"; transport = "SPI_89"; error_policy = "classify_corrected_deferred_uncorrected"; scope = "zena_css_architecture" };
    };
    shared_memory = {
        {
            name = "si_cl1_hipc"; base = 0xE0130000; size = 0x00080000;
            backing = "ap-si-cl1-hipc"; producer = "zephyr_si1"; consumer = "linux";
            remoteproc_state = "detached"; scope = "fvp_cfg2_extension";
            regions = {
                { name = "resource_table"; offset = 0x00000000; size = 0x00020000 };
                { name = "vring0"; offset = 0x00020000; size = 0x00020000 };
                { name = "vring1"; offset = 0x00040000; size = 0x00020000 };
                { name = "virtio_buffer"; offset = 0x00060000; size = 0x00020000 };
            };
        };
    };
    required_error_paths = {
        "timeout"; "malformed_descriptor"; "duplicate_notification";
        "request_during_reset"; "peer_offline";
    };
}
