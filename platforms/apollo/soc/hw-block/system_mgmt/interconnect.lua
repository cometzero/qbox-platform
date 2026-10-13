-- SPDX-License-Identifier: BSD-3-Clause
local HOST_SI_ATU_PHYS_BASE = 0x4000031000000
local HOST_SI_CONTROL_WINDOW_SIZE = 0x00010000
local HOST_AP_ATU_PHYS_BASE = 0x20000D0080000
local HOST_AP_ATU_LOGICAL_BASE = 0x40000000
local HOST_AP_ATU_LOGICAL_SIZE = 0x00800000
local HOST_SMDEXP2SMD_ATU_PHYS_BASE = 0x20000D0070000
local SYSTEM_ATU_BUILD_CONFIG = 0x000000C5

local interconnect = {}

function interconnect.define_si(ctx, platform)
    local options = ctx.options
    local atu_trace_address_min = tonumber(options.getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MIN", "0"))
    local atu_trace_address_max = tonumber(options.getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MAX", "0"))
    assert(platform.host_si_atu == nil, "duplicate Apollo object: host_si_atu")
    platform.host_si_atu = {
        moduletype = "rse_atu";
        trace = options.atu_trace;
        trace_limit = options.atu_trace_limit;
        trace_filter = options.atu_trace_filter;
        trace_address_min = atu_trace_address_min;
        trace_address_max = atu_trace_address_max;
        build_config = SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = HOST_SI_ATU_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }
end

function interconnect.define_ap(ctx, platform)
    local options = ctx.options
    local atu_trace_address_min = tonumber(options.getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MIN", "0"))
    local atu_trace_address_max = tonumber(options.getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MAX", "0"))
    assert(platform.host_ap_atu == nil, "duplicate Apollo object: host_ap_atu")
    platform.host_ap_atu = {
        moduletype = "rse_atu";
        trace = options.atu_trace;
        trace_limit = options.atu_trace_limit;
        trace_filter = options.atu_trace_filter;
        trace_address_min = atu_trace_address_min;
        trace_address_max = atu_trace_address_max;
        enable_dmi = options.enable_ap_cpus and options.atu_dmi;
        build_config = SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = HOST_AP_ATU_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        translation_socket = options.enable_ap_cpus and {
            address = HOST_AP_ATU_LOGICAL_BASE;
            size = HOST_AP_ATU_LOGICAL_SIZE;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
            priority = 10;
        } or nil;
        initiator_socket = options.enable_ap_cpus and {bind = "&system_router.target_socket"} or nil;
        log_level = 0;
    }
end

function interconnect.define_smd(ctx, platform)
    local options = ctx.options
    local atu_trace_address_min = tonumber(options.getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MIN", "0"))
    local atu_trace_address_max = tonumber(options.getenv_or(
        "QBOX_RDASPEN_ATU_TRACE_ADDRESS_MAX", "0"))
    assert(platform.host_smdexp2smd_atu == nil, "duplicate Apollo object: host_smdexp2smd_atu")
    platform.host_smdexp2smd_atu = {
        moduletype = "rse_atu";
        trace = options.atu_trace;
        trace_limit = options.atu_trace_limit;
        trace_filter = options.atu_trace_filter;
        trace_address_min = atu_trace_address_min;
        trace_address_max = atu_trace_address_max;
        build_config = SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = HOST_SMDEXP2SMD_ATU_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }
end

return interconnect
