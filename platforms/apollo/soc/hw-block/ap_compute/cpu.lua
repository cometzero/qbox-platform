-- CPU PPI constants use architectural INTIDs, as required by QEMU PPI sockets.
local AP_ARCH_TIMER_VIRT_INTID = 16 + 11
local AP_ARCH_TIMER_SECURE_INTID = 16 + 13
local AP_ARCH_TIMER_NON_SECURE_INTID = 16 + 14
local AP_ARCH_TIMER_HYP_INTID = 16 + 10
local AP_GIC_MAINTENANCE_INTID = 25
local AP_PMU_INTID = 23
local AP_RAS_FAULT_INTID = 17

local cpu = {}

function cpu.define_initiator(ctx, platform)
    local options = ctx.options

    assert(platform.ap_global_peripheral_initiator == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_global_peripheral_initiator = {
            moduletype = "global_peripheral_initiator";
            args = {"&platform.ap_qemu_inst", "&platform.ap_cpu_0"};
            request_origin_id = ctx.request_context.origin.ap_global_peripheral;
            request_domain_id = ctx.request_context.domain.ap;
            global_initiator = {bind = "&system_router.target_socket"};
        }
    end
end

function cpu.define(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE
    local AP_HW = hw.AP_HW

    if options.enable_ap_cpus then
        for i = 0, (AP_HW.gic_active_redist_regions-1) do
            assert(platform["ap_gic"]["redist_iface_" .. i] == nil, "duplicate Apollo object")
            platform["ap_gic"]["redist_iface_" .. i] = {
                address = AP_ADDRESS.gic_redist + (i * AP_SIZE.gic_redist);
                size = AP_SIZE.gic_redist;
                bind = "&system_router.initiator_socket";
            }
        end

        for i = 0, (options.AP_NUM_CPUS-1) do
            assert(platform["ap_cpu_ras_fault_sync_" .. i] == nil, "duplicate Apollo object")
            platform["ap_cpu_ras_fault_sync_" .. i] = {
                moduletype = "signal_fanout";
                signal_out = {
                    bind = "&ap_gic.ppi_in_cpu_" .. i .. "_" .. AP_RAS_FAULT_INTID;
                };
            }
            local cpu = {
                moduletype = "cpu_arm_cortexA720AE";
                args = {"&platform.ap_qemu_inst"};
                mem = {bind = "&system_router.target_socket"};
                has_el3 = true;
                has_el2 = true;
                irq_timer_phys_out = {
                    bind = "&ap_gic.ppi_in_cpu_" .. i .. "_" .. AP_ARCH_TIMER_NON_SECURE_INTID;
                };
                irq_timer_virt_out = {
                    bind = "&ap_gic.ppi_in_cpu_" .. i .. "_" .. AP_ARCH_TIMER_VIRT_INTID;
                };
                irq_timer_hyp_out = {
                    bind = "&ap_gic.ppi_in_cpu_" .. i .. "_" .. AP_ARCH_TIMER_HYP_INTID;
                };
                irq_timer_sec_out = {
                    bind = "&ap_gic.ppi_in_cpu_" .. i .. "_" .. AP_ARCH_TIMER_SECURE_INTID;
                };
                gicv3_maintenance_interrupt = {
                    bind = "&ap_gic.ppi_in_cpu_" .. i .. "_" .. AP_GIC_MAINTENANCE_INTID;
                };
                pmu_interrupt = {bind = "&ap_gic.ppi_in_cpu_" .. i .. "_" .. AP_PMU_INTID};
                ras_fault_interrupt = {
                    bind = "&ap_cpu_ras_fault_sync_" .. i .. ".signal_in";
                };
                ras_uncontainable_interrupt = {
                    bind = "&si_cl0_ap_ras_cluster" .. math.floor(i / 4) ..
                        ".cpu_fault_" .. (i % 4);
                };
                psci_conduit = options.getenv_or("QBOX_RDASPEN_AP_PSCI_CONDUIT", "disabled");
                mp_affinity = options.mp_affinity(i);
                start_powered_off = i ~= 0;
                start_in_reset = true;
                reset_power_on = true;
                cntfrq_hz = AP_HW.arch_timer_frequency_hz;
                trace_pc = options.ap_pc_trace;
                trace_pc_file = options.ap_pc_trace_file;
                trace_pc_interval = options.ap_pc_trace_interval;
                trace_pc_limit = options.ap_pc_trace_limit;
                trace_exception_state = options.ap_exception_trace;
                construction_priority = -200 + i;
                request_origin_id = ctx.request_context.origin.ap_cpu_base + i;
                request_domain_id = ctx.request_context.domain.ap;
                requester_id = i;
            }
            assert(platform["ap_cpu_" .. tostring(i)] == nil, "duplicate Apollo object")
            platform["ap_cpu_" .. tostring(i)] = cpu
            assert(platform["ap_cpu_counter_mirror_" .. tostring(i)] == nil, "duplicate Apollo object")
            platform["ap_cpu_counter_mirror_" .. tostring(i)] = {
                moduletype = "qemu_arm_counter_mirror";
                args = {
                    "&platform.ap_cpu_" .. i;
                    "&platform.css_system_counter";
                };
            }

            assert(platform["ap_gic"]["irq_out_" .. i] == nil, "duplicate Apollo object")
            platform["ap_gic"]["irq_out_" .. i] = {bind = "&ap_cpu_" .. i .. ".irq_in"}
            assert(platform["ap_gic"]["fiq_out_" .. i] == nil, "duplicate Apollo object")
            platform["ap_gic"]["fiq_out_" .. i] = {bind = "&ap_cpu_" .. i .. ".fiq_in"}
            assert(platform["ap_gic"]["virq_out_" .. i] == nil, "duplicate Apollo object")
            platform["ap_gic"]["virq_out_" .. i] = {bind = "&ap_cpu_" .. i .. ".virq_in"}
            assert(platform["ap_gic"]["vfiq_out_" .. i] == nil, "duplicate Apollo object")
            platform["ap_gic"]["vfiq_out_" .. i] = {bind = "&ap_cpu_" .. i .. ".vfiq_in"}
        end
    end
end

return cpu
