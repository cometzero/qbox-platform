/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <rse_protection_ctrl.h>
#include <systemc>
#include <tlm>
#include <tlm-extensions/request-context.h>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class rse_ppc_filter : public sc_core::sc_module
{
    static constexpr uint32_t PERIPHNSPPC0 = 0x070;
    static constexpr uint32_t PERIPHSPPPC0 = 0x0b0;
    static constexpr uint32_t PERIPHNSPPPC0 = 0x0b0;

    rse_protection_ctrl& m_sacfg;
    rse_protection_ctrl& m_nsacfg;

    bool policy_allows(const RequestContext& context) const
    {
        const uint32_t mask = p_policy_mask.get_value();

        if (!context.secure_valid || !context.privileged_valid || mask == 0) {
            return false;
        }
        if (!context.secure &&
            (m_sacfg.policy_value(p_non_secure_offset.get_value()) & mask) == 0) {
            return false;
        }
        if (context.privileged) {
            return true;
        }

        const rse_protection_ctrl& privilege_ctrl =
            context.secure ? m_sacfg : m_nsacfg;
        const uint32_t offset = context.secure ?
            p_secure_unprivileged_offset.get_value() :
            p_non_secure_unprivileged_offset.get_value();
        return (privilege_ctrl.policy_value(offset) & mask) != 0;
    }

    bool access_allowed(tlm::tlm_generic_payload& trans,
                        RequestAccessPath expected_path) const
    {
        RequestContextTlmExtension* extension = nullptr;
        trans.get_extension(extension);
        if (extension == nullptr) {
            return false;
        }

        const RequestContext& context = extension->get_context();
        return context.access_path == expected_path && policy_allows(context);
    }

    void deny(tlm::tlm_generic_payload& trans) const
    {
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

public:
    cci::cci_param<uint32_t> p_policy_mask;
    cci::cci_param<uint32_t> p_non_secure_offset;
    cci::cci_param<uint32_t> p_secure_unprivileged_offset;
    cci::cci_param<uint32_t> p_non_secure_unprivileged_offset;

    tlm_utils::simple_target_socket<rse_ppc_filter, DEFAULT_TLM_BUSWIDTH>
        target_socket;
    tlm_utils::simple_initiator_socket<rse_ppc_filter, DEFAULT_TLM_BUSWIDTH>
        initiator_socket;

    rse_ppc_filter(sc_core::sc_module_name name, rse_protection_ctrl& sacfg,
                   rse_protection_ctrl& nsacfg)
        : sc_core::sc_module(name)
        , m_sacfg(sacfg)
        , m_nsacfg(nsacfg)
        , p_policy_mask("policy_mask", 0)
        , p_non_secure_offset("non_secure_offset", PERIPHNSPPC0)
        , p_secure_unprivileged_offset("secure_unprivileged_offset",
                                       PERIPHSPPPC0)
        , p_non_secure_unprivileged_offset("non_secure_unprivileged_offset",
                                           PERIPHNSPPPC0)
        , target_socket("target_socket")
        , initiator_socket("initiator_socket")
    {
        target_socket.register_b_transport(this, &rse_ppc_filter::b_transport);
        target_socket.register_transport_dbg(this,
                                             &rse_ppc_filter::transport_dbg);
        target_socket.register_get_direct_mem_ptr(
            this, &rse_ppc_filter::get_direct_mem_ptr);
    }

    rse_ppc_filter(sc_core::sc_module_name name, sc_core::sc_object* sacfg,
                   sc_core::sc_object* nsacfg);

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        if (!access_allowed(trans, RequestAccessPath::REGULAR) ||
            initiator_socket.size() == 0) {
            deny(trans);
            return;
        }
        initiator_socket->b_transport(trans, delay);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        deny(trans);
        return 0;
    }

    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi&)
    {
        trans.set_dmi_allowed(false);
        return false;
    }
};

extern "C" void module_register();
