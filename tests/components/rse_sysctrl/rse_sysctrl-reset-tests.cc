/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <cstring>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <rse_sysctrl.h>

namespace {

constexpr uint64_t RESET_SYNDROME = 0x100;
constexpr uint64_t RESET_MASK = 0x104;
constexpr uint64_t SWRESET = 0x108;
constexpr uint32_t SWRESETREQ = 1u << 5;

class ResetSink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> reset;
    bool saw_asserted = false;
    bool saw_deasserted = false;

    explicit ResetSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , reset("reset")
    {
        reset.register_value_changed_cb([this](bool asserted) {
            saw_asserted |= asserted;
            saw_deasserted |= !asserted;
        });
    }
};

class BusInitiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<BusInitiator> socket;

    explicit BusInitiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }
};

uint32_t access32(rse_sysctrl& dut, uint64_t offset, tlm::tlm_command command,
                  uint32_t value = 0)
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

uint32_t read32(rse_sysctrl& dut, uint64_t offset)
{
    return access32(dut, offset, tlm::TLM_READ_COMMAND);
}

void write32(rse_sysctrl& dut, uint64_t offset, uint32_t value)
{
    (void)access32(dut, offset, tlm::TLM_WRITE_COMMAND, value);
}

} // namespace

TEST(RseSysctrlResetTest, SoftwareResetRestartsSystemAndLatchesSyndrome)
{
    rse_sysctrl dut("rse_sysctrl");
    ResetSink sink("reset_sink");
    BusInitiator initiator("initiator");

    initiator.socket.bind(dut.target_socket);
    dut.system_reset.bind(dut.reset);
    dut.system_reset.bind(sink.reset);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    write32(dut, RESET_MASK, 0xffffffffu);
    write32(dut, SWRESET, 0xab000000u | SWRESETREQ);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

    EXPECT_TRUE(sink.saw_asserted);
    EXPECT_TRUE(sink.saw_deasserted);
    EXPECT_EQ(read32(dut, SWRESET), 0x00000000u);
    EXPECT_EQ(read32(dut, RESET_MASK), 0x00000000u);
    EXPECT_EQ(read32(dut, RESET_SYNDROME), 0xab000000u | SWRESETREQ);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
