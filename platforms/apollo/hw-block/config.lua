-- Apollo QVP shared setup for QBox.

function top()
    local str = debug.getinfo(2, "S").source:sub(2)
    if str:match("(.*/)")
    then
        return str:match("(.*/)")
    else
        return "./"
    end
end

function getenv_or(name, default)
    local value = os.getenv(name)
    if value == nil or value == "" then
        return default
    end
    return value
end

function getenv_number_or(name, default)
    local value = tonumber(getenv_or(name, default))
    assert(value ~= nil, name.." must be numeric")
    return value
end

function getenv_bool_or(name, default)
    local value = getenv_or(name, default and "true" or "false")
    return value == "true" or value == "1" or value == "yes"
end

function repeat_value(value, count)
    local values = {}
    for _=1,count do
        values[#values + 1] = value
    end
    return values
end

function mp_affinity(cpu_index)
    local cluster = math.floor(cpu_index / 4)
    local core = cpu_index % 4
    return (cluster * 0x10000) + (core * 0x100)
end

function ap_cpu_reset_bind_targets(count)
    local targets = {}
    for i=0,(count-1) do
        targets[#targets + 1] = "&ap_cpu_"..i..".reset"
    end
    return table.concat(targets, ";")
end

function ap_system_reset_bind_targets()
    local targets = {
        "&ap_bl2_reset_loader.reset";
        "&host_ap_bl2_header_sram.reset";
        "&host_ap_mhu_ns_shared_sram.reset";
        "&ap_reset_gpio.reset_in";
    }

    return table.concat(targets, ";")
end

print("Apollo QVP shared config running...")

root = top().."../../../../../"
deploy = root.."build/tmp_baremetal/deploy/images/fvp-rd-aspen/"

rse_rom = getenv_or("QBOX_RDASPEN_RSE_ROM", deploy.."rse-rom-image.img")
rse_flash = getenv_or("QBOX_RDASPEN_RSE_FLASH", deploy.."rse-flash-image.img")
rse_otp = getenv_or("QBOX_RDASPEN_RSE_OTP", deploy.."rse-otp-image.img")
ap_flash = getenv_or("QBOX_RDASPEN_AP_FLASH", deploy.."ap-flash-image.img")
flash_writeback = getenv_or("QBOX_RDASPEN_FLASH_WRITEBACK", "false") == "true"
flash_defer_backing_flush_interval =
    getenv_number_or("QBOX_RDASPEN_FLASH_DEFER_FLUSH_INTERVAL", 1024)
AP_BL2_ELF = getenv_or(
    "QBOX_RDASPEN_AP_BL2_ELF",
    root.."build/tmp_baremetal/work/fvp_rd_aspen-poky-linux/trusted-firmware-a/2.14.0+git/build/rdaspen/debug/bl2/bl2.elf")
ap_virtio = {
    disk_image = getenv_or("QBOX_RDASPEN_ROOTFS", deploy.."baremetal-image-fvp-rd-aspen.wic");
    extra_disk_images = {
        getenv_or("QBOX_RDASPEN_EXTRA_BLK1", deploy.."efi-capsule-update-disk-image-fvp-rd-aspen.img");
        getenv_or("QBOX_RDASPEN_EXTRA_BLK2", root.."build/qbox-fvp-rd-aspen/rd-aspen-extra-blk2.raw");
        getenv_or("QBOX_RDASPEN_EXTRA_BLK3", root.."build/qbox-fvp-rd-aspen/rd-aspen-extra-blk3.raw");
    };
    netdev = getenv_or(
        "QBOX_APOLLO_NETDEV",
        getenv_or("QBOX_RDASPEN_NETDEV", "type=user,hostfwd=tcp::2222-:22"));
    mmio_size = 0x00010000;
    block_base = {0x30020000; 0x30030000; 0x30040000; 0x30050000};
    block_irq = {257; 258; 259; 260};
    net_base = 0x30060000;
    net_irq = 261;
    rng_base = 0x30080000;
    rng_irq = 263;
    trace = getenv_or("QBOX_RDASPEN_VIRTIO_TRACE", "false") == "true";
    trace_file = getenv_or("QBOX_RDASPEN_VIRTIO_TRACE_FILE", "");
    trace_limit = tonumber(getenv_or("QBOX_RDASPEN_VIRTIO_TRACE_LIMIT", "4096"));
    trace_filter = getenv_or("QBOX_RDASPEN_VIRTIO_TRACE_FILTER", "control");
}
host_si_cl0_sram_map_file = getenv_or(
    "QBOX_RDASPEN_HOST_SI_CL0_SRAM_MAP_FILE",
    "")
host_si_cl1_sram_map_file = getenv_or(
    "QBOX_RDASPEN_HOST_SI_CL1_SRAM_MAP_FILE",
    "")
host_ap_shared_sram_map_file = getenv_or(
    "QBOX_RDASPEN_HOST_AP_SHARED_SRAM_MAP_FILE",
    "")
host_ap_bl2_header_sram_map_file = getenv_or(
    "QBOX_RDASPEN_HOST_AP_BL2_HEADER_SRAM_MAP_FILE",
    "")
host_sram_shared_memory =
    getenv_or("QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY", "false") == "true"
function host_sram_shared_memory_enabled(map_file)
    return host_sram_shared_memory and map_file == ""
end
provisioning_bundle = getenv_or(
    "QBOX_RDASPEN_PROVISIONING_BUNDLE",
    deploy.."combined_provisioning_message.bin")
rse_log = getenv_or(
    "QBOX_RDASPEN_RSE_LOG",
    root.."build/qbox-fvp-rd-aspen/qbox-rse.log")
secure_console_log = getenv_or(
    "QBOX_RDASPEN_SECURE_CONSOLE_LOG",
    root.."build/qbox-fvp-rd-aspen/qbox-secure-console.log")
primary_console_log = getenv_or(
    "QBOX_RDASPEN_PRIMARY_CONSOLE_LOG",
    root.."build/qbox-fvp-rd-aspen/qbox-primary-console.log")
rse_uart_read_file = getenv_or("QBOX_RDASPEN_UART_READ_FILE", "/dev/null")
secure_uart_read_file = getenv_or(
    "QBOX_RDASPEN_SECURE_UART_READ_FILE",
    "/dev/null")
primary_uart_read_file = getenv_or(
    "QBOX_RDASPEN_PRIMARY_UART_READ_FILE",
    "/dev/null")
qemu_args = getenv_or("QBOX_RDASPEN_RSE_QEMU_ARGS", "")
ap_qemu_args = getenv_or("QBOX_RDASPEN_AP_QEMU_ARGS", "")
ap_pc_trace = getenv_or("QBOX_RDASPEN_AP_PC_TRACE", "false") == "true"
ap_pc_trace_file = getenv_or(
    "QBOX_RDASPEN_AP_PC_TRACE_FILE",
    root.."build/qbox-fvp-rd-aspen/ap-pc-trace.log")
ap_pc_trace_interval = tonumber(getenv_or("QBOX_RDASPEN_AP_PC_TRACE_INTERVAL", "1"))
ap_pc_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_AP_PC_TRACE_LIMIT", "4096"))
ap_exception_trace = getenv_or("QBOX_RDASPEN_AP_EXCEPTION_TRACE", "false") == "true"
enable_ap_cpus = getenv_or("QBOX_RDASPEN_ENABLE_AP_CPUS", "false") == "true"
apollo_si_mode = getenv_or("QBOX_APOLLO_FULL_SI_MODE", "service-model")
apollo_live_cl1 =
    getenv_or("QBOX_APOLLO_FULL_LIVE_CL1", "false") == "true" or
    apollo_si_mode == "live-cl1" or apollo_si_mode == "live-cl0-cl1"
apollo_live_cl0 =
    getenv_or("QBOX_APOLLO_FULL_LIVE_CL0", "false") == "true" or
    apollo_si_mode == "live-cl0-cl1"
rse_pc_trace = getenv_or("QBOX_RDASPEN_RSE_PC_TRACE", "false") == "true"
rse_pc_trace_file = getenv_or(
    "QBOX_RDASPEN_RSE_PC_TRACE_FILE",
    root.."build/qbox-fvp-rd-aspen/rse-pc-trace.log")
rse_pc_trace_interval = tonumber(getenv_or("QBOX_RDASPEN_RSE_PC_TRACE_INTERVAL", "1"))
rse_pc_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_RSE_PC_TRACE_LIMIT", "4096"))
rse_exception_trace = getenv_or("QBOX_RDASPEN_RSE_EXCEPTION_TRACE", "false") == "true"
rse_hotpath_accel = getenv_or("QBOX_RDASPEN_RSE_HOTPATH_ACCEL", "false") == "true"
rse_hotpath_memcpy_addr = tonumber(getenv_or("QBOX_RDASPEN_RSE_HOTPATH_MEMCPY_ADDR", "0x11000488"))
rse_hotpath_memset_addr = tonumber(getenv_or("QBOX_RDASPEN_RSE_HOTPATH_MEMSET_ADDR", "0x11000448"))
rse_hotpath_max_bytes = tonumber(getenv_or("QBOX_RDASPEN_RSE_HOTPATH_MAX_BYTES", tostring(16 * 1024 * 1024)))
rse_hotpath_profile_file = getenv_or("QBOX_RDASPEN_RSE_HOTPATH_PROFILE_FILE", "")
rse_hotpath_profile_interval = tonumber(getenv_or("QBOX_RDASPEN_RSE_HOTPATH_PROFILE_INTERVAL", "1024"))
rse_lms_accel = getenv_or("QBOX_RDASPEN_RSE_LMS_ACCEL", "false") == "true"
rse_lms_verify_addr = tonumber(getenv_or("QBOX_RDASPEN_RSE_LMS_VERIFY_ADDR", "0x11009bad"))
rse_lms_max_data_bytes = tonumber(getenv_or("QBOX_RDASPEN_RSE_LMS_MAX_DATA_BYTES", tostring(16 * 1024 * 1024)))
rse_bl2_load_profile =
    getenv_or("QBOX_RDASPEN_RSE_BL2_LOAD_PROFILE", "false") == "true"
rse_bl2_boot_go_for_image_id_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_GO_FOR_IMAGE_ID_ADDR", "0x3101e288"))
rse_bl2_boot_load_image_to_sram_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_LOAD_IMAGE_TO_SRAM_ADDR", "0x3101e758"))
rse_bl2_boot_enc_load_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_LOAD_ADDR", "0x3101eeb6"))
rse_bl2_boot_enc_set_key_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_SET_KEY_ADDR", "0x3101ef52"))
rse_bl2_boot_enc_decrypt_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_DECRYPT_ADDR", "0x3101ef8c"))
rse_bl2_bootutil_img_validate_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOTUTIL_IMG_VALIDATE_ADDR", "0x3101f010"))
rse_bl2_bootutil_img_hash_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOTUTIL_IMG_HASH_ADDR", "0x3101f3aa"))
rse_bl2_bootutil_verify_sig_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOTUTIL_VERIFY_SIG_ADDR", "0x3101f5bc"))
rse_bl2_boot_image_count =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_IMAGE_COUNT", "5"))
rse_bl2_boot_state_curr_img_offset =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_CURR_IMG_OFFSET", "0x10c8"))
rse_bl2_boot_state_imgs_offset =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_IMGS_OFFSET", "0x0"))
rse_bl2_boot_state_image_stride =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_IMAGE_STRIDE", "88"))
rse_bl2_boot_state_slot_stride =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_SLOT_STRIDE", "44"))
rse_bl2_boot_state_slot_usage_offset =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_SLOT_USAGE_OFFSET", "0x10d0"))
rse_bl2_boot_state_slot_usage_stride =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATE_SLOT_USAGE_STRIDE", "16"))
rse_bl2_boot_slot_usage_img_dst_offset =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_SLOT_USAGE_IMG_DST_OFFSET", "8"))
rse_bl2_boot_slot_usage_img_sz_offset =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_SLOT_USAGE_IMG_SZ_OFFSET", "12"))
rse_bl2_load_accel =
    getenv_or("QBOX_RDASPEN_RSE_BL2_LOAD_ACCEL", "false") == "true"
rse_bl2_load_accel_max_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_LOAD_ACCEL_MAX_BYTES", tostring(16 * 1024 * 1024)))
rse_bl2_boot_enc_accel =
    getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_ACCEL", "false") == "true"
rse_bl2_img_hash_accel =
    getenv_or("QBOX_RDASPEN_RSE_BL2_IMG_HASH_ACCEL", "false") == "true"
rse_bl2_img_hash_max_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_IMG_HASH_MAX_BYTES", tostring(16 * 1024 * 1024)))
rse_bl2_img_hash_max_seed_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_IMG_HASH_MAX_SEED_BYTES", "4096"))
rse_bl2_verify_sig_accel =
    getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_ACCEL", "false") == "true"
rse_bl2_verify_sig_skip =
    getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_SKIP", "false") == "true"
rse_bl2_bootutil_keys_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOTUTIL_KEYS_ADDR", "0x31000454"))
rse_bl2_bootutil_key_cnt_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOTUTIL_KEY_CNT_ADDR", "0x3102b424"))
rse_bl2_fih_success_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_FIH_SUCCESS_ADDR", "0x310027dc"))
rse_bl2_verify_sig_max_key_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_MAX_KEY_BYTES", "512"))
rse_bl2_verify_sig_max_sig_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_MAX_SIG_BYTES", "128"))
rse_bl2_delay_accel =
    getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_ACCEL", "false") == "true"
rse_bl2_delay_cycles_addr =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_CYCLES_ADDR", "0x31021aca"))
rse_bl2_delay_max_cycles =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_MAX_CYCLES", "50000000"))
rse_bl2_delay_expected_hits =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_EXPECTED_HITS", "3"))
rse_bl2_boot_status_enckey_offset =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_STATUS_ENCKEY_OFFSET", "0x0c"))
rse_bl2_boot_enc_key_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_KEY_BYTES", "16"))
rse_bl2_boot_enc_key_stride =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_KEY_STRIDE", "16"))
rse_bl2_boot_enc_slots =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_SLOTS", "2"))
rse_bl2_boot_enc_max_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_BOOT_ENC_MAX_BYTES", "4096"))
cc3xx_trace = getenv_or("QBOX_RDASPEN_CC3XX_TRACE", "false") == "true"
cc3xx_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_CC3XX_TRACE_LIMIT", "64"))
cc3xx_trace_filter = getenv_or("QBOX_RDASPEN_CC3XX_TRACE_FILTER", "all")
cc3xx_trace_address_min = tonumber(getenv_or("QBOX_RDASPEN_CC3XX_TRACE_ADDRESS_MIN", "0"))
cc3xx_stats_file = getenv_or("QBOX_RDASPEN_CC3XX_STATS_FILE", "")
cc3xx_stats_interval = tonumber(getenv_or("QBOX_RDASPEN_CC3XX_STATS_INTERVAL", "0"))
cc3xx_backend = getenv_or("QBOX_RDASPEN_CC3XX_BACKEND", "systemc")
assert(cc3xx_backend == "systemc" or cc3xx_backend == "qemu-native",
       "QBOX_RDASPEN_CC3XX_BACKEND must be systemc or qemu-native")
smmu_backend = getenv_or("QBOX_RDASPEN_SMMU_BACKEND", "systemc-mmu720ae")
assert(smmu_backend == "qemu-arm-smmuv3" or smmu_backend == "systemc-mmu720ae",
       "QBOX_RDASPEN_SMMU_BACKEND must be qemu-arm-smmuv3 or systemc-mmu720ae")
dma350_trace = getenv_or("QBOX_RDASPEN_DMA350_TRACE", "false") == "true"
dma350_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_DMA350_TRACE_LIMIT", "64"))
dma350_trace_filter = getenv_or("QBOX_RDASPEN_DMA350_TRACE_FILTER", "all")
dma350_trace_address_min = tonumber(getenv_or("QBOX_RDASPEN_DMA350_TRACE_ADDRESS_MIN", "0"))
sysctrl_trace = getenv_or("QBOX_RDASPEN_SYSCTRL_TRACE", "false") == "true"
sysctrl_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_SYSCTRL_TRACE_LIMIT", "64"))
lcm_trace = getenv_or("QBOX_RDASPEN_LCM_TRACE", "false") == "true"
lcm_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_LCM_TRACE_LIMIT", "64"))
rse_lcm_lcs = getenv_number_or("QBOX_RDASPEN_RSE_LCM_LCS", "4008617381")
rse_lcm_tp_mode = getenv_number_or("QBOX_RDASPEN_RSE_LCM_TP_MODE", "286348714")
rse_lcm_sp_enable = getenv_number_or("QBOX_RDASPEN_RSE_LCM_SP_ENABLE", "0")
boot_flash_trace = getenv_or("QBOX_RDASPEN_BOOT_FLASH_TRACE", "false") == "true"
boot_flash_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_BOOT_FLASH_TRACE_LIMIT", "64"))
boot_flash_dmi = getenv_or("QBOX_RDASPEN_BOOT_FLASH_DMI", "false") == "true"
boot_flash_dmi_ranges = getenv_or("QBOX_RDASPEN_BOOT_FLASH_DMI_RANGES", "")
host_memory_dmi = getenv_or("QBOX_RDASPEN_HOST_MEMORY_DMI", "false") == "true"
host_si_sram_dmi = getenv_or("QBOX_RDASPEN_HOST_SI_SRAM_DMI", "false") == "true"
ap_flash_dmi_ranges = getenv_or("QBOX_RDASPEN_AP_FLASH_DMI_RANGES", "")
rse_boot_flash_stats_file = getenv_or(
    "QBOX_RDASPEN_RSE_BOOT_FLASH_STATS_FILE",
    "")
ap_flash_stats_file = getenv_or("QBOX_RDASPEN_AP_FLASH_STATS_FILE", "")
flash_stats_interval = tonumber(getenv_or("QBOX_RDASPEN_FLASH_STATS_INTERVAL", "0"))
atu_trace = getenv_or("QBOX_RDASPEN_ATU_TRACE", "false") == "true"
atu_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_ATU_TRACE_LIMIT", "64"))
atu_trace_filter = getenv_or("QBOX_RDASPEN_ATU_TRACE_FILTER", "all")
atu_trace_address_min = tonumber(getenv_or("QBOX_RDASPEN_ATU_TRACE_ADDRESS_MIN", "0"))
atu_trace_address_max = tonumber(getenv_or("QBOX_RDASPEN_ATU_TRACE_ADDRESS_MAX", "0"))
atu_dmi = getenv_or("QBOX_RDASPEN_ATU_DMI", "false") == "true"
kmu_trace = getenv_or("QBOX_RDASPEN_KMU_TRACE", "false") == "true"
kmu_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_KMU_TRACE_LIMIT", "64"))
kmu_trace_filter = getenv_or("QBOX_RDASPEN_KMU_TRACE_FILTER", "all")
integrity_checker_trace = getenv_or("QBOX_RDASPEN_INTEGRITY_CHECKER_TRACE", "false") == "true"
integrity_checker_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_INTEGRITY_CHECKER_TRACE_LIMIT", "64"))
sam_trace = getenv_or("QBOX_RDASPEN_SAM_TRACE", "false") == "true"
sam_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_SAM_TRACE_LIMIT", "64"))
host_ppu_trace = getenv_or("QBOX_RDASPEN_HOST_PPU_TRACE", "false") == "true"
host_ppu_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_HOST_PPU_TRACE_LIMIT", "64"))
mhu_trace = getenv_or("QBOX_RDASPEN_MHU_TRACE", "false") == "true"
mhu_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_MHU_TRACE_LIMIT", "256"))
mhu_trace_file = getenv_or(
    "QBOX_RDASPEN_MHU_TRACE_FILE",
    root.."build/qbox-fvp-rd-aspen/mhuv3-trace.log")
ap_power_domain_reset_delay_ns = tonumber(
    getenv_or("QBOX_RDASPEN_AP_POWER_DOMAIN_RESET_DELAY_NS", "1"))
rse_local_crypto = getenv_or("QBOX_RDASPEN_RSE_LOCAL_CRYPTO", "true") == "true"
rse_local_boot_flash = getenv_or("QBOX_RDASPEN_RSE_LOCAL_BOOT_FLASH", "true") == "true"
rse_local_peripherals = rse_local_crypto or rse_local_boot_flash
rse_split_cpu0_dtcm_alias = getenv_or(
    "QBOX_RDASPEN_RSE_SPLIT_CPU0_DTCM_ALIAS",
    "false") == "true"
rse_split_cpu0_itcm_alias = getenv_or(
    "QBOX_RDASPEN_RSE_SPLIT_CPU0_ITCM_ALIAS",
    "false") == "true"
rse_itcm_dmi = getenv_or("QBOX_RDASPEN_RSE_ITCM_DMI", "true") == "true"
rse_dtcm_dmi = getenv_or("QBOX_RDASPEN_RSE_DTCM_DMI", "true") == "true"
rse_vm_dmi = getenv_or("QBOX_RDASPEN_RSE_VM_DMI", "true") == "true"
rse_vmaddrwidth = getenv_number_or(
    "QBOX_RDASPEN_RSE_VMADDRWIDTH",
    "18")
assert(rse_vmaddrwidth >= 18 and rse_vmaddrwidth <= 31,
       "QBOX_RDASPEN_RSE_VMADDRWIDTH must be in range 18..31")
rse_reset_syndrome = getenv_number_or(
    "QBOX_RDASPEN_RSE_RESET_SYNDROME",
    "0x80000000")
rse_cpuwait = getenv_number_or(
    "QBOX_RDASPEN_RSE_CPUWAIT",
    "0x0000000F")
rse_dma_boot_en = getenv_number_or(
    "QBOX_RDASPEN_RSE_DMA_BOOT_EN",
    "0x00000001")
rse_dma_boot_addr = getenv_number_or(
    "QBOX_RDASPEN_RSE_DMA_BOOT_ADDR",
    "0x00000000")
RSE_ROM_BASE_S = 0x11000000
RSE_ROM_SIZE = 0x00020000
RSE_ITCM_BASE_NS = 0x00000000
RSE_ITCM_CPU0_BASE_NS = 0x0A000000
RSE_ITCM_BASE_S = 0x10000000
RSE_ITCM_CPU0_BASE_S = 0x1A000000
RSE_ITCM_SIZE = 0x00008000
RSE_DTCM_BASE_NS = 0x20000000
RSE_DTCM_CPU0_BASE_NS = 0x24000000
RSE_DTCM_BASE_S = 0x30000000
RSE_DTCM_CPU0_BASE_S = 0x34000000
RSE_DTCM_SIZE = 0x00008000
RSE_VM0_BASE_S = 0x31000000
RSE_VM_SIZE = 2 ^ rse_vmaddrwidth
RSE_VM1_BASE_S = RSE_VM0_BASE_S + RSE_VM_SIZE
RSE_PROVISIONING_OFFSET = 0x00020000
RSE_BOOT_FLASH_BASE_S = 0xB0000000
RSE_BOOT_FLASH_SIZE = 0x04000000
RSE_HOST_ACCESS_BASE_NS = 0x60000000
RSE_HOST_ACCESS_BASE_S = 0x70000000
RSE_HOST_ACCESS_SIZE = 0x10000000

RSE_HOST_UART0_BASE_NS = RSE_HOST_ACCESS_BASE_NS + 0x0FF00000
RSE_HOST_UART0_BASE_S = RSE_HOST_ACCESS_BASE_S + 0x0FF00000
RSE_NSACFG_BASE_NS = 0x40080000
RSE_DMA350_BASE_S = 0x50002000
RSE_SACFG_BASE_S = 0x50080000
RSE_KMU_BASE_S = 0x5009E000
RSE_SAM_BASE_S = 0x5009F000
RSE_ATU_BASE_S = 0x50150000
RSE_CC3XX_BASE_S = 0x50154000
RSE_SYSCNTR_CNTRL_BASE_S = 0x5015A000
RSE_SYSCNTR_READ_BASE_S = 0x5015B000
RSE_INTEGRITY_CHECKER_BASE_S = 0x5015C000
RSE_TRAM_BASE_S = 0x5015D000
RSE_MHU0_SENDER_BASE_S = 0x50160000
RSE_MHU0_RECEIVER_BASE_S = 0x50170000
RSE_MHU2_SENDER_BASE_S = 0x501A0000
RSE_MHU2_RECEIVER_BASE_S = 0x501B0000

function rse_cc3xx_component(target_bind, initiator_bind)
    local component = {
        moduletype = cc3xx_backend == "qemu-native" and "qemu_cc3xx" or "cc3xx";
        trace = cc3xx_trace;
        trace_limit = cc3xx_trace_limit;
        trace_skip = tonumber(getenv_or("QBOX_RDASPEN_CC3XX_TRACE_SKIP", "0"));
        trace_filter = cc3xx_trace_filter;
        trace_address_min = cc3xx_trace_address_min;
        stats_file = cc3xx_stats_file;
        stats_interval = cc3xx_stats_interval;
        target_socket = {
            address = RSE_CC3XX_BASE_S;
            size = 0x00002000;
            bind = target_bind;
        };
        initiator_socket = {bind = initiator_bind};
        log_level = 0;
    }

    if cc3xx_backend == "qemu-native" then
        component.args = {"&qemu_inst"}
        component.size = 0x00002000
    end

    return component
end
RSE_LOCAL_MHU_FRAME_SIZE = 0x00010000
RSE_LCM_BASE_S = 0x500A0000
RSE_LCM_SIZE = 0x00011000
RSE_SYSCTRL_BASE_S = 0x58021000
RSE_INTEG_LAYER_BASE_S = 0x58100000
RSE_MPC_VM0_BASE_S = 0x50083000
RSE_MPC_VM1_BASE_S = 0x50084000
RSE_OTP_WRAPPER_BASE_S = 0x58111000
RSE_NVIC_BASE = 0xE000E000
RSE_NVIC_SIZE = 0x00010000
RSE_NVIC_NUM_IRQ = 160
RSE_REMOTE_SIGNAL_COUNT = RSE_NVIC_NUM_IRQ
RSE_IRQ_CMU_MHU0_RECEIVER = 41
RSE_IRQ_CMU_MHU2_RECEIVER = 45
RSE_IRQ_SI_CL0_RSE_CMU_MHU_RECEIVER = 139
-- Keep the AP/SI map constants as chunk globals so the generated platform
-- stays below Lua 5.1's 200-local main-function limit.
HOST_AP_SHARED_SRAM_PHYS_BASE = 0x00000000
HOST_AP_SHARED_SRAM_SIZE = 0x00100000
HOST_AP_SDS_MEM_SIZE = 0x00000DC0
HOST_AP_SDS_RESET_SYNDROME_PHYS_BASE = HOST_AP_SHARED_SRAM_PHYS_BASE + 0x50
SDS_RESET_SYNDROME_SYS_RESET_REQ = 0x00000008
HOST_AP_SCMI_PAYLOAD_BASE = HOST_AP_SHARED_SRAM_PHYS_BASE + HOST_AP_SDS_MEM_SIZE
HOST_AP_BL2_PHYS_BASE = HOST_AP_SHARED_SRAM_PHYS_BASE + 0x00082000
AP_BL2_RESET = {
    data_phys_base = HOST_AP_SHARED_SRAM_PHYS_BASE + 0x00098000;
    stacks_phys_base = HOST_AP_SHARED_SRAM_PHYS_BASE + 0x00098F40;
    bss_phys_base = HOST_AP_SHARED_SRAM_PHYS_BASE + 0x0009A000;
    xlat_phys_base = HOST_AP_SHARED_SRAM_PHYS_BASE + 0x000A2000;
    data_elf_offset = 0x00017000;
    data_size = 0x00000F35;
    stacks_size = 0x00001000;
    bss_size = 0x00008000;
    xlat_size = 0x0000E000;
}
HOST_AP_BL2_HEADER_SRAM_PHYS_BASE = 0x00100000
HOST_AP_BL2_HEADER_SRAM_SIZE = 0x00080000
HOST_AP_FLASH_PHY_BASE = 0x38000000
HOST_AP_FLASH_IMAGE_SIZE = 0x08000000
HOST_AP_FLASH_LOGICAL_BASE = 0x703A6000
AP_FLASH_FIP_PRIMARY_OFFSET = 0x00007000
AP_FLASH_FIP_SIZE = 0x00240000
HOST_AP_TRUSTED_NVCTR_BASE = 0x32030000
HOST_AP_TRUSTED_NVCTR_SIZE = 0x00010000
HOST_AP_DRAM1_BASE = 0x80000000
HOST_AP_DRAM1_SIZE = 0x7F000000
HOST_AP_SPMC_BASE = 0xFFC00000
HOST_AP_SPMC_SIZE = 0x003FC000
HOST_AP_DRAM2_BASE = 0x20000000000
HOST_AP_DRAM2_SIZE = 0x80000000
HOST_AP_ATU_LOGICAL_BASE = 0x40000000
HOST_AP_ATU_LOGICAL_SIZE = 0x00800000
AP_NUM_CPUS = enable_ap_cpus and getenv_number_or("QBOX_APOLLO_NUM_CPUS", "16") or 0
assert(not enable_ap_cpus or (AP_NUM_CPUS >= 1 and AP_NUM_CPUS <= 16), "QBOX_APOLLO_NUM_CPUS must be 1..16 when AP CPUs are enabled")
AP_GIC_NUM_CPUS = enable_ap_cpus and AP_NUM_CPUS or 1
ARCH_TIMER_VIRT_IRQ = 16 + 11
ARCH_TIMER_S_EL1_IRQ = 16 + 13
ARCH_TIMER_NS_EL1_IRQ = 16 + 14
ARCH_TIMER_NS_EL2_IRQ = 16 + 10
AP_SECURE_UART_BASE = 0x1A410000
AP_PRIMARY_UART_BASE = 0x1A400000
AP_SECURE_WDOG_BASE = 0x1A460000
AP_SECURE_WDOG_REFRESH_BASE = 0x1A470000
AP_SECURE_WDOG_SIZE = 0x00010000
AP_SID_BASE = 0x1A4A0000
AP_SID_SIZE = 0x00010000
AP_SYS_TIMCTL_BASE = 0x1A810000
AP_SYS_CNT_BASE_S = 0x1A820000
AP_SYS_CNT_BASE_NS = 0x1A830000
AP_SYS_TIMER_SIZE = 0x00010000
AP_RGIC2LGIC_MESSREG_BASE = 0x5FFF0000
AP_RGIC2LGIC_MESSREG_SIZE = 0x00010000
AP_FMU_REGION_BASE = 0x1D000000
AP_FMU_SUBWINDOW_SIZE = 0x00100000
AP_FMU_MODELED_SIZE = 0x00050000
AP_CL0_NI710AE_FMU_BASE = AP_FMU_REGION_BASE
AP_CL1_NI710AE_FMU_BASE = AP_FMU_REGION_BASE + AP_FMU_SUBWINDOW_SIZE
AP_CL2_NI710AE_FMU_BASE = AP_FMU_REGION_BASE + 2 * AP_FMU_SUBWINDOW_SIZE
AP_CL3_NI710AE_FMU_BASE = AP_FMU_REGION_BASE + 3 * AP_FMU_SUBWINDOW_SIZE
AP_SYS_TIMER_IRQ = 49
AP_PRIMARY_UART_IRQ = 52
AP_SECURE_UART_IRQ = 53
AP_SI_SCMI_MHU_PBX_IRQ = 112
AP_SI_SCMI_MHU_MBX_IRQ = 113
AP_GIC_DIST_BASE = 0x20800000
AP_GIC_REDIST_BASE = 0x20880000
AP_GIC_REDIST_SIZE = 0x00040000
AP_GIC_REDIST_REGIONS = 16
AP_GIC_ACTIVE_REDIST_REGIONS = AP_GIC_NUM_CPUS
AP_GIC_LEGACY_DIST_BASE = 0x20000000
AP_GIC_LEGACY_REDIST_BASE = 0x200C0000
AP_GIC_LEGACY_REDIST_SIZE = 0x00020000

-- Host-visible Safety Island windows
HOST_SI_CL0_CL_UTIL_BASE = 0x4000028000000
HOST_SI_CL1_CL_UTIL_BASE = 0x4000028800000
HOST_SI_CL_UTIL_SIZE = 0x00800000
HOST_SI_CLUS_PPU_OFFSET = 0x00010000
HOST_SI_CORE0_PPU_OFFSET = 0x00040000
HOST_SI_PIK_PHYS_BASE = 0x400002A600000
HOST_SI_SCR_PHYS_BASE = 0x400002A6B0000
HOST_SI_ATU_PHYS_BASE = 0x4000031000000
HOST_RSE_SI_MHU_PHYS_BASE = 0x400003C000000
HOST_RSE_SI_MHU_SIZE = 0x01000000
RSE_MHU_FRAME_SIZE = 0x00020000
HOST_RSE_SI_SSRAM_PHYS_BASE = 0x4000040000000
HOST_RSE_SI_SSRAM_SIZE = 0x00040000
HOST_SI_CL0_SRAM_PHYS_BASE = 0x4000120000000
HOST_SI_CL1_SRAM_PHYS_BASE = 0x4000140000000
HOST_SI_SRAM_WINDOW_SIZE = 0x01000000
HOST_SI_CONTROL_WINDOW_SIZE = 0x00010000
HOST_AP_SI_SCMI_MHU_PBX_PHYS_BASE = 0x400003B080000
HOST_AP_SI_SCMI_MHU_MBX_PHYS_BASE = 0x400003B0C0000
HOST_AP_SI_MHU_FRAME_SIZE = 0x00030000
HOST_AP_SI_PFDI_MONITOR_MHU_PBX_PHYS_BASE = 0x400003B380000
HOST_AP_SCMI_PFDI_MONITOR_BASE = HOST_AP_SCMI_PAYLOAD_BASE + 0x00000100
HOST_AP_SCMI_PFDI_MONITOR_STRIDE = 40
HOST_AP_SCMI_PFDI_MONITOR_CHANNELS = 16
HOST_AP_ATU_PHYS_BASE = 0x20000D0080000
HOST_SMDEXP2SMD_ATU_PHYS_BASE = 0x20000D0070000
HOST_CSS_COUNTERS_TIMERS_PHYS_BASE = 0x20000D0100000
HOST_CSS_COUNTERS_TIMERS_SIZE = 0x00030000
HOST_SYSTOP_PIK_PHYS_BASE = 0x20000D0200000
HOST_SMCF_SRAM_PHYS_BASE = 0x2000060000000
HOST_SMCF_SRAM_SIZE = 0x00002000
HOST_AP_RSE_MHU_PHYS_BASE = 0x300001B600000
HOST_AP_RSE_MHU_SIZE = 0x00060000
MHU_V3_FRAME_SIZE = 0x00030000
HOST_AP_MHU_POINTER_ACCESS_PHYS_BASE = 0x0FFFE0000
HOST_AP_MHU_POINTER_ACCESS_SIZE = 0x00020000
HOST_AP_RSE_MAILBOX_PHYS_BASE = 0xFFFFC000
HOST_AP_RSE_MAILBOX_SIZE = 0x00004000

-- TF-A RD-Aspen BL2 expects the SCP-created SDS region at ARM_SHARED_RAM_BASE.
-- Seed the FVP-observed region descriptor and reset-syndrome structure so
-- measured boot follows the same cold-reset path as the Arm FVP logs.
HOST_AP_SDS_REGION_DATA = {
    0x1007AA7A, HOST_AP_SDS_MEM_SIZE, -- signature, 7 structures, schema 1.0
    0x01000001, 0x00000011, 0x00000000, 0x00000000, -- AP CPU info
    0x01000002, 0x00000011, 0x00000000, 0x00000000, -- ROM version
    0x01000003, 0x00000011, 0x00000000, 0x00000000, -- RAM version
    0x01000004, 0x00000011, 0x20000000, 0x00000003, -- cfg2, FVP
    0x01000005, 0x00000011, 0x00000000, 0x00000000, -- reset syndrome
    0x01000006, 0x00000011, 0x00000007, 0x00000000, -- SCP/DMC/msg ready
    0x01000009, 0x00000021, 0x00000000, 0x00000000, -- SCP image metadata
    0x00000000, 0x00000000,
}

-- TF-A RD-Aspen certificates in the AP FIP carry trusted/non-trusted NV
-- counter values 31 and 223 respectively.
-- The platform implementation treats the backing counters as read-only, so
-- matching the FVP-visible values is required before authenticated loading.
HOST_AP_TRUSTED_NVCTR_DATA = {
    0x0000001F, -- TFW_NVCTR_BASE, TFW_NVCTR_VAL
    0x000000DF, -- NTFW_CTR_BASE, NTFW_NVCTR_VAL
}

function rse_tcm_aliases(split_cpu0_alias, ns_address, cpu0_s_address, cpu0_ns_address, size)
    local aliases = {
        ns = {
            address = ns_address;
            size = size;
        };
    }

    if not split_cpu0_alias then
        aliases.cpu0_s = {
            address = cpu0_s_address;
            size = size;
        }
        aliases.cpu0_ns = {
            address = cpu0_ns_address;
            size = size;
        }
    end

    return aliases
end

function ap_smmu_component()
    if smmu_backend == "systemc-mmu720ae" then
        return {
            moduletype = "mmu720ae";
            mem = {
                address = 0x1C0000000;
                size = 0x08000000;
                bind = "&host_router.initiator_socket";
            };
            downstream_socket = {bind = "&host_router.target_socket"};
            ptw_socket = {bind = "&host_router.target_socket"};
            irq_combined = {bind = "&ap_gic.spi_in_65"};
            stage = "1";
            profile = "zena-css-cfg2";
        }
    end

    return {
        moduletype = "arm_smmuv3";
        args = {"&platform.ap_qemu_inst", "&platform.ap_gpex_0"};
        mem = {
            address = 0x1C0000000;
            size = 0x08000000;
            bind = "&host_router.initiator_socket";
        };
        irq_out_0 = {bind = "&ap_gic.spi_in_65"};
        stage = "1";
    }
end

local config = {}

function lower_decode_priority(target, priority)
    if target ~= nil then
        target.priority = priority
    end
end

function config.create(apollo_dir)
    return {
        apollo_dir = apollo_dir;
        apollo_root = root;
        apollo_si_mode = apollo_si_mode;
        apollo_live_cl0 = apollo_live_cl0;
        apollo_live_cl1 = apollo_live_cl1;
        APOLLO_SI_CL1_HIPC_SHARED_BASE = 0xe0130000;
        APOLLO_SI_CL1_HIPC_SHARED_SIZE = 0x00080000;
        getenv_or = getenv_or;
        getenv_number_or = getenv_number_or;
        getenv_bool_or = getenv_bool_or;
        lower_decode_priority = lower_decode_priority;
        config = {
            rse = {qemu_args = qemu_args};
            ap = {enable_cpus = enable_ap_cpus; qemu_args = ap_qemu_args};
            ros = {virtio = ap_virtio};
            system_mgmt = {};
            si_cl0 = {};
            si_cl1 = {};
        };
    }
end

return config
