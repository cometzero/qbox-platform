local AP_ARCH_TIMER_FREQUENCY_HZ = 125000000
-- CPU PPI constants use architectural INTIDs, as required by QEMU PPI sockets.
local AP_ARCH_TIMER_VIRT_INTID = 16 + 11
local AP_ARCH_TIMER_SECURE_INTID = 16 + 13
local AP_ARCH_TIMER_NON_SECURE_INTID = 16 + 14
local AP_ARCH_TIMER_HYP_INTID = 16 + 10
local AP_GIC_MAINTENANCE_INTID = 25
local AP_PMU_INTID = 23
local AP_RAS_FAULT_INTID = 17

local cpu = {}

function cpu.define(ctx, platform)
    local options = ctx.options

    if options.enable_ap_cpus then
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
                cntfrq_hz = AP_ARCH_TIMER_FREQUENCY_HZ;
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

function cpu.define_reset(ctx, platform)
    local options = ctx.options
    assert(platform.ap_reset_gpio == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_reset_gpio = {
            moduletype = "reset_gpio";
            args = {"&platform.ap_qemu_inst"};
            reset_out = {bind = options.ap_cpu_reset_bind_targets(options.AP_NUM_CPUS)};
            log_level = 0;
        }
    end

end

return cpu
