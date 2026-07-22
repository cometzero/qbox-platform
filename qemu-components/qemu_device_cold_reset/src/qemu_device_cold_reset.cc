/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <qemu_device_cold_reset.h>

#include <stdexcept>

namespace {

QemuDevice& require_device(sc_core::sc_object* object)
{
    QemuDevice* device = dynamic_cast<QemuDevice*>(object);
    if (device == nullptr) {
        throw std::invalid_argument("expected QemuDevice");
    }
    return *device;
}

}

qemu_device_cold_reset::qemu_device_cold_reset(
    sc_core::sc_module_name name, sc_core::sc_object* device)
    : sc_core::sc_module(name)
    , m_device(require_device(device))
    , reset("reset")
{
    reset.register_value_changed_cb([this](bool asserted) {
        if (asserted) {
            m_device.get_qemu_inst().execute_on_iothread_sync([this] {
                m_device.get_qemu_dev().cold_reset();
            });
        }
    });
}

void module_register()
{
    GSC_MODULE_REGISTER_C(qemu_device_cold_reset, sc_core::sc_object*);
}
