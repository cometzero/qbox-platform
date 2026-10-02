-- SPDX-License-Identifier: BSD-3-Clause
-- Host execution policy, evaluated once before hardware construction.
return function(options, apollo_dir)
    local AP_MAX_CPUS = 16
    options.qemu_args = options.getenv_or("QBOX_RDASPEN_RSE_QEMU_ARGS", "")
    options.ap_qemu_args = options.getenv_or("QBOX_RDASPEN_AP_QEMU_ARGS", "")
    options.ap_pc_trace = options.getenv_or("QBOX_RDASPEN_AP_PC_TRACE", "false") == "true"
    options.ap_pc_trace_file = options.getenv_or(
        "QBOX_RDASPEN_AP_PC_TRACE_FILE",
        options.root.."build/qbox-fvp-rd-aspen/ap-pc-trace.log")
    options.ap_pc_trace_interval = tonumber(options.getenv_or("QBOX_RDASPEN_AP_PC_TRACE_INTERVAL", "1"))
    options.ap_pc_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_AP_PC_TRACE_LIMIT", "4096"))
    options.ap_exception_trace = options.getenv_or("QBOX_RDASPEN_AP_EXCEPTION_TRACE", "false") == "true"
    options.enable_ap_cpus = options.getenv_or("QBOX_RDASPEN_ENABLE_AP_CPUS", "false") == "true"
    options.AP_NUM_CPUS = options.enable_ap_cpus and
        options.getenv_number_or("QBOX_APOLLO_NUM_CPUS", "4") or 0
    assert(not options.enable_ap_cpus or
           (options.AP_NUM_CPUS >= 1 and options.AP_NUM_CPUS <= AP_MAX_CPUS and
            options.AP_NUM_CPUS % 1 == 0),
           "QBOX_APOLLO_NUM_CPUS must be an integer in 1..16 when AP CPUs are enabled")
    options.AP_GIC_NUM_CPUS = options.enable_ap_cpus and options.AP_NUM_CPUS or 1
    options.rse_pc_trace = options.getenv_or("QBOX_RDASPEN_RSE_PC_TRACE", "false") == "true"
    options.rse_pc_trace_file = options.getenv_or(
        "QBOX_RDASPEN_RSE_PC_TRACE_FILE",
        options.root.."build/qbox-fvp-rd-aspen/rse-pc-trace.log")
    options.rse_pc_trace_interval = tonumber(options.getenv_or("QBOX_RDASPEN_RSE_PC_TRACE_INTERVAL", "1"))
    options.rse_pc_trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_RSE_PC_TRACE_LIMIT", "4096"))
    options.rse_exception_trace = options.getenv_or("QBOX_RDASPEN_RSE_EXCEPTION_TRACE", "false") == "true"
    options.rse_hotpath_accel = options.getenv_or("QBOX_RDASPEN_RSE_HOTPATH_ACCEL", "false") == "true"
    options.rse_hotpath_max_bytes = tonumber(options.getenv_or("QBOX_RDASPEN_RSE_HOTPATH_MAX_BYTES", tostring(16 * 1024 * 1024)))
    options.rse_hotpath_profile_file = options.getenv_or("QBOX_RDASPEN_RSE_HOTPATH_PROFILE_FILE", "")
    options.rse_hotpath_profile_interval = tonumber(options.getenv_or("QBOX_RDASPEN_RSE_HOTPATH_PROFILE_INTERVAL", "1024"))
    options.rse_lms_accel = options.getenv_or("QBOX_RDASPEN_RSE_LMS_ACCEL", "false") == "true"
    options.rse_lms_max_data_bytes = tonumber(options.getenv_or("QBOX_RDASPEN_RSE_LMS_MAX_DATA_BYTES", tostring(16 * 1024 * 1024)))
    options.rse_bl2_load_profile =
        options.getenv_or("QBOX_RDASPEN_RSE_BL2_LOAD_PROFILE", "false") == "true"
    options.rse_bl2_boot_image_count =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_IMAGE_COUNT", "5"))
    options.rse_bl2_boot_state_curr_img_offset =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_CURR_IMG_OFFSET", "0x10c8"))
    options.rse_bl2_boot_state_imgs_offset =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_IMGS_OFFSET", "0x0"))
    options.rse_bl2_boot_state_image_stride =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_IMAGE_STRIDE", "88"))
    options.rse_bl2_boot_state_slot_stride =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_SLOT_STRIDE", "44"))
    options.rse_bl2_boot_state_slot_usage_offset =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_SLOT_USAGE_OFFSET", "0x10d0"))
    options.rse_bl2_boot_state_slot_usage_stride =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_SLOT_USAGE_STRIDE", "16"))
    options.rse_bl2_boot_slot_usage_img_dst_offset =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_SLOT_USAGE_IMG_DST_OFFSET", "8"))
    options.rse_bl2_boot_slot_usage_img_sz_offset =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_SLOT_USAGE_IMG_SZ_OFFSET", "12"))
    options.rse_bl2_load_accel =
        options.getenv_or("QBOX_RDASPEN_RSE_BL2_LOAD_ACCEL", "false") == "true"
    options.rse_bl2_load_accel_max_bytes =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_LOAD_ACCEL_MAX_BYTES", tostring(16 * 1024 * 1024)))
    options.rse_bl2_boot_enc_accel =
        options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_ACCEL", "false") == "true"
    options.rse_bl2_img_hash_accel =
        options.getenv_or("QBOX_RDASPEN_RSE_BL2_IMG_HASH_ACCEL", "false") == "true"
    options.rse_bl2_img_hash_max_bytes =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_IMG_HASH_MAX_BYTES", tostring(16 * 1024 * 1024)))
    options.rse_bl2_img_hash_max_seed_bytes =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_IMG_HASH_MAX_SEED_BYTES", "4096"))
    options.rse_bl2_verify_sig_accel =
        options.getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_ACCEL", "false") == "true"
    options.rse_bl2_verify_sig_skip =
        options.getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_SKIP", "false") == "true"
    options.rse_bl2_verify_sig_max_key_bytes =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_MAX_KEY_BYTES", "512"))
    options.rse_bl2_verify_sig_max_sig_bytes =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_MAX_SIG_BYTES", "128"))
    options.rse_bl2_delay_accel =
        options.getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_ACCEL", "false") == "true"
    options.rse_bl2_delay_max_cycles =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_MAX_CYCLES", "50000000"))
    options.rse_bl2_delay_expected_hits =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_EXPECTED_HITS", "2"))
    options.rse_bl2_boot_status_enckey_offset =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATUS_ENCKEY_OFFSET", "0x0c"))
    options.rse_bl2_boot_enc_key_bytes =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_KEY_BYTES", "16"))
    options.rse_bl2_boot_enc_key_stride =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_KEY_STRIDE", "16"))
    options.rse_bl2_boot_enc_slots =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_SLOTS", "2"))
    options.rse_bl2_boot_enc_max_bytes =
        tonumber(options.getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_MAX_BYTES", "4096"))
end
