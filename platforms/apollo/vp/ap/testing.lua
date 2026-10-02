local testing = {}

function testing.define_faults(ctx, platform, hw, policy)
    local AP_IRQ = hw.AP_IRQ
    local AP_HW = hw.AP_HW

    assert(platform.ap_smmu_event_fanout == nil, "duplicate Apollo object")
    if policy.fault_event_test_enabled then
        platform.ap_smmu_event_fanout = {
            moduletype = "signal_fanout";
            signal_out = {
                bind = "&ap_gic.spi_in_" .. AP_IRQ.smmu_event .. ";&ap_smmu_fault_observer.fault_in";
            };
        }
    end

    assert(platform.ap_smmu_fault_observer == nil, "duplicate Apollo object")
    if policy.fault_event_test_enabled then
        platform.ap_smmu_fault_observer = {
            moduletype = "zena_fmu";
            bank_count = AP_HW.smmu_fault_bank_count;
            record_count = AP_HW.smmu_fault_record_count;
            enforce_sys_key = false;
            fault_input_enabled = true;
            fault_input_record = 1;
            fault_source = "ap_smmu_0.irq_eventq";
            fault_id = "smmuv3-eventq";
            fault_sink = "ap_gic.spi_in_" .. AP_IRQ.smmu_event;
            event_log = ctx.getenv_or("QBOX_APOLLO_FAULT_EVENT_LOG", "");
            log_level = 0;
        }
    end
end

function testing.define_endpoints(ctx, platform, hw, policy)
    assert(platform.ap_pcie_irq_test_endpoint == nil, "duplicate Apollo object")
    if policy.pcie_irq_test_enabled then
        platform.ap_pcie_irq_test_endpoint = {
            moduletype = "virtio_net_pci";
            args = {"&platform.ap_qemu_inst", "&platform.ap_gpex_0"};
            addr = "01.0";
            mac = "52:54:00:12:34:56";
            netdev_str = "type=user";
        }
    end
end

return testing
