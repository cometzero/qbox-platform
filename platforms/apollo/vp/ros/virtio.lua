-- QVP extensions; these assignments do not describe physical RoS peripherals.
local virtio = {}

local ROS_BLOCK_DEVICE_COUNT = 4
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

function virtio.define(ctx, platform, hw)
    local options = ctx.options
    local ROS_MMIO_SIZE = hw.ROS_MMIO_SIZE

    assert(platform.ap_virtioblk_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_virtioblk_0 = {
            moduletype = "virtio_mmio_blk";
            args = {"&platform.ap_qemu_inst"};
            mem = {
                address = ROS_VIRTIO_BLOCK_BASES[1];
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
                mirror_4k_aperture = true;
            };
            irq_out = {bind = "&ap_gic.spi_in_" .. ROS_VIRTIO_BLOCK_SPIS[1]};
            blkdev_str = "file=" .. options.ap_virtio.disk_image .. ",format=raw,if=none,cache=writeback";
            trace = options.ap_virtio.trace;
            trace_file = options.ap_virtio.trace_file;
            trace_limit = options.ap_virtio.trace_limit;
            trace_filter = options.ap_virtio.trace_filter;
        }
    end

    assert(platform.ap_virtioblk_1 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_virtioblk_1 = {
            moduletype = "virtio_mmio_blk";
            args = {"&platform.ap_qemu_inst"};
            mem = {
                address = ROS_VIRTIO_BLOCK_BASES[2];
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
                mirror_4k_aperture = true;
            };
            irq_out = {bind = "&ap_gic.spi_in_" .. ROS_VIRTIO_BLOCK_SPIS[2]};
            blkdev_str = "file=" .. options.ap_virtio.extra_disk_images[1] .. ",format=raw,if=none,cache=writeback";
            trace = options.ap_virtio.trace;
            trace_file = options.ap_virtio.trace_file;
            trace_limit = options.ap_virtio.trace_limit;
            trace_filter = options.ap_virtio.trace_filter;
        }
    end

    assert(platform.ap_virtioblk_2 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_virtioblk_2 = {
            moduletype = "virtio_mmio_blk";
            args = {"&platform.ap_qemu_inst"};
            mem = {
                address = ROS_VIRTIO_BLOCK_BASES[3];
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
                mirror_4k_aperture = true;
            };
            irq_out = {bind = "&ap_gic.spi_in_" .. ROS_VIRTIO_BLOCK_SPIS[3]};
            blkdev_str = "file=" .. options.ap_virtio.extra_disk_images[2] .. ",format=raw,if=none,cache=writeback";
            trace = options.ap_virtio.trace;
            trace_file = options.ap_virtio.trace_file;
            trace_limit = options.ap_virtio.trace_limit;
            trace_filter = options.ap_virtio.trace_filter;
        }
    end

    assert(platform.ap_virtioblk_3 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_virtioblk_3 = {
            moduletype = "virtio_mmio_blk";
            args = {"&platform.ap_qemu_inst"};
            mem = {
                address = ROS_VIRTIO_BLOCK_BASES[4];
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
                mirror_4k_aperture = true;
            };
            irq_out = {bind = "&ap_gic.spi_in_" .. ROS_VIRTIO_BLOCK_SPIS[4]};
            blkdev_str = "file=" .. options.ap_virtio.extra_disk_images[3] .. ",format=raw,if=none,cache=writeback";
            trace = options.ap_virtio.trace;
            trace_file = options.ap_virtio.trace_file;
            trace_limit = options.ap_virtio.trace_limit;
            trace_filter = options.ap_virtio.trace_filter;
        }
    end

    assert(platform.ap_virtionet_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_virtionet_0 = {
            moduletype = "virtio_mmio_net";
            args = {"&platform.ap_qemu_inst"};
            mem = {
                address = ROS_VIRTIO_NET_BASE;
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
                mirror_4k_aperture = true;
            };
            irq_out = {bind = "&ap_gic.spi_in_" .. ROS_VIRTIO_NET_SPI};
            netdev_str = options.ap_virtio.netdev;
            trace = options.ap_virtio.trace;
            trace_file = options.ap_virtio.trace_file;
            trace_limit = options.ap_virtio.trace_limit;
            trace_filter = options.ap_virtio.trace_filter;
        }
    end

    assert(platform.ap_virtiorng_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_virtiorng_0 = {
            moduletype = "virtio_mmio_rng";
            args = {"&platform.ap_qemu_inst"};
            mem = {
                address = ROS_VIRTIO_RNG_BASE;
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
                mirror_4k_aperture = true;
            };
            irq_out = {bind = "&ap_gic.spi_in_" .. ROS_VIRTIO_RNG_SPI};
            trace = options.ap_virtio.trace;
            trace_file = options.ap_virtio.trace_file;
            trace_limit = options.ap_virtio.trace_limit;
            trace_filter = options.ap_virtio.trace_filter;
        }
    end
end

virtio.map = {
    ROS_BLOCK_DEVICE_COUNT = ROS_BLOCK_DEVICE_COUNT;
    ROS_VIRTIO_P9_BASE = ROS_VIRTIO_P9_BASE;
    ROS_VIRTIO_P9_INTID = ROS_VIRTIO_P9_INTID;
    ROS_VIRTIO_BLOCK_BASES = ROS_VIRTIO_BLOCK_BASES;
    ROS_VIRTIO_BLOCK_SPIS = ROS_VIRTIO_BLOCK_SPIS;
    ROS_VIRTIO_BLOCK_INTIDS = ROS_VIRTIO_BLOCK_INTIDS;
    ROS_VIRTIO_NET_BASE = ROS_VIRTIO_NET_BASE;
    ROS_VIRTIO_NET_SPI = ROS_VIRTIO_NET_SPI;
    ROS_VIRTIO_NET_INTID = ROS_VIRTIO_NET_INTID;
    ROS_VIRTIO_RNG_BASE = ROS_VIRTIO_RNG_BASE;
    ROS_VIRTIO_RNG_SPI = ROS_VIRTIO_RNG_SPI;
    ROS_VIRTIO_RNG_INTID = ROS_VIRTIO_RNG_INTID;
}

function virtio.bind_ap_view_targets(platform, bind_ap_target)
    for i = 0, (4-1) do
        local virtio = platform["ap_virtioblk_" .. i]
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
end

function virtio.lower_decode_priorities(platform, lower_decode_priority, priority)
    for i = 0, (4-1) do
        local virtio = platform["ap_virtioblk_" .. i]
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
end

return virtio
