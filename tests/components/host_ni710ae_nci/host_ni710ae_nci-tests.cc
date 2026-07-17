/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <type_traits>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <host_ni710ae_nci.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace {

static_assert(
    std::is_same<
        decltype(host_ni710ae_nci::protected_target_socket),
        tlm_utils::simple_target_socket_b<
            host_ni710ae_nci, DEFAULT_TLM_BUSWIDTH,
            tlm::tlm_base_protocol_types,
            sc_core::SC_ZERO_OR_MORE_BOUND>>::value,
    "unused NI-710AE protected paths must allow zero bindings");

constexpr uint64_t ROOT_CHILD_INFO = 0x004;
constexpr uint64_t ROOT_FIRST_POINTER = 0x008;
constexpr uint64_t COMPONENT_BASE = 0x0100;
constexpr uint64_t COMPONENT_STRIDE = 0x0100;
constexpr uint64_t COMPONENT_NUM_SUBFEATURES = 0x024;
constexpr uint64_t COMPONENT_SUBFEATURES = 0x028;
constexpr uint64_t APU_BASE = 0x2000;
constexpr uint64_t APU_STRIDE = 0x1000;
constexpr uint64_t APU_CTLR = 0x0ff8;
constexpr uint64_t APU_IIDR = 0x0ffc;
constexpr uint64_t APU_PRBAR_LOW = 0x000;
constexpr uint64_t APU_PRBAR_HIGH = 0x004;
constexpr uint64_t APU_PRLAR_LOW = 0x008;
constexpr uint64_t APU_PRID_LOW = 0x010;

constexpr uint32_t NODE_ASNI = 0x04;
constexpr uint32_t NODE_AMNI = 0x05;
constexpr uint32_t NODE_APU = 0x00;

uint32_t node(uint32_t type, uint32_t id)
{
    return (id << 16) | type;
}

uint32_t access32(host_ni710ae_nci& dut, uint64_t offset,
                  tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    auto data = value;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

uint32_t read32(host_ni710ae_nci& dut, uint64_t offset)
{
    return access32(dut, offset, tlm::TLM_READ_COMMAND);
}

void write32(host_ni710ae_nci& dut, uint64_t offset, uint32_t value)
{
    (void)access32(dut, offset, tlm::TLM_WRITE_COMMAND, value);
}

class CountingTarget : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<CountingTarget, DEFAULT_TLM_BUSWIDTH>
        target_socket;
    unsigned int accesses = 0;
    unsigned int dmi_requests = 0;
    uint8_t dmi_storage = 0;
    uint64_t dmi_start = 0;
    uint64_t dmi_end = 0xffff;

    explicit CountingTarget(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , target_socket("target_socket")
    {
        target_socket.register_b_transport(this, &CountingTarget::b_transport);
        target_socket.register_transport_dbg(this, &CountingTarget::transport_dbg);
        target_socket.register_get_direct_mem_ptr(
            this, &CountingTarget::get_direct_mem_ptr);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++accesses;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        ++accesses;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }

    bool get_direct_mem_ptr(tlm::tlm_generic_payload&, tlm::tlm_dmi& dmi)
    {
        ++dmi_requests;
        dmi.set_start_address(dmi_start);
        dmi.set_end_address(dmi_end);
        dmi.set_dmi_ptr(&dmi_storage);
        dmi.allow_read_write();
        return true;
    }
};

RequestContext request_context(uint64_t origin, uint32_t domain,
                               uint32_t requester, bool secure)
{
    RequestContext context = make_request_context(origin, domain, requester, 0);
    context.secure = secure;
    context.secure_valid = true;
    context.access_path = RequestAccessPath::REGULAR;
    return context;
}

tlm::tlm_response_status protected_access(host_ni710ae_nci& dut,
                                          uint64_t address,
                                          tlm::tlm_command command,
                                          const RequestContext& context,
                                          bool debug = false)
{
    uint32_t data = 0;
    tlm::tlm_generic_payload trans;
    RequestContextTlmExtension extension(context);
    trans.set_address(address);
    trans.set_command(command);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_extension(&extension);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (debug)
        (void)dut.protected_transport_dbg(trans);
    else
        dut.protected_b_transport(trans, delay);

    trans.clear_extension<RequestContextTlmExtension>();
    return trans.get_response_status();
}

void program_secure_rw_region(host_ni710ae_nci& dut, uint64_t base,
                              uint64_t end, bool lock)
{
    write32(dut, APU_BASE + APU_PRBAR_HIGH,
            static_cast<uint32_t>(base >> 32));
    write32(dut, APU_BASE + APU_PRLAR_LOW,
            static_cast<uint32_t>(end) & 0xffffffc0u);
    write32(dut, APU_BASE + 0x00c,
            static_cast<uint32_t>(end >> 32));
    write32(dut, APU_BASE + APU_PRID_LOW, 0x00000a00u);
    write32(dut, APU_BASE + APU_PRBAR_LOW,
            (static_cast<uint32_t>(base) & 0xffffffc0u) | 1u |
                (lock ? 4u : 0u));
    write32(dut, APU_BASE + APU_CTLR, 0x00000005u);
}

} // namespace

TEST(HostNi710AeNciTest, MhuMidTopologyExposesConfiguredAsniApu)
{
    host_ni710ae_nci dut("host_ni710ae_mhu_mid");
    dut.p_topology = 1u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, ROOT_CHILD_INFO), 1u);
    EXPECT_EQ(read32(dut, ROOT_FIRST_POINTER), COMPONENT_BASE);
    EXPECT_EQ(read32(dut, COMPONENT_BASE), node(NODE_ASNI, 4));
    EXPECT_EQ(read32(dut, COMPONENT_BASE + COMPONENT_NUM_SUBFEATURES), 1u);
    EXPECT_EQ(read32(dut, COMPONENT_BASE + COMPONENT_SUBFEATURES), NODE_APU);
    EXPECT_EQ(read32(dut, COMPONENT_BASE + COMPONENT_SUBFEATURES + 4),
              APU_BASE);
}

TEST(HostNi710AeNciTest, SecondaryTopologyExposesRseMmAsni)
{
    host_ni710ae_nci dut("host_ni710ae_secondary");
    dut.p_topology = 2u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, ROOT_CHILD_INFO), 1u);
    EXPECT_EQ(read32(dut, COMPONENT_BASE), node(NODE_ASNI, 1));
}

TEST(HostNi710AeNciTest, PrimaryMidTopologyExposesApolloConfiguredComponents)
{
    host_ni710ae_nci dut("host_ni710ae_primary_mid");
    dut.p_topology = 4u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, ROOT_CHILD_INFO), 5u);
    EXPECT_EQ(read32(dut, COMPONENT_BASE), node(NODE_ASNI, 0));
    EXPECT_EQ(read32(dut, COMPONENT_BASE + COMPONENT_STRIDE), node(NODE_ASNI, 5));
    EXPECT_EQ(read32(dut, COMPONENT_BASE + 2 * COMPONENT_STRIDE),
              node(NODE_ASNI, 6));
    EXPECT_EQ(read32(dut, COMPONENT_BASE + 3 * COMPONENT_STRIDE),
              node(NODE_ASNI, 7));
    EXPECT_EQ(read32(dut, COMPONENT_BASE + 4 * COMPONENT_STRIDE),
              node(NODE_AMNI, 14));
}

TEST(HostNi710AeNciTest, ApuRegionAndControlWritesArePreserved)
{
    host_ni710ae_nci dut("host_ni710ae_apu_writes");
    dut.p_topology = 1u;
    dut.before_end_of_elaboration();

    write32(dut, APU_BASE + 0x000, 0x3c000001u);
    write32(dut, APU_BASE + 0x008, 0x3c0bfff1u);
    write32(dut, APU_BASE + APU_CTLR, 0x00000005u);

    EXPECT_EQ(read32(dut, APU_BASE + 0x000), 0x3c000001u);
    EXPECT_EQ(read32(dut, APU_BASE + 0x008), 0x3c0bfff1u);
    EXPECT_EQ(read32(dut, APU_BASE + APU_CTLR), 0x00000005u);
}

TEST(HostNi710AeNciTest, ApuIidrResetValueIsConfigurableAndReadOnly)
{
    host_ni710ae_nci dut("host_ni710ae_iidr");
    dut.p_topology = 1u;
    dut.p_apu_iidr = 0x12345678u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, APU_BASE + APU_IIDR), 0x12345678u);
    write32(dut, APU_BASE + APU_IIDR, 0);
    EXPECT_EQ(read32(dut, APU_BASE + APU_IIDR), 0x12345678u);
}

TEST(HostNi710AeNciTest, EachPrimaryComponentGetsDistinctApuBlock)
{
    host_ni710ae_nci dut("host_ni710ae_apu_blocks");
    dut.p_topology = 4u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, COMPONENT_BASE + COMPONENT_SUBFEATURES + 4),
              APU_BASE);
    EXPECT_EQ(read32(dut, COMPONENT_BASE + COMPONENT_STRIDE +
                         COMPONENT_SUBFEATURES + 4),
              APU_BASE + APU_STRIDE);

    write32(dut, APU_BASE + 0x000, 0x11111111u);
    write32(dut, APU_BASE + APU_STRIDE + 0x000, 0x22222222u);
    EXPECT_EQ(read32(dut, APU_BASE + 0x000), 0x11111111u);
    EXPECT_EQ(read32(dut, APU_BASE + APU_STRIDE + 0x000), 0x22222222u);
}

TEST(HostNi710AeNciTest, ResetPolicyAllowsOwnerAndDeniesApWithoutSideEffect)
{
    host_ni710ae_nci dut("host_ni710ae_reset_policy");
    CountingTarget target("host_ni710ae_reset_target");
    dut.p_topology = 1u;
    dut.p_reset_owner_domain_id = 3u;
    dut.initiator_socket.bind(target.target_socket);
    dut.before_end_of_elaboration();

    EXPECT_EQ(protected_access(dut, 0x1000, tlm::TLM_READ_COMMAND,
                               request_context(0x3000, 3, 0, true)),
              tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(target.accesses, 1u);

    EXPECT_EQ(protected_access(dut, 0x1000, tlm::TLM_WRITE_COMMAND,
                               request_context(0x1000, 1, 0, true)),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    RequestContext debug_context = request_context(0x1000, 1, 0, true);
    debug_context.access_path = RequestAccessPath::DEBUG;
    EXPECT_EQ(protected_access(dut, 0x1000, tlm::TLM_READ_COMMAND,
                               debug_context, true),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(target.accesses, 1u);
    EXPECT_EQ(dut.denied_count(), 2u);
    EXPECT_EQ(dut.last_denied_origin(), 0x1000u);
    EXPECT_EQ(dut.last_denied_domain(), 1u);

    tlm::tlm_generic_payload dmi_trans;
    tlm::tlm_dmi dmi;
    EXPECT_FALSE(dut.protected_get_direct_mem_ptr(dmi_trans, dmi));
    EXPECT_EQ(target.dmi_requests, 0u);

    const RequestContext owner = request_context(0x3000, 3, 0, true);
    RequestContextTlmExtension dmi_context(owner);
    dmi_trans.set_extension(&dmi_context);
    EXPECT_TRUE(dut.protected_get_direct_mem_ptr(dmi_trans, dmi));
    EXPECT_EQ(target.dmi_requests, 1u);
    EXPECT_TRUE(dmi_trans.is_dmi_allowed());
    dmi_trans.clear_extension<RequestContextTlmExtension>();
}

TEST(HostNi710AeNciTest, ProgrammedRegionEnforcesSecurityForNormalAndDebug)
{
    host_ni710ae_nci dut("host_ni710ae_programmed_policy");
    CountingTarget target("host_ni710ae_programmed_target");
    dut.p_topology = 1u;
    dut.initiator_socket.bind(target.target_socket);
    dut.before_end_of_elaboration();
    program_secure_rw_region(dut, 0x4000, 0x4fff, false);
    target.dmi_start = 0x4000;
    target.dmi_end = 0x4fff;

    EXPECT_EQ(protected_access(dut, 0x4040, tlm::TLM_WRITE_COMMAND,
                               request_context(0x1000, 1, 0, true)),
              tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(protected_access(dut, 0x4040, tlm::TLM_READ_COMMAND,
                               request_context(0x1000, 1, 0, true), true),
              tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(target.accesses, 2u);

    EXPECT_EQ(protected_access(dut, 0x4040, tlm::TLM_READ_COMMAND,
                               request_context(0x1000, 1, 0, false)),
              tlm::TLM_GENERIC_ERROR_RESPONSE);
    EXPECT_EQ(protected_access(dut, 0x5000, tlm::TLM_READ_COMMAND,
                               request_context(0x1000, 1, 0, true)),
              tlm::TLM_GENERIC_ERROR_RESPONSE);
    EXPECT_EQ(target.accesses, 2u);
    EXPECT_EQ(dut.denied_count(), 2u);

    tlm::tlm_generic_payload dmi_trans;
    tlm::tlm_dmi dmi;
    RequestContext secure = request_context(0x1000, 1, 0, true);
    RequestContextTlmExtension secure_extension(secure);
    dmi_trans.set_extension(&secure_extension);
    EXPECT_TRUE(dut.protected_get_direct_mem_ptr(dmi_trans, dmi));
    dmi_trans.clear_extension<RequestContextTlmExtension>();

    RequestContext non_secure = request_context(0x1000, 1, 0, false);
    RequestContextTlmExtension non_secure_extension(non_secure);
    dmi_trans.set_extension(&non_secure_extension);
    EXPECT_FALSE(dut.protected_get_direct_mem_ptr(dmi_trans, dmi));
    dmi_trans.clear_extension<RequestContextTlmExtension>();
}

TEST(HostNi710AeNciTest, LockedRegionIgnoresReprogrammingUntilReset)
{
    host_ni710ae_nci dut("host_ni710ae_lock_policy");
    dut.p_topology = 1u;
    dut.before_end_of_elaboration();
    program_secure_rw_region(dut, 0x8000, 0x8fff, true);

    const uint32_t original_base = read32(dut, APU_BASE + APU_PRBAR_LOW);
    const uint32_t original_end = read32(dut, APU_BASE + APU_PRLAR_LOW);
    write32(dut, APU_BASE + APU_PRBAR_LOW, 0x00009001u);
    write32(dut, APU_BASE + APU_PRLAR_LOW, 0x00009fc0u);

    EXPECT_EQ(read32(dut, APU_BASE + APU_PRBAR_LOW), original_base);
    EXPECT_EQ(read32(dut, APU_BASE + APU_PRLAR_LOW), original_end);
    dut.before_end_of_elaboration();
    EXPECT_EQ(read32(dut, APU_BASE + APU_PRBAR_LOW), 0u);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
