/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <cstdint>

#include <arm.h>
#include <arm_system_counter.h>
#include <device.h>
#include <module_factory_registery.h>
#include <qemu_timer_api.h>
#include <systemc>

class qemu_arm_counter_mirror : public sc_core::sc_module
{
private:
    QemuCpuArm& m_cpu;
    gs::arm_system_counter& m_counter;

    void publish();
    void publish_loop();

public:
    SC_HAS_PROCESS(qemu_arm_counter_mirror);

    qemu_arm_counter_mirror(sc_core::sc_module_name name,
                            sc_core::sc_object* cpu,
                            sc_core::sc_object* counter);

    uint64_t synchronize();
    uint64_t local_count();
    uint64_t local_generation();
};

class qemu_arm_mmio_counter_mirror : public sc_core::sc_module
{
private:
    QemuDevice& m_timer;
    gs::arm_system_counter& m_counter;

    void publish();
    void publish_loop();

public:
    SC_HAS_PROCESS(qemu_arm_mmio_counter_mirror);

    qemu_arm_mmio_counter_mirror(sc_core::sc_module_name name,
                                 sc_core::sc_object* timer,
                                 sc_core::sc_object* counter);

    uint64_t synchronize();
    uint64_t local_count();
    qbox_platform::qemu_timer::ArmMMIOTimerSnapshot snapshot(
        uint32_t frame);
};

extern "C" void module_register();
