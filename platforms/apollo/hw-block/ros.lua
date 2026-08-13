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
}

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
end

return ros
