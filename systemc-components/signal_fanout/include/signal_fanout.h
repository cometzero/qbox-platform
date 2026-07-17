/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <utility>
#include <vector>

#include <module_factory_registery.h>
#include <ports/multiinitiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>

class signal_fanout : public sc_core::sc_module
{
    std::vector<bool> m_pending_values;
    sc_core::sc_event m_event;

    void emit()
    {
        const std::vector<bool> pending = std::move(m_pending_values);
        m_pending_values.clear();
        signal_out.async_write_vector(pending);
    }

public:
    SC_HAS_PROCESS(signal_fanout);

    TargetSignalSocket<bool> signal_in;
    MultiInitiatorSignalSocket<> signal_out;

    explicit signal_fanout(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal_in("signal_in")
        , signal_out("signal_out")
    {
        signal_in.register_value_changed_cb([this](bool value) {
            m_pending_values.push_back(value);
            m_event.notify(sc_core::SC_ZERO_TIME);
        });

        SC_METHOD(emit);
        sensitive << m_event;
        dont_initialize();
    }
};
