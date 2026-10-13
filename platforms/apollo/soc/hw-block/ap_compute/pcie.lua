local AP_GPEX_REQUESTER_ID = 0x40
-- GIC SPI constants are zero-based SPI socket indices (not architectural INTIDs).
local AP_GPEX_PIO_BASE = 0x60200000
local AP_GPEX_MMIO_BASE = 0x60300000
local AP_GPEX_ECAM_BASE = 0x43B50000
local AP_GPEX_MMIO_HIGH_BASE = 0x400000000
local AP_PCIE_EPC_BASE = 0x30300000
local AP_PCIE_EPC_OUTBOUND_BASE = 0x30400000
local AP_GPEX_PIO_SIZE = 0x00100000
local AP_GPEX_MMIO_SIZE = 0x1FD00000
local AP_GPEX_ECAM_SIZE = 0x10000000
local AP_GPEX_MMIO_HIGH_SIZE = 0x200000000
local AP_PCIE_EPC_SIZE = 0x1000
local AP_PCIE_EPC_OUTBOUND_SIZE = 0x400000
local AP_GPEX_SPIS = {300; 301; 302; 303}

local pcie = {}

function pcie.define(ctx, platform)
    local options = ctx.options

    assert(platform.ap_gpex_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_gpex_0 = {
            moduletype = "qemu_gpex";
            args = {ctx.instances.ap};
            request_origin_id = ctx.request_context.origin.ap_gpex;
            request_domain_id = ctx.request_context.domain.ap;
            requester_id = AP_GPEX_REQUESTER_ID;
            pci_requester_id = true;
            bus_master = {bind = "&system_router.target_socket"};
            pio_iface = {
                address = AP_GPEX_PIO_BASE;
                size = AP_GPEX_PIO_SIZE;
                bind = "&system_router.initiator_socket";
            };
            mmio_iface = {
                address = AP_GPEX_MMIO_BASE;
                size = AP_GPEX_MMIO_SIZE;
                bind = "&system_router.initiator_socket";
            };
            ecam_iface = {
                address = AP_GPEX_ECAM_BASE;
                size = AP_GPEX_ECAM_SIZE;
                bind = "&system_router.initiator_socket";
            };
            mmio_iface_high = {
                address = AP_GPEX_MMIO_HIGH_BASE;
                size = AP_GPEX_MMIO_HIGH_SIZE;
                bind = "&system_router.initiator_socket";
            };
            irq_out_0 = {bind = "&ap_gic.spi_in_" .. AP_GPEX_SPIS[1]};
            irq_out_1 = {bind = "&ap_gic.spi_in_" .. AP_GPEX_SPIS[2]};
            irq_out_2 = {bind = "&ap_gic.spi_in_" .. AP_GPEX_SPIS[3]};
            irq_out_3 = {bind = "&ap_gic.spi_in_" .. AP_GPEX_SPIS[4]};
        }
    end
    -- Port-group 1 has a local EPC register bank even in its default RC mode.
    -- Without the linked PCI function, the EPC reports EP mode inactive.
    assert(platform.ap_pcie_epc == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_pcie_epc = {
            moduletype = "qemu_pcie_epc";
            args = {ctx.instances.ap};
            request_origin_id = ctx.request_context.origin.ap_pcie_epc;
            request_domain_id = ctx.request_context.domain.ap;
            regs = {
                address = AP_PCIE_EPC_BASE;
                size = AP_PCIE_EPC_SIZE;
                bind = "&system_router.initiator_socket";
            };
            outbound = {
                address = AP_PCIE_EPC_OUTBOUND_BASE;
                size = AP_PCIE_EPC_OUTBOUND_SIZE;
                bind = "&system_router.initiator_socket";
            };
            local_master = {bind = "&system_router.target_socket"};
        }
    end

    -- Only x2 port-group 2 supports x1+x1 bifurcation; group 3 stays x2.
    if options.enable_ap_cpus then
        local bifurcation = options.pcie_bifurcation
        assert(bifurcation == "none" or bifurcation == "port2",
               "QBOX_APOLLO_PCIE_BIFURCATION must be none or port2")
        local split2 = bifurcation == "port2"
        local widths = {4, 4, split2 and 1 or 2, 2}
        if split2 then widths[5] = 1 end
        for slot = 1, 5 do
            local width = widths[slot]
            if width then
                local port_name = "ap_pcie_root_port_" .. slot
                assert(platform[port_name] == nil, "duplicate Apollo object")
                platform[port_name] = {
                    moduletype = "qemu_pcie_root_port";
                    args = {ctx.instances.ap, "&platform.ap_gpex_0"};
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
