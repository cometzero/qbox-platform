-- Apollo QVP shared setup for QBox.

local AP_CORES_PER_CLUSTER = 4
local AP_MAX_CLUSTERS = 4
local AP_MAX_CPUS = AP_CORES_PER_CLUSTER * AP_MAX_CLUSTERS
local MONITOR_DEFAULT_PORT = 18080
local REQUEST_DOMAIN = {
    system = 0;
    ap = 1;
    smd = 2;
    rse = 3;
    si_cl0 = 4;
    si_cl1 = 5;
}
local REQUEST_ORIGIN = {
    ap_cpu_base = 0x1000;
    ap_gpex = 0x1100;
    ap_global_peripheral = 0x1200;
    ap_loader = 0x1F00;
    rse_cpu = 0x3000;
    si_cl0_cpu_base = 0x4000;
    si_cl0_loader = 0x4F00;
    si_cl1_cpu_base = 0x5000;
    si_cl1_loader = 0x5F00;
}
local REQUEST_CAPABILITY = {
    boot_loader = 1;
    authenticated_image = 2;
}

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
    local cluster = math.floor(cpu_index / AP_CORES_PER_CLUSTER)
    local core = cpu_index % AP_CORES_PER_CLUSTER
    return (cluster * 0x10000) + (core * 0x100)
end

function ap_cpu_index(cluster, core)
    return cluster * AP_CORES_PER_CLUSTER + core
end

function ap_cpu_reset_bind_targets(count)
    local targets = {}
    for i=0,(count-1) do
        targets[#targets + 1] = "&ap_cpu_"..i..".reset"
    end
    return table.concat(targets, ";")
end

function ap_cold_reset_bind_targets()
    local targets = {
        "&ap_bl2_reset_loader.reset";
        "&host_ap_bl2_header_sram.reset";
        "&ap_reset_gpio.reset_in";
        "&host_ap_si_ns_scmi_mhu_pbx.reset";
        "&host_ap_si_ns_scmi_mhu_mbx.reset";
        "&host_ap_si_scmi_mhu_pbx.reset";
        "&host_ap_si_scmi_mhu_mbx.reset";
        "&host_ap_si_cl1_mhu_pbx.reset";
        "&host_ap_si_cl1_mhu_mbx.reset";
        "&host_ap_si_pfdi_monitor_mhu_pbx.reset";
        "&host_ap_si_pfdi_monitor_mhu_mbx.reset";
        "&host_ap_rse_mhu_pbx.reset";
        "&host_ap_rse_mhu_mbx.reset";
    }

    if smmu_backend == "systemc-mmu720ae" then
        targets[#targets + 1] = "&ap_smmu_0.reset"
    end

    return table.concat(targets, ";")
end

function ap_system_reset_bind_targets()
    local targets = {}
    for cpu=0,(AP_NUM_CPUS-1) do
        local cluster = math.floor(cpu / AP_CORES_PER_CLUSTER)
        local core = cpu % AP_CORES_PER_CLUSTER
        targets[#targets + 1] =
            "&si_cl0_ap_cluster"..cluster.."_core"..core.."_ppu.reset"
    end
    targets[#targets + 1] = ap_cold_reset_bind_targets()

    return table.concat(targets, ";")
end

function apollo_system_reset_bind_targets()
    local targets = {
        ap_system_reset_bind_targets();
        "&rse_sysctrl.reset";
        "&rse_watchdog_ns.reset";
        "&rse_watchdog_s.reset";
        "&rse_mhu0_sender_s.reset";
        "&rse_mhu0_receiver_s.reset";
        "&rse_mhu2_sender_s.reset";
        "&rse_mhu2_receiver_s.reset";
        "&host_rse_si_mhu_pbx.reset";
        "&host_rse_si_mhu_mbx.reset";
        "&host_smd_gpio_cold_reset.reset";
        "&host_smd_gpio.reset";
    }

    if rse_local_crypto then
        targets[#targets + 1] = "&rse_cpu_pass.rse_kmu_regs.reset"
        targets[#targets + 1] = "&rse_cpu_pass.rse_cc3xx.reset"
    else
        targets[#targets + 1] = "&rse_kmu_regs.reset"
        targets[#targets + 1] = "&rse_cc3xx.reset"
    end

    targets[#targets + 1] = "&rse_cpu_pass.cpu_0.accel_reset"
    targets[#targets + 1] = "&rse_sys_rss_reset_fanout.reset_in"

    targets[#targets + 1] = "&host_si_cl0_clus_ppu.reset"
    targets[#targets + 1] = "&host_si_cl0_core0_ppu.reset"
    targets[#targets + 1] = "&si_cl0_ni710ae_primary_nci.reset"
    targets[#targets + 1] = "&si_cl0_ni710ae_secondary_nci.reset"
    targets[#targets + 1] = "&si_cl0_ni710ae_mhu_nci.reset"
    targets[#targets + 1] = "&si_cl0_qemu_inst.reset"
    for _, frame in ipairs({
        "si_cl0_ap_ns_mhu_pbx";
        "si_cl0_ap_ns_mhu_mbx";
        "si_cl0_ap_scmi_mhu_pbx";
        "si_cl0_ap_scmi_mhu_mbx";
        "si_cl0_ap_pfdi_monitor_mhu_pbx";
        "si_cl0_ap_pfdi_monitor_mhu_mbx";
        "si_cl0_rse_mhu_pbx";
        "si_cl0_rse_mhu_mbx";
    }) do
        targets[#targets + 1] = "&"..frame..".reset"
    end

    targets[#targets + 1] = "&host_si_cl1_clus_ppu.reset"
    targets[#targets + 1] = "&si_cl1_cluster_ppu.reset"
    for cpu=0,3 do
        targets[#targets + 1] = "&si_cl1_core"..cpu.."_ppu.reset"
    end
    targets[#targets + 1] = "&si_cl1_qemu_inst.reset"
    for _, frame in ipairs({
        "si_cl1_hipc_mhu_pbx";
        "si_cl1_hipc_mhu_mbx";
        "si_cl1_pfdi_mhu_pbx";
        "si_cl1_pfdi_reply_mhu_mbx";
    }) do
        targets[#targets + 1] = "&"..frame..".reset"
    end
    targets[#targets + 1] = "&si_cl0_pfdi_mhu_pbx.reset"
    targets[#targets + 1] = "&si_cl0_pfdi_mhu_mbx.reset"

    return table.concat(targets, ";")
end

print("Apollo QVP shared config running...")

root = top().."../../../../../../"
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
uart_poll_interval_ms = tonumber(getenv_or(
    "QBOX_RDASPEN_UART_POLL_INTERVAL_MS",
    "100"))
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
AP_NUM_CPUS = enable_ap_cpus and
    getenv_number_or("QBOX_APOLLO_NUM_CPUS", "4") or 0
assert(not enable_ap_cpus or
       (AP_NUM_CPUS >= 1 and AP_NUM_CPUS <= AP_MAX_CPUS),
       "QBOX_APOLLO_NUM_CPUS must be 1..16 when AP CPUs are enabled")
AP_GIC_NUM_CPUS = enable_ap_cpus and AP_NUM_CPUS or 1
rse_pc_trace = getenv_or("QBOX_RDASPEN_RSE_PC_TRACE", "false") == "true"
rse_pc_trace_file = getenv_or(
    "QBOX_RDASPEN_RSE_PC_TRACE_FILE",
    root.."build/qbox-fvp-rd-aspen/rse-pc-trace.log")
rse_pc_trace_interval = tonumber(getenv_or("QBOX_RDASPEN_RSE_PC_TRACE_INTERVAL", "1"))
rse_pc_trace_limit = tonumber(getenv_or("QBOX_RDASPEN_RSE_PC_TRACE_LIMIT", "4096"))
rse_exception_trace = getenv_or("QBOX_RDASPEN_RSE_EXCEPTION_TRACE", "false") == "true"
rse_hotpath_accel = getenv_or("QBOX_RDASPEN_RSE_HOTPATH_ACCEL", "false") == "true"
rse_hotpath_max_bytes = tonumber(getenv_or("QBOX_RDASPEN_RSE_HOTPATH_MAX_BYTES", tostring(16 * 1024 * 1024)))
rse_hotpath_profile_file = getenv_or("QBOX_RDASPEN_RSE_HOTPATH_PROFILE_FILE", "")
rse_hotpath_profile_interval = tonumber(getenv_or("QBOX_RDASPEN_RSE_HOTPATH_PROFILE_INTERVAL", "1024"))
rse_lms_accel = getenv_or("QBOX_RDASPEN_RSE_LMS_ACCEL", "false") == "true"
rse_lms_max_data_bytes = tonumber(getenv_or("QBOX_RDASPEN_RSE_LMS_MAX_DATA_BYTES", tostring(16 * 1024 * 1024)))
rse_bl2_load_profile =
    getenv_or("QBOX_RDASPEN_RSE_BL2_LOAD_PROFILE", "false") == "true"
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
rse_bl2_verify_sig_max_key_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_MAX_KEY_BYTES", "512"))
rse_bl2_verify_sig_max_sig_bytes =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_VERIFY_SIG_MAX_SIG_BYTES", "128"))
rse_bl2_delay_accel =
    getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_ACCEL", "false") == "true"
rse_bl2_delay_max_cycles =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_MAX_CYCLES", "50000000"))
rse_bl2_delay_expected_hits =
    tonumber(getenv_or("QBOX_RDASPEN_RSE_BL2_DELAY_EXPECTED_HITS", "2"))
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
rse_flash_backend = getenv_or(
    "QBOX_RDASPEN_RSE_FLASH_BACKEND",
    "qemu-cfi-local")
assert(rse_flash_backend == "systemc-strata" or
       rse_flash_backend == "qemu-cfi-local",
       "QBOX_RDASPEN_RSE_FLASH_BACKEND must be systemc-strata or qemu-cfi-local")
assert(rse_flash_backend ~= "qemu-cfi-local" or rse_local_boot_flash,
       "qemu-cfi-local requires QBOX_RDASPEN_RSE_LOCAL_BOOT_FLASH=true")
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
rse_smd_counter_mirror = getenv_bool_or(
    "QBOX_APOLLO_RSE_SMD_COUNTER_MIRROR",
    true)
rse_lsc_input_hz = rse_smd_counter_mirror and 125000000 or
    getenv_number_or("QBOX_APOLLO_RSE_LSC_INPUT_HZ", "125000000")
assert(rse_lsc_input_hz > 0,
       "QBOX_APOLLO_RSE_LSC_INPUT_HZ must be greater than zero")
rse_cpuwait = getenv_number_or(
    "QBOX_RDASPEN_RSE_CPUWAIT",
    "0x0000000F")
rse_dma_boot_en = getenv_number_or(
    "QBOX_RDASPEN_RSE_DMA_BOOT_EN",
    "0x00000001")
local monitor_enabled = getenv_bool_or("QBOX_APOLLO_MONITOR", false)
local monitor_port = getenv_number_or(
    "QBOX_APOLLO_MONITOR_PORT",
    MONITOR_DEFAULT_PORT)
assert(monitor_port >= 1 and monitor_port <= 65535 and monitor_port % 1 == 0,
       "QBOX_APOLLO_MONITOR_PORT must be an integer in range 1..65535")

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
        request_context = {
            domain = REQUEST_DOMAIN;
            origin = REQUEST_ORIGIN;
            capability = REQUEST_CAPABILITY;
        };
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
            monitor = {enabled = monitor_enabled; port = monitor_port};
        };
    }
end

return config
