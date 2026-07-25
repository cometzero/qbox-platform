/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <device.h>
#include <module_factory_registery.h>
#include <ports/target.h>
#include <ports/target-signal-socket.h>
#include <arm_system_counter.h>
#include <qemu_clock_source.h>

class qemu_sse_counter : public QemuDevice
{
protected:
    qemu_clock_source& m_clock_source;

public:
    QemuTargetSocket<> control;
    QemuTargetSocket<> status;
    TargetSignalSocket<bool> reset;

    qemu_sse_counter(const sc_core::sc_module_name& name,
                     sc_core::sc_object* instance,
                     sc_core::sc_object* clock_source);
    qemu_sse_counter(const sc_core::sc_module_name& name,
                     QemuInstance& instance,
                     qemu_clock_source& clock_source);

    void before_end_of_elaboration() override;
    void end_of_elaboration() override;
    uint64_t local_count();
};

class qemu_sse_counter_mirror : public qemu_sse_counter
{
private:
    gs::arm_system_counter& m_counter;

    void publish();
    void publish_loop();

public:
    SC_HAS_PROCESS(qemu_sse_counter_mirror);

    qemu_sse_counter_mirror(const sc_core::sc_module_name& name,
                            sc_core::sc_object* instance,
                            sc_core::sc_object* clock_source,
                            sc_core::sc_object* counter);

    uint64_t synchronize();
};

extern "C" void module_register();
