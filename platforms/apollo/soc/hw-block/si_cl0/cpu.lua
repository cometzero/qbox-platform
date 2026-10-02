local module = {}

function module.create(ctx, hardware, runtime, interrupts)
    local SI_CONST = hardware
    local irq_routes = interrupts

    local block = {}

    function block.define(ctx, platform, qemu_instance)
        assert(
            platform.si_cl0_cpu_0 == nil,
            "duplicate object: platform.si_cl0_cpu_0")
        platform.si_cl0_cpu_0 = {
            moduletype = "cpu_arm_cortexR82";
            args = {qemu_instance};
            mem = {bind = "&si_cl0_ni710ae_primary_nci.protected_target_socket"};
            has_el2 = true;
            psci_conduit = "smc";
            start_powered_off = false;
            start_in_reset = true;
            reset_power_on = true;
            rvbar = runtime.entry;
            cntfrq_hz = 125000000;
            mp_affinity = 0x0;
            request_origin_id = ctx.request_context.origin.si_cl0_cpu_base;
            request_domain_id = ctx.request_context.domain.si_cl0;
            requester_id = 0;
            request_secure = true;
            request_secure_valid = true;
            irq_timer_sec_out = {
                bind = "&si_cl0_gic.ppi_in_cpu_0_" ..
                    irq_routes.ppi_index(ctx, "si_secure_timer");
            };
            irq_timer_phys_out = {
                bind = "&si_cl0_gic.ppi_in_cpu_0_" ..
                    irq_routes.ppi_index(ctx, "si_physical_timer");
            };
            irq_timer_virt_out = {
                bind = "&si_cl0_gic.ppi_in_cpu_0_" ..
                    irq_routes.ppi_index(ctx, "si_virtual_timer");
            };
            irq_timer_hyp_out = {
                bind = "&si_cl0_gic.ppi_in_cpu_0_" ..
                    irq_routes.ppi_index(ctx, "si_hypervisor_timer");
            };
        }

        assert(
            platform.si_cl0_cpu_counter_mirror == nil,
            "duplicate object: platform.si_cl0_cpu_counter_mirror")
        platform.si_cl0_cpu_counter_mirror = {
            moduletype = "qemu_arm_counter_mirror";
            args = {
                "&platform.si_cl0_cpu_0";
                "&platform.css_system_counter";
            };
        }

        platform.si_cl0_gic.irq_out_0 = {bind = "&si_cl0_cpu_0.irq_in"}
        platform.si_cl0_gic.fiq_out_0 = {bind = "&si_cl0_cpu_0.fiq_in"}
        platform.si_cl0_gic.virq_out_0 = {bind = "&si_cl0_cpu_0.virq_in"}
        platform.si_cl0_gic.vfiq_out_0 = {bind = "&si_cl0_cpu_0.vfiq_in"}
    end

    return block
end

return module
