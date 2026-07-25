/* SPDX-License-Identifier: BSD-3-Clause */

#include <qemu_clock_source.h>

#include <stdexcept>

namespace {

QemuInstance& require_instance(sc_core::sc_object* object)
{
    QemuInstance* instance = dynamic_cast<QemuInstance*>(object);
    if (instance == nullptr) {
        throw std::invalid_argument("expected QemuInstance");
    }
    return *instance;
}

}

qemu_clock_source::qemu_clock_source(const sc_core::sc_module_name& name,
                                     sc_core::sc_object* instance)
    : qemu_clock_source(name, require_instance(instance))
{
}

qemu_clock_source::qemu_clock_source(const sc_core::sc_module_name& name,
                                     QemuInstance& instance)
    : sc_core::sc_module(name)
    , m_inst(instance)
    , p_frequency_hz("frequency_hz", 0,
                     "Required QEMU source clock frequency in Hz")
{
}

void qemu_clock_source::initialize()
{
    if (m_initialized) {
        return;
    }
    if (p_frequency_hz.get_value() == 0) {
        throw std::invalid_argument("qemu_clock_source frequency_hz is required");
    }
    qemu::LibQemu& qemu = m_inst.get();
    m_owner = qemu.object_new("container", nullptr);
    m_clock = qbox_platform::qemu_timer::clock_new(qemu, m_owner, "clock");
    if (!qbox_platform::qemu_timer::clock_update_hz(
            qemu, m_clock, p_frequency_hz.get_value())) {
        throw std::out_of_range("qemu_clock_source frequency_hz is unsupported");
    }
    m_initialized = true;
}

QemuClock* qemu_clock_source::clock()
{
    initialize();
    return m_clock;
}

void qemu_clock_source::set_frequency(uint64_t frequency_hz)
{
    initialize();
    if (frequency_hz == 0 ||
        !qbox_platform::qemu_timer::clock_update_hz(
            m_inst.get(), m_clock, frequency_hz)) {
        throw std::out_of_range(
            "qemu_clock_source frequency_hz is unsupported");
    }
}

void qemu_clock_source::before_end_of_elaboration()
{
    initialize();
}

void module_register()
{
    GSC_MODULE_REGISTER_C(qemu_clock_source, sc_core::sc_object*);
}
