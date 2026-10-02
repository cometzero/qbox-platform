local dma = {}

function dma.define(ctx, platform, hw)
    local options = ctx.options
    local ROS_MMIO_SIZE = hw.ROS_MMIO_SIZE
    local ROS_DMA350_BASE = hw.ROS_DMA350_BASE
    local ROS_DMA350_CHANNELS = hw.ROS_DMA350_CHANNELS
    local ROS_DMA350_TRIGGERS = hw.ROS_DMA350_TRIGGERS
    local ROS_DMA350_NONSEC_SPI = hw.ROS_DMA350_NONSEC_SPI
    local ROS_I2S_DMA_BASE = hw.ROS_I2S_DMA_BASE
    local ROS_I2S_DMA_SPI = hw.ROS_I2S_DMA_SPI

    assert(platform.dma350_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.dma350_0 = {
            moduletype = "dma350";
            channel_count = ROS_DMA350_CHANNELS;
            trigger_count = ROS_DMA350_TRIGGERS;
            trace = options.dma350_trace;
            trace_limit = options.dma350_trace_limit;
            trace_filter = options.dma350_trace_filter;
            irq_comb_nonsec = {bind = "&ap_gic.spi_in_" .. ROS_DMA350_NONSEC_SPI};
            target_socket = {
                address = ROS_DMA350_BASE;
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
            };
            initiator_socket = {bind = "&host_router.target_socket"};
        }
    end

    assert(platform.dma350_1 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.dma350_1 = {
            moduletype = "dma350";
            channel_count = 8;
            trigger_count = 8;
            irq_comb_nonsec = {bind = "&ap_gic.spi_in_" .. ROS_I2S_DMA_SPI};
            target_socket = {
                address = ROS_I2S_DMA_BASE;
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
            };
            initiator_socket = {bind = "&host_router.target_socket"};
        }
    end
end

return dma
