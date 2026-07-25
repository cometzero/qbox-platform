/* SPDX-License-Identifier: BSD-3-Clause */

#include <qemu_sse_counter.h>

#include <limits>
#include <stdexcept>

namespace {

template <typename T>
T& require_object(sc_core::sc_object* object, const char* type)
{
    T* typed = dynamic_cast<T*>(object);
    if (typed == nullptr) {
        throw std::invalid_argument(std::string("expected ") + type);
    }
    return *typed;
}

uint64_t effective_frequency(
    const gs::arm_system_counter::StateSnapshot& snapshot)
{
    using Wide = unsigned __int128;
    constexpr uint64_t scale = uint64_t(1)
                               << gs::arm_system_counter::fractional_bits;
    const Wide numerator =
        Wide(snapshot.input_frequency_hz) * snapshot.increment_8_24;

    if (numerator == 0 &&
        snapshot.input_frequency_hz <=
            std::numeric_limits<unsigned int>::max()) {
        return snapshot.input_frequency_hz;
    }
    if (numerator % scale != 0) {
        throw std::invalid_argument(
            "RSE counter mirror requires an integer effective frequency");
    }
    const Wide frequency = numerator / scale;
    if (frequency == 0 ||
        frequency > std::numeric_limits<unsigned int>::max()) {
        throw std::out_of_range(
            "RSE counter mirror frequency is unsupported");
    }
    return static_cast<uint64_t>(frequency);
}

}

qemu_sse_counter::qemu_sse_counter(const sc_core::sc_module_name& name,
                                   sc_core::sc_object* instance,
                                   sc_core::sc_object* clock_source)
    : qemu_sse_counter(
          name, require_object<QemuInstance>(instance, "QemuInstance"),
          require_object<qemu_clock_source>(clock_source,
                                            "qemu_clock_source"))
{
}

qemu_sse_counter::qemu_sse_counter(const sc_core::sc_module_name& name,
                                   QemuInstance& instance,
                                   qemu_clock_source& clock_source)
    : QemuDevice(name, instance, "sse-counter")
    , m_clock_source(clock_source)
    , control("control", instance)
    , status("status", instance)
    , reset("reset")
{
    reset.register_value_changed_cb([this](bool asserted) {
        if (asserted) {
            qbox_platform::qemu_timer::cold_reset(m_dev);
        }
    });
}

void qemu_sse_counter::before_end_of_elaboration()
{
    QemuDevice::before_end_of_elaboration();
    qbox_platform::qemu_timer::connect_clock(
        m_dev, "CLK", m_clock_source.clock());
}

void qemu_sse_counter::end_of_elaboration()
{
    QemuDevice::set_sysbus_as_parent_bus();
    QemuDevice::end_of_elaboration();
    qemu::SysBusDevice sysbus(m_dev);
    control.init(sysbus, 0);
    status.init(sysbus, 1);
}

uint64_t qemu_sse_counter::local_count()
{
    return qbox_platform::qemu_timer::get_sse_counter_value(m_dev);
}

qemu_sse_counter_mirror::qemu_sse_counter_mirror(
    const sc_core::sc_module_name& name, sc_core::sc_object* instance,
    sc_core::sc_object* clock_source, sc_core::sc_object* counter)
    : qemu_sse_counter(
          name, require_object<QemuInstance>(instance, "QemuInstance"),
          require_object<qemu_clock_source>(clock_source,
                                            "qemu_clock_source"))
    , m_counter(require_object<gs::arm_system_counter>(
          counter, "arm_system_counter"))
{
    SC_THREAD(publish_loop);
}

void qemu_sse_counter_mirror::publish()
{
    synchronize();
}

uint64_t qemu_sse_counter_mirror::synchronize()
{
    const gs::arm_system_counter::StateSnapshot snapshot =
        m_counter.snapshot_at(sc_core::sc_time_stamp());
    const bool running =
        snapshot.running() && snapshot.increment_8_24 != 0;
    const uint64_t frequency = effective_frequency(snapshot);

    m_clock_source.set_frequency(frequency);
    qbox_platform::qemu_timer::set_sse_counter_snapshot(
        m_dev, snapshot.anchor_count, running);
    return snapshot.anchor_count;
}

void qemu_sse_counter_mirror::publish_loop()
{
    publish();
    while (true) {
        sc_core::wait(m_counter.state_changed_event());
        publish();
    }
}

void module_register()
{
    GSC_MODULE_REGISTER_C(qemu_sse_counter, sc_core::sc_object*,
                          sc_core::sc_object*);
    GSC_MODULE_REGISTER_C(qemu_sse_counter_mirror, sc_core::sc_object*,
                          sc_core::sc_object*, sc_core::sc_object*);
}
