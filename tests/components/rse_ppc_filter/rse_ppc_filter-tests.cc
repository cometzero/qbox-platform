/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <libqemu/libqemu.h>
#include <libqemu-cxx/libqemu-cxx.h>
#include <rse_ppc_filter.h>
#include <systemc>
#include <tlm>
#include <tlm-extensions/request-context.h>
#include <tlm_utils/simple_target_socket.h>

namespace {

constexpr uint64_t PERIPHNSPPC0 = 0x070;
constexpr uint64_t PERIPHSPPPC0 = 0x0b0;
constexpr uint64_t PERIPHNSPPPC0 = 0x0b0;
constexpr uint64_t PERIPHNSPPCEXP0 = 0x080;
constexpr uint64_t PERIPHNSPPPCEXP0 = 0x0c0;
constexpr uint32_t TIMER0_MASK = 1u;

class downstream : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<downstream> target_socket;
    unsigned int accesses = 0;

    explicit downstream(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , target_socket("target_socket")
    {
        target_socket.register_b_transport(
            this, &downstream::b_transport);
        target_socket.register_transport_dbg(
            this, &downstream::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        (void)delay;
        ++accesses;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        ++accesses;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }
};

void write_policy(rse_protection_ctrl& ctrl, uint64_t offset, uint32_t value)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    ctrl.b_transport(trans, delay);
    ASSERT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
}

tlm::tlm_response_status access(
    rse_ppc_filter& filter, bool secure, bool privileged,
    bool attach_context = true)
{
    uint32_t value = 0;
    tlm::tlm_generic_payload trans;
    trans.set_address(0);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));

    RequestContext context;
    context.secure = secure;
    context.secure_valid = true;
    context.privileged = privileged;
    context.privileged_valid = true;
    RequestContextTlmExtension extension(context);
    if (attach_context) {
        trans.set_extension(&extension);
    }

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    filter.b_transport(trans, delay);
    if (attach_context) {
        trans.clear_extension(&extension);
    }
    return trans.get_response_status();
}

}

TEST(RsePpcFilterTest, SecurePrivilegedAccessIsAllowedAtReset)
{
    rse_protection_ctrl sacfg("secure_allow_sacfg");
    rse_protection_ctrl nsacfg("secure_allow_nsacfg");
    rse_ppc_filter filter("secure_allow_filter", &sacfg, &nsacfg);
    downstream sink("secure_allow_sink");
    filter.initiator_socket.bind(sink.target_socket);

    EXPECT_EQ(access(filter, true, true), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(sink.accesses, 1u);
}

TEST(RsePpcFilterTest, MissingContextIsDenied)
{
    rse_protection_ctrl sacfg("missing_context_sacfg");
    rse_protection_ctrl nsacfg("missing_context_nsacfg");
    rse_ppc_filter filter("missing_context_filter", &sacfg, &nsacfg);
    downstream sink("missing_context_sink");
    filter.initiator_socket.bind(sink.target_socket);

    EXPECT_EQ(access(filter, true, true, false),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(sink.accesses, 0u);
}

TEST(RsePpcFilterTest, SecureUnprivilegedAccessFollowsSecurePolicy)
{
    rse_protection_ctrl sacfg("secure_user_sacfg");
    rse_protection_ctrl nsacfg("secure_user_nsacfg");
    rse_ppc_filter filter("secure_user_filter", &sacfg, &nsacfg);
    downstream sink("secure_user_sink");
    filter.initiator_socket.bind(sink.target_socket);

    EXPECT_EQ(access(filter, true, false),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    write_policy(sacfg, PERIPHSPPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, true, false), tlm::TLM_OK_RESPONSE);
}

TEST(RsePpcFilterTest, NonSecurePrivilegedAccessRequiresSecurityPolicy)
{
    rse_protection_ctrl sacfg("nonsecure_priv_sacfg");
    rse_protection_ctrl nsacfg("nonsecure_priv_nsacfg");
    rse_ppc_filter filter("nonsecure_priv_filter", &sacfg, &nsacfg);
    downstream sink("nonsecure_priv_sink");
    filter.initiator_socket.bind(sink.target_socket);

    EXPECT_EQ(access(filter, false, true),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    write_policy(sacfg, PERIPHNSPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, false, true), tlm::TLM_OK_RESPONSE);
}

TEST(RsePpcFilterTest, NonSecureUnprivilegedAccessRequiresBothPolicies)
{
    rse_protection_ctrl sacfg("nonsecure_user_sacfg");
    rse_protection_ctrl nsacfg("nonsecure_user_nsacfg");
    rse_ppc_filter filter("nonsecure_user_filter", &sacfg, &nsacfg);
    downstream sink("nonsecure_user_sink");
    filter.initiator_socket.bind(sink.target_socket);

    write_policy(sacfg, PERIPHNSPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, false, false),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    write_policy(nsacfg, PERIPHNSPPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, false, false), tlm::TLM_OK_RESPONSE);
}

TEST(RsePpcFilterTest, PpcExp0OffsetSelectsExpansionPolicyRegisters)
{
    rse_protection_ctrl sacfg("exp0_sacfg");
    rse_protection_ctrl nsacfg("exp0_nsacfg");
    rse_ppc_filter filter("exp0_filter", &sacfg, &nsacfg);
    downstream sink("exp0_sink");
    filter.initiator_socket.bind(sink.target_socket);

    write_policy(sacfg, PERIPHNSPPC0, TIMER0_MASK);
    write_policy(nsacfg, PERIPHNSPPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, false, false),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);

    write_policy(sacfg, PERIPHNSPPCEXP0, TIMER0_MASK);
    EXPECT_EQ(access(filter, false, false),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    write_policy(nsacfg, PERIPHNSPPPCEXP0, TIMER0_MASK);
    EXPECT_EQ(access(filter, false, false), tlm::TLM_OK_RESPONSE);
}

TEST(RsePpcFilterTest, QemuUserAttributeMapsToUnprivilegedContext)
{
    ::MemTxAttrs raw_attrs {};
    raw_attrs.secure = true;
    raw_attrs.user = true;
    qemu::MemoryRegionOps::MemTxAttrs attrs(raw_attrs);

    const RequestContext context = normalize_qemu_request_context(
        RequestContext {}, attrs.secure, attrs.user,
        RequestAccessPath::REGULAR);

    EXPECT_TRUE(context.secure);
    EXPECT_TRUE(context.secure_valid);
    EXPECT_FALSE(context.privileged);
    EXPECT_TRUE(context.privileged_valid);
}

TEST(RsePpcFilterTest, QemuKernelAttributeMapsToPrivilegedContext)
{
    ::MemTxAttrs raw_attrs {};
    raw_attrs.user = false;
    qemu::MemoryRegionOps::MemTxAttrs attrs(raw_attrs);

    const RequestContext context = normalize_qemu_request_context(
        RequestContext {}, attrs.secure, attrs.user,
        RequestAccessPath::REGULAR);

    EXPECT_TRUE(context.privileged);
    EXPECT_TRUE(context.privileged_valid);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    auto global_broker =
        cci::cci_get_global_broker(cci::cci_originator("rse_ppc_filter_test"));
    global_broker.set_preset_cci_value(
        "secure_allow_filter.policy_mask", cci::cci_value(TIMER0_MASK));
    global_broker.set_preset_cci_value(
        "missing_context_filter.policy_mask", cci::cci_value(TIMER0_MASK));
    global_broker.set_preset_cci_value(
        "secure_user_filter.policy_mask", cci::cci_value(TIMER0_MASK));
    global_broker.set_preset_cci_value(
        "nonsecure_priv_filter.policy_mask", cci::cci_value(TIMER0_MASK));
    global_broker.set_preset_cci_value(
        "nonsecure_user_filter.policy_mask", cci::cci_value(TIMER0_MASK));
    global_broker.set_preset_cci_value(
        "exp0_filter.policy_mask", cci::cci_value(TIMER0_MASK));
    global_broker.set_preset_cci_value(
        "exp0_filter.ppc_register_offset", cci::cci_value(0x10u));

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
