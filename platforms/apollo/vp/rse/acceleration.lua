-- Firmware symbol addresses and optional host acceleration policy, not SoC registers.
local module = {}

function module.create(ctx)
    local getenv_number_or = ctx.options.getenv_number_or
    local getenv_or = ctx.options.getenv_or
    local rse_bl2_boot_enc_accel = ctx.options.rse_bl2_boot_enc_accel
    local rse_bl2_boot_enc_key_bytes = ctx.options.rse_bl2_boot_enc_key_bytes
    local rse_bl2_boot_enc_key_stride = ctx.options.rse_bl2_boot_enc_key_stride
    local rse_bl2_boot_enc_max_bytes = ctx.options.rse_bl2_boot_enc_max_bytes
    local rse_bl2_boot_enc_slots = ctx.options.rse_bl2_boot_enc_slots
    local rse_bl2_boot_image_count = ctx.options.rse_bl2_boot_image_count
    local rse_bl2_boot_slot_usage_img_dst_offset = ctx.options.rse_bl2_boot_slot_usage_img_dst_offset
    local rse_bl2_boot_slot_usage_img_sz_offset = ctx.options.rse_bl2_boot_slot_usage_img_sz_offset
    local rse_bl2_boot_state_curr_img_offset = ctx.options.rse_bl2_boot_state_curr_img_offset
    local rse_bl2_boot_state_image_stride = ctx.options.rse_bl2_boot_state_image_stride
    local rse_bl2_boot_state_imgs_offset = ctx.options.rse_bl2_boot_state_imgs_offset
    local rse_bl2_boot_state_slot_stride = ctx.options.rse_bl2_boot_state_slot_stride
    local rse_bl2_boot_state_slot_usage_offset = ctx.options.rse_bl2_boot_state_slot_usage_offset
    local rse_bl2_boot_state_slot_usage_stride = ctx.options.rse_bl2_boot_state_slot_usage_stride
    local rse_bl2_boot_status_enckey_offset = ctx.options.rse_bl2_boot_status_enckey_offset
    local rse_bl2_delay_accel = ctx.options.rse_bl2_delay_accel
    local rse_bl2_delay_expected_hits = ctx.options.rse_bl2_delay_expected_hits
    local rse_bl2_delay_max_cycles = ctx.options.rse_bl2_delay_max_cycles
    local rse_bl2_img_hash_accel = ctx.options.rse_bl2_img_hash_accel
    local rse_bl2_img_hash_max_bytes = ctx.options.rse_bl2_img_hash_max_bytes
    local rse_bl2_img_hash_max_seed_bytes = ctx.options.rse_bl2_img_hash_max_seed_bytes
    local rse_bl2_load_accel = ctx.options.rse_bl2_load_accel
    local rse_bl2_load_accel_max_bytes = ctx.options.rse_bl2_load_accel_max_bytes
    local rse_bl2_load_profile = ctx.options.rse_bl2_load_profile
    local rse_bl2_verify_sig_accel = ctx.options.rse_bl2_verify_sig_accel
    local rse_bl2_verify_sig_max_key_bytes = ctx.options.rse_bl2_verify_sig_max_key_bytes
    local rse_bl2_verify_sig_max_sig_bytes = ctx.options.rse_bl2_verify_sig_max_sig_bytes
    local rse_bl2_verify_sig_skip = ctx.options.rse_bl2_verify_sig_skip
    local rse_exception_trace = ctx.options.rse_exception_trace
    local rse_hotpath_accel = ctx.options.rse_hotpath_accel
    local rse_hotpath_max_bytes = ctx.options.rse_hotpath_max_bytes
    local rse_hotpath_profile_file = ctx.options.rse_hotpath_profile_file
    local rse_hotpath_profile_interval = ctx.options.rse_hotpath_profile_interval
    local rse_lms_accel = ctx.options.rse_lms_accel
    local rse_lms_max_data_bytes = ctx.options.rse_lms_max_data_bytes
    local rse_pc_trace = ctx.options.rse_pc_trace
    local rse_pc_trace_file = ctx.options.rse_pc_trace_file
    local rse_pc_trace_interval = ctx.options.rse_pc_trace_interval
    local rse_pc_trace_limit = ctx.options.rse_pc_trace_limit
    local RSE_RUNTIME_ADDRESS = {
        hotpath_memcpy = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_HOTPATH_MEMCPY_ADDR", "0x11000488"));
        hotpath_memset = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_HOTPATH_MEMSET_ADDR", "0x11000448"));
        lms_verify = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_LMS_VERIFY_ADDR", "0x11009bad"));
        bl2_boot_go_for_image_id = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOT_GO_FOR_IMAGE_ID_ADDR", "0x3101e288"));
        bl2_boot_load_image_to_sram = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOT_LOAD_IMAGE_TO_SRAM_ADDR", "0x3101e758"));
        bl2_boot_enc_load = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOT_ENC_LOAD_ADDR", "0x3101eeb6"));
        bl2_boot_enc_set_key = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOT_ENC_SET_KEY_ADDR", "0x3101ef52"));
        bl2_boot_enc_decrypt = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOT_ENC_DECRYPT_ADDR", "0x3101ef8c"));
        bl2_bootutil_img_validate = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_IMG_VALIDATE_ADDR", "0x3101f010"));
        bl2_bootutil_img_hash = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_IMG_HASH_ADDR", "0x3101f3aa"));
        bl2_bootutil_verify_sig = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_VERIFY_SIG_ADDR", "0x3101f5bc"));
        bl2_bootutil_keys = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_KEYS_ADDR", "0x31000454"));
        bl2_bootutil_key_cnt = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_BOOTUTIL_KEY_CNT_ADDR", "0x3102b424"));
        bl2_fih_success = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_FIH_SUCCESS_ADDR", "0x310027dc"));
        bl2_delay_cycles = tonumber(getenv_or(
            "QBOX_RDASPEN_RSE_BL2_DELAY_CYCLES_ADDR", "0x31021aca"));
        cc3xx_trace_min = tonumber(getenv_or(
            "QBOX_RDASPEN_CC3XX_TRACE_ADDRESS_MIN", "0"));
        dma350_trace_min = tonumber(getenv_or(
            "QBOX_RDASPEN_DMA350_TRACE_ADDRESS_MIN", "0"));
        atu_trace_min = tonumber(getenv_or(
            "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MIN", "0"));
        atu_trace_max = tonumber(getenv_or(
            "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MAX", "0"));
        dma_boot = getenv_number_or(
            "QBOX_RDASPEN_RSE_DMA_BOOT_ADDR", "0x00000000");
    }


    local block = {}

    function block.configure_cpu(ctx, cpu)
        cpu.trace_pc = rse_pc_trace;
        cpu.trace_exception_state = rse_exception_trace;
        cpu.trace_pc_file = rse_pc_trace_file;
        cpu.trace_pc_interval = rse_pc_trace_interval;
        cpu.trace_pc_limit = rse_pc_trace_limit;
        cpu.hotpath_accel = rse_hotpath_accel;
        cpu.hotpath_memcpy_addr = RSE_RUNTIME_ADDRESS.hotpath_memcpy;
        cpu.hotpath_memset_addr = RSE_RUNTIME_ADDRESS.hotpath_memset;
        cpu.hotpath_max_bytes = rse_hotpath_max_bytes;
        cpu.hotpath_profile_file = rse_hotpath_profile_file;
        cpu.hotpath_profile_interval = rse_hotpath_profile_interval;
        cpu.lms_accel = rse_lms_accel;
        cpu.lms_verify_addr = RSE_RUNTIME_ADDRESS.lms_verify;
        cpu.lms_max_data_bytes = rse_lms_max_data_bytes;
        cpu.bl2_load_profile = rse_bl2_load_profile;
        cpu.bl2_boot_go_for_image_id_addr = RSE_RUNTIME_ADDRESS.bl2_boot_go_for_image_id;
        cpu.bl2_boot_load_image_to_sram_addr = RSE_RUNTIME_ADDRESS.bl2_boot_load_image_to_sram;
        cpu.bl2_boot_enc_load_addr = RSE_RUNTIME_ADDRESS.bl2_boot_enc_load;
        cpu.bl2_boot_enc_set_key_addr = RSE_RUNTIME_ADDRESS.bl2_boot_enc_set_key;
        cpu.bl2_boot_enc_decrypt_addr = RSE_RUNTIME_ADDRESS.bl2_boot_enc_decrypt;
        cpu.bl2_bootutil_img_validate_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_img_validate;
        cpu.bl2_bootutil_img_hash_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_img_hash;
        cpu.bl2_bootutil_verify_sig_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_verify_sig;
        cpu.bl2_boot_image_count = rse_bl2_boot_image_count;
        cpu.bl2_boot_state_curr_img_offset = rse_bl2_boot_state_curr_img_offset;
        cpu.bl2_boot_state_imgs_offset = rse_bl2_boot_state_imgs_offset;
        cpu.bl2_boot_state_image_stride = rse_bl2_boot_state_image_stride;
        cpu.bl2_boot_state_slot_stride = rse_bl2_boot_state_slot_stride;
        cpu.bl2_boot_state_slot_usage_offset = rse_bl2_boot_state_slot_usage_offset;
        cpu.bl2_boot_state_slot_usage_stride = rse_bl2_boot_state_slot_usage_stride;
        cpu.bl2_boot_slot_usage_img_dst_offset = rse_bl2_boot_slot_usage_img_dst_offset;
        cpu.bl2_boot_slot_usage_img_sz_offset = rse_bl2_boot_slot_usage_img_sz_offset;
        cpu.bl2_load_accel = rse_bl2_load_accel;
        cpu.bl2_load_accel_max_bytes = rse_bl2_load_accel_max_bytes;
        cpu.bl2_boot_enc_accel = rse_bl2_boot_enc_accel;
        cpu.bl2_boot_status_enckey_offset = rse_bl2_boot_status_enckey_offset;
        cpu.bl2_boot_enc_key_bytes = rse_bl2_boot_enc_key_bytes;
        cpu.bl2_boot_enc_key_stride = rse_bl2_boot_enc_key_stride;
        cpu.bl2_boot_enc_slots = rse_bl2_boot_enc_slots;
        cpu.bl2_boot_enc_max_bytes = rse_bl2_boot_enc_max_bytes;
        cpu.bl2_img_hash_accel = rse_bl2_img_hash_accel;
        cpu.bl2_img_hash_max_bytes = rse_bl2_img_hash_max_bytes;
        cpu.bl2_img_hash_max_seed_bytes = rse_bl2_img_hash_max_seed_bytes;
        cpu.bl2_verify_sig_accel = rse_bl2_verify_sig_accel;
        cpu.bl2_verify_sig_skip = rse_bl2_verify_sig_skip;
        cpu.bl2_bootutil_keys_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_keys;
        cpu.bl2_bootutil_key_cnt_addr = RSE_RUNTIME_ADDRESS.bl2_bootutil_key_cnt;
        cpu.bl2_fih_success_addr = RSE_RUNTIME_ADDRESS.bl2_fih_success;
        cpu.bl2_verify_sig_max_key_bytes = rse_bl2_verify_sig_max_key_bytes;
        cpu.bl2_verify_sig_max_sig_bytes = rse_bl2_verify_sig_max_sig_bytes;
        cpu.bl2_delay_accel = rse_bl2_delay_accel;
        cpu.bl2_delay_cycles_addr = RSE_RUNTIME_ADDRESS.bl2_delay_cycles;
        cpu.bl2_delay_max_cycles = rse_bl2_delay_max_cycles;
        cpu.bl2_delay_expected_hits = rse_bl2_delay_expected_hits;
    end

    block.addresses = RSE_RUNTIME_ADDRESS
    return block
end

return module
