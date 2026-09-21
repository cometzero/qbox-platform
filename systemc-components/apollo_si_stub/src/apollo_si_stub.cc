/* SPDX-License-Identifier: BSD-3-Clause */
#include <apollo_si_stub.h>

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(apollo_si_stub);
}
