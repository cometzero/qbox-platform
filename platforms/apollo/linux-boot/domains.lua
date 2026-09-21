-- SPDX-License-Identifier: BSD-3-Clause
-- Explicit firmware substitutes for the AP-only profile. No SI/RSE CPUs.
local domains = {}

function domains.define(ctx, platform)
    platform.linux_domain_stub = {
        moduletype = "apollo_linux_stub";
        target_socket = {
            address = 0x0f000000; size = 0x400;
            bind = "&ap_router.initiator_socket";
        };
    }

    -- The original DT describes SCMI over the non-secure MHU pair. Reuse
    -- the SystemC MHU responder, with immediate protocol replies and IRQs.
    -- Only BASE discovery is supported. DVFS/power requests fail explicitly;
    -- no SCP, voltage or clock changes are simulated.
    -- SRAM at 0x180000 is supplied by the common AP hardware definition.
    for _, spec in ipairs({
        {name = "linux_scmi_pbx", frame = "pbx", base = 0x40020000, irq = 112},
        {name = "linux_scmi_mbx", frame = "mbx", base = 0x40050000, irq = 113},
    }) do
        platform[spec.name] = {
            moduletype = "mhu320ae";
            frame = spec.frame;
            pair = "linux_scmi_stub";
            protocol = "scmi";
            scmi_transport = "linux-stub";
            tx_shmem = 0x00180000;
            rx_shmem = 0x00180100;
            init_shmem = true;
            power_domain_reset_count = 0;
            power_domain_reset_assert_on_power_off = false;
            power_domain_reset_pulse_on_power_on = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = spec.base; size = 0x30000;
                bind = "&ap_router.initiator_socket";
            };
            initiator_socket = {bind = "&ap_router.target_socket"};
            irq = {bind = "&ap_gic.spi_in_"..spec.irq};
            log_level = 0;
        }
    end
    -- Functional SI firmware substitute: attach/resource table, RPMsg and an
    -- ethsi1 ARP/ICMP peer. No SI CPU or Zephyr firmware is executed.
    platform.linux_si_stub = {
        moduletype = "apollo_si_stub";
        target_socket = {
            address = 0x400b0000; size = 0x60000;
            bind = "&ap_router.initiator_socket";
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        irq_pbx = {bind = "&ap_gic.spi_in_120"};
        irq_mbx = {bind = "&ap_gic.spi_in_121"};
        trace = mhu_trace;
        log_level = 0;
    }

    -- AP secure RSE mailbox windows retain their framing, but every PSA
    -- request returns PSA_ERROR_NOT_SUPPORTED and zero output lengths.
    for _, spec in ipairs({
        {name = "linux_rse_pbx", frame = "pbx", base = 0x40680000},
        {name = "linux_rse_mbx", frame = "mbx", base = 0x406b0000},
    }) do
        platform[spec.name] = {
            moduletype = "mhu320ae";
            frame = spec.frame;
            pair = "linux_rse_stub";
            protocol = "doorbell";
            direct_boot_compat = true;
            direct_boot_status = -134;
            init_shmem = false;
            target_socket = {
                address = spec.base; size = 0x30000;
                bind = "&ap_router.initiator_socket";
            };
            initiator_socket = {bind = "&ap_router.target_socket"};
            log_level = 0;
        }
    end
    -- RSE/FF-A/TEE/PFDI SMC requests enter the SMCCC stub and return -1.
end

return domains
