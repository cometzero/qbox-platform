/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <device.h>
#include <module_factory_registery.h>
#include <ports/target-signal-socket.h>
#include <systemc>

class qemu_device_cold_reset : public sc_core::sc_module
{
    QemuDevice& m_device;

public:
    TargetSignalSocket<bool> reset;

    qemu_device_cold_reset(sc_core::sc_module_name name,
                           sc_core::sc_object* device);
};

extern "C" void module_register();
