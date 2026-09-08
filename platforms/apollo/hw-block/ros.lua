local ros = {}

local ROS_BLOCK_DEVICE_COUNT = 4
local ROS_MMIO_SIZE = 0x00010000
local ROS_SYSTEM_REGISTERS_BASE = 0x30000000
local ROS_VIRTIO_P9_BASE = 0x30010000
local ROS_VIRTIO_P9_INTID = 288
local ROS_VIRTIO_BLOCK_BASES = {
    0x30020000;
    0x30030000;
    0x30040000;
    0x30050000;
}
local ROS_VIRTIO_BLOCK_SPIS = {257; 258; 259; 260}
local ROS_VIRTIO_BLOCK_INTIDS = {289; 290; 291; 292}
local ROS_VIRTIO_NET_BASE = 0x30060000
local ROS_VIRTIO_NET_SPI = 261
local ROS_VIRTIO_NET_INTID = 293
local ROS_VIRTIO_RNG_BASE = 0x30080000
local ROS_VIRTIO_RNG_SPI = 263
local ROS_VIRTIO_RNG_INTID = 295
local ROS_VSI_BASES = {0x30090000; 0x300A0000}
local ROS_VSI_INTIDS = {296; 297}
local ROS_RTC_BASE = 0x300D0000
local ROS_RTC_SPI = 268
local ROS_RTC_INTID = 300
local ROS_UART_BASES = {0x300E0000; 0x300F0000}
local ROS_UART_INTIDS = {301; 302}
local ROS_DW_I2C_BASES = {
    0x30100000;
    0x30110000;
    0x30120000;
    0x30130000;
    0x30140000;
    0x30150000;
}
local ROS_DW_I2C_SPIS = {320; 321; 322; 323; 324; 325}
local ROS_DW_I2C_INTIDS = {352; 353; 354; 355; 356; 357}
local ROS_DW_SSI_BASES = {
    0x30160000;
    0x30170000;
    0x30180000;
    0x30190000;
}
local ROS_DW_SSI_SPIS = {326; 327; 328; 329}
local ROS_DW_SSI_INTIDS = {358; 359; 360; 361}
local ROS_DW_UART_BASES = {
    0x301A0000;
    0x301B0000;
    0x301C0000;
    0x301D0000;
}
local ROS_DW_UART_SPIS = {330; 331; 332; 333}
local ROS_DW_UART_INTIDS = {362; 363; 364; 365}

function ros.define(ctx, platform)
    platform.ap_virtioblk_0 = enable_ap_cpus and {
        moduletype = "virtio_mmio_blk";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ROS_VIRTIO_BLOCK_BASES[1];
            size = ROS_MMIO_SIZE;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ROS_VIRTIO_BLOCK_SPIS[1]};
        blkdev_str = "file="..ap_virtio.disk_image..",format=raw,if=none,cache=writeback";
        trace = ap_virtio.trace;
        trace_file = ap_virtio.trace_file;
        trace_limit = ap_virtio.trace_limit;
        trace_filter = ap_virtio.trace_filter;
    } or nil

    platform.ap_virtioblk_1 = enable_ap_cpus and {
        moduletype = "virtio_mmio_blk";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ROS_VIRTIO_BLOCK_BASES[2];
            size = ROS_MMIO_SIZE;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ROS_VIRTIO_BLOCK_SPIS[2]};
        blkdev_str = "file="..ap_virtio.extra_disk_images[1]..",format=raw,if=none,cache=writeback";
        trace = ap_virtio.trace;
        trace_file = ap_virtio.trace_file;
        trace_limit = ap_virtio.trace_limit;
        trace_filter = ap_virtio.trace_filter;
    } or nil

    platform.ap_virtioblk_2 = enable_ap_cpus and {
        moduletype = "virtio_mmio_blk";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ROS_VIRTIO_BLOCK_BASES[3];
            size = ROS_MMIO_SIZE;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ROS_VIRTIO_BLOCK_SPIS[3]};
        blkdev_str = "file="..ap_virtio.extra_disk_images[2]..",format=raw,if=none,cache=writeback";
        trace = ap_virtio.trace;
        trace_file = ap_virtio.trace_file;
        trace_limit = ap_virtio.trace_limit;
        trace_filter = ap_virtio.trace_filter;
    } or nil

    platform.ap_virtioblk_3 = enable_ap_cpus and {
        moduletype = "virtio_mmio_blk";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ROS_VIRTIO_BLOCK_BASES[4];
            size = ROS_MMIO_SIZE;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ROS_VIRTIO_BLOCK_SPIS[4]};
        blkdev_str = "file="..ap_virtio.extra_disk_images[3]..",format=raw,if=none,cache=writeback";
        trace = ap_virtio.trace;
        trace_file = ap_virtio.trace_file;
        trace_limit = ap_virtio.trace_limit;
        trace_filter = ap_virtio.trace_filter;
    } or nil

    platform.ap_virtionet_0 = enable_ap_cpus and {
        moduletype = "virtio_mmio_net";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ROS_VIRTIO_NET_BASE;
            size = ROS_MMIO_SIZE;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ROS_VIRTIO_NET_SPI};
        netdev_str = ap_virtio.netdev;
        trace = ap_virtio.trace;
        trace_file = ap_virtio.trace_file;
        trace_limit = ap_virtio.trace_limit;
        trace_filter = ap_virtio.trace_filter;
    } or nil

    platform.ap_virtiorng_0 = enable_ap_cpus and {
        moduletype = "virtio_mmio_rng";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ROS_VIRTIO_RNG_BASE;
            size = ROS_MMIO_SIZE;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ROS_VIRTIO_RNG_SPI};
        trace = ap_virtio.trace;
        trace_file = ap_virtio.trace_file;
        trace_limit = ap_virtio.trace_limit;
        trace_filter = ap_virtio.trace_filter;
    } or nil

    platform.ap_rtc_0 = enable_ap_cpus and {
        moduletype = "pl031";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ROS_RTC_BASE;
            size = ROS_MMIO_SIZE;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ROS_RTC_SPI};
    } or nil

    for i=0,5 do
        local controller = "ap_dw_i2c_"..i
        local eeprom = controller.."_eeprom"
        platform[controller] = enable_ap_cpus and {
            moduletype = "dw_apb_i2c";
            dylib_path = "dw-apb-i2c";
            target_socket = {
                address = ROS_DW_I2C_BASES[i + 1];
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
            };
            i2c_socket = {bind = "&"..eeprom..".i2c_socket"};
            irq = {bind = "&ap_gic.spi_in_"..ROS_DW_I2C_SPIS[i + 1]};
        } or nil
        platform[eeprom] = enable_ap_cpus and {
            moduletype = "dw_i2c_eeprom";
            dylib_path = "dw-apb-i2c";
            address = 0x50;
            size = 256;
            address_width = 8;
            page_size = 8;
        } or nil
    end

    for i=0,3 do
        platform["ap_dw_ssi_"..i] = enable_ap_cpus and {
            moduletype = "dw_apb_ssi";
            dylib_path = "dw-apb-ssi";
            clock_frequency_hz = 24000000;
            fifo_depth = 16;
            num_chip_selects = 1;
            target_socket = {
                address = ROS_DW_SSI_BASES[i + 1];
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
            };
            irq = {bind = "&ap_gic.spi_in_"..ROS_DW_SSI_SPIS[i + 1]};
        } or nil
    end

    for i=0,3 do
        local uart = "ap_dw_uart_"..i
        platform[uart] = enable_ap_cpus and {
            moduletype = "dw_apb_uart";
            dylib_path = "dw-apb-uart";
            clock_frequency_hz = 24000000;
            target_socket = {
                address = ROS_DW_UART_BASES[i + 1];
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
            };
            irq = {bind = "&ap_gic.spi_in_"..ROS_DW_UART_SPIS[i + 1]};
        } or nil
        if i == 0 or i == 2 then
            platform[uart].backend_socket = {
                bind = "&ap_dw_uart_"..(i + 1)..".backend_socket";
            }
        end
    end

end

ros.peripherals = {
    system = {
        registers = {
            base = ROS_SYSTEM_REGISTERS_BASE;
            size = ROS_MMIO_SIZE;
            modeled = false;
        };
    };
    virtio = {
        p9 = {
            base = ROS_VIRTIO_P9_BASE;
            size = ROS_MMIO_SIZE;
            irq = ROS_VIRTIO_P9_INTID;
            modeled = false;
        };
        block = {
            {name = "ap_virtioblk_0"; base = ROS_VIRTIO_BLOCK_BASES[1]; size = ROS_MMIO_SIZE; irq = ROS_VIRTIO_BLOCK_INTIDS[1]; modeled = true};
            {name = "ap_virtioblk_1"; base = ROS_VIRTIO_BLOCK_BASES[2]; size = ROS_MMIO_SIZE; irq = ROS_VIRTIO_BLOCK_INTIDS[2]; modeled = true};
            {name = "ap_virtioblk_2"; base = ROS_VIRTIO_BLOCK_BASES[3]; size = ROS_MMIO_SIZE; irq = ROS_VIRTIO_BLOCK_INTIDS[3]; modeled = true};
            {name = "ap_virtioblk_3"; base = ROS_VIRTIO_BLOCK_BASES[4]; size = ROS_MMIO_SIZE; irq = ROS_VIRTIO_BLOCK_INTIDS[4]; modeled = true};
        };
        net = {name = "ap_virtionet_0"; base = ROS_VIRTIO_NET_BASE; size = ROS_MMIO_SIZE; irq = ROS_VIRTIO_NET_INTID; modeled = true};
        rng = {name = "ap_virtiorng_0"; base = ROS_VIRTIO_RNG_BASE; size = ROS_MMIO_SIZE; irq = ROS_VIRTIO_RNG_INTID; modeled = true};
    };
    safety = {
        vsi = {
            {base = ROS_VSI_BASES[1]; size = ROS_MMIO_SIZE; irq = ROS_VSI_INTIDS[1]; modeled = false};
            {base = ROS_VSI_BASES[2]; size = ROS_MMIO_SIZE; irq = ROS_VSI_INTIDS[2]; modeled = false};
        };
    };
    time = {
        rtc = {name = "ap_rtc_0"; base = ROS_RTC_BASE; size = ROS_MMIO_SIZE; irq = ROS_RTC_INTID; modeled = true};
    };
    console = {
        uart = {
            {base = ROS_UART_BASES[1]; size = ROS_MMIO_SIZE; irq = ROS_UART_INTIDS[1]; modeled = false};
            {base = ROS_UART_BASES[2]; size = ROS_MMIO_SIZE; irq = ROS_UART_INTIDS[2]; modeled = false};
        };
    };
    dwc = {
        i2c = {
            {name = "ap_dw_i2c_0"; base = ROS_DW_I2C_BASES[1]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2C_INTIDS[1]; modeled = true};
            {name = "ap_dw_i2c_1"; base = ROS_DW_I2C_BASES[2]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2C_INTIDS[2]; modeled = true};
            {name = "ap_dw_i2c_2"; base = ROS_DW_I2C_BASES[3]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2C_INTIDS[3]; modeled = true};
            {name = "ap_dw_i2c_3"; base = ROS_DW_I2C_BASES[4]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2C_INTIDS[4]; modeled = true};
            {name = "ap_dw_i2c_4"; base = ROS_DW_I2C_BASES[5]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2C_INTIDS[5]; modeled = true};
            {name = "ap_dw_i2c_5"; base = ROS_DW_I2C_BASES[6]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2C_INTIDS[6]; modeled = true};
        };
        ssi = {
            {name = "ap_dw_ssi_0"; base = ROS_DW_SSI_BASES[1]; size = ROS_MMIO_SIZE; irq = ROS_DW_SSI_INTIDS[1]; modeled = true};
            {name = "ap_dw_ssi_1"; base = ROS_DW_SSI_BASES[2]; size = ROS_MMIO_SIZE; irq = ROS_DW_SSI_INTIDS[2]; modeled = true};
            {name = "ap_dw_ssi_2"; base = ROS_DW_SSI_BASES[3]; size = ROS_MMIO_SIZE; irq = ROS_DW_SSI_INTIDS[3]; modeled = true};
            {name = "ap_dw_ssi_3"; base = ROS_DW_SSI_BASES[4]; size = ROS_MMIO_SIZE; irq = ROS_DW_SSI_INTIDS[4]; modeled = true};
        };
        uart = {
            {name = "ap_dw_uart_0"; base = ROS_DW_UART_BASES[1]; size = ROS_MMIO_SIZE; irq = ROS_DW_UART_INTIDS[1]; modeled = true};
            {name = "ap_dw_uart_1"; base = ROS_DW_UART_BASES[2]; size = ROS_MMIO_SIZE; irq = ROS_DW_UART_INTIDS[2]; modeled = true};
            {name = "ap_dw_uart_2"; base = ROS_DW_UART_BASES[3]; size = ROS_MMIO_SIZE; irq = ROS_DW_UART_INTIDS[3]; modeled = true};
            {name = "ap_dw_uart_3"; base = ROS_DW_UART_BASES[4]; size = ROS_MMIO_SIZE; irq = ROS_DW_UART_INTIDS[4]; modeled = true};
        };
    };
}

local function visit_dwc_targets(platform, visitor)
    for i=0,5 do
        local device = platform["ap_dw_i2c_"..i]
        if device ~= nil then
            visitor(device.target_socket)
        end
    end
    for i=0,3 do
        local ssi = platform["ap_dw_ssi_"..i]
        local uart = platform["ap_dw_uart_"..i]
        if ssi ~= nil then
            visitor(ssi.target_socket)
        end
        if uart ~= nil then
            visitor(uart.target_socket)
        end
    end
end

function ros.bind_ap_view_targets(platform, bind_ap_target)
    for i=0,(ROS_BLOCK_DEVICE_COUNT-1) do
        local virtio = platform["ap_virtioblk_"..i]
        if virtio ~= nil and virtio.mem ~= nil then
            bind_ap_target(virtio.mem)
        end
    end

    if platform.ap_virtionet_0 ~= nil and platform.ap_virtionet_0.mem ~= nil then
        bind_ap_target(platform.ap_virtionet_0.mem)
    end
    if platform.ap_virtiorng_0 ~= nil and platform.ap_virtiorng_0.mem ~= nil then
        bind_ap_target(platform.ap_virtiorng_0.mem)
    end
    if platform.ap_rtc_0 ~= nil and platform.ap_rtc_0.mem ~= nil then
        bind_ap_target(platform.ap_rtc_0.mem)
    end
    visit_dwc_targets(platform, bind_ap_target)
    if platform.pinctrl_peri0 then
        bind_ap_target(platform.pinctrl_peri0.target_socket)
    end
    if platform.pinctrl_peri1 then
        bind_ap_target(platform.pinctrl_peri1.target_socket)
    end
end

function ros.lower_decode_priorities(platform, lower_decode_priority, priority)
    for i=0,(ROS_BLOCK_DEVICE_COUNT-1) do
        local virtio = platform["ap_virtioblk_"..i]
        if virtio ~= nil and virtio.mem ~= nil then
            lower_decode_priority(virtio.mem, priority)
        end
    end

    if platform.ap_virtionet_0 ~= nil and platform.ap_virtionet_0.mem ~= nil then
        lower_decode_priority(platform.ap_virtionet_0.mem, priority)
    end
    if platform.ap_virtiorng_0 ~= nil and platform.ap_virtiorng_0.mem ~= nil then
        lower_decode_priority(platform.ap_virtiorng_0.mem, priority)
    end
    if platform.ap_rtc_0 ~= nil and platform.ap_rtc_0.mem ~= nil then
        lower_decode_priority(platform.ap_rtc_0.mem, priority)
    end
    visit_dwc_targets(platform, function(target)
        lower_decode_priority(target, priority)
    end)
    if platform.pinctrl_peri0 then
        lower_decode_priority(platform.pinctrl_peri0.target_socket, priority)
    end
    if platform.pinctrl_peri1 then
        lower_decode_priority(platform.pinctrl_peri1.target_socket, priority)
    end
end

return ros
