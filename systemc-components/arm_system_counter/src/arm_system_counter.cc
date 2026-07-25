/* SPDX-License-Identifier: BSD-3-Clause */

#include <arm_system_counter.h>

#include <limits>
#include <stdexcept>

namespace gs {

namespace {

using Wide = unsigned __int128;

constexpr Wide fixed_modulus =
    Wide(1) << (64 + arm_system_counter::fractional_bits);
constexpr Wide fixed_mask = fixed_modulus - 1;

Wide fixed_value(const arm_system_counter::StateSnapshot& state)
{
    return (Wide(state.anchor_count) << arm_system_counter::fractional_bits) |
           state.fractional_count;
}

Wide tick_product_modulo(Wide ticks, uint64_t increment)
{
    const uint64_t low_ticks = static_cast<uint64_t>(ticks);
    const Wide low_product = Wide(low_ticks) * increment;
    const Wide high_ticks = ticks >> 64;
    const Wide high_product_low =
        (high_ticks * increment) & arm_system_counter::fractional_mask;
    return (low_product + (high_product_low << 64)) & fixed_mask;
}

}

arm_system_counter::arm_system_counter(
    const sc_core::sc_module_name& name)
    : sc_core::sc_module(name)
    , reset("reset")
    , p_input_frequency_hz("input_frequency_hz", 125000000)
    , p_integer_increment("integer_increment", 1)
    , p_reported_frequency_hz("reported_frequency_hz", 125000000)
    , p_enabled("enabled", true)
    , p_halt_on_debug("halt_on_debug", false)
    , p_initial_count("initial_count", 0)
    , m_state_changed("state_changed")
{
    if (p_input_frequency_hz.get_value() == 0 ||
        p_integer_increment.get_value() >
            (std::numeric_limits<uint64_t>::max() >> fractional_bits)) {
        throw std::invalid_argument("invalid arm_system_counter configuration");
    }

    m_state.anchor_count = p_initial_count.get_value();
    m_state.anchor_time_ticks = sc_core::sc_time_stamp().value();
    m_state.input_frequency_hz = p_input_frequency_hz.get_value();
    m_state.increment_8_24 =
        p_integer_increment.get_value() << fractional_bits;
    m_state.reported_frequency_hz = p_reported_frequency_hz.get_value();
    m_state.enabled = p_enabled.get_value();
    m_state.halt_on_debug = p_halt_on_debug.get_value();
    m_reset_state = m_state;
    reset.register_value_changed_cb([this](bool asserted) {
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        if (asserted) {
            reset_at(now);
            set_enabled_at(false, now);
        } else {
            set_enabled_at(p_enabled.get_value(), now);
        }
    });
}

sc_dt::uint64 arm_system_counter::ticks_per_second()
{
    const sc_dt::uint64 ticks =
        sc_core::sc_time(1, sc_core::SC_SEC).value();
    if (ticks == 0) {
        throw std::logic_error("invalid SystemC time resolution");
    }
    return ticks;
}

void arm_system_counter::validate_state(const StateSnapshot& state)
{
    if (state.input_frequency_hz == 0 ||
        state.fractional_count > fractional_mask ||
        state.input_tick_remainder >= ticks_per_second()) {
        throw std::invalid_argument("invalid arm_system_counter state");
    }
}

void arm_system_counter::validate_time(const StateSnapshot& state,
                                       sc_dt::uint64 absolute_ticks)
{
    if (absolute_ticks < state.anchor_time_ticks) {
        throw std::out_of_range(
            "counter timestamp precedes the current anchor");
    }
}

void arm_system_counter::materialize(StateSnapshot& state,
                                     sc_dt::uint64 absolute_ticks)
{
    validate_state(state);
    validate_time(state, absolute_ticks);

    const sc_dt::uint64 elapsed_ticks =
        absolute_ticks - state.anchor_time_ticks;
    const Wide input_numerator =
        Wide(elapsed_ticks) * state.input_frequency_hz +
        state.input_tick_remainder;
    const Wide input_ticks = input_numerator / ticks_per_second();
    state.input_tick_remainder =
        static_cast<uint64_t>(input_numerator % ticks_per_second());

    Wide value = fixed_value(state);
    if (state.running() && state.increment_8_24 != 0) {
        value = (value +
                 tick_product_modulo(input_ticks, state.increment_8_24)) &
                fixed_mask;
    }
    state.anchor_count =
        static_cast<uint64_t>(value >> fractional_bits);
    state.fractional_count =
        static_cast<uint32_t>(value & fractional_mask);
    state.anchor_time_ticks = absolute_ticks;
}

arm_system_counter::StateSnapshot arm_system_counter::snapshot() const
{
    return m_state;
}

arm_system_counter::StateSnapshot arm_system_counter::snapshot_at(
    const sc_core::sc_time& time) const
{
    StateSnapshot evaluated = m_state;
    materialize(evaluated, time_ticks(time));
    return evaluated;
}

arm_system_counter::StateSnapshot arm_system_counter::save_state_at(
    const sc_core::sc_time& time) const
{
    return snapshot_at(time);
}

uint64_t arm_system_counter::count_at(const sc_core::sc_time& time) const
{
    return snapshot_at(time).anchor_count;
}

void arm_system_counter::notify_state_changed_at(
    const sc_core::sc_time& time)
{
    m_state_changed.cancel();
    const sc_core::sc_time now = sc_core::sc_time_stamp();
    m_state_changed.notify(
        time <= now ? sc_core::SC_ZERO_TIME : time - now);
}

bool arm_system_counter::mutate_at(
    const sc_core::sc_time& time,
    const std::function<bool(StateSnapshot&)>& mutation)
{
    StateSnapshot changed = m_state;
    materialize(changed, time_ticks(time));
    if (!mutation(changed)) {
        return false;
    }
    changed.generation = m_state.generation + 1;
    m_state = changed;
    notify_state_changed_at(time);
    return true;
}

bool arm_system_counter::set_input_frequency_at(
    uint64_t frequency_hz, const sc_core::sc_time& time)
{
    if (frequency_hz == 0) {
        throw std::invalid_argument("input frequency must be nonzero");
    }
    return mutate_at(time, [frequency_hz](StateSnapshot& state) {
        if (state.input_frequency_hz == frequency_hz) {
            return false;
        }
        state.input_frequency_hz = frequency_hz;
        return true;
    });
}

bool arm_system_counter::set_integer_increment_at(
    uint64_t increment, const sc_core::sc_time& time)
{
    if (increment >
        (std::numeric_limits<uint64_t>::max() >> fractional_bits)) {
        throw std::invalid_argument(
            "integer increment exceeds fixed-point range");
    }
    const uint64_t scale = increment << fractional_bits;
    return mutate_at(time, [scale](StateSnapshot& state) {
        if (state.increment_8_24 == scale) {
            return false;
        }
        state.increment_8_24 = scale;
        return true;
    });
}

bool arm_system_counter::set_scale_8_24_at(
    uint32_t scale, const sc_core::sc_time& time)
{
    return mutate_at(time, [scale](StateSnapshot& state) {
        if (state.increment_8_24 == scale) {
            return false;
        }
        state.increment_8_24 = scale;
        return true;
    });
}

bool arm_system_counter::set_control_at(
    bool enabled, bool halt_on_debug, const sc_core::sc_time& time)
{
    return mutate_at(time, [enabled, halt_on_debug](StateSnapshot& state) {
        if (state.enabled == enabled &&
            state.halt_on_debug == halt_on_debug) {
            return false;
        }
        state.enabled = enabled;
        state.halt_on_debug = halt_on_debug;
        return true;
    });
}

bool arm_system_counter::set_enabled_at(
    bool enabled, const sc_core::sc_time& time)
{
    return mutate_at(time, [enabled](StateSnapshot& state) {
        if (state.enabled == enabled) {
            return false;
        }
        state.enabled = enabled;
        return true;
    });
}

bool arm_system_counter::set_halt_on_debug_at(
    bool halt_on_debug, const sc_core::sc_time& time)
{
    return mutate_at(time, [halt_on_debug](StateSnapshot& state) {
        if (state.halt_on_debug == halt_on_debug) {
            return false;
        }
        state.halt_on_debug = halt_on_debug;
        return true;
    });
}

bool arm_system_counter::set_debug_halted_at(
    bool debug_halted, const sc_core::sc_time& time)
{
    return mutate_at(time, [debug_halted](StateSnapshot& state) {
        if (state.debug_halted == debug_halted) {
            return false;
        }
        state.debug_halted = debug_halted;
        return true;
    });
}

bool arm_system_counter::set_reported_frequency_at(
    uint64_t frequency_hz, const sc_core::sc_time& time)
{
    return mutate_at(time, [frequency_hz](StateSnapshot& state) {
        if (state.reported_frequency_hz == frequency_hz) {
            return false;
        }
        state.reported_frequency_hz = frequency_hz;
        return true;
    });
}

bool arm_system_counter::reanchor_at(
    uint64_t count, uint32_t fractional_count,
    const sc_core::sc_time& time)
{
    if (fractional_count > fractional_mask) {
        throw std::invalid_argument("fractional count exceeds 24 bits");
    }
    materialize(m_state, time_ticks(time));
    m_state.anchor_count = count;
    m_state.fractional_count = fractional_count;
    ++m_state.generation;
    notify_state_changed_at(time);
    return true;
}

bool arm_system_counter::write_count_part_at(
    uint32_t value, bool high, const sc_core::sc_time& time)
{
    materialize(m_state, time_ticks(time));
    if (high) {
        m_state.anchor_count =
            (uint64_t(value) << 32) | uint32_t(m_state.anchor_count);
    } else {
        m_state.anchor_count =
            (m_state.anchor_count & 0xffffffff00000000ULL) | value;
    }
    m_state.fractional_count = 0;
    ++m_state.generation;
    notify_state_changed_at(time);
    return true;
}

bool arm_system_counter::write_count_low_at(
    uint32_t value, const sc_core::sc_time& time)
{
    return write_count_part_at(value, false, time);
}

bool arm_system_counter::write_count_high_at(
    uint32_t value, const sc_core::sc_time& time)
{
    return write_count_part_at(value, true, time);
}

void arm_system_counter::restore_state_at(
    const StateSnapshot& state, const sc_core::sc_time& time)
{
    validate_state(state);
    validate_time(m_state, time_ticks(time));
    const uint64_t generation = m_state.generation + 1;
    m_state = state;
    m_state.anchor_time_ticks = time_ticks(time);
    m_state.generation = generation;
    notify_state_changed_at(time);
}

void arm_system_counter::reset_at(const sc_core::sc_time& time)
{
    validate_time(m_state, time_ticks(time));
    const uint64_t generation = m_state.generation + 1;
    m_state = m_reset_state;
    m_state.anchor_time_ticks = time_ticks(time);
    m_state.generation = generation;
    notify_state_changed_at(time);
}

}

extern "C" void module_register()
{
    typedef gs::arm_system_counter arm_system_counter;
    GSC_MODULE_REGISTER_C(arm_system_counter);
}
