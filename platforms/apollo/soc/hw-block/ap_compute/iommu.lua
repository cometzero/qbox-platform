local AP_SMMU_BASE = 0x1C0000000
local AP_SMMU_SIZE = 0x08000000

local iommu = {}

local function ap_smmu_component(ctx, hw)
    local options = ctx.options
    local AP_ADDRESS, AP_SIZE, AP_IRQ, AP_HW = hw.AP_ADDRESS, hw.AP_SIZE, hw.AP_IRQ, hw.AP_HW
    local event_irq_target = "&ap_gic.spi_in_" .. AP_IRQ.smmu_event
    if options.getenv_bool_or("QBOX_APOLLO_FAULT_EVENT_TEST", false) then
        event_irq_target = "&ap_smmu_event_fanout.signal_in"
    end

    if options.smmu_backend == "systemc-mmu720ae" then
        return {
            moduletype = "smmuv3";
            pamax = AP_HW.smmu_pamax;
            sidsize = AP_HW.smmu_sid_size;
            ato = false;
            num_tbu = AP_HW.smmu_tbu_count;
            iidr = AP_HW.smmu_iidr;
            target_socket = {
                address = AP_SMMU_BASE;
                size = AP_SMMU_SIZE;
                bind = "&system_router.initiator_socket";
            };
            dma = {bind = "&system_router.target_socket"};
            irq_eventq = {bind = event_irq_target};
        }
    end

    return {
        moduletype = "arm_smmuv3";
        args = {"&platform.ap_qemu_inst", "&platform.ap_gpex_0"};
        mem = {
            address = AP_SMMU_BASE;
            size = AP_SMMU_SIZE;
            bind = "&system_router.initiator_socket";
        };
        irq_out_0 = {bind = event_irq_target};
        stage = "1";
    }
end

function iommu.define(ctx, platform, hw, policy)
    local options = ctx.options
    if not options.enable_ap_cpus then
        return
    end

    assert(platform.ap_smmu_0 == nil, "duplicate AP SMMU")
    platform.ap_smmu_0 = ap_smmu_component(ctx, hw)
    if options.smmu_backend == "systemc-mmu720ae" then
        assert(platform.ap_smmu_lti00 == nil, "duplicate AP SMMU TBU")
        platform.ap_smmu_lti00 = {
            moduletype = "smmuv3_tbu";
            args = {"&platform.ap_smmu_0"};
            topology_id = 0x40;
            requester_id_from_context = not policy.pcie_irq_test_enabled;
            upstream_socket = {};
            downstream_socket = {bind = "&system_router.target_socket"};
        }
    end
end

return iommu
