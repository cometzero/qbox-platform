/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm-extensions/request-context.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class host_ni710ae_nci : public sc_core::sc_module
{
    static constexpr uint64_t REG_BYTES = 0x10000;
    static constexpr uint32_t ROOT_OFFSET = 0x0000;
    static constexpr uint32_t COMPONENT_BASE = 0x0100;
    static constexpr uint32_t COMPONENT_STRIDE = 0x0100;
    static constexpr uint32_t APU_BASE = 0x2000;
    static constexpr uint32_t APU_STRIDE = 0x1000;
    static constexpr uint32_t APU_REGION_COUNT = 32;
    static constexpr uint32_t APU_REGION_STRIDE = 0x20;
    static constexpr uint32_t APU_PRBAR_LOW = 0x00;
    static constexpr uint32_t APU_PRBAR_HIGH = 0x04;
    static constexpr uint32_t APU_PRLAR_LOW = 0x08;
    static constexpr uint32_t APU_PRLAR_HIGH = 0x0c;
    static constexpr uint32_t APU_PRID_LOW = 0x10;
    static constexpr uint32_t APU_PRID_HIGH = 0x14;

    static constexpr uint16_t NODE_TYPE_GCN = 0x00;
    static constexpr uint16_t NODE_TYPE_ASNI = 0x04;
    static constexpr uint16_t NODE_TYPE_AMNI = 0x05;
    static constexpr uint16_t NODE_TYPE_SUBFEATURE_APU = 0x00;

    static constexpr uint32_t DOMAIN_CHILD_INFO = 0x004;
    static constexpr uint32_t DOMAIN_POINTERS = 0x008;
    static constexpr uint32_t COMPONENT_NUM_SUBFEATURES = 0x024;
    static constexpr uint32_t COMPONENT_SUBFEATURES = 0x028;
    static constexpr uint32_t SUBFEATURE_ENTRY_BYTES = 0x008;

    static constexpr uint32_t APU_CTLR = 0x0ff8;
    static constexpr uint32_t APU_IIDR = 0x0ffc;
    static constexpr uint32_t APU_CTLR_ENABLE = 1u << 0;
    static constexpr uint32_t APU_CTLR_SYNC_ERROR = 1u << 2;
    static constexpr uint32_t APU_REGION_ENABLE = 1u << 0;
    static constexpr uint32_t APU_REGION_BACKGROUND = 1u << 1;
    static constexpr uint32_t APU_REGION_LOCK = 1u << 2;
    static constexpr uint32_t APU_ADDRESS_LOW_MASK = 0xffffffc0u;
    static constexpr uint32_t APU_ID_VALID_MASK = 0x0fu;
    static constexpr uint8_t APU_NS_WRITE = 0x01u;
    static constexpr uint8_t APU_S_WRITE = 0x02u;
    static constexpr uint8_t APU_NS_READ = 0x04u;
    static constexpr uint8_t APU_S_READ = 0x08u;

    enum topology_t : uint32_t {
        TOPOLOGY_MHU_MIN = 0,
        TOPOLOGY_MHU_MID = 1,
        TOPOLOGY_SECONDARY = 2,
        TOPOLOGY_PRIMARY_MIN = 3,
        TOPOLOGY_PRIMARY_MID = 4,
    };

    struct component_desc {
        uint16_t type;
        uint16_t id;
    };

    std::array<uint8_t, REG_BYTES> m_regs {};
    unsigned int m_trace_count = 0;
    uint64_t m_denied_count = 0;
    uint64_t m_last_denied_address = 0;
    uint64_t m_last_denied_origin = 0;
    uint32_t m_last_denied_domain = 0;
    uint32_t m_last_denied_requester = 0;
    bool m_dmi_invalidation_pending = false;
    sc_core::sc_event m_dmi_invalidation_event;

    static bool is_supported_length(unsigned int len)
    {
        return len == 1 || len == 2 || len == 4 || len == 8;
    }

    uint32_t load32(uint32_t offset) const
    {
        uint32_t value = 0;
        std::memcpy(&value, &m_regs[offset], sizeof(value));
        return value;
    }

    void store32(uint32_t offset, uint32_t value)
    {
        std::memcpy(&m_regs[offset], &value, sizeof(value));
    }

    uint32_t protected_apu_base() const
    {
        return APU_BASE + p_protected_apu_index.get_value() * APU_STRIDE;
    }

    static uint64_t combine_address(uint32_t high, uint32_t low)
    {
        return (static_cast<uint64_t>(high) << 32) |
               (low & APU_ADDRESS_LOW_MASK);
    }

    bool apu_region_register(uint32_t offset, uint32_t& region,
                             uint32_t& register_offset,
                             uint32_t& region_base) const
    {
        if (offset < APU_BASE)
            return false;

        const uint32_t block_relative = (offset - APU_BASE) % APU_STRIDE;
        if (block_relative >= APU_REGION_COUNT * APU_REGION_STRIDE)
            return false;

        region = block_relative / APU_REGION_STRIDE;
        register_offset = block_relative % APU_REGION_STRIDE;
        region_base = offset - register_offset;
        return true;
    }

    static uint32_t node_type_word(uint16_t type, uint16_t id)
    {
        return (static_cast<uint32_t>(id) << 16) | type;
    }

    static std::vector<component_desc> components_for_topology(uint32_t topology)
    {
        switch (topology) {
        case TOPOLOGY_MHU_MIN:
            return { { NODE_TYPE_ASNI, 3 } };
        case TOPOLOGY_MHU_MID:
            return { { NODE_TYPE_ASNI, 4 } };
        case TOPOLOGY_SECONDARY:
            return { { NODE_TYPE_ASNI, 1 } };
        case TOPOLOGY_PRIMARY_MIN:
            return {
                { NODE_TYPE_ASNI, 0 },
                { NODE_TYPE_ASNI, 3 },
                { NODE_TYPE_ASNI, 4 },
                { NODE_TYPE_ASNI, 5 },
                { NODE_TYPE_AMNI, 10 },
            };
        case TOPOLOGY_PRIMARY_MID:
        default:
            return {
                { NODE_TYPE_ASNI, 0 },
                { NODE_TYPE_ASNI, 5 },
                { NODE_TYPE_ASNI, 6 },
                { NODE_TYPE_ASNI, 7 },
                { NODE_TYPE_AMNI, 14 },
            };
        }
    }

    void seed_discovery_component(uint32_t index, const component_desc& desc)
    {
        const uint32_t component_offset =
            COMPONENT_BASE + (index * COMPONENT_STRIDE);
        const uint32_t apu_offset = APU_BASE + (index * APU_STRIDE);

        store32(DOMAIN_POINTERS + (index * sizeof(uint32_t)),
                component_offset);
        store32(component_offset, node_type_word(desc.type, desc.id));
        store32(component_offset + COMPONENT_NUM_SUBFEATURES, 1);
        store32(component_offset + COMPONENT_SUBFEATURES,
                NODE_TYPE_SUBFEATURE_APU);
        store32(component_offset + COMPONENT_SUBFEATURES + sizeof(uint32_t),
                apu_offset);
        store32(apu_offset + APU_IIDR, p_apu_iidr.get_value());
    }

    void reset_registers()
    {
        m_regs.fill(0);
        m_denied_count = 0;
        m_last_denied_address = 0;
        m_last_denied_origin = 0;
        m_last_denied_domain = 0;
        m_last_denied_requester = 0;
        const auto components = components_for_topology(p_topology.get_value());
        store32(ROOT_OFFSET, node_type_word(NODE_TYPE_GCN, 0));
        store32(DOMAIN_CHILD_INFO, static_cast<uint32_t>(components.size()));
        for (uint32_t i = 0; i < components.size(); ++i) {
            seed_discovery_component(i, components[i]);
        }
    }

    bool is_apu_iidr(uint32_t offset) const
    {
        if (offset < APU_BASE)
            return false;
        const uint32_t apu_relative = offset - APU_BASE;
        return (apu_relative % APU_STRIDE) == APU_IIDR;
    }

    bool protected_policy_register(uint32_t offset) const
    {
        const uint32_t base = protected_apu_base();
        return (offset >= base &&
                offset < base + APU_REGION_COUNT * APU_REGION_STRIDE) ||
               offset == base + APU_CTLR;
    }

    void schedule_protected_dmi_invalidation()
    {
        if (m_dmi_invalidation_pending)
            return;
        m_dmi_invalidation_pending = true;
        m_dmi_invalidation_event.notify(sc_core::SC_ZERO_TIME);
    }

    void invalidate_protected_dmi()
    {
        m_dmi_invalidation_pending = false;
        if (protected_target_socket.size() != 0) {
            protected_target_socket->invalidate_direct_mem_ptr(
                0, std::numeric_limits<uint64_t>::max());
        }
    }

    void write32(uint32_t offset, uint32_t value)
    {
        if (is_apu_iidr(offset))
            return;

        const bool policy_register = protected_policy_register(offset);
        const bool policy_was_enabled =
            (load32(protected_apu_base() + APU_CTLR) & APU_CTLR_ENABLE) != 0;

        uint32_t region = 0;
        uint32_t register_offset = 0;
        uint32_t region_base = 0;
        if (apu_region_register(offset, region, register_offset,
                                region_base)) {
            (void)region;
            const uint32_t old_prbar = load32(region_base + APU_PRBAR_LOW);
            if ((old_prbar & APU_REGION_LOCK) != 0)
                return;
            if (register_offset == APU_PRBAR_LOW)
                value |= old_prbar & APU_REGION_LOCK;
        }
        store32(offset, value);
        const bool policy_is_enabled =
            (load32(protected_apu_base() + APU_CTLR) & APU_CTLR_ENABLE) != 0;
        if (policy_register && (policy_was_enabled || policy_is_enabled))
            schedule_protected_dmi_invalidation();
    }

    uint8_t region_permission(uint32_t region_base,
                              const RequestContext& context) const
    {
        const uint32_t valid = load32(region_base + APU_PRLAR_LOW) &
                               APU_ID_VALID_MASK;
        const uint32_t id_words[2] = {
            load32(region_base + APU_PRID_LOW),
            load32(region_base + APU_PRID_HIGH),
        };

        if (valid == 0)
            return static_cast<uint8_t>((id_words[0] >> 8) & 0xffu);
        if (!context.requester_valid || context.requester_id > 0xffu)
            return 0;

        for (uint32_t index = 0; index < 4; ++index) {
            if ((valid & (1u << index)) == 0)
                continue;
            const uint32_t word = id_words[index / 2];
            const uint32_t shift = (index % 2) * 16;
            const uint8_t entity = static_cast<uint8_t>((word >> shift) & 0xffu);
            if (entity == context.requester_id)
                return static_cast<uint8_t>((word >> (shift + 8)) & 0xffu);
        }

        return 0;
    }

    bool region_matches(uint32_t region_base, uint64_t address,
                        unsigned int length, bool background) const
    {
        const uint32_t prbar_low = load32(region_base + APU_PRBAR_LOW);
        if ((prbar_low & APU_REGION_ENABLE) == 0 ||
            ((prbar_low & APU_REGION_BACKGROUND) != 0) != background)
            return false;

        const uint64_t base = combine_address(
            load32(region_base + APU_PRBAR_HIGH), prbar_low);
        const uint64_t end = combine_address(
            load32(region_base + APU_PRLAR_HIGH),
            load32(region_base + APU_PRLAR_LOW)) | 0x3fu;
        const uint64_t access_length = length == 0 ? 1 : length;
        return end >= base && address >= base && address <= end &&
               access_length - 1 <= end - address;
    }

    bool region_contains_dmi(uint32_t region_base,
                             const tlm::tlm_dmi& dmi,
                             bool background) const
    {
        const uint32_t prbar_low = load32(region_base + APU_PRBAR_LOW);
        if ((prbar_low & APU_REGION_ENABLE) == 0 ||
            ((prbar_low & APU_REGION_BACKGROUND) != 0) != background)
            return false;

        const uint64_t base = combine_address(
            load32(region_base + APU_PRBAR_HIGH), prbar_low);
        const uint64_t end = combine_address(
            load32(region_base + APU_PRLAR_HIGH),
            load32(region_base + APU_PRLAR_LOW)) | 0x3fu;
        return dmi.get_end_address() >= dmi.get_start_address() &&
               dmi.get_start_address() >= base &&
               dmi.get_end_address() <= end;
    }

    bool region_overlaps_dmi(uint32_t region_base,
                             const tlm::tlm_dmi& dmi,
                             bool background) const
    {
        const uint32_t prbar_low = load32(region_base + APU_PRBAR_LOW);
        if ((prbar_low & APU_REGION_ENABLE) == 0 ||
            ((prbar_low & APU_REGION_BACKGROUND) != 0) != background)
            return false;

        const uint64_t base = combine_address(
            load32(region_base + APU_PRBAR_HIGH), prbar_low);
        const uint64_t end = combine_address(
            load32(region_base + APU_PRLAR_HIGH),
            load32(region_base + APU_PRLAR_LOW)) | 0x3fu;
        return dmi.get_end_address() >= base &&
               dmi.get_start_address() <= end;
    }

    bool permission_allows_dmi(uint32_t region_base,
                               const RequestContext& context,
                               const tlm::tlm_dmi& dmi) const
    {
        const uint8_t permission = region_permission(region_base, context);
        const uint8_t read_permission =
            context.secure ? APU_S_READ : APU_NS_READ;
        const uint8_t write_permission =
            context.secure ? APU_S_WRITE : APU_NS_WRITE;
        return (!dmi.is_read_allowed() ||
                (permission & read_permission) != 0) &&
               (!dmi.is_write_allowed() ||
                (permission & write_permission) != 0);
    }

    bool programmed_dmi_allowed(const RequestContext& context,
                                const tlm::tlm_dmi& dmi) const
    {
        if (!context.secure_valid)
            return false;

        const uint32_t apu_base = protected_apu_base();
        for (unsigned int background = 0; background < 2; ++background) {
            for (uint32_t region = 0; region < APU_REGION_COUNT; ++region) {
                const uint32_t region_base =
                    apu_base + region * APU_REGION_STRIDE;
                if (!region_overlaps_dmi(region_base, dmi,
                                         background != 0))
                    continue;
                return region_contains_dmi(region_base, dmi,
                                           background != 0) &&
                       permission_allows_dmi(region_base, context, dmi);
            }
        }
        return false;
    }

    bool reset_access_allowed(const RequestContext& context) const
    {
        if (context.domain_valid &&
            context.domain_id == p_reset_owner_domain_id.get_value())
            return true;

        const uint32_t trusted = REQUEST_CONTEXT_CAP_BOOT_LOADER |
                                 REQUEST_CONTEXT_CAP_AUTHENTICATED_IMAGE;
        return p_allow_trusted_loader.get_value() &&
               (context.capabilities & trusted) != 0;
    }

    bool protected_access_allowed(tlm::tlm_generic_payload& trans) const
    {
        RequestContextTlmExtension* extension = nullptr;
        trans.get_extension(extension);
        if (extension == nullptr)
            return false;

        const RequestContext& context = extension->get_context();
        const uint32_t apu_base = protected_apu_base();
        if ((load32(apu_base + APU_CTLR) & APU_CTLR_ENABLE) == 0)
            return reset_access_allowed(context);
        if (!context.secure_valid)
            return false;

        uint32_t selected = REG_BYTES;
        for (unsigned int background = 0; background < 2; ++background) {
            for (uint32_t region = 0; region < APU_REGION_COUNT; ++region) {
                const uint32_t region_base =
                    apu_base + region * APU_REGION_STRIDE;
                if (region_matches(region_base, trans.get_address(),
                                   trans.get_data_length(), background != 0)) {
                    selected = region_base;
                    break;
                }
            }
            if (selected != REG_BYTES)
                break;
        }
        if (selected == REG_BYTES)
            return false;

        const uint8_t permission = region_permission(selected, context);
        if (trans.get_command() == tlm::TLM_READ_COMMAND)
            return (permission & (context.secure ? APU_S_READ : APU_NS_READ)) != 0;
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
            return (permission & (context.secure ? APU_S_WRITE : APU_NS_WRITE)) != 0;
        return false;
    }

    void record_denied(tlm::tlm_generic_payload& trans)
    {
        ++m_denied_count;
        m_last_denied_address = trans.get_address();

        RequestContextTlmExtension* extension = nullptr;
        trans.get_extension(extension);
        if (extension == nullptr)
            return;
        const RequestContext& context = extension->get_context();
        m_last_denied_origin = context.origin_id;
        m_last_denied_domain = context.domain_id;
        m_last_denied_requester = context.requester_id;
    }

    void trace_denied(tlm::tlm_generic_payload& trans)
    {
        if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value())
            return;

        ++m_trace_count;
        std::cerr << name() << " protected_deny "
                  << (trans.get_command() == tlm::TLM_READ_COMMAND ? "read" :
                                                                      "write")
                  << " address=0x" << std::hex << trans.get_address()
                  << " len=0x" << trans.get_data_length();

        RequestContextTlmExtension* extension = nullptr;
        trans.get_extension(extension);
        if (extension != nullptr) {
            const RequestContext& context = extension->get_context();
            std::cerr << " origin=0x" << context.origin_id
                      << " domain=0x" << context.domain_id
                      << " requester=0x" << context.requester_id
                      << " secure=" << context.secure
                      << " valid=" << context.domain_valid << "/"
                      << context.requester_valid << "/"
                      << context.secure_valid;
        } else {
            std::cerr << " context=missing";
        }
        std::cerr << std::dec << std::endl;
    }

    void fail_protected_access(tlm::tlm_generic_payload& trans)
    {
        record_denied(trans);
        trace_denied(trans);
        trans.set_dmi_allowed(false);
        const uint32_t control = load32(protected_apu_base() + APU_CTLR);
        trans.set_response_status((control & APU_CTLR_SYNC_ERROR) != 0 ?
                                      tlm::TLM_GENERIC_ERROR_RESPONSE :
                                      tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (data == nullptr || !is_supported_length(len) ||
            offset + len > m_regs.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &m_regs[offset], len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            if (len == sizeof(uint32_t) && (offset % sizeof(uint32_t)) == 0) {
                uint32_t value = 0;
                std::memcpy(&value, data, sizeof(value));
                write32(static_cast<uint32_t>(offset), value);
            } else {
                std::memcpy(&m_regs[offset], data, len);
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trace_access(trans, offset, len, debug);

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    void trace_access(tlm::tlm_generic_payload& trans, uint64_t offset,
                      unsigned int len, bool debug)
    {
        if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value())
            return;

        ++m_trace_count;
        uint32_t value = 0;
        if (len <= sizeof(value))
            std::memcpy(&value, trans.get_data_ptr(), len);

        std::cerr << name() << " "
                  << (debug ? "dbg_" : "")
                  << (trans.get_command() == tlm::TLM_READ_COMMAND ? "read" :
                                                                      "write")
                  << " offset=0x" << std::hex << offset
                  << " len=0x" << len
                  << " value=0x" << value
                  << std::dec << std::endl;
    }

public:
    SC_HAS_PROCESS(host_ni710ae_nci);

    cci::cci_param<uint32_t> p_topology;
    cci::cci_param<uint32_t> p_apu_iidr;
    cci::cci_param<uint32_t> p_protected_apu_index;
    cci::cci_param<uint32_t> p_reset_owner_domain_id;
    cci::cci_param<bool> p_allow_trusted_loader;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    tlm_utils::simple_target_socket<host_ni710ae_nci, DEFAULT_TLM_BUSWIDTH>
        target_socket;
    tlm_utils::simple_target_socket_b<
        host_ni710ae_nci, DEFAULT_TLM_BUSWIDTH,
        tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND>
        protected_target_socket;
    tlm_utils::simple_initiator_socket_b<
        host_ni710ae_nci, DEFAULT_TLM_BUSWIDTH, tlm::tlm_base_protocol_types,
        sc_core::SC_ZERO_OR_MORE_BOUND> initiator_socket;

    explicit host_ni710ae_nci(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_topology("topology", TOPOLOGY_PRIMARY_MID)
        , p_apu_iidr("apu_iidr", 0)
        , p_protected_apu_index("protected_apu_index", 0)
        , p_reset_owner_domain_id("reset_owner_domain_id", 3)
        , p_allow_trusted_loader("allow_trusted_loader", true)
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 64)
        , target_socket("target_socket")
        , protected_target_socket("protected_target_socket")
        , initiator_socket("initiator_socket")
    {
        reset_registers();
        target_socket.register_b_transport(this,
                                           &host_ni710ae_nci::b_transport);
        target_socket.register_transport_dbg(this,
                                             &host_ni710ae_nci::transport_dbg);
        protected_target_socket.register_b_transport(
            this, &host_ni710ae_nci::protected_b_transport);
        protected_target_socket.register_transport_dbg(
            this, &host_ni710ae_nci::protected_transport_dbg);
        protected_target_socket.register_get_direct_mem_ptr(
            this, &host_ni710ae_nci::protected_get_direct_mem_ptr);
        initiator_socket.register_invalidate_direct_mem_ptr(
            this, &host_ni710ae_nci::invalidate_direct_mem_ptr);
        SC_METHOD(invalidate_protected_dmi);
        sensitive << m_dmi_invalidation_event;
        dont_initialize();
    }

    void before_end_of_elaboration() override
    {
        reset_registers();
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        trans.set_dmi_allowed(false);
        access(trans, false);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, true) ? trans.get_data_length() : 0;
    }

    void protected_b_transport(tlm::tlm_generic_payload& trans,
                               sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        if (!protected_access_allowed(trans)) {
            fail_protected_access(trans);
            return;
        }
        if (initiator_socket.size() == 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        initiator_socket->b_transport(trans, delay);
    }

    unsigned int protected_transport_dbg(tlm::tlm_generic_payload& trans)
    {
        trans.set_dmi_allowed(false);
        if (!protected_access_allowed(trans)) {
            fail_protected_access(trans);
            return 0;
        }
        if (initiator_socket.size() == 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }
        return initiator_socket->transport_dbg(trans);
    }

    bool protected_get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                      tlm::tlm_dmi& dmi)
    {
        RequestContextTlmExtension* extension = nullptr;
        trans.get_extension(extension);
        if (extension == nullptr || initiator_socket.size() == 0) {
            trans.set_dmi_allowed(false);
            return false;
        }

        const RequestContext& context = extension->get_context();
        const bool reset_policy =
            (load32(protected_apu_base() + APU_CTLR) & APU_CTLR_ENABLE) == 0;
        if (reset_policy && !reset_access_allowed(context)) {
            trans.set_dmi_allowed(false);
            return false;
        }

        const bool downstream_dmi =
            initiator_socket->get_direct_mem_ptr(trans, dmi);
        const bool policy_allowed = downstream_dmi &&
            (reset_policy || programmed_dmi_allowed(context, dmi));
        if (downstream_dmi && p_trace.get_value() &&
            m_trace_count < p_trace_limit.get_value()) {
            ++m_trace_count;
            std::cerr << name() << " protected_dmi address=0x" << std::hex
                      << trans.get_address() << " downstream="
                      << downstream_dmi << " allowed=" << policy_allowed
                      << " range=0x"
                      << dmi.get_start_address() << "-0x"
                      << dmi.get_end_address() << std::dec << std::endl;
        }
        trans.set_dmi_allowed(policy_allowed);
        return policy_allowed;
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end)
    {
        if (protected_target_socket.size() != 0)
            protected_target_socket->invalidate_direct_mem_ptr(start, end);
    }

    uint64_t denied_count() const { return m_denied_count; }
    uint64_t last_denied_address() const { return m_last_denied_address; }
    uint64_t last_denied_origin() const { return m_last_denied_origin; }
    uint32_t last_denied_domain() const { return m_last_denied_domain; }
    uint32_t last_denied_requester() const { return m_last_denied_requester; }
};

extern "C" void module_register();
