local pcie = {}

function pcie.define(ctx, platform, hw, policy)
    local options = ctx.options

    if options.enable_ap_cpus and not policy.pcie_irq_test_enabled then
        for slot = 1, 5 do
            local port_name = "ap_pcie_root_port_" .. slot
            if platform[port_name] then
                assert(platform["ap_pcie_test_net_" .. slot] == nil, "duplicate Apollo object")
                if policy.pcie_test_endpoints_enabled then
                    platform["ap_pcie_test_net_" .. slot] = {
                        moduletype = "virtio_net_pci";
                        args = {"&platform.ap_qemu_inst", "&platform." .. port_name};
                        addr = "00.0";
                        mac = string.format("52:54:00:50:00:%02x", slot);
                        netdev_str = "type=user";
                        iommu_platform = true;
                    }
                end
            end
        end
        assert(platform.ap_pcie_test_ep == nil, "duplicate Apollo object")
        if policy.pcie_ep_loopback then
            platform.ap_pcie_test_ep = {
                moduletype = "qemu_pcie_test_ep";
                args = {"&platform.ap_qemu_inst", "&platform.ap_pcie_root_port_1",
                        "&platform.ap_pcie_epc"};
            }
        end
        -- Group 3 stays x2, independent of group 2's x1+x1 bifurcation.
        assert(platform.ap_nvme_0 == nil, "duplicate Apollo object")
        if policy.nvme_enabled then
            platform.ap_nvme_0 = {
                moduletype = "nvme";
                args = {"&platform.ap_qemu_inst", "&platform.ap_pcie_root_port_4"};
                image_path = policy.nvme_image;
                serial = ctx.getenv_or("QBOX_APOLLO_NVME_SERIAL", "APOLLO-NVME-SSD");
                format = "raw";
                addr = "00.0";
                x_speed = "32";
                x_width = "2";
            }
        end
    end
end

return pcie
