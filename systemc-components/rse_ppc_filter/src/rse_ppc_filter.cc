/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <rse_ppc_filter.h>

#include <stdexcept>

namespace {

rse_protection_ctrl& require_ctrl(sc_core::sc_object* object)
{
    rse_protection_ctrl* ctrl = dynamic_cast<rse_protection_ctrl*>(object);
    if (ctrl == nullptr) {
        throw std::invalid_argument("expected rse_protection_ctrl");
    }
    return *ctrl;
}

}

rse_ppc_filter::rse_ppc_filter(sc_core::sc_module_name name,
                               sc_core::sc_object* sacfg,
                               sc_core::sc_object* nsacfg)
    : rse_ppc_filter(name, require_ctrl(sacfg), require_ctrl(nsacfg))
{
}

void module_register()
{
    GSC_MODULE_REGISTER_C(rse_ppc_filter, sc_core::sc_object*,
                          sc_core::sc_object*);
}
