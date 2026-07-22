/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <cstring>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <rse_ppc_filter.h>
#include <rse_protection_ctrl.h>
#include <systemc>
#include <tlm>
#include <tlm-extensions/request-context.h>
#include <tlm_utils/simple_target_socket.h>

namespace {

constexpr uint64_t PERIPHNSPPC0 = 0x070;
constexpr uint64_t PERIPHSPPPC0 = 0x0b0;
constexpr uint64_t PERIPHNSPPPC0 = 0x0b0;
constexpr uint32_t TIMER0_MASK = 1u << 0;

class Backing : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<Backing, DEFAULT_TLM_BUSWIDTH> socket;
    uint32_t value = 0;
    unsigned int accesses = 0;

    explicit Backing(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &Backing::b_transport);
        socket.register_transport_dbg(this, &Backing::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++accesses;
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&value, trans.get_data_ptr(), sizeof(value));
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        b_transport(trans, delay);
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

RequestContext context(bool secure, bool privileged)
{
    RequestContext result;
    result.secure = secure;
    result.secure_valid = true;
    result.privileged = privileged;
    result.privileged_valid = true;
    result.access_path = RequestAccessPath::REGULAR;
    return result;
}

tlm::tlm_response_status access(rse_ppc_filter& filter, uint32_t& value,
                                const RequestContext* request,
                                bool debug = false)
{
    tlm::tlm_generic_payload trans;
    RequestContextTlmExtension extension;
    trans.set_address(0);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    if (request != nullptr) {
        extension.set_context(*request);
        trans.set_extension(&extension);
    }

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (debug) {
        (void)filter.transport_dbg(trans);
    } else {
        filter.b_transport(trans, delay);
    }
    if (request != nullptr) {
        trans.clear_extension<RequestContextTlmExtension>();
    }
    return trans.get_response_status();
}

class RsePpcFilterTest : public ::testing::Test
{
protected:
    rse_protection_ctrl sacfg { "sacfg" };
    rse_protection_ctrl nsacfg { "nsacfg" };
    rse_ppc_filter filter { "filter", sacfg, nsacfg };
    Backing backing { "backing" };

    RsePpcFilterTest()
    {
        filter.p_policy_mask = TIMER0_MASK;
        filter.initiator_socket.bind(backing.socket);
    }
};

TEST_F(RsePpcFilterTest, PrivilegedSecureAccessIsAlwaysAllowed)
{
    auto request = context(true, true);
    uint32_t value = 0x11223344;

    EXPECT_EQ(access(filter, value, &request), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(backing.value, value);
}

TEST_F(RsePpcFilterTest, NonSecureAccessTracksPpc0Policy)
{
    auto request = context(false, true);
    uint32_t value = 0x12345678;

    EXPECT_EQ(access(filter, value, &request),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(backing.accesses, 0u);

    write_policy(sacfg, PERIPHNSPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, value, &request), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(backing.value, value);
}

TEST_F(RsePpcFilterTest, UnprivilegedAccessTracksSecuritySpecificPolicy)
{
    auto secure = context(true, false);
    auto non_secure = context(false, false);
    uint32_t value = 0xa5a5a5a5;

    write_policy(sacfg, PERIPHNSPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, value, &secure),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access(filter, value, &non_secure),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);

    write_policy(sacfg, PERIPHSPPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, value, &secure), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(access(filter, value, &non_secure),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);

    write_policy(nsacfg, PERIPHNSPPPC0, TIMER0_MASK);
    EXPECT_EQ(access(filter, value, &non_secure), tlm::TLM_OK_RESPONSE);
}

TEST_F(RsePpcFilterTest, SecureAndNonSecureRequestsReachOneBackingState)
{
    auto secure = context(true, true);
    auto non_secure = context(false, true);
    uint32_t secure_value = 0x11112222;
    uint32_t non_secure_value = 0x33334444;
    write_policy(sacfg, PERIPHNSPPC0, TIMER0_MASK);

    EXPECT_EQ(access(filter, secure_value, &secure), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(backing.value, secure_value);
    EXPECT_EQ(access(filter, non_secure_value, &non_secure),
              tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(backing.value, non_secure_value);
    EXPECT_EQ(backing.accesses, 2u);
}

TEST_F(RsePpcFilterTest, MissingSecurityOrPrivilegeIsDenied)
{
    uint32_t value = 0x55aa55aa;
    auto missing_privilege = context(true, true);
    missing_privilege.privileged_valid = false;
    auto missing_security = context(true, true);
    missing_security.secure_valid = false;

    EXPECT_EQ(access(filter, value, nullptr),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access(filter, value, &missing_privilege),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access(filter, value, &missing_security),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(backing.accesses, 0u);
}

TEST_F(RsePpcFilterTest, DebugAccessIsFailClosed)
{
    uint32_t value = 0x55aa55aa;
    auto request = context(true, true);
    request.access_path = RequestAccessPath::DEBUG;

    EXPECT_EQ(access(filter, value, &request, true),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(backing.accesses, 0u);
}

TEST_F(RsePpcFilterTest, DirectMemoryAccessIsNeverGranted)
{
    tlm::tlm_generic_payload trans;
    tlm::tlm_dmi dmi;
    auto request = context(true, true);
    RequestContextTlmExtension extension(request);
    trans.set_address(0);
    trans.set_extension(&extension);

    EXPECT_FALSE(filter.get_direct_mem_ptr(trans, dmi));
    EXPECT_FALSE(trans.is_dmi_allowed());
    trans.clear_extension<RequestContextTlmExtension>();
}

}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
