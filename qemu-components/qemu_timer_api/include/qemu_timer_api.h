/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <cstdint>

#include <libqemu-cxx/libqemu-cxx.h>

struct QemuClock;

namespace qbox_platform {
namespace qemu_timer {

struct ArmMMIOTimerSnapshot {
    uint64_t count = 0;
    uint64_t cval = 0;
    uint32_t cntfrq = 0;
    uint32_t ctl = 0;
    uint32_t irq_level = 0;
};

struct ArmSSETimerSnapshot {
    uint64_t count = 0;
    uint64_t cval = 0;
    uint64_t counter_frequency_hz = 0;
    uint32_t cntfrq = 0;
    uint32_t ctl = 0;
};

QemuClock* clock_new(qemu::LibQemu& qemu, const qemu::Object& parent,
                     const char* name);
bool clock_update_hz(qemu::LibQemu& qemu, QemuClock* clock,
                     uint64_t frequency_hz);
void connect_clock(qemu::Device& device, const char* name, QemuClock* clock);
void cold_reset(qemu::Device& device);
void set_sse_counter_snapshot(qemu::Device& counter, uint64_t count,
                              bool running);
uint64_t get_sse_counter_value(qemu::Device& counter);
void set_arm_mmio_counter_snapshot(qemu::Device& timer, uint64_t count,
                                   bool running, uint32_t frequency_hz);
uint64_t get_arm_mmio_counter_value(qemu::Device& timer);
bool get_arm_mmio_timer_snapshot(qemu::Device& timer, uint32_t frame,
                                 ArmMMIOTimerSnapshot& snapshot);
bool get_sse_timer_snapshot(qemu::Device& counter, qemu::Device& timer,
                            ArmSSETimerSnapshot& snapshot);

}
}
