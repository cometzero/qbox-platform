/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <vector>

#include <apollo_cpu_ras.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

constexpr uint64_t ERR_STATUS = 0x10;
constexpr uint64_t STATUS_V = 1ull << 30;
constexpr uint64_t STATUS_UE = 1ull << 29;

class TlmInitiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<TlmInitiator, DEFAULT_TLM_BUSWIDTH> socket;

    explicit TlmInitiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }
};

class FaultSource : public sc_core::sc_module
{
public:
    InitiatorSignalSocket<bool> signal;

    explicit FaultSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
    {
    }
};

class SignalSink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> signal;
    std::vector<bool> observed;

    explicit SignalSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
    {
        signal.register_value_changed_cb(
            [this](bool value) { observed.push_back(value); });
    }
};

uint64_t access64(TlmInitiator& initiator, uint64_t offset,
                  tlm::tlm_command command, uint64_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    initiator.socket->b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

}

TEST(ApolloCpuRasTest, UncontainableFaultPublishesRecordAndClusterIrq)
{
    apollo_cpu_ras dut("ras");
    TlmInitiator initiator("initiator");
    FaultSource fault("fault");
    SignalSink irq("irq");

    initiator.socket.bind(dut.record_2);
    fault.signal.bind(dut.cpu_fault_2);
    dut.cluster_irq.bind(irq.signal);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    fault.signal->write(true);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_PS));

    EXPECT_EQ(
        access64(initiator, ERR_STATUS, tlm::TLM_READ_COMMAND),
        STATUS_V | STATUS_UE);
    ASSERT_FALSE(irq.observed.empty());
    EXPECT_TRUE(irq.observed.front());

    (void)access64(
        initiator,
        ERR_STATUS,
        tlm::TLM_WRITE_COMMAND,
        STATUS_V | STATUS_UE);
    EXPECT_EQ(access64(initiator, ERR_STATUS, tlm::TLM_READ_COMMAND), 0u);
    EXPECT_FALSE(irq.observed.back());
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
