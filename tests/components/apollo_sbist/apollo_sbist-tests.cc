/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <cstring>
#include <vector>

#include <apollo_sbist.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace {

struct ObservedWrite {
    uint64_t address;
    uint32_t value;
};

class FmuTarget : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<FmuTarget, DEFAULT_TLM_BUSWIDTH> socket;
    std::vector<ObservedWrite> writes;

    explicit FmuTarget(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &FmuTarget::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        uint32_t value = 0;
        std::memcpy(&value, trans.get_data_ptr(), sizeof(value));
        writes.push_back({trans.get_address(), value});
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

template <typename T>
T access(apollo_sbist& dut, uint64_t offset, tlm::tlm_command command,
         T value = 0)
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

}

TEST(ApolloSbistTest, StoresFctlrAndInjectsCpuSpecificSi0FmuRecords)
{
    apollo_sbist dut("sbist");
    FmuTarget fmu("fmu");
    dut.fmu_initiator.bind(fmu.socket);

    (void)access<uint64_t>(dut, 0x10000, tlm::TLM_WRITE_COMMAND, 2);
    EXPECT_EQ(access<uint64_t>(dut, 0x10000, tlm::TLM_READ_COMMAND), 2u);
    EXPECT_TRUE(fmu.writes.empty());

    (void)access<uint64_t>(dut, 0x00000, tlm::TLM_WRITE_COMMAND, 4);
    (void)access<uint64_t>(dut, 0x30000, tlm::TLM_WRITE_COMMAND, 4);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));

    ASSERT_EQ(fmu.writes.size(), 4u);
    EXPECT_EQ(fmu.writes[0].address, 0x2a528bfcu);
    EXPECT_EQ(fmu.writes[0].value, 0xbeu);
    EXPECT_EQ(fmu.writes[1].address, 0x2a528690u);
    EXPECT_EQ(fmu.writes[1].value, 1u << 9);
    EXPECT_EQ(fmu.writes[2].address, 0x2a528bfcu);
    EXPECT_EQ(fmu.writes[3].address, 0x2a5286a8u);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
