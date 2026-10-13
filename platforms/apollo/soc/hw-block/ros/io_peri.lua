local io_peri = {}

-- Each wired peripheral request has a dedicated channel with the same ID:
-- SPI0 TX/RX 0/1, SPI1 TX/RX 2/3, UART0 TX/RX 4/5, UART1 TX/RX 6/7,
-- A second eight-channel DMA-350 serves I2S0 TX/RX 0/1 and I2S1 TX/RX 2/3.
-- The DMA-350 channel selector remains programmable; Linux preserves this
-- one-to-one board wiring when it allocates peripheral channels.
local function bind_dma_requests(platform, device, name, tx_id)
    device.dma_tx_req = {bind = "&dma350_0.trig_in_" .. tx_id}
    device.dma_rx_req = {bind = "&dma350_0.trig_in_" .. (tx_id + 1)}
    platform.dma350_0["trig_ack_" .. tx_id] = {bind = "&" .. name .. ".dma_tx_ack"}
    platform.dma350_0["trig_ack_" .. (tx_id + 1)] = {bind = "&" .. name .. ".dma_rx_ack"}
end

function io_peri.define(ctx, platform, hw)
    local options = ctx.options
    local ROS_MMIO_SIZE = hw.ROS_MMIO_SIZE
    local ROS_RTC_BASE = hw.ROS_RTC_BASE
    local ROS_RTC_SPI = hw.ROS_RTC_SPI
    local ROS_DW_I2C_BASES = hw.ROS_DW_I2C_BASES
    local ROS_DW_I2C_SPIS = hw.ROS_DW_I2C_SPIS
    local ROS_DW_SSI_BASES = hw.ROS_DW_SSI_BASES
    local ROS_DW_SSI_SPIS = hw.ROS_DW_SSI_SPIS
    local ROS_DW_UART_BASES = hw.ROS_DW_UART_BASES
    local ROS_DW_UART_SPIS = hw.ROS_DW_UART_SPIS
    local ROS_DW_I2S_BASES = hw.ROS_DW_I2S_BASES
    local ROS_DW_I2S_SPIS = hw.ROS_DW_I2S_SPIS

    assert(platform.ap_rtc_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_rtc_0 = {
            moduletype = "pl031";
            args = {ctx.instances.ap};
            mem = {
                address = ROS_RTC_BASE;
                size = ROS_MMIO_SIZE;
                bind = "&host_router.initiator_socket";
                mirror_4k_aperture = true;
            };
            irq_out = {bind = "&ap_gic.spi_in_" .. ROS_RTC_SPI};
        }
    end

    for i = 0, 5 do
        local controller = "ap_dw_i2c_" .. i
        assert(platform[controller] == nil, "duplicate Apollo object")
        if options.enable_ap_cpus then
            platform[controller] = {
                moduletype = "dw_apb_i2c";
                dylib_path = "dw-apb-i2c";
                target_socket = {
                    address = ROS_DW_I2C_BASES[i + 1];
                    size = ROS_MMIO_SIZE;
                    bind = "&host_router.initiator_socket";
                };
                irq = {bind = "&ap_gic.spi_in_" .. ROS_DW_I2C_SPIS[i + 1]};
            }
        end
    end

    for i = 0, 3 do
        assert(platform["ap_dw_ssi_" .. i] == nil, "duplicate Apollo object")
        if options.enable_ap_cpus then
            platform["ap_dw_ssi_" .. i] = {
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
                irq = {bind = "&ap_gic.spi_in_" .. ROS_DW_SSI_SPIS[i + 1]};
            }
        end
        if platform["ap_dw_ssi_" .. i] and i < 2 then
            bind_dma_requests(platform, platform["ap_dw_ssi_" .. i], "ap_dw_ssi_" .. i, 2 * i)
        end
    end

    for i = 0, 3 do
        local uart = "ap_dw_uart_" .. i
        assert(platform[uart] == nil, "duplicate Apollo object")
        if options.enable_ap_cpus then
            platform[uart] = {
                moduletype = "dw_apb_uart";
                dylib_path = "dw-apb-uart";
                clock_frequency_hz = 24000000;
                target_socket = {
                    address = ROS_DW_UART_BASES[i + 1];
                    size = ROS_MMIO_SIZE;
                    bind = "&host_router.initiator_socket";
                };
                irq = {bind = "&ap_gic.spi_in_" .. ROS_DW_UART_SPIS[i + 1]};
            }
        end
        if platform[uart] and i < 2 then
            bind_dma_requests(platform, platform[uart], uart, 4 + 2 * i)
        end
    end

    for i = 0, 1 do
        local i2s = "ap_dw_i2s_" .. i
        assert(platform[i2s] == nil, "duplicate Apollo object")
        if options.enable_ap_cpus then
            platform[i2s] = {
                moduletype = "dw_apb_i2s";
                dylib_path = "dw-apb-i2s";
                master_mode = i == 0;
                transmitter_enabled = true;
                receiver_enabled = true;
                -- Transaction pacing for the temporally decoupled AP CPUs.
                -- Physical clock/overrun timing is tested with this disabled.
                functional_pacing = true;
                target_socket = {
                    address = ROS_DW_I2S_BASES[i + 1];
                    size = ROS_MMIO_SIZE;
                    bind = "&host_router.initiator_socket";
                };
                irq = {bind = "&ap_gic.spi_in_" .. ROS_DW_I2S_SPIS[i + 1]};
            }
        end
    end
    if platform.ap_dw_i2s_0 then
        for i = 0, 1 do
            local name = "ap_dw_i2s_" .. i
            local device = platform[name]
            device.dma_tx_req = {bind = "&dma350_1.trig_in_" .. (2 * i)}
            device.dma_rx_req = {bind = "&dma350_1.trig_in_" .. (2 * i + 1)}
            platform.dma350_1["trig_ack_" .. (2 * i)] = {bind = "&" .. name .. ".dma_tx_ack"}
            platform.dma350_1["trig_ack_" .. (2 * i + 1)] = {bind = "&" .. name .. ".dma_rx_ack"}
        end
    end
end

return io_peri
