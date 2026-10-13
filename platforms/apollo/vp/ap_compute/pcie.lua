local pcie = {}

function pcie.define(ctx, platform, policy)
    local options = ctx.options

    if policy.pcie_irq_test_enabled then
        platform.ap_gpex_0.pci_requester_id = false
        if platform.ap_smmu_lti00 then
            platform.ap_smmu_lti00.requester_id_from_context = false
        end
        for slot = 1,5 do platform["ap_pcie_root_port_" .. slot] = nil end
    elseif policy.pcie_ep_loopback then
        platform.ap_pcie_root_port_2 = nil
    end

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

function pcie.policy(ctx)
    local options = ctx.options

    local pcie_irq_test_enabled =
        options.enable_ap_cpus and
        ctx.getenv_bool_or("QBOX_APOLLO_PCIE_IRQ_TEST", false)
    local pcie_test_endpoints_enabled = options.enable_ap_cpus and not pcie_irq_test_enabled and
        ctx.getenv_bool_or("QBOX_APOLLO_PCIE_TEST_ENDPOINTS", false)
    local pcie_ep_loopback = options.enable_ap_cpus and
        ctx.getenv_bool_or("QBOX_APOLLO_PCIE_EP_LOOPBACK", false)
    local nvme_image = ctx.getenv_or("QBOX_APOLLO_NVME_IMAGE", "")
    local nvme_enabled = options.enable_ap_cpus and nvme_image ~= ""
    assert(not nvme_enabled or
           (not pcie_irq_test_enabled and not pcie_test_endpoints_enabled),
           "NVMe cannot share the PCIe NIC/legacy IRQ test profiles")
    assert(not nvme_enabled or nvme_image:sub(1, 1) == "/",
           "QBOX_APOLLO_NVME_IMAGE must be an absolute image path")
    assert(not pcie_ep_loopback or
           (not pcie_irq_test_enabled and not pcie_test_endpoints_enabled and
            ctx.getenv_or("QBOX_APOLLO_PCIE_BIFURCATION", "none") == "none"),
           "PCIe EP loopback cannot be combined with other PCIe test profiles")
    local pci_qemu_args = options.ap_qemu_args
    if pcie_test_endpoints_enabled then
        -- Per-RID MMIO forwarding uses bounce buffers for virtio DMA mappings.
        pci_qemu_args = pci_qemu_args ..
            " -global virtio-net-pci.x-max-bounce-buffer-size=1048576"
    end
    local fault_event_test_enabled =
        options.enable_ap_cpus and
        ctx.getenv_bool_or("QBOX_APOLLO_FAULT_EVENT_TEST", false)
    return {
        pcie_irq_test_enabled = pcie_irq_test_enabled;
        pcie_test_endpoints_enabled = pcie_test_endpoints_enabled;
        pcie_ep_loopback = pcie_ep_loopback;
        nvme_image = nvme_image;
        nvme_enabled = nvme_enabled;
        pci_qemu_args = pci_qemu_args;
        fault_event_test_enabled = fault_event_test_enabled;
    }
end

return pcie
