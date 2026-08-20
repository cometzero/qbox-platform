/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <zena_device_fmu.h>
#include <zena_fmu.h>
#include <zena_ni710ae_fmu.h>

namespace {

constexpr uint32_t STATUS_V = 1u << 30;
constexpr uint32_t STATUS_UE = 1u << 29;
constexpr uint32_t STATUS_CI = 1u << 19;
constexpr uint64_t BANK_BYTES = 0x10000;
constexpr uint64_t RECORD_STRIDE = 0x40;
constexpr uint64_t ERR_STATUS = 0x10;

class TlmInitiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<TlmInitiator, DEFAULT_TLM_BUSWIDTH>
        socket;

    explicit TlmInitiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    uint32_t access32(uint64_t address, tlm::tlm_command command,
                      uint32_t value = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_address(address);
        trans.set_command(command);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        return value;
    }

    uint64_t access64(uint64_t address, tlm::tlm_command command,
                      uint64_t value = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_address(address);
        trans.set_command(command);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        return value;
    }
};

uint32_t parent_read32(zena_fmu& parent, uint64_t address)
{
    uint32_t value = 0;
    tlm::tlm_generic_payload trans;
    trans.set_address(address);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    parent.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

}

TEST(ZenaLeafFmuTest, DeviceFmuReportsCriticalAndNonCriticalParentRecords)
{
    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("zena_device_fmu_test"));
    broker.set_preset_cci_value(
        "device_leaf.parent_base", cci::cci_value(uint64_t(0)));
    broker.set_preset_cci_value(
        "device_leaf.parent_critical_record", cci::cci_value(204u));
    broker.set_preset_cci_value(
        "device_leaf.parent_non_critical_record", cci::cci_value(203u));

    zena_fmu parent("device_parent");
    zena_device_fmu leaf("device_leaf");
    TlmInitiator initiator("device_initiator");
    initiator.socket.bind(leaf.target_socket);
    leaf.fault_socket.bind(parent.target_socket);
    parent.before_end_of_elaboration();

    const uint32_t protection = (1u << 28) | (2u << 8);
    initiator.access32(0xf00, tlm::TLM_WRITE_COMMAND, protection | 1u);
    initiator.access32(0xf08, tlm::TLM_WRITE_COMMAND, protection | 1u);
    initiator.access32(0xf04, tlm::TLM_WRITE_COMMAND, protection);

    EXPECT_EQ(initiator.access64(0xe00, tlm::TLM_READ_COMMAND), 1ull << 2);
    EXPECT_EQ(parent_read32(
                  parent, 4 * BANK_BYTES + 204 * RECORD_STRIDE + ERR_STATUS) &
                  (STATUS_V | STATUS_CI),
              STATUS_V | STATUS_CI);

    initiator.access64(0x10 + 2 * 0x40, tlm::TLM_WRITE_COMMAND, 0);
    initiator.access32(0xf08, tlm::TLM_WRITE_COMMAND, protection);
    initiator.access32(0xf04, tlm::TLM_WRITE_COMMAND, protection);
    EXPECT_EQ(initiator.access64(0xe00, tlm::TLM_READ_COMMAND), 1ull << 3);
    EXPECT_EQ(parent_read32(
                  parent, 4 * BANK_BYTES + 203 * RECORD_STRIDE + ERR_STATUS) &
                  (STATUS_V | STATUS_UE),
              STATUS_V | STATUS_UE);
}

TEST(ZenaLeafFmuTest, Ni710FmuPreservesSyndromeAfterAck)
{
    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("zena_ni710ae_fmu_test"));
    broker.set_preset_cci_value(
        "ni_leaf.parent_base", cci::cci_value(uint64_t(0)));
    broker.set_preset_cci_value(
        "ni_leaf.node_index", cci::cci_value(11u));
    broker.set_preset_cci_value(
        "ni_leaf.parent_critical_record", cci::cci_value(3u));

    zena_fmu parent("ni_parent");
    zena_ni710ae_fmu leaf("ni_leaf");
    TlmInitiator initiator("ni_initiator");
    initiator.socket.bind(leaf.target_socket);
    leaf.fault_socket.bind(parent.target_socket);
    parent.before_end_of_elaboration();

    EXPECT_EQ(initiator.access32(0xffc8, tlm::TLM_READ_COMMAND), 12u);
    EXPECT_EQ(initiator.access32(0x20 + 11 * 0x40,
                                 tlm::TLM_READ_COMMAND),
              0x61u);
    initiator.access32(0xe204, tlm::TLM_WRITE_COMMAND, 0x3ffffu);
    initiator.access32(0xe208, tlm::TLM_WRITE_COMMAND, 2u);

    const uint64_t status_address = 0x10 + 11 * 0x40;
    const uint32_t status =
        initiator.access32(status_address, tlm::TLM_READ_COMMAND);
    EXPECT_EQ((status >> 8) & 0x1fu, 2u);
    EXPECT_EQ(parent_read32(
                  parent, BANK_BYTES + 3 * RECORD_STRIDE + ERR_STATUS) &
                  (STATUS_V | STATUS_CI),
              STATUS_V | STATUS_CI);

    initiator.access32(status_address, tlm::TLM_WRITE_COMMAND, status);
    const uint32_t acknowledged =
        initiator.access32(status_address, tlm::TLM_READ_COMMAND);
    EXPECT_EQ(acknowledged & STATUS_V, 0u);
    EXPECT_EQ((acknowledged >> 8) & 0x1fu, 2u);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
