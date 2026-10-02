local pcie = {}

function pcie.define(ctx, platform, hw, policy)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE
    local AP_IRQ = hw.AP_IRQ
    local AP_HW = hw.AP_HW

    assert(platform.ap_gpex_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_gpex_0 = {
            moduletype = "qemu_gpex";
            args = {"&platform.ap_qemu_inst"};
            request_origin_id = ctx.request_context.origin.ap_gpex;
            request_domain_id = ctx.request_context.domain.ap;
            requester_id = AP_HW.gpex_requester_id;
            pci_requester_id = not policy.pcie_irq_test_enabled;
            bus_master = {bind = "&system_router.target_socket"};
            pio_iface = {
                address = AP_ADDRESS.gpex_pio;
                size = AP_SIZE.gpex_pio;
                bind = "&system_router.initiator_socket";
            };
            mmio_iface = {
                address = AP_ADDRESS.gpex_mmio;
                size = AP_SIZE.gpex_mmio;
                bind = "&system_router.initiator_socket";
            };
            ecam_iface = {
                address = AP_ADDRESS.gpex_ecam;
                size = AP_SIZE.gpex_ecam;
                bind = "&system_router.initiator_socket";
            };
            mmio_iface_high = {
                address = AP_ADDRESS.gpex_mmio_high;
                size = AP_SIZE.gpex_mmio_high;
                bind = "&system_router.initiator_socket";
            };
            irq_out_0 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.gpex[1]};
            irq_out_1 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.gpex[2]};
            irq_out_2 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.gpex[3]};
            irq_out_3 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.gpex[4]};
        }
    end
    -- Port-group 1 has a local EPC register bank even in its default RC mode.
    -- Without the linked PCI function, the EPC reports EP mode inactive.
    assert(platform.ap_pcie_epc == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_pcie_epc = {
            moduletype = "qemu_pcie_epc";
            args = {"&platform.ap_qemu_inst"};
            request_origin_id = ctx.request_context.origin.ap_pcie_epc;
            request_domain_id = ctx.request_context.domain.ap;
            regs = {
                address = AP_ADDRESS.pcie_epc;
                size = AP_SIZE.pcie_epc;
                bind = "&system_router.initiator_socket";
            };
            outbound = {
                address = AP_ADDRESS.pcie_epc_outbound;
                size = AP_SIZE.pcie_epc_outbound;
                bind = "&system_router.initiator_socket";
            };
            local_master = {bind = "&system_router.target_socket"};
        }
    end

    -- Only x2 port-group 2 supports x1+x1 bifurcation; group 3 stays x2.
    if options.enable_ap_cpus and not policy.pcie_irq_test_enabled then
        local bifurcation = ctx.getenv_or("QBOX_APOLLO_PCIE_BIFURCATION", "none")
        assert(bifurcation == "none" or bifurcation == "port2",
               "QBOX_APOLLO_PCIE_BIFURCATION must be none or port2")
        local split2 = bifurcation == "port2"
        local widths = {4, 4, split2 and 1 or 2, 2}
        if policy.pcie_ep_loopback then widths[2] = nil end
        if split2 then widths[5] = 1 end
        for slot = 1, 5 do
            local width = widths[slot]
            if width then
                local port_name = "ap_pcie_root_port_" .. slot
                assert(platform[port_name] == nil, "duplicate Apollo object")
                platform[port_name] = {
                    moduletype = "qemu_pcie_root_port";
                    args = {"&platform.ap_qemu_inst", "&platform.ap_gpex_0"};
                    addr = string.format("%02x.0", slot);
                    chassis = slot;
                    slot = slot;
                    port = slot;
                    x_speed = "32";
                    x_width = tostring(width);
                }
            end
        end
    end
end

return pcie
