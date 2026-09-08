/* SPDX-License-Identifier: BSD-3-Clause */

#include <apollo_runtime_injection.h>

#include <limits>
#include <qemu-instance.h>
#include <test/test.h>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

using gs::RuntimeActionRequest;
using gs::RuntimeActionState;
using gs::RuntimeActionTrigger;
using gs::RuntimeActionValue;
using gs::RuntimeResourceType;

RuntimeActionRequest request(const std::string& target,
                             const std::string& action)
{
    RuntimeActionRequest result;
    result.target = target;
    result.action = action;
    return result;
}

void set_bool(RuntimeActionRequest& request, const std::string& name,
              bool value)
{
    request.parameters.emplace(name, RuntimeActionValue(value));
}

void set_u64(RuntimeActionRequest& request, const std::string& name,
             uint64_t value)
{
    request.parameters.emplace(name, RuntimeActionValue(value));
}

void set_string(RuntimeActionRequest& request, const std::string& name,
                const std::string& value)
{
    request.parameters.emplace(name, RuntimeActionValue(value));
}

bool snapshot_bool(const gs::RuntimeTargetSnapshotReply& reply,
                   const std::string& name)
{
    TEST_ASSERT(reply.ok());
    const auto found = reply.value.values.find(name);
    TEST_ASSERT(found != reply.value.values.end());
    TEST_ASSERT(found->second.type() == RuntimeActionValue::Type::BOOLEAN);
    return found->second.boolean();
}

uint64_t snapshot_u64(const gs::RuntimeTargetSnapshotReply& reply,
                      const std::string& name)
{
    TEST_ASSERT(reply.ok());
    const auto found = reply.value.values.find(name);
    TEST_ASSERT(found != reply.value.values.end());
    TEST_ASSERT(found->second.type() ==
                RuntimeActionValue::Type::UNSIGNED_INTEGER);
    return found->second.unsigned_integer();
}

class ApolloRuntimeInjectionTest : public TestBench
{
    QemuInstanceManager m_inst_manager;
    QemuInstance m_inst;
    qemu_pl061 m_host_gpio;
    qemu_pl061 m_rse_gpio_0;
    qemu_pl061 m_rse_gpio_1;
    gicx00_multiview m_gic;
    zena_ssu m_ssu;
    gs::arm_system_counter m_counter;
    apollo_runtime_injection m_service;
    tlm_utils::simple_initiator_socket<
        ApolloRuntimeInjectionTest, DEFAULT_TLM_BUSWIDTH> m_ssu_access;
    TargetSignalSocket<bool> m_gic_spi;
    TargetSignalSocket<bool> m_system_reset;
    TargetSignalSocket<bool> m_board_gpio;

    void run_test()
    {
        wait(sc_core::SC_ZERO_TIME);

        const auto capabilities = m_service.capabilities();
        TEST_ASSERT(capabilities.size() == 28);
        TEST_ASSERT(capabilities[0].resource_type ==
                    RuntimeResourceType::INTERRUPT);
        TEST_ASSERT(capabilities[1].resource_type == RuntimeResourceType::EVENT);
        TEST_ASSERT(capabilities[2].resource_type ==
                    RuntimeResourceType::CONTROL);
        TEST_ASSERT(capabilities[3].target == "apollo.control.system-reset");

        auto no_reset = request("apollo.gpio.rse0.pin0", "drive-high");
        no_reset.clear_on_reset = false;
        const auto rejected_policy = m_service.submit(no_reset);
        TEST_ASSERT(!rejected_policy.ok());
        TEST_ASSERT(rejected_policy.error_code ==
                    "unsupported-reset-policy");

        auto delayed = request("apollo.gpio.rse0.pin0", "pulse");
        delayed.trigger.type =
            RuntimeActionTrigger::Type::RELATIVE_SIMULATION_TIME;
        delayed.trigger.delay_ns = 100;
        set_string(delayed, "active_level", "high");
        set_u64(delayed, "duration_ns", 50);
        const auto scheduled = m_service.submit(delayed);
        TEST_ASSERT(scheduled.ok());
        TEST_ASSERT(scheduled.value.state == RuntimeActionState::SCHEDULED);
        TEST_ASSERT(scheduled.value.has_requested_sim_time_ns);
        TEST_ASSERT(scheduled.value.requested_sim_time_ns ==
                    static_cast<uint64_t>(sc_core::sc_time_stamp() /
                                          sc_core::sc_time(1, sc_core::SC_NS)) +
                        100);

        const auto duplicate = m_service.submit(delayed);
        TEST_ASSERT(!duplicate.ok());
        TEST_ASSERT(duplicate.error_code == "target-busy");

        const auto cancelled = m_service.cancel(scheduled.value.id);
        TEST_ASSERT(cancelled.ok());
        TEST_ASSERT(cancelled.value.state == RuntimeActionState::CANCELLED);
        TEST_ASSERT(!cancelled.value.has_applied_sim_time_ns);
        TEST_ASSERT(!snapshot_bool(
            m_service.target_snapshot("apollo.gpio.rse0.pin0"), "level"));

        auto reset_delayed = delayed;
        reset_delayed.trigger.delay_ns = 200;
        const auto reset_scheduled = m_service.submit(reset_delayed);
        TEST_ASSERT(reset_scheduled.ok());
        m_service.reset->write(true);
        const auto reset_status = m_service.status(reset_scheduled.value.id);
        TEST_ASSERT(reset_status.ok());
        TEST_ASSERT(reset_status.value.state == RuntimeActionState::CANCELLED);
        TEST_ASSERT(!reset_status.value.has_applied_sim_time_ns);
        TEST_ASSERT(!snapshot_bool(
            m_service.target_snapshot("apollo.gpio.rse0.pin0"), "level"));
        m_service.reset->write(false);

        auto stale = request("apollo.gpio.rse0.pin0", "read");
        stale.has_expected_generation = true;
        stale.expected_generation = 0;
        const auto stale_result = m_service.submit(stale);
        TEST_ASSERT(!stale_result.ok());
        TEST_ASSERT(stale_result.error_code == "stale-generation");

        auto absolute = request("apollo.gpio.rse0.pin0", "read");
        absolute.trigger.type =
            RuntimeActionTrigger::Type::ABSOLUTE_SIMULATION_TIME;
        absolute.trigger.time_ns =
            static_cast<uint64_t>(sc_core::sc_time_stamp() /
                                  sc_core::sc_time(1, sc_core::SC_NS)) +
            20;
        const auto absolute_result = m_service.submit(absolute);
        TEST_ASSERT(absolute_result.ok());
        TEST_ASSERT(absolute_result.value.state ==
                    RuntimeActionState::SCHEDULED);
        wait(sc_core::sc_time(10, sc_core::SC_NS));
        TEST_ASSERT(m_service.status(absolute_result.value.id).value.state ==
                    RuntimeActionState::SCHEDULED);
        wait(sc_core::sc_time(15, sc_core::SC_NS));
        TEST_ASSERT(m_service.status(absolute_result.value.id).value.state ==
                    RuntimeActionState::COMPLETED);

        auto overflow = request("apollo.gpio.rse0.pin0", "read");
        overflow.trigger.type =
            RuntimeActionTrigger::Type::RELATIVE_SIMULATION_TIME;
        overflow.trigger.delay_ns = std::numeric_limits<uint64_t>::max();
        TEST_ASSERT(m_service.submit(overflow).error_code ==
                    "simulation-time-overflow");

        auto drive = request("apollo.gpio.rse0.pin0", "drive-high");
        const auto driven = m_service.submit(drive);
        TEST_ASSERT(driven.ok());
        TEST_ASSERT(driven.value.state == RuntimeActionState::ACTIVE);
        TEST_ASSERT(snapshot_bool(
            m_service.target_snapshot("apollo.gpio.rse0.pin0"), "level"));

        const auto release = m_service.submit(
            request("apollo.gpio.rse0.pin0", "release"));
        TEST_ASSERT(release.ok());
        TEST_ASSERT(release.value.state == RuntimeActionState::COMPLETED);
        TEST_ASSERT(!snapshot_bool(
            m_service.target_snapshot("apollo.gpio.rse0.pin0"), "level"));

        auto pulse = request("apollo.gpio.rse0.pin0", "pulse");
        set_string(pulse, "active_level", "high");
        set_u64(pulse, "duration_ns", 50);
        const auto pulsed = m_service.submit(pulse);
        TEST_ASSERT(pulsed.ok());
        TEST_ASSERT(pulsed.value.state == RuntimeActionState::ACTIVE);
        wait(sc_core::sc_time(25, sc_core::SC_NS));
        TEST_ASSERT(snapshot_bool(
            m_service.target_snapshot("apollo.gpio.rse0.pin0"), "level"));
        wait(sc_core::sc_time(50, sc_core::SC_NS));
        const auto pulse_status = m_service.status(pulsed.value.id);
        TEST_ASSERT(pulse_status.ok());
        TEST_ASSERT(pulse_status.value.state ==
                    RuntimeActionState::COMPLETED);
        TEST_ASSERT(pulse_status.value.has_cleared_sim_time_ns);
        TEST_ASSERT(!snapshot_bool(
            m_service.target_snapshot("apollo.gpio.rse0.pin0"), "level"));

        auto output = request("apollo.gpio.rse0.pin1", "set-direction");
        set_string(output, "direction", "output");
        TEST_ASSERT(m_service.submit(output).ok());
        TEST_ASSERT(m_service.submit(
            request("apollo.gpio.rse0.pin1", "write-output-high")).ok());
        wait(sc_core::SC_ZERO_TIME);
        const auto output_snapshot =
            m_service.target_snapshot("apollo.gpio.rse0.pin1");
        TEST_ASSERT(snapshot_bool(output_snapshot, "level"));
        TEST_ASSERT(snapshot_bool(output_snapshot, "observed_output_level"));

        auto board_output = request(
            "apollo.gpio.host_smd.pin0", "set-direction");
        set_string(board_output, "direction", "output");
        TEST_ASSERT(m_service.submit(board_output).ok());
        TEST_ASSERT(m_service.submit(request(
            "apollo.gpio.host_smd.pin0", "write-output-high")).ok());
        wait(sc_core::SC_ZERO_TIME);
        TEST_ASSERT(m_board_gpio.read());
        TEST_ASSERT(snapshot_bool(m_service.target_snapshot(
            "apollo.gpio.host_smd.pin0"), "observed_output_level"));

        auto control = request(
            "apollo.control.css-system-counter", "set-control");
        set_bool(control, "enabled", false);
        set_bool(control, "halt_on_debug", false);
        const auto controlled = m_service.submit(control);
        TEST_ASSERT(controlled.ok());
        TEST_ASSERT(controlled.value.state == RuntimeActionState::ACTIVE);
        TEST_ASSERT(!snapshot_bool(m_service.target_snapshot(
            "apollo.control.css-system-counter"), "enabled"));
        TEST_ASSERT(m_service.cancel(controlled.value.id).ok());
        TEST_ASSERT(snapshot_bool(m_service.target_snapshot(
            "apollo.control.css-system-counter"), "enabled"));

        auto fault = request(
            "apollo.event.si-cl0-ssu-fault", "trigger-fault");
        set_bool(fault, "critical", true);
        const auto faulted = m_service.submit(fault);
        TEST_ASSERT(faulted.ok());
        TEST_ASSERT(faulted.value.state == RuntimeActionState::COMPLETED);
        TEST_ASSERT(snapshot_bool(m_service.target_snapshot(
            "apollo.event.si-cl0-ssu-fault"), "fault_active"));

        uint32_t view = 1;
        tlm::tlm_generic_payload view_write;
        sc_core::sc_time view_delay = sc_core::SC_ZERO_TIME;
        view_write.set_address(8);
        view_write.set_command(tlm::TLM_WRITE_COMMAND);
        view_write.set_data_ptr(reinterpret_cast<unsigned char*>(&view));
        view_write.set_data_length(sizeof(view));
        view_write.set_streaming_width(sizeof(view));
        m_gic.b_transport_dist_iviewr(view_write, view_delay);
        TEST_ASSERT(view_write.get_response_status() ==
                    tlm::TLM_OK_RESPONSE);

        auto valid_gic = request(
            "platform.si_gic_multiview", "pulse-spi");
        set_u64(valid_gic, "view", 1);
        set_u64(valid_gic, "intid", 32);
        set_u64(valid_gic, "duration_ns", 10);
        const auto gic_pulse = m_service.submit(valid_gic);
        TEST_ASSERT(gic_pulse.ok());
        TEST_ASSERT(gic_pulse.value.state == RuntimeActionState::ACTIVE);
        TEST_ASSERT(m_gic_spi.read());
        wait(sc_core::sc_time(15, sc_core::SC_NS));
        TEST_ASSERT(!m_gic_spi.read());
        TEST_ASSERT(m_service.status(gic_pulse.value.id).value.state ==
                    RuntimeActionState::COMPLETED);

        auto invalid_gic = request(
            "platform.si_gic_multiview", "pulse-spi");
        set_u64(invalid_gic, "view", 0);
        set_u64(invalid_gic, "intid", 32);
        set_u64(invalid_gic, "duration_ns", 10);
        const auto gic_result = m_service.submit(invalid_gic);
        TEST_ASSERT(!gic_result.ok());
        TEST_ASSERT(gic_result.value.state == RuntimeActionState::FAILED);

        auto system_reset = request("apollo.control.system-reset", "pulse");
        system_reset.reset_domain = "system";
        set_u64(system_reset, "duration_ns", 10);
        const auto reset_pulse = m_service.submit(system_reset);
        TEST_ASSERT(reset_pulse.ok());
        TEST_ASSERT(reset_pulse.value.state == RuntimeActionState::COMPLETED);
        TEST_ASSERT(m_system_reset.read());
        wait(sc_core::sc_time(15, sc_core::SC_NS));
        TEST_ASSERT(!m_system_reset.read());

        const auto generation_before_duplicate = snapshot_u64(
            m_service.target_snapshot("apollo.control.system-reset"),
            "generation");
        m_service.reset->write(true);
        const auto generation_after_assert = snapshot_u64(
            m_service.target_snapshot("apollo.control.system-reset"),
            "generation");
        m_service.reset->write(true);
        TEST_ASSERT(snapshot_u64(
            m_service.target_snapshot("apollo.control.system-reset"),
            "generation") == generation_after_assert);
        TEST_ASSERT(generation_after_assert ==
                    generation_before_duplicate + 1);
        m_service.reset->write(false);

        for (std::size_t i = 0; i < 128; ++i) {
            auto pending = request("apollo.gpio.rse0.pin2", "read");
            pending.trigger.type =
                RuntimeActionTrigger::Type::RELATIVE_SIMULATION_TIME;
            pending.trigger.delay_ns = 1000000 + i;
            TEST_ASSERT(m_service.submit(pending).ok());
        }
        auto saturated = request("apollo.gpio.rse0.pin3", "read");
        saturated.trigger.type =
            RuntimeActionTrigger::Type::RELATIVE_SIMULATION_TIME;
        saturated.trigger.delay_ns = 2000000;
        TEST_ASSERT(m_service.submit(saturated).error_code ==
                    "request-history-full");
        m_service.reset->write(true);
        m_service.reset->write(false);
        TEST_ASSERT(m_service.submit(
            request("apollo.gpio.rse0.pin3", "read")).ok());

        m_service.end_of_simulation();
        TEST_ASSERT(m_service.submit(
            request("apollo.gpio.rse0.pin3", "read")).error_code ==
                    "simulation-unavailable");

        sc_core::sc_stop();
    }

public:
    ApolloRuntimeInjectionTest(const sc_core::sc_module_name& name)
        : TestBench(name)
        , m_inst("qemu", &m_inst_manager, qemu::Target::AARCH64)
        , m_host_gpio("host_gpio", m_inst)
        , m_rse_gpio_0("rse_gpio_0", m_inst)
        , m_rse_gpio_1("rse_gpio_1", m_inst)
        , m_gic("gic")
        , m_ssu("ssu")
        , m_counter("counter")
        , m_service("runtime", &m_gic, &m_ssu, &m_counter,
                    &m_host_gpio, &m_rse_gpio_0, &m_rse_gpio_1)
        , m_ssu_access("ssu_access")
        , m_gic_spi("gic_spi")
        , m_system_reset("system_reset")
        , m_board_gpio("board_gpio")
    {
        m_ssu_access.bind(m_ssu.target_socket);
        m_gic.spi_out[0].bind(m_gic_spi);
        m_service.system_reset.bind(m_system_reset);
        m_host_gpio.gpio_out[0].bind(m_board_gpio);
        SC_THREAD(run_test);
    }
};

}

int sc_main(int argc, char* argv[])
{
    return run_testbench<ApolloRuntimeInjectionTest>(argc, argv);
}
