/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <module_factory_registery.h>

#include "signal_or.h"

extern "C" void module_register() { GSC_MODULE_REGISTER_C(signal_or); }
