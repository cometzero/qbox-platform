/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gic720ae_messreg.h>

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(gic720ae_messreg);
}
