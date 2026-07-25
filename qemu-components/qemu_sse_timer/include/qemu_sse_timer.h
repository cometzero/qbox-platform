/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <device.h>
#include <module_factory_registery.h>
#include <ports/qemu-initiator-signal-socket.h>
#include <ports/target.h>
#include <ports/target-signal-socket.h>
#include <qemu_sse_counter.h>

class qemu_sse_timer : public QemuDevice
{
private:
    qemu_sse_counter& m_counter;

public:
    QemuTargetSocket<> socket;
    QemuInitiatorSignalSocket irq;
    TargetSignalSocket<bool> reset;

    qemu_sse_timer(const sc_core::sc_module_name& name,
                   sc_core::sc_object* instance,
                   sc_core::sc_object* counter);
    qemu_sse_timer(const sc_core::sc_module_name& name,
                   QemuInstance& instance, qemu_sse_counter& counter);

    void before_end_of_elaboration() override;
    void end_of_elaboration() override;
    qbox_platform::qemu_timer::ArmSSETimerSnapshot snapshot();
};

extern "C" void module_register();
