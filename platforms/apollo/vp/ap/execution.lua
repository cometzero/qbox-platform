local execution = {}

local AP_QEMU = {
    accel_env = "QBOX_APOLLO_FULL_AP_ACCEL";
    accel = "tcg";
    tcg_mode_env = "QBOX_APOLLO_FULL_AP_TCG_MODE";
    tcg_mode = "MULTI";
    sync_policy_env = "QBOX_APOLLO_FULL_AP_SYNC_POLICY";
    sync_policy = "multithread-freerunning";
    time_sync_strategy_env = "QBOX_APOLLO_FULL_AP_TIME_SYNC_STRATEGY";
    time_sync_strategy = "quantum_keeper";
}

function execution.policy(ctx)
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

function execution.define(ctx, platform, policy)
    local options = ctx.options

    assert(platform.ap_qemu_inst_mgr == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_qemu_inst_mgr = {
            moduletype = "QemuInstanceManager";
            construction_priority = -300;
        }
    end

    assert(platform.ap_qemu_inst == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_qemu_inst = {
            moduletype = "QemuInstance";
            args = {"&platform.ap_qemu_inst_mgr", "AARCH64"};
            accel = ctx.getenv_or(AP_QEMU.accel_env, AP_QEMU.accel);
            tcg_mode = ctx.getenv_or(AP_QEMU.tcg_mode_env, AP_QEMU.tcg_mode);
            sync_policy = ctx.getenv_or(
                AP_QEMU.sync_policy_env,
                AP_QEMU.sync_policy);
            time_sync_strategy = ctx.getenv_or(
                AP_QEMU.time_sync_strategy_env,
                AP_QEMU.time_sync_strategy);
            managed_start_in_reset_release = true;
            qemu_args = policy.pci_qemu_args;
            construction_priority = -299;
        }
    end
end

return execution
