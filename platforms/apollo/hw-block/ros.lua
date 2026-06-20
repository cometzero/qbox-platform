local ros = {}

function ros.define(ctx, platform)
    platform.ap_virtioblk_0 = enable_ap_cpus and {
        moduletype = "virtio_mmio_blk";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = ap_virtio.block_base[1];
            size = ap_virtio.mmio_size;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ap_virtio.block_irq[1]};
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
            address = ap_virtio.block_base[2];
            size = ap_virtio.mmio_size;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ap_virtio.block_irq[2]};
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
            address = ap_virtio.block_base[3];
            size = ap_virtio.mmio_size;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ap_virtio.block_irq[3]};
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
            address = ap_virtio.block_base[4];
            size = ap_virtio.mmio_size;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ap_virtio.block_irq[4]};
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
            address = ap_virtio.net_base;
            size = ap_virtio.mmio_size;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ap_virtio.net_irq};
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
            address = ap_virtio.rng_base;
            size = ap_virtio.mmio_size;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_"..ap_virtio.rng_irq};
        trace = ap_virtio.trace;
        trace_file = ap_virtio.trace_file;
        trace_limit = ap_virtio.trace_limit;
        trace_filter = ap_virtio.trace_filter;
    } or nil


    platform.ap_rtc_0 = enable_ap_cpus and {
        moduletype = "pl031";
        args = {"&platform.ap_qemu_inst"};
        mem = {
            address = 0x300D0000;
            size = 0x00010000;
            bind = "&host_router.initiator_socket";
            mirror_4k_aperture = true;
        };
        irq_out = {bind = "&ap_gic.spi_in_268"};
    } or nil


end


ros.peripherals = {
    system = {
        registers = {base = 0x30000000, size = 0x10000, modeled = false};
    };
    virtio = {
        p9 = {base = 0x30010000, size = 0x10000, irq = 288, modeled = false};
        block = {
            {name = "ap_virtioblk_0", base = 0x30020000, size = 0x10000, irq = 289, modeled = true};
            {name = "ap_virtioblk_1", base = 0x30030000, size = 0x10000, irq = 290, modeled = true};
            {name = "ap_virtioblk_2", base = 0x30040000, size = 0x10000, irq = 291, modeled = true};
            {name = "ap_virtioblk_3", base = 0x30050000, size = 0x10000, irq = 292, modeled = true};
        };
        net = {name = "ap_virtionet_0", base = 0x30060000, size = 0x10000, irq = 293, modeled = true};
        rng = {name = "ap_virtiorng_0", base = 0x30080000, size = 0x10000, irq = 295, modeled = true};
    };
    safety = {
        vsi = {
            {base = 0x30090000, size = 0x10000, irq = 296, modeled = false};
            {base = 0x300a0000, size = 0x10000, irq = 297, modeled = false};
        };
    };
    time = {
        rtc = {name = "ap_rtc_0", base = 0x300d0000, size = 0x10000, irq = 300, modeled = true};
    };
    console = {
        uart = {
            {base = 0x300e0000, size = 0x10000, irq = 301, modeled = false};
            {base = 0x300f0000, size = 0x10000, irq = 302, modeled = false};
        };
    };
}

function ros.bind_ap_view_targets(platform, bind_ap_target)
    for i=0,3 do
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
    for i=0,3 do
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
