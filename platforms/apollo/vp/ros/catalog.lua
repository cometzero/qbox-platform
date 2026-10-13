local function create(hw, vp)
    local ROS_MMIO_SIZE = hw.ROS_MMIO_SIZE
    local ROS_SYSTEM_REGISTERS_BASE = hw.ROS_SYSTEM_REGISTERS_BASE
    local ROS_VIRTIO_P9_BASE = vp.ROS_VIRTIO_P9_BASE
    local ROS_VIRTIO_P9_INTID = vp.ROS_VIRTIO_P9_INTID
    local ROS_VIRTIO_BLOCK_BASES = vp.ROS_VIRTIO_BLOCK_BASES
    local ROS_VIRTIO_BLOCK_INTIDS = vp.ROS_VIRTIO_BLOCK_INTIDS
    local ROS_VIRTIO_NET_BASE = vp.ROS_VIRTIO_NET_BASE
    local ROS_VIRTIO_NET_INTID = vp.ROS_VIRTIO_NET_INTID
    local ROS_VIRTIO_RNG_BASE = vp.ROS_VIRTIO_RNG_BASE
    local ROS_VIRTIO_RNG_INTID = vp.ROS_VIRTIO_RNG_INTID
    local ROS_VSI_BASES = hw.ROS_VSI_BASES
    local ROS_VSI_INTIDS = hw.ROS_VSI_INTIDS
    local ROS_RTC_BASE = hw.ROS_RTC_BASE
    local ROS_RTC_INTID = hw.ROS_RTC_INTID
    local ROS_UART_BASES = hw.ROS_UART_BASES
    local ROS_UART_INTIDS = hw.ROS_UART_INTIDS
    local ROS_DW_I2C_BASES = hw.ROS_DW_I2C_BASES
    local ROS_DW_I2C_INTIDS = hw.ROS_DW_I2C_INTIDS
    local ROS_DW_SSI_BASES = hw.ROS_DW_SSI_BASES
    local ROS_DW_SSI_INTIDS = hw.ROS_DW_SSI_INTIDS
    local ROS_DW_UART_BASES = hw.ROS_DW_UART_BASES
    local ROS_DW_UART_INTIDS = hw.ROS_DW_UART_INTIDS
    local ROS_DW_I2S_BASES = hw.ROS_DW_I2S_BASES
    local ROS_DW_I2S_INTIDS = hw.ROS_DW_I2S_INTIDS
    local ROS_DMA350_BASE = hw.ROS_DMA350_BASE
    local ROS_DMA350_CHANNELS = hw.ROS_DMA350_CHANNELS
    local ROS_DMA350_TRIGGERS = hw.ROS_DMA350_TRIGGERS
    local ROS_DMA350_NONSEC_SPI = hw.ROS_DMA350_NONSEC_SPI
    local ROS_I2S_DMA_BASE = hw.ROS_I2S_DMA_BASE
    local ROS_I2S_DMA_SPI = hw.ROS_I2S_DMA_SPI

    local peripherals = {
        dma = {
            name = "dma350_0";
            base = ROS_DMA350_BASE;
            size = ROS_MMIO_SIZE;
            channels = ROS_DMA350_CHANNELS;
            trigger_inputs = ROS_DMA350_TRIGGERS;
            dedicated_channel_requests = true;
            first_irq = ROS_DMA350_NONSEC_SPI + 32;
            interrupt_count = 1;
            modeled = true;
        };
        i2s_dma = {
            name = "dma350_1";
            base = ROS_I2S_DMA_BASE;
            size = ROS_MMIO_SIZE;
            channels = 8;
            trigger_inputs = 8;
            first_irq = ROS_I2S_DMA_SPI + 32;
            interrupt_count = 1;
            modeled = true;
        };
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
            i2s = {
                {name = "ap_dw_i2s_0"; base = ROS_DW_I2S_BASES[1]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2S_INTIDS[1]; dma_tx = 0; dma_rx = 1; modeled = true};
                {name = "ap_dw_i2s_1"; base = ROS_DW_I2S_BASES[2]; size = ROS_MMIO_SIZE; irq = ROS_DW_I2S_INTIDS[2]; dma_tx = 2; dma_rx = 3; modeled = true};
            };
        };
    }

    return peripherals
end

return {create = create}
