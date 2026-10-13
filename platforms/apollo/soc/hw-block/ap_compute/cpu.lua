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
                args = {ctx.instances.ap};
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
                psci_conduit = options.ap_psci_conduit;
                mp_affinity = math.floor(i / 4) * 0x10000 + (i % 4) * 0x100;
                start_powered_off = i ~= 0;
                start_in_reset = true;
                reset_power_on = true;
                cntfrq_hz = AP_ARCH_TIMER_FREQUENCY_HZ;
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

function cpu.define_instance(ctx, platform, policy)
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
            managed_start_in_reset_release = true;
            construction_priority = -299;
        }
    end
end

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

return cpu
