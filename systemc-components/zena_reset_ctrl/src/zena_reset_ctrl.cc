/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zena_reset_ctrl.h>

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(zena_reset_ctrl);
}
