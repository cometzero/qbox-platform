/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstddef>
#include <vector>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>

class signal_or : public sc_core::sc_module
{
    std::vector<bool> m_input_values;
    bool m_output_value = false;
    sc_core::sc_event m_update_event;

    void update_output()
    {
        bool value = false;
        for (const bool input_value : m_input_values) value = value || input_value;

        if (value == m_output_value) return;

        m_output_value = value;
        signal_out->write(value);
    }

public:
    SC_HAS_PROCESS(signal_or);

    cci::cci_param<unsigned int> p_num_inputs;
    sc_core::sc_vector<TargetSignalSocket<bool>> signal_in;
    InitiatorSignalSocket<bool> signal_out;

    explicit signal_or(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_num_inputs("num_inputs", 2)
        , signal_in("signal_in", p_num_inputs.get_value(),
                    [](const char* socket_name, size_t) { return new TargetSignalSocket<bool>(socket_name); })
        , signal_out("signal_out")
    {
        m_input_values.assign(p_num_inputs.get_value(), false);
        for (size_t index = 0; index < signal_in.size(); ++index) {
            signal_in[index].register_value_changed_cb([this, index](bool value) {
                m_input_values[index] = value;
                m_update_event.notify(sc_core::SC_ZERO_TIME);
            });
        }

        SC_METHOD(update_output);
        sensitive << m_update_event;
        dont_initialize();
    }
};

extern "C" void module_register();
