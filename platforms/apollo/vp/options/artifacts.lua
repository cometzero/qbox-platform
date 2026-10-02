-- SPDX-License-Identifier: BSD-3-Clause
-- Host artifacts policy, evaluated once before hardware construction.
return function(options, apollo_dir)
    options.root = apollo_dir.."../../../../../"
    options.deploy = options.root.."build/tmp_baremetal/deploy/images/fvp-rd-aspen/"

    options.rse_rom = options.getenv_or("QBOX_RDASPEN_RSE_ROM", options.deploy.."rse-rom-image.img")
    options.rse_flash = options.getenv_or("QBOX_RDASPEN_RSE_FLASH", options.deploy.."rse-flash-image.img")
    options.rse_otp = options.getenv_or("QBOX_RDASPEN_RSE_OTP", options.deploy.."rse-otp-image.img")
    options.ap_flash = options.getenv_or("QBOX_RDASPEN_AP_FLASH", options.deploy.."ap-flash-image.img")
    options.flash_writeback = options.getenv_or("QBOX_RDASPEN_FLASH_WRITEBACK", "false") == "true"
    options.flash_defer_backing_flush_interval =
        options.getenv_number_or("QBOX_RDASPEN_FLASH_DEFER_FLUSH_INTERVAL", 1024)
    options.AP_BL2_ELF = options.getenv_or(
        "QBOX_RDASPEN_AP_BL2_ELF",
        options.root.."build/tmp_baremetal/work/fvp_rd_aspen-poky-linux/trusted-firmware-a/2.14.0+git/build/rdaspen/debug/bl2/bl2.elf")
    options.ap_virtio = {
        disk_image = options.getenv_or("QBOX_RDASPEN_ROOTFS", options.deploy.."baremetal-image-fvp-rd-aspen.wic");
        extra_disk_images = {
            options.getenv_or("QBOX_RDASPEN_EXTRA_BLK1", options.deploy.."efi-capsule-update-disk-image-fvp-rd-aspen.img");
            options.getenv_or("QBOX_RDASPEN_EXTRA_BLK2", options.root.."build/qbox-fvp-rd-aspen/rd-aspen-extra-blk2.raw");
            options.getenv_or("QBOX_RDASPEN_EXTRA_BLK3", options.root.."build/qbox-fvp-rd-aspen/rd-aspen-extra-blk3.raw");
        };
        netdev = options.getenv_or(
            "QBOX_APOLLO_NETDEV",
            options.getenv_or("QBOX_RDASPEN_NETDEV", "type=user,hostfwd=tcp::2222-:22"));
        trace = options.getenv_or("QBOX_RDASPEN_VIRTIO_TRACE", "false") == "true";
        trace_file = options.getenv_or("QBOX_RDASPEN_VIRTIO_TRACE_FILE", "");
        trace_limit = tonumber(options.getenv_or("QBOX_RDASPEN_VIRTIO_TRACE_LIMIT", "4096"));
        trace_filter = options.getenv_or("QBOX_RDASPEN_VIRTIO_TRACE_FILTER", "control");
    }
    options.host_si_cl0_sram_map_file = options.getenv_or(
        "QBOX_RDASPEN_HOST_SI_CL0_SRAM_MAP_FILE",
        "")
    options.host_si_cl1_sram_map_file = options.getenv_or(
        "QBOX_RDASPEN_HOST_SI_CL1_SRAM_MAP_FILE",
        "")
    options.host_ap_shared_sram_map_file = options.getenv_or(
        "QBOX_RDASPEN_HOST_AP_SHARED_SRAM_MAP_FILE",
        "")
    options.host_ap_bl2_header_sram_map_file = options.getenv_or(
        "QBOX_RDASPEN_HOST_AP_BL2_HEADER_SRAM_MAP_FILE",
        "")
    options.host_sram_shared_memory =
        options.getenv_or("QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY", "false") == "true"
    function options.host_sram_shared_memory_enabled(map_file)
        return options.host_sram_shared_memory and map_file == ""
    end
    options.provisioning_bundle = options.getenv_or(
        "QBOX_RDASPEN_PROVISIONING_BUNDLE",
        options.deploy.."combined_provisioning_message.bin")
    options.rse_log = options.getenv_or(
        "QBOX_RDASPEN_RSE_LOG",
        options.root.."build/qbox-fvp-rd-aspen/qbox-rse.log")
    options.secure_console_log = options.getenv_or(
        "QBOX_RDASPEN_SECURE_CONSOLE_LOG",
        options.root.."build/qbox-fvp-rd-aspen/qbox-secure-console.log")
    options.primary_console_log = options.getenv_or(
        "QBOX_RDASPEN_PRIMARY_CONSOLE_LOG",
        options.root.."build/qbox-fvp-rd-aspen/qbox-primary-console.log")
    options.rse_uart_read_file = options.getenv_or("QBOX_RDASPEN_UART_READ_FILE", "/dev/null")
    options.secure_uart_read_file = options.getenv_or(
        "QBOX_RDASPEN_SECURE_UART_READ_FILE",
        "/dev/null")
    options.primary_uart_read_file = options.getenv_or(
        "QBOX_RDASPEN_PRIMARY_UART_READ_FILE",
        "/dev/null")
    options.uart_poll_interval_ms = tonumber(options.getenv_or(
        "QBOX_RDASPEN_UART_POLL_INTERVAL_MS",
        "100"))
end
