/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <host_gtimer.h>

#include <arm_system_counter.h>
#include <stdexcept>

namespace {

gs::arm_system_counter& require_counter(sc_core::sc_object* object)
{
    gs::arm_system_counter* counter =
        dynamic_cast<gs::arm_system_counter*>(object);
    if (counter == nullptr) {
        throw std::invalid_argument("expected arm_system_counter");
    }
    return *counter;
}

}

host_gtimer::host_gtimer(sc_core::sc_module_name name,
                         sc_core::sc_object* counter)
    : host_gtimer(name, require_counter(counter))
{
}

host_gtimer::FrontendSnapshot host_gtimer::snapshot_at(
    int64_t absolute_ns) const
{
    FrontendSnapshot result;
    result.observed = p_counter_base.get_value() ||
                      p_counter_control.get_value() ||
                      p_counter_read.get_value();
    if (!result.observed) {
        return result;
    }

    const uint32_t low_offset = p_counter_control.get_value() ? CNTCV_L : PCTL;
    const uint32_t high_offset = p_counter_control.get_value() ? CNTCV_H : PCTH;
    result.counter = uint64_t(read32(low_offset, absolute_ns)) |
                     (uint64_t(read32(high_offset, absolute_ns)) << 32);
    const auto state = m_counter.snapshot();
    result.input_frequency_hz = state.input_frequency_hz;
    result.reported_frequency_hz = state.reported_frequency_hz;
    result.increment = state.integer_increment();
    return result;
}
