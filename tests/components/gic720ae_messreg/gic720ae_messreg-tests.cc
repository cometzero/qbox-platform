/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>

#include <cci/utils/broker.h>
#include <gic720ae_messreg.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>

namespace {

template <typename T>
T access(gic720ae_messreg& dut, uint64_t offset, tlm::tlm_command command,
         T value, tlm::tlm_response_status expected)
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

    EXPECT_EQ(trans.get_response_status(), expected);
    return data;
}

template <typename T>
T read(gic720ae_messreg& dut, uint64_t offset)
{
    return access<T>(dut, offset, tlm::TLM_READ_COMMAND, 0,
                     tlm::TLM_OK_RESPONSE);
}

template <typename T>
void write(gic720ae_messreg& dut, uint64_t offset, T value)
{
    (void)access<T>(dut, offset, tlm::TLM_WRITE_COMMAND, value,
                    tlm::TLM_OK_RESPONSE);
}

}

TEST(Gic720aeMessregTest, ResetsToZeroAndStoresValues)
{
    gic720ae_messreg dut("messreg_storage");

    EXPECT_EQ(read<uint32_t>(dut, 0), 0u);

    write<uint32_t>(dut, 0x10, 0xa5a55a5au);
    write<uint64_t>(dut, 0x20, 0x1122334455667788ull);
    write<uint8_t>(dut, 0x27, 0xeeu);

    EXPECT_EQ(read<uint32_t>(dut, 0x10), 0xa5a55a5au);
    EXPECT_EQ(read<uint64_t>(dut, 0x20), 0xee22334455667788ull);
}

TEST(Gic720aeMessregTest, RejectsUnsupportedSizeAndOutOfRangeAccess)
{
    gic720ae_messreg dut("messreg_bounds");

    (void)access<uint16_t>(dut, 0x10000, tlm::TLM_READ_COMMAND, 0,
                           tlm::TLM_ADDRESS_ERROR_RESPONSE);

    tlm::tlm_generic_payload trans;
    uint8_t data[3] = {};
    trans.set_address(0);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(data);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
}

TEST(Gic720aeMessregTest, RejectsUnsupportedCommand)
{
    gic720ae_messreg dut("messreg_command");
    tlm::tlm_generic_payload trans;
    uint32_t data = 0;

    trans.set_address(0);
    trans.set_command(tlm::TLM_IGNORE_COMMAND);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_COMMAND_ERROR_RESPONSE);
}

TEST(Gic720aeMessregTest, DebugTransportPreservesStorage)
{
    gic720ae_messreg dut("messreg_debug");
    tlm::tlm_generic_payload trans;
    uint32_t data = 0x12345678u;

    trans.set_address(0x30);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    EXPECT_EQ(dut.transport_dbg(trans), sizeof(data));

    data = 0;
    trans.set_command(tlm::TLM_READ_COMMAND);
    EXPECT_EQ(dut.transport_dbg(trans), sizeof(data));
    EXPECT_EQ(data, 0x12345678u);

    trans.set_address(0x10000);
    EXPECT_EQ(dut.transport_dbg(trans), 0u);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
