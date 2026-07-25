/* SPDX-License-Identifier: BSD-3-Clause */

#include <qemu_timer_api.h>

namespace qbox_platform {
namespace qemu_timer {

QemuClock* clock_new(qemu::LibQemu& qemu, const qemu::Object& parent,
                     const char* name)
{
    QemuClock* clock =
        qemu.plugin_api().clock_new(parent.get_qemu_obj(), name);
    if (clock == nullptr) {
        throw qemu::LibQemuException("failed to create QEMU clock");
    }
    return clock;
}

bool clock_update_hz(qemu::LibQemu& qemu, QemuClock* clock,
                     uint64_t frequency_hz)
{
    return qemu.plugin_api().clock_update_hz(clock, frequency_hz);
}

void connect_clock(qemu::Device& device, const char* name, QemuClock* clock)
{
    device.get_inst().plugin_api().qdev_connect_clock_in(
        reinterpret_cast<QemuDevice*>(device.get_qemu_obj()), name, clock);
}

void cold_reset(qemu::Device& device)
{
    qemu::LibQemu& qemu = device.get_inst();
    qemu.lock_iothread();
    qemu.plugin_api().device_cold_reset(
        reinterpret_cast<QemuDevice*>(device.get_qemu_obj()));
    qemu.unlock_iothread();
}

void set_sse_counter_snapshot(qemu::Device& counter, uint64_t count,
                              bool running)
{
    qemu::LibQemu& qemu = counter.get_inst();
    qemu.lock_iothread();
    qemu.plugin_api().sse_counter_set_snapshot(
        counter.get_qemu_obj(), count, running);
    qemu.unlock_iothread();
}

uint64_t get_sse_counter_value(qemu::Device& counter)
{
    qemu::LibQemu& qemu = counter.get_inst();
    qemu.lock_iothread();
    const uint64_t value =
        qemu.plugin_api().sse_counter_get_value(counter.get_qemu_obj());
    qemu.unlock_iothread();
    return value;
}

void set_arm_mmio_counter_snapshot(qemu::Device& timer, uint64_t count,
                                   bool running, uint32_t frequency_hz)
{
    qemu::LibQemu& qemu = timer.get_inst();
    qemu.lock_iothread();
    qemu.plugin_api().arm_arch_timer_mmio_set_snapshot(
        timer.get_qemu_obj(), count, running, frequency_hz);
    qemu.unlock_iothread();
}

uint64_t get_arm_mmio_counter_value(qemu::Device& timer)
{
    qemu::LibQemu& qemu = timer.get_inst();
    qemu.lock_iothread();
    const uint64_t value =
        qemu.plugin_api().arm_arch_timer_mmio_get_value(
            timer.get_qemu_obj());
    qemu.unlock_iothread();
    return value;
}

bool get_arm_mmio_timer_snapshot(qemu::Device& timer, uint32_t frame,
                                 ArmMMIOTimerSnapshot& snapshot)
{
    LibQemuArmArchTimerMMIOFrameSnapshot raw {};
    qemu::LibQemu& qemu = timer.get_inst();
    qemu.lock_iothread();
    const bool captured =
        qemu.plugin_api().arm_arch_timer_mmio_get_frame_snapshot(
            timer.get_qemu_obj(), frame, &raw);
    qemu.unlock_iothread();
    if (!captured) {
        return false;
    }
    snapshot.count = raw.count;
    snapshot.cval = raw.cval;
    snapshot.cntfrq = raw.cntfrq;
    snapshot.ctl = raw.ctl;
    snapshot.irq_level = raw.irq_level;
    return true;
}

bool get_sse_timer_snapshot(qemu::Device& counter, qemu::Device& timer,
                            ArmSSETimerSnapshot& snapshot)
{
    LibQemuArmSSETimerSnapshot raw {};
    qemu::LibQemu& qemu = timer.get_inst();
    qemu.lock_iothread();
    const bool captured = qemu.plugin_api().sse_timer_get_snapshot(
        counter.get_qemu_obj(), timer.get_qemu_obj(), &raw);
    qemu.unlock_iothread();
    if (!captured) {
        return false;
    }
    snapshot.count = raw.count;
    snapshot.cval = raw.cval;
    snapshot.counter_frequency_hz = raw.counter_frequency_hz;
    snapshot.cntfrq = raw.cntfrq;
    snapshot.ctl = raw.ctl;
    return true;
}

}
}
