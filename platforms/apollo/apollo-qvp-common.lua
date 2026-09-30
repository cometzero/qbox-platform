-- Shared AP hardware construction. Domain composition belongs to each entrypoint.
local common = {}

function common.load(apollo_dir)
    local config = dofile(apollo_dir.."hw-block/config.lua")
    local ctx = config.create(apollo_dir)
    ctx.ap_compute = dofile(apollo_dir.."hw-block/ap_compute.lua")
    ctx.ros = dofile(apollo_dir.."hw-block/ros.lua")
    return ctx
end

function common.define_ap(ctx, platform)
    ctx.ap_compute.define(ctx, platform)
    ctx.ros.define(ctx, platform)
    dofile(ctx.apollo_dir.."hw-block/pinctrl.lua").define(platform)
end

function common.connect_board(ctx, platform)
    dofile(ctx.apollo_dir.."board/pca9539.lua").connect(platform)
    dofile(ctx.apollo_dir.."board/peri0-loopback.lua").connect(platform)
end

-- Apply after AP address-view routing, preserving the MMIO/IRQ contracts.
function common.use_qemu_audio(platform)
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

return common
