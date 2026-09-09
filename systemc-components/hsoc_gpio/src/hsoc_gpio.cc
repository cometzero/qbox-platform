/* SPDX-License-Identifier: BSD-3-Clause */

#include <hsoc_gpio.h>

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(hsoc_gpio);
}
