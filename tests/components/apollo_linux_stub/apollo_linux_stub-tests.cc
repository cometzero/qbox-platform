/* SPDX-License-Identifier: BSD-3-Clause */
#include <apollo_linux_stub.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>

TEST(ApolloLinuxStub, SmcccUnsupportedAndDebugContracts)
{
    apollo_linux_stub dut("linux_stub");
    uint64_t data = 0;
    tlm::tlm_generic_payload trans;
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    sc_core::sc_time delay(7, sc_core::SC_NS);
    auto transfer = [&](uint64_t offset, bool write, uint64_t value = 0) {
        trans.set_address(offset);
        trans.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        data = value;
        dut.b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        EXPECT_FALSE(trans.is_dmi_allowed());
        EXPECT_EQ(delay, sc_core::sc_time(7, sc_core::SC_NS));
        return data;
    };
    transfer(0, true, 0x80000000);
    transfer(0x20, true, 1);
    EXPECT_EQ(transfer(0x28, false), 0x10001u);
    transfer(0, true, 0xc2001234);
    transfer(0x20, true, 1);
    EXPECT_EQ(transfer(0x28, false), UINT64_MAX);
    EXPECT_EQ(transfer(0x30, false), 2u);
    EXPECT_EQ(transfer(0x38, false), 0xc2001234u);
    trans.set_address(0x20);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    data = 1;
    EXPECT_EQ(dut.transport_dbg(trans), 8u);
    EXPECT_EQ(transfer(0x30, false), 2u);
    // Interleave two CPUs before executing either bank.
    transfer(0, true, 0x80000000);
    transfer(0x40, true, 0xc2001234);
    transfer(0x20, true, 1);
    transfer(0x60, true, 1);
    EXPECT_EQ(transfer(0x28, false), 0x10001u);
    EXPECT_EQ(transfer(0x68, false), UINT64_MAX);
    EXPECT_EQ(transfer(0x70, false), 1u);
    trans.set_address(0x400);
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    trans.set_address(0);
    trans.set_data_length(4);
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_BURST_ERROR_RESPONSE);
    trans.set_data_length(8);
    unsigned char enable = 0xff;
    trans.set_byte_enable_ptr(&enable);
    trans.set_byte_enable_length(1);
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
