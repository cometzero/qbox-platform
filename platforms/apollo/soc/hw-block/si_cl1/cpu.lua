-- SPI constants are socket indices; PPI constants are architectural INTIDs.
local SI_SECURE_TIMER_INTID = 29
local SI_PHYSICAL_TIMER_INTID = 20
local SI_VIRTUAL_TIMER_INTID = 27
local SI_HYPERVISOR_TIMER_INTID = 19

local SI_CL1_CPU_COUNT = 4
local ARCH_TIMER_FREQUENCY_HZ = 125000000

local SI_CL1_AFFINITY_BASE = 0x00010000
local SI_CL1_AFFINITY_STRIDE = 0x00000100

local module = {}

function module.create(ctx, runtime)

    local block = {}

    function block.define(ctx, platform, qemu_instance)
        local gic_name = "si_cl1_gic"
        local gic = platform[gic_name]

        for i = 0,(SI_CL1_CPU_COUNT-1) do
            local gic_cpu = i
            assert(
                platform["si_cl1_cpu_" .. i] == nil,
                "duplicate object: platform." .. ("si_cl1_cpu_" .. i))
            platform["si_cl1_cpu_" .. i] = {
                moduletype = "cpu_arm_cortexR82";
                args = {qemu_instance};
                mem = {bind = "&si_cl1_router.target_socket"};
                has_el2 = true;
                psci_conduit = "smc";
                start_powered_off = false;
                start_in_reset = true;
                reset_power_on = true;
                rvbar = runtime.entry;
                mp_affinity = SI_CL1_AFFINITY_BASE +
                    (i * SI_CL1_AFFINITY_STRIDE);
                cntfrq_hz = ARCH_TIMER_FREQUENCY_HZ;
                request_origin_id = ctx.request_context.origin.si_cl1_cpu_base + i;
                request_domain_id = ctx.request_context.domain.si_cl1;
                requester_id = i;
                irq_timer_sec_out = {
                    bind = "&" .. gic_name .. ".ppi_in_cpu_" ..
                        gic_cpu .. "_" ..
                        SI_SECURE_TIMER_INTID;
                };
                irq_timer_phys_out = {
                    bind = "&" .. gic_name .. ".ppi_in_cpu_" ..
                        gic_cpu .. "_" ..
                        SI_PHYSICAL_TIMER_INTID;
                };
                irq_timer_virt_out = {
                    bind = "&" .. gic_name .. ".ppi_in_cpu_" ..
                        gic_cpu .. "_" ..
                        SI_VIRTUAL_TIMER_INTID;
                };
                irq_timer_hyp_out = {
                    bind = "&" .. gic_name .. ".ppi_in_cpu_" ..
                        gic_cpu .. "_" ..
                        SI_HYPERVISOR_TIMER_INTID;
                };
            }
            assert(
                platform["si_cl1_cpu_counter_mirror_" .. i] == nil,
                "duplicate object: platform." .. ("si_cl1_cpu_counter_mirror_" .. i))
            platform["si_cl1_cpu_counter_mirror_" .. i] = {
                moduletype = "qemu_arm_counter_mirror";
                args = {
                    "&platform.si_cl1_cpu_" .. i;
                    "&platform.css_system_counter";
                };
            }
            for _, signal in ipairs({"irq", "fiq", "virq", "vfiq"}) do
                gic[signal .. "_out_" .. gic_cpu] = {
                    bind = "&si_cl1_cpu_" .. i .. "." .. signal .. "_in";
                }
            end
        end
    end

    return block
end

return module
