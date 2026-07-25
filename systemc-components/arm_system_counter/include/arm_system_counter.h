/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <cstdint>
#include <functional>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/target-signal-socket.h>
#include <systemc>

namespace gs {

class arm_system_counter : public sc_core::sc_module
{
public:
    static constexpr uint32_t fractional_bits = 24;
    static constexpr uint32_t fractional_mask =
        (uint32_t(1) << fractional_bits) - 1;

    struct StateSnapshot {
        uint64_t anchor_count = 0;
        sc_dt::uint64 anchor_time_ticks = 0;
        uint64_t input_tick_remainder = 0;
        uint32_t fractional_count = 0;
        uint64_t input_frequency_hz = 125000000;
        uint64_t increment_8_24 = uint64_t(1) << fractional_bits;
        uint64_t reported_frequency_hz = 125000000;
        bool enabled = true;
        bool halt_on_debug = false;
        bool debug_halted = false;
        uint64_t generation = 0;

        uint64_t integer_increment() const
        {
            return increment_8_24 >> fractional_bits;
        }

        bool running() const
        {
            return enabled && !(halt_on_debug && debug_halted);
        }
    };

private:
    cci::cci_param<uint64_t> p_input_frequency_hz;
    cci::cci_param<uint64_t> p_integer_increment;
    cci::cci_param<uint64_t> p_reported_frequency_hz;
    cci::cci_param<bool> p_enabled;
    cci::cci_param<bool> p_halt_on_debug;
    cci::cci_param<uint64_t> p_initial_count;

    StateSnapshot m_state;
    StateSnapshot m_reset_state;
    sc_core::sc_event m_state_changed;

    static sc_dt::uint64 ticks_per_second();
    static void validate_state(const StateSnapshot& state);
    static void validate_time(const StateSnapshot& state,
                              sc_dt::uint64 absolute_ticks);
    static void materialize(StateSnapshot& state,
                            sc_dt::uint64 absolute_ticks);
    void notify_state_changed_at(const sc_core::sc_time& time);
    bool mutate_at(
        const sc_core::sc_time& time,
        const std::function<bool(StateSnapshot&)>& mutation);
    bool write_count_part_at(uint32_t value, bool high,
                             const sc_core::sc_time& time);

public:
    TargetSignalSocket<bool> reset;

    explicit arm_system_counter(const sc_core::sc_module_name& name);

    static sc_dt::uint64 time_ticks(const sc_core::sc_time& time)
    {
        return time.value();
    }

    StateSnapshot snapshot() const;
    StateSnapshot snapshot_at(const sc_core::sc_time& time) const;
    StateSnapshot save_state_at(const sc_core::sc_time& time) const;
    uint64_t count_at(const sc_core::sc_time& time) const;
    const sc_core::sc_event& state_changed_event() const
    {
        return m_state_changed;
    }

    bool set_input_frequency_at(uint64_t frequency_hz,
                                const sc_core::sc_time& time);
    bool set_integer_increment_at(uint64_t increment,
                                  const sc_core::sc_time& time);
    bool set_scale_8_24_at(uint32_t scale,
                           const sc_core::sc_time& time);
    bool set_control_at(bool enabled, bool halt_on_debug,
                        const sc_core::sc_time& time);
    bool set_enabled_at(bool enabled, const sc_core::sc_time& time);
    bool set_halt_on_debug_at(bool halt_on_debug,
                              const sc_core::sc_time& time);
    bool set_debug_halted_at(bool debug_halted,
                             const sc_core::sc_time& time);
    bool set_reported_frequency_at(uint64_t frequency_hz,
                                   const sc_core::sc_time& time);
    bool reanchor_at(uint64_t count, uint32_t fractional_count,
                     const sc_core::sc_time& time);
    bool write_count_low_at(uint32_t value,
                            const sc_core::sc_time& time);
    bool write_count_high_at(uint32_t value,
                             const sc_core::sc_time& time);

    void restore_state_at(const StateSnapshot& state,
                          const sc_core::sc_time& time);
    void reset_at(const sc_core::sc_time& time);
};

}

extern "C" void module_register();
