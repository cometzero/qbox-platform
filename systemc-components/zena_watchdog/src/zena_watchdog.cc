/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zena_watchdog.h>

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(zena_watchdog);
}
