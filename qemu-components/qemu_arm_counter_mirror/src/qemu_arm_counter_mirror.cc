/* SPDX-License-Identifier: BSD-3-Clause */

#include <qemu_arm_counter_mirror.h>

#include <limits>
#include <stdexcept>

namespace {

QemuCpuArm& require_cpu(sc_core::sc_object* object)
{
    QemuCpuArm* cpu = dynamic_cast<QemuCpuArm*>(object);
    if (cpu == nullptr) {
        throw std::invalid_argument("expected QemuCpuArm");
    }
    return *cpu;
}

QemuDevice& require_device(sc_core::sc_object* object)
{
    QemuDevice* device = dynamic_cast<QemuDevice*>(object);
    if (device == nullptr) {
        throw std::invalid_argument("expected QemuDevice");
    }
    return *device;
}

gs::arm_system_counter& require_counter(sc_core::sc_object* object)
{
    gs::arm_system_counter* counter =
        dynamic_cast<gs::arm_system_counter*>(object);
    if (counter == nullptr) {
        throw std::invalid_argument("expected arm_system_counter");
    }
    return *counter;
}

uint32_t effective_frequency(
    const gs::arm_system_counter::StateSnapshot& snapshot)
{
    using Wide = unsigned __int128;
    constexpr uint64_t scale = uint64_t(1)
                               << gs::arm_system_counter::fractional_bits;
    const Wide numerator =
        Wide(snapshot.input_frequency_hz) * snapshot.increment_8_24;

    if (numerator == 0 &&
        snapshot.input_frequency_hz <=
            std::numeric_limits<uint32_t>::max()) {
        return static_cast<uint32_t>(snapshot.input_frequency_hz);
    }
    if (numerator % scale != 0) {
        throw std::invalid_argument(
            "QEMU counter mirror requires an integer effective frequency");
    }
    const Wide frequency = numerator / scale;
    if (frequency == 0 ||
        frequency > std::numeric_limits<uint32_t>::max()) {
        throw std::out_of_range(
            "QEMU counter mirror frequency is outside uint32 range");
    }
    return static_cast<uint32_t>(frequency);
}

}

qemu_arm_counter_mirror::qemu_arm_counter_mirror(
    sc_core::sc_module_name name, sc_core::sc_object* cpu,
    sc_core::sc_object* counter)
    : sc_core::sc_module(name)
    , m_cpu(require_cpu(cpu))
    , m_counter(require_counter(counter))
{
    SC_THREAD(publish_loop);
}

void qemu_arm_counter_mirror::publish()
{
    synchronize();
}

uint64_t qemu_arm_counter_mirror::synchronize()
{
    const gs::arm_system_counter::StateSnapshot snapshot =
        m_counter.snapshot_at(sc_core::sc_time_stamp());
    const bool running =
        snapshot.running() && snapshot.increment_8_24 != 0;
    m_cpu.set_gt_counter_mirror(
        true, running, snapshot.anchor_count,
        effective_frequency(snapshot), snapshot.generation);
    return snapshot.anchor_count;
}

void qemu_arm_counter_mirror::publish_loop()
{
    publish();
    while (true) {
        sc_core::wait(m_counter.state_changed_event());
        publish();
    }
}

uint64_t qemu_arm_counter_mirror::local_count()
{
    return m_cpu.get_gt_counter_value();
}

uint64_t qemu_arm_counter_mirror::local_generation()
{
    return m_cpu.get_gt_counter_generation();
}

qemu_arm_mmio_counter_mirror::qemu_arm_mmio_counter_mirror(
    sc_core::sc_module_name name, sc_core::sc_object* timer,
    sc_core::sc_object* counter)
    : sc_core::sc_module(name)
    , m_timer(require_device(timer))
    , m_counter(require_counter(counter))
{
    SC_THREAD(publish_loop);
}

void qemu_arm_mmio_counter_mirror::publish()
{
    synchronize();
}

uint64_t qemu_arm_mmio_counter_mirror::synchronize()
{
    const gs::arm_system_counter::StateSnapshot snapshot =
        m_counter.snapshot_at(sc_core::sc_time_stamp());
    const bool running =
        snapshot.running() && snapshot.increment_8_24 != 0;
    qemu::Device device = m_timer.get_qemu_dev();
    qbox_platform::qemu_timer::set_arm_mmio_counter_snapshot(
        device, snapshot.anchor_count, running, effective_frequency(snapshot));
    return snapshot.anchor_count;
}

void qemu_arm_mmio_counter_mirror::publish_loop()
{
    publish();
    while (true) {
        sc_core::wait(m_counter.state_changed_event());
        publish();
    }
}

uint64_t qemu_arm_mmio_counter_mirror::local_count()
{
    qemu::Device device = m_timer.get_qemu_dev();
    return qbox_platform::qemu_timer::get_arm_mmio_counter_value(device);
}

qbox_platform::qemu_timer::ArmMMIOTimerSnapshot
qemu_arm_mmio_counter_mirror::snapshot(uint32_t frame)
{
    qbox_platform::qemu_timer::ArmMMIOTimerSnapshot snapshot;
    qemu::Device device = m_timer.get_qemu_dev();
    if (!qbox_platform::qemu_timer::get_arm_mmio_timer_snapshot(
            device, frame, snapshot)) {
        throw std::runtime_error("failed to capture QEMU MMIO timer");
    }
    return snapshot;
}

void module_register()
{
    GSC_MODULE_REGISTER_C(qemu_arm_counter_mirror, sc_core::sc_object*,
                          sc_core::sc_object*);
    GSC_MODULE_REGISTER_C(qemu_arm_mmio_counter_mirror,
                          sc_core::sc_object*, sc_core::sc_object*);
}
