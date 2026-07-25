/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <rse_ppc_filter.h>

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(rse_ppc_filter, sc_core::sc_object*,
                          sc_core::sc_object*);
}
