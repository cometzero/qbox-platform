/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

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

    static rse_protection_ctrl& require_ctrl(sc_core::sc_object* object)
    {
        auto* ctrl = dynamic_cast<rse_protection_ctrl*>(object);
        if (ctrl == nullptr) {
            throw std::invalid_argument(
                "rse_ppc_filter requires rse_protection_ctrl");
        }
        return *ctrl;
    }

    bool access_allowed(const tlm::tlm_generic_payload& trans) const
    {
        const auto* extension =
            trans.get_extension<RequestContextTlmExtension>();
        if (extension == nullptr) {
            return false;
        }

        const RequestContext& context = extension->get_context();
        if (!context.secure_valid || !context.privileged_valid) {
            return false;
        }

        const uint32_t mask = p_policy_mask.get_value();
        if (context.secure) {
            return context.privileged ||
                   m_sacfg.policy_allows(PERIPHSPPPC0, mask);
        }

        if (!m_sacfg.policy_allows(PERIPHNSPPC0, mask)) {
            return false;
        }
        return context.privileged ||
               m_nsacfg.policy_allows(PERIPHNSPPPC0, mask);
    }

    void deny(tlm::tlm_generic_payload& trans) const
    {
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

public:
    cci::cci_param<uint32_t> p_policy_mask;
    tlm_utils::simple_target_socket<rse_ppc_filter, DEFAULT_TLM_BUSWIDTH>
        target_socket;
    tlm_utils::simple_initiator_socket<rse_ppc_filter, DEFAULT_TLM_BUSWIDTH>
        initiator_socket;

    rse_ppc_filter(sc_core::sc_module_name name, sc_core::sc_object* sacfg,
                   sc_core::sc_object* nsacfg)
        : sc_core::sc_module(name)
        , m_sacfg(require_ctrl(sacfg))
        , m_nsacfg(require_ctrl(nsacfg))
        , p_policy_mask("policy_mask", 0)
        , target_socket("target_socket")
        , initiator_socket("initiator_socket")
    {
        target_socket.register_b_transport(
            this, &rse_ppc_filter::b_transport);
        target_socket.register_transport_dbg(
            this, &rse_ppc_filter::transport_dbg);
        target_socket.register_get_direct_mem_ptr(
            this, &rse_ppc_filter::get_direct_mem_ptr);
        initiator_socket.register_invalidate_direct_mem_ptr(
            this, &rse_ppc_filter::invalidate_direct_mem_ptr);
    }

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        if (!access_allowed(trans)) {
            deny(trans);
            return;
        }
        initiator_socket->b_transport(trans, delay);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        if (!access_allowed(trans)) {
            deny(trans);
            return 0;
        }
        return initiator_socket->transport_dbg(trans);
    }

    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                            tlm::tlm_dmi& dmi)
    {
        (void)dmi;
        deny(trans);
        return false;
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end)
    {
        target_socket->invalidate_direct_mem_ptr(start, end);
    }
};

extern "C" void module_register();
