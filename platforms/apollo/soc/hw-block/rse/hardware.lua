-- RSE address, interrupt and sizing contracts; VM width is profile configurable.
local hardware = {}

function hardware.create(ctx)
    local rse_vmaddrwidth = ctx.options.rse_vmaddrwidth
    local RSE_ADDRESS = {
        rom_secure = 0x11000000;
        itcm_non_secure = 0x00000000;
        itcm_cpu0_non_secure = 0x0A000000;
        itcm_secure = 0x10000000;
        itcm_cpu0_secure = 0x1A000000;
        dtcm_non_secure = 0x20000000;
        dtcm_cpu0_non_secure = 0x24000000;
        dtcm_secure = 0x30000000;
        dtcm_cpu0_secure = 0x34000000;
        vm0_secure = 0x31000000;
        boot_flash_secure = 0xB0000000;
        host_access_non_secure = 0x60000000;
        host_access_secure = 0x70000000;
        nsacfg_non_secure = 0x40080000;
        wdog_non_secure_control = 0x48040000;
        wdog_non_secure_refresh = 0x48041000;
        dma350_secure = 0x50002000;
        sacfg_secure = 0x50080000;
        kmu_secure = 0x5009E000;
        sam_secure = 0x5009F000;
        lcm_secure = 0x500A0000;
        cpu0_secctrl_secure = 0x50011000;
        cpu0_pwrctrl_secure = 0x50012000;
        cpu0_pwrctrl_non_secure = 0x40012000;
        cpu0_identity_secure = 0x5001F000;
        cpu0_identity_non_secure = 0x4001F000;
        sic_secure = 0x50140000;
        atu_secure = 0x50150000;
        mpc_sic_secure = 0x50151000;
        cc3xx_secure = 0x50154000;
        syscounter_control_secure = 0x5015A000;
        syscounter_read_secure = 0x5015B000;
        integrity_checker_secure = 0x5015C000;
        tram_secure = 0x5015D000;
        mhu0_sender_secure = 0x50160000;
        mhu0_receiver_secure = 0x50170000;
        mhu2_sender_secure = 0x501A0000;
        mhu2_receiver_secure = 0x501B0000;
        timer0_non_secure = 0x48000000;
        timer1_non_secure = 0x48001000;
        timer2_non_secure = 0x48002000;
        timer3_non_secure = 0x48003000;
        timer0_secure = 0x58000000;
        timer1_secure = 0x58001000;
        timer2_secure = 0x58002000;
        timer3_secure = 0x58003000;
        gpio0_non_secure = 0x40100000;
        gpio1_non_secure = 0x40101000;
        gpio0_secure = 0x50100000;
        gpio1_secure = 0x50101000;
        sysctrl_secure = 0x58021000;
        wdog_secure_control = 0x58040000;
        wdog_secure_refresh = 0x58041000;
        integ_layer_secure = 0x58100000;
        mpc_vm0_secure = 0x50083000;
        mpc_vm1_secure = 0x50084000;
        otp_wrapper_secure = 0x58111000;
        nvic = 0xE000E000;
        kmu_hw_slot_export = 0x50154400;
        remote_base = 0x00000000;
    }
    local RSE_SIZE = {
        rom = 0x00020000;
        itcm = 0x00008000;
        dtcm = 0x00008000;
        vm = 2 ^ rse_vmaddrwidth;
        provisioning_offset = 0x00020000;
        boot_flash = 0x04000000;
        flash_sector = 0x00001000;
        host_access = 0x10000000;
        lcm = 0x00011000;
        local_mhu_frame = 0x00010000;
        nvic = 0x00021000; -- SCS plus the Armv8-M non-secure alias at +0x20000.
        register_window = 0x00001000;
        double_register_window = 0x00002000;
        uart_window = 0x00010000;
        remote_post_nvic = 0x00100000;
    }
    RSE_ADDRESS.vm1_secure = RSE_ADDRESS.vm0_secure + RSE_SIZE.vm
    RSE_ADDRESS.host_uart0_non_secure =
        RSE_ADDRESS.host_access_non_secure + 0x0FF00000
    RSE_ADDRESS.host_uart0_secure =
        RSE_ADDRESS.host_access_secure + 0x0FF00000
    local RSE_IRQ = {
        timer0 = 3;
        timer1 = 4;
        timer2 = 5;
        timer3 = 27;
        gpio_combined = 34;
        cmu_mhu0_receiver = 41;
        cmu_mhu2_receiver = 45;
    }
    local RSE_HW = {
        nvic_num_irq = 160;
        watchdog_clock_hz = 32000000;
        kmu_build_config = 0x003D0005;
        kmu_hw_slot_config = 0x00D60100;
        sam_build_config = 0x00000700;
        mpc_block_config = 0x00000007;
        atu_build_config = 0x000000C5;
        integrity_checker_build_config = 0x00000109;
    }

    return {RSE_ADDRESS = RSE_ADDRESS; RSE_SIZE = RSE_SIZE; RSE_IRQ = RSE_IRQ; RSE_HW = RSE_HW;}
end

return hardware
