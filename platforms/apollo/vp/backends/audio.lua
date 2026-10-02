-- SPDX-License-Identifier: BSD-3-Clause
-- Native DMA350/I2S backend adaptation; preserve routed MMIO and IRQ edges.
local audio = {}

function audio.connect(platform)
    if not platform.dma350_0 then return end
    for i=0,1 do
        local old_dma = platform["dma350_"..i]
        local dma = {
            moduletype = "qemu_dma350";
            args = {"&platform.ap_qemu_inst"};
            target_socket = old_dma.target_socket;
            irq_comb_nonsec = old_dma.irq_comb_nonsec;
        }
        if i == 0 then
            for channel=0,7 do
                dma["trig_ack_"..channel] = old_dma["trig_ack_"..channel]
            end
        end
        platform["dma350_"..i] = dma
        local old_i2s = platform["ap_dw_i2s_"..i]
        platform["ap_dw_i2s_"..i] = {
            moduletype = "qemu_dw_apb_i2s";
            args = {"&platform.ap_qemu_inst"};
            master_mode = i == 0;
            peer = "platform.ap_dw_i2s_"..(1 - i);
            dma_controller = "platform.dma350_1";
            tx_trigger = 2 * i;
            rx_trigger = 2 * i + 1;
            target_socket = old_i2s.target_socket;
            irq = old_i2s.irq;
        }
    end
    -- Native audio has fixed routing and no pinmux gate input.
    platform.pinctrl_peri0.peripheral_enable_14 = nil
    platform.pinctrl_peri0.peripheral_enable_15 = nil
    print("Apollo QVP audio: qemu-components arm-dma350 + dw-apb-i2s")
end

return audio
