/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <apollo_timer_snapshot.h>

#include <arm_system_counter.h>
#include <limits>
#include <sstream>
#include <stdexcept>

int64_t apollo_timer_snapshot::current_time_ns()
{
    return gs::arm_system_counter::absolute_ns(sc_core::sc_time_stamp());
}

std::string apollo_timer_snapshot::observation_fields(
    uint64_t observed_counter, int64_t observation_time_ns,
    const char* counter_basis)
{
    std::ostringstream out;
    out << "\"observed_counter\":" << observed_counter
        << ",\"observation_time_ns\":" << observation_time_ns
        << ",\"counter_basis\":\"" << counter_basis << "\"";
    return out.str();
}

std::string apollo_timer_snapshot::deadline_fields(uint64_t cval,
                                                    uint32_t ctl,
                                                    uint32_t irq)
{
    std::ostringstream out;
    out << "\"cval\":" << cval
        << ",\"enabled\":" << ((ctl & 1u) != 0 ? "true" : "false")
        << ",\"masked\":" << ((ctl & 2u) != 0 ? "true" : "false")
        << ",\"istatus\":" << ((ctl & 4u) != 0 ? "true" : "false")
        << ",\"irq\":" << irq;
    return out.str();
}

std::string apollo_timer_snapshot::capture(const char* name,
                                           int64_t expected_ns)
{
    if (current_time_ns() != expected_ns) {
        throw std::runtime_error("SystemC snapshot timestamp mismatch");
    }
    const auto smd = m_smd.snapshot_at(expected_ns);
    const auto ap_cpu = m_ap_cpu.timer_snapshot(
        qemu::ArmGenericTimerOutput::PHYS);
    const auto ap_ns = m_ap_mmio.snapshot(0);
    const auto ap_s_normal = m_ap_mmio.snapshot(1);
    const auto ap_s = m_ap_mmio.snapshot_with_secure_frame_access(1);
    const auto ap_s_restored = m_ap_mmio.snapshot(1);
    const auto si0_cpu = m_si0_cpu.timer_snapshot(
        qemu::ArmGenericTimerOutput::PHYS);
    const auto si0_base = m_si0_cntbase.snapshot_at(expected_ns);
    const auto si1_cpu = m_si1_cpu.timer_snapshot(
        qemu::ArmGenericTimerOutput::PHYS);
    const auto rse0 = m_rse_timer0.snapshot();
    const auto rse1 = m_rse_timer1.snapshot();
    const auto rse2 = m_rse_timer2.snapshot();
    const auto rse3 = m_rse_timer3.snapshot();

    const auto observe_css = [this](
        const char* view, qemu_arm_generic_timer_counter_bridge& bridge,
        int64_t qemu_virtual_ns, uint64_t observed_count) {
        const auto epoch = bridge.epoch_snapshot();
        if ((epoch.offset_ns > 0 &&
             qemu_virtual_ns >
                 std::numeric_limits<int64_t>::max() - epoch.offset_ns) ||
            (epoch.offset_ns < 0 &&
             qemu_virtual_ns <
                 std::numeric_limits<int64_t>::min() - epoch.offset_ns)) {
            throw std::runtime_error(std::string(view) +
                                     " observation time overflow");
        }
        const int64_t observation_ns = qemu_virtual_ns + epoch.offset_ns;
        const uint64_t provider_count =
            m_smd.snapshot_at(observation_ns).counter;
        if (observed_count != provider_count) {
            std::ostringstream detail;
            detail << view << " counter/provider mismatch: observed="
                   << observed_count << " provider=" << provider_count
                   << " qemu_ns=" << qemu_virtual_ns
                   << " epoch_offset_ns=" << epoch.offset_ns
                   << " systemc_observation_ns=" << observation_ns;
            throw std::runtime_error(detail.str());
        }
        return observation_ns;
    };
    auto& ap_bridge = m_ap_cpu.timer_counter_bridge();
    auto& si0_bridge = m_si0_cpu.timer_counter_bridge();
    auto& si1_bridge = m_si1_cpu.timer_counter_bridge();
    const int64_t ap_cpu_observation = observe_css(
        "ap_cpu0", ap_bridge, ap_cpu.qemu_virtual_ns,
        ap_cpu.physical_count);
    const int64_t ap_ns_observation = observe_css(
        "ap_refclk_ns", ap_bridge, ap_ns.qemu_virtual_ns, ap_ns.count);
    const int64_t ap_s_observation = observe_css(
        "ap_refclk_s", ap_bridge, ap_s.qemu_virtual_ns, ap_s.count);
    const int64_t si0_observation = observe_css(
        "si0_cpu0", si0_bridge, si0_cpu.qemu_virtual_ns,
        si0_cpu.physical_count);
    const int64_t si1_observation = observe_css(
        "si1_cpu0", si1_bridge, si1_cpu.qemu_virtual_ns,
        si1_cpu.physical_count);
    if (!smd.observed || !si0_base.observed || !ap_ns.count_accessible ||
        !ap_ns.frequency_accessible || !ap_ns.timer_accessible ||
        !ap_s.count_accessible || !ap_s.frequency_accessible ||
        !ap_s.timer_accessible) {
        throw std::runtime_error("required timer register view is inaccessible");
    }
    if (ap_s_normal.cntacr != 0 || ap_s_normal.count_accessible ||
        ap_s_normal.frequency_accessible || ap_s_normal.timer_accessible ||
        ap_s_normal.count != 0 || ap_s_normal.cntfrq != 0 ||
        ap_s_normal.cval != 0 || ap_s_normal.ctl != 0 ||
        ap_s_normal.irq_level != 0) {
        throw std::runtime_error("secure timer frame is not RAZ when disabled");
    }
    if (ap_s_restored.cntacr != ap_s_normal.cntacr) {
        throw std::runtime_error("temporary CNTACR access was not restored");
    }
    if (ap_s_restored.count_accessible ||
        ap_s_restored.frequency_accessible ||
        ap_s_restored.timer_accessible || ap_s_restored.count != 0 ||
        ap_s_restored.cntfrq != 0 || ap_s_restored.cval != 0 ||
        ap_s_restored.ctl != 0 || ap_s_restored.irq_level != 0) {
        throw std::runtime_error("secure timer frame did not return to RAZ");
    }
    const uint64_t css_count = smd.counter;
    if (si0_base.counter != css_count) {
        throw std::runtime_error("CSS timer views differ at one timestamp");
    }
    if (!m_rse_timer0.shares_counter_with(m_rse_timer1) ||
        !m_rse_timer0.shares_counter_with(m_rse_timer2) ||
        !m_rse_timer0.shares_counter_with(m_rse_timer3) ||
        rse0.counter_frequency_hz == 0 ||
        rse0.counter_frequency_hz != rse1.counter_frequency_hz ||
        rse0.counter_frequency_hz != rse2.counter_frequency_hz ||
        rse0.counter_frequency_hz != rse3.counter_frequency_hz) {
        throw std::runtime_error("RSE timer views do not share one LSC state");
    }

    std::ostringstream out;
    out << "{\"name\":\"" << name << "\",\"marker\":\"" << name
        << "\",\"sim_time_ns\":" << expected_ns << ",\"views\":{"
        << "\"smd\":{\"domain\":\"css\",\"counter\":" << smd.counter
        << ",\"input_frequency_hz\":" << smd.input_frequency_hz
        << ",\"increment\":" << smd.increment
        << ",\"reported_frequency_hz\":" << smd.reported_frequency_hz
        << "," << observation_fields(smd.counter, expected_ns,
                                      "common_sample_time")
        << ",\"observed\":true},"
        << "\"ap_cpu0\":{\"domain\":\"css\",\"counter\":"
        << css_count << ",\"reported_frequency_hz\":"
        << ap_cpu.cntfrq << ","
        << observation_fields(ap_cpu.physical_count, ap_cpu_observation,
                              "common_sample_time")
        << ",\"observed\":true},"
        << "\"ap_refclk_ns\":{\"domain\":\"css\",\"counter\":"
        << css_count << ",\"reported_frequency_hz\":" << ap_ns.cntfrq
        << "," << deadline_fields(ap_ns.cval, ap_ns.ctl, 49)
        << "," << observation_fields(ap_ns.count, ap_ns_observation,
                                      "common_sample_time")
        << ",\"cntacr_original\":" << ap_ns.cntacr
        << ",\"observed\":true},"
        << "\"ap_refclk_s\":{\"domain\":\"css\",\"counter\":"
        << css_count << ",\"reported_frequency_hz\":" << ap_s.cntfrq
        << "," << deadline_fields(ap_s.cval, ap_s.ctl, 48)
        << "," << observation_fields(ap_s.count, ap_s_observation,
                                      "common_sample_time")
        << ",\"cntacr_original\":" << ap_s_normal.cntacr
        << ",\"access_control_state\":\"enabled\",\"observed\":true},"
        << "\"si0_cpu0\":{\"domain\":\"css\",\"counter\":"
        << css_count << ",\"reported_frequency_hz\":"
        << si0_cpu.cntfrq << ","
        << observation_fields(si0_cpu.physical_count, si0_observation,
                              "common_sample_time")
        << ",\"observed\":true},"
        << "\"si0_cntbase\":{\"domain\":\"css\",\"counter\":"
        << css_count << ",\"reported_frequency_hz\":"
        << si0_base.reported_frequency_hz << ","
        << observation_fields(si0_base.counter, expected_ns,
                              "common_sample_time")
        << ",\"observed\":true},"
        << "\"si1_cpu0\":{\"domain\":\"css\",\"counter\":"
        << css_count << ",\"reported_frequency_hz\":"
        << si1_cpu.cntfrq << ","
        << observation_fields(si1_cpu.physical_count, si1_observation,
                              "common_sample_time")
        << ",\"observed\":true},";
    const qemu::ArmSSETimerSnapshot rse[] = {rse0, rse1, rse2, rse3};
    const uint32_t irq[] = {3, 4, 5, 27};
    for (unsigned int i = 0; i < 4; ++i) {
        if (i != 0) {
            out << ',';
        }
        out << "\"rse_timer" << i << "\":{\"domain\":\"rse\",\"counter\":"
            << rse[i].count << ",\"input_frequency_hz\":"
            << rse[i].counter_frequency_hz
            << ",\"reported_frequency_hz\":" << rse[i].cntfrq
            << "," << deadline_fields(rse[i].cval, rse[i].ctl, irq[i])
            << "," << observation_fields(
                   rse[i].count, rse[i].qemu_virtual_ns,
                   "qemu_virtual_observation")
            << ",\"observed\":true}";
    }
    out << "}}";
    return out.str();
}
