/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>

#include <cci/utils/broker.h>

int run_canonical_gic_integration(
    const std::string& scenario,
    cci_utils::consuming_broker& broker);
