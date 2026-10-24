/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <module_factory_container.h>
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

class WideSignalPair : public sc_core::sc_module
{
public:
    InitiatorSignalSocket<uint64_t> output;
    sc_core::sc_port<sc_core::sc_signal_inout_if<uint64_t>, 0,
                     sc_core::SC_ZERO_OR_MORE_BOUND> optional_output;
    TargetSignalSocket<uint64_t> input, optional_input;
    explicit WideSignalPair(sc_core::sc_module_name name)
        : sc_core::sc_module(name), output("output"), optional_output("optional_output"),
          input("input"), optional_input("optional_input") {}
};

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

    void protected_write(uint64_t address, uint32_t value)
    {
        access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
        access32(address, tlm::TLM_WRITE_COMMAND, value);
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

class AnnotatedInitiator : public TlmInitiator
{
    const sc_core::sc_signal<bool>& m_irq;
    zena_device_fmu& m_leaf;

    uint32_t access_after(uint64_t address, tlm::tlm_command command,
                          uint32_t value, unsigned int nanoseconds,
                          unsigned int response_nanoseconds = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_address(address);
        trans.set_command(command);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        sc_core::sc_time delay(nanoseconds, sc_core::SC_NS);
        const auto arrival = sc_core::sc_time_stamp() + delay;
        socket->b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        EXPECT_EQ(delay, sc_core::SC_ZERO_TIME);
        EXPECT_EQ(sc_core::sc_time_stamp(), arrival +
                  sc_core::sc_time(response_nanoseconds, sc_core::SC_NS));
        return value;
    }

    void run()
    {
        protected_write(8, 0x2003);
        access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
        access_after(0xf04, tlm::TLM_WRITE_COMMAND, 3u << 8, 100);
        EXPECT_EQ(access_after(0xf1c, tlm::TLM_READ_COMMAND, 0, 7, 8) & 1, 1u);
        EXPECT_NE(access32(0x10, tlm::TLM_READ_COMMAND) & STATUS_V, 0u);
        EXPECT_EQ(access_after(0xf1c, tlm::TLM_READ_COMMAND, 0, 1) & 0x3f, 0u);
        EXPECT_NE(access32(0x10, tlm::TLM_READ_COMMAND) & STATUS_V, 0u);
        sc_core::wait(sc_core::SC_ZERO_TIME);
        EXPECT_TRUE(m_irq.read());
        access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
        access_after(0x10, tlm::TLM_WRITE_COMMAND, STATUS_V | STATUS_UE, 10);
        EXPECT_EQ(access_after(0xf1c, tlm::TLM_READ_COMMAND, 0, 8) & 0x3f, 0u);
        sc_core::wait(sc_core::SC_ZERO_TIME);
        EXPECT_FALSE(m_irq.read());
        protected_write(0xf04, 3u << 8);
        EXPECT_EQ(access_after(0xf1c, tlm::TLM_READ_COMMAND, 0, 0, 8) & 1, 1u);
        EXPECT_EQ(access_after(0xf1c, tlm::TLM_READ_COMMAND, 0, 0) & 0x3f, 0u);
        EXPECT_TRUE(m_leaf.runtime_set_block_response(0, 0, 4, true));
        protected_write(0xf14, 1u << 8);
        EXPECT_EQ(access_after(0xf1c, tlm::TLM_READ_COMMAND, 0, 0, 8) & 1, 1u);
        EXPECT_EQ(access_after(0xf1c, tlm::TLM_READ_COMMAND, 0, 24) & 0x3f, 0u);
        done = true;
    }

    void observe()
    {
        sc_core::wait(99, sc_core::SC_NS);
        EXPECT_EQ(access32(0xf1c, tlm::TLM_READ_COMMAND) & 1, 0u);
        EXPECT_EQ(access32(0x10, tlm::TLM_READ_COMMAND) & STATUS_V, 0u);
        EXPECT_FALSE(m_irq.read());
        sc_core::wait(2, sc_core::SC_NS);
        EXPECT_FALSE(m_irq.read());
        EXPECT_EQ(access32(0xf1c, tlm::TLM_READ_COMMAND) & 1, 1u);
        EXPECT_TRUE(m_irq.read());
    }

public:
    bool done = false;
    SC_HAS_PROCESS(AnnotatedInitiator);
    AnnotatedInitiator(sc_core::sc_module_name name, const sc_core::sc_signal<bool>& irq,
                       zena_device_fmu& leaf)
        : TlmInitiator(name), m_irq(irq), m_leaf(leaf)
    {
        SC_THREAD(run);
        SC_THREAD(observe);
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

TEST(ZenaLeafFmuTest, GicDiscoveryLockAndHighWordAccess)
{
    zena_device_fmu leaf("discovery_leaf");
    TlmInitiator initiator("discovery_initiator");
    initiator.socket.bind(leaf.target_socket);
    EXPECT_EQ(initiator.access32(0xfc8, tlm::TLM_READ_COMMAND), 12u);
    EXPECT_EQ(initiator.access32(0, tlm::TLM_READ_COMMAND), 0x800002u);
    EXPECT_EQ(initiator.access32(0x40, tlm::TLM_READ_COMMAND), 0x22u);
    EXPECT_EQ(initiator.access32(8, tlm::TLM_READ_COMMAND), 3u);
    initiator.access32(8, tlm::TLM_WRITE_COMMAND, 0);
    EXPECT_EQ(initiator.access32(8, tlm::TLM_READ_COMMAND), 3u);
    EXPECT_EQ(initiator.access32(10 * 0x40 + 0x10, tlm::TLM_READ_COMMAND) >> 8 & 0xff, 8u);
    initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
    initiator.access32(0x0c, tlm::TLM_WRITE_COMMAND, ~0u);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0xbeu);
    initiator.access32(8, tlm::TLM_WRITE_COMMAND, ~0u);
    EXPECT_EQ(initiator.access32(8, tlm::TLM_READ_COMMAND), 0x2003u);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
    EXPECT_EQ(initiator.access32(0x0c, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, GicOverflowClearPreservesFirstSyndromeAndResends)
{
    zena_device_fmu leaf("overflow_leaf");
    TlmInitiator initiator("overflow_initiator");
    initiator.socket.bind(leaf.target_socket);
    const uint32_t protection = (2u << 28) | (9u << 8);
    const uint64_t status = 4 * 0x40 + 0x10;
    initiator.protected_write(0xf04, protection);
    initiator.protected_write(0xf04, protection);
    initiator.protected_write(0xf04, (2u << 28) | (10u << 8));
    uint64_t value = initiator.access64(status, tlm::TLM_READ_COMMAND);
    EXPECT_EQ((value >> 8) & 0xff, 9u);
    EXPECT_NE(value & (1ull << 27), 0u);
    EXPECT_NE(value & (1ull << 46), 0u);
    EXPECT_NE(value & (1ull << 44), 0u);
    initiator.protected_write(status, 0);
    EXPECT_EQ(initiator.access64(status, tlm::TLM_READ_COMMAND), value);
    initiator.protected_write(status, STATUS_V | STATUS_UE);
    EXPECT_EQ(initiator.access64(status, tlm::TLM_READ_COMMAND), value);
    initiator.protected_write(status, STATUS_V | STATUS_UE | (1u << 27));
    value = initiator.access64(status, tlm::TLM_READ_COMMAND);
    EXPECT_EQ((value >> 8) & 0xff, 10u);
    EXPECT_EQ(value & (1ull << 46), 0u);
    initiator.protected_write(status, STATUS_V | STATUS_UE);
    EXPECT_EQ(initiator.access64(status, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, GicBlockIsolationAndErrorUpdate)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("blocks_test"));
    broker.set_preset_cci_value("blocks_leaf.gci_count", cci::cci_value(2u));
    zena_device_fmu leaf("blocks_leaf");
    TlmInitiator initiator("blocks_initiator");
    initiator.socket.bind(leaf.target_socket);
    EXPECT_EQ(initiator.access32(6 * 0x40 + 4, tlm::TLM_READ_COMMAND), 1u);
    initiator.protected_write(0xf00, (3u << 28) | (3u << 8));
    initiator.protected_write(0xf04, (3u << 28) | (3u << 8));
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    initiator.protected_write(0xf04, (3u << 28) | (1u << 16) | (3u << 8));
    initiator.protected_write(0xf00, (3u << 28) | (3u << 8) | 1u);
    initiator.protected_write(0xf04, (3u << 28) | (3u << 8));
    const uint64_t status = 6 * 0x40 + 0x10;
    EXPECT_NE(initiator.access64(status, tlm::TLM_READ_COMMAND) & (1ull << 47), 0u);
    initiator.protected_write(status, STATUS_V | STATUS_UE);
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    initiator.protected_write(0xf14, (3u << 28) | (1u << 16) | (255u << 8));
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    initiator.protected_write(0xf28, 3u << 28);
    EXPECT_EQ(initiator.access64(status, tlm::TLM_READ_COMMAND) >> 32 & 0xfff, 0u);
    EXPECT_NE(initiator.access32(0xe00, tlm::TLM_READ_COMMAND) & (1u << 6), 0u);
    initiator.protected_write(0xf14, (3u << 28) | (2u << 16) | (3u << 8));
    EXPECT_NE(initiator.access32(0xf1c, tlm::TLM_READ_COMMAND) & 8u, 0u);
}

TEST(ZenaLeafFmuTest, MhuProfileCriticalityPagesAndHardwareFault)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("mhu_fmu_test"));
    broker.set_preset_cci_value("mhu_fmu_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("mhu_fmu_leaf");
    TlmInitiator initiator("mhu_fmu_initiator");
    TlmInitiator source("mhu_fmu_source");
    initiator.socket.bind(leaf.target_socket);
    source.socket.bind(leaf.error_socket);
    EXPECT_EQ(initiator.access32(0xfc8, tlm::TLM_READ_COMMAND), 6u);
    EXPECT_EQ(initiator.access32(0xe10, tlm::TLM_READ_COMMAND), 0x49d0143bu);
    EXPECT_EQ(initiator.access64(6 * 0x40, tlm::TLM_READ_COMMAND), 0u);
    for (unsigned int record = 0; record < 6; ++record) {
        EXPECT_EQ(initiator.access64(record * 0x40 + 0x10,
                                      tlm::TLM_READ_COMMAND), 1u);
    }
    const uint32_t sec = (1u << 28) | (20u << 8);
    initiator.protected_write(0xf14, sec);
    EXPECT_EQ(initiator.access32(0xf18, tlm::TLM_READ_COMMAND), 2u);
    initiator.protected_write(0xf04, sec);
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 1u << 3);
    initiator.protected_write(3 * 0x40 + 0x10, STATUS_V | STATUS_UE);
    EXPECT_EQ(initiator.access64(3 * 0x40 + 0x10, tlm::TLM_READ_COMMAND), 1u);
    initiator.protected_write(0xf10, 6);
    initiator.protected_write(0xf0c, sec);
    EXPECT_EQ(initiator.access32(0xf18, tlm::TLM_READ_COMMAND), 6u);
    source.access32(0, tlm::TLM_WRITE_COMMAND, sec);
    const uint64_t status = initiator.access64(2 * 0x40 + 0x10, tlm::TLM_READ_COMMAND);
    EXPECT_NE(status & STATUS_CI, 0u);
    EXPECT_EQ(status & (1ull << 44), 0u);
    EXPECT_EQ(status >> 8 & 0xff, 20u);
    leaf.reset();
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
    initiator.protected_write(0xf14, sec);
    EXPECT_EQ(initiator.access32(0xf18, tlm::TLM_READ_COMMAND), 2u);
}

TEST(ZenaLeafFmuTest, NonSecureAccessDoesNotUnlockAndLogsSecurityFault)
{
    zena_device_fmu leaf("secure_leaf");
    TlmInitiator initiator("secure_initiator");
    initiator.socket.bind(leaf.target_socket);
    RequestContext context;
    context.secure_valid = true;
    context.secure = false;
    RequestContextTlmExtension extension(context);
    uint32_t value = 0xbe;
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0xf20);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_extension(&extension);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    leaf.b_transport(trans, delay);
    trans.clear_extension(&extension);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
    EXPECT_EQ(initiator.access32(10 * 0x40 + 0x10, tlm::TLM_READ_COMMAND) >> 8 & 0xff, 9u);
    trans.set_address(0xfffffffffffffffcul);
    leaf.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    trans.set_address(0xf20);
    unsigned char enable = 0x00;
    trans.set_byte_enable_ptr(&enable);
    trans.set_byte_enable_length(1);
    leaf.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
}

TEST(ZenaLeafFmuTest, Ni710FmuPreservesSyndromeAfterAck)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("zena_ni710ae_fmu_test"));
    broker.set_preset_cci_value("ni_leaf.node_index", cci::cci_value(11u));
    zena_ni710ae_fmu leaf("ni_leaf");
    TlmInitiator initiator("ni_initiator");
    initiator.socket.bind(leaf.target_socket);
    auto write = [&](uint64_t address, uint32_t value) {
        initiator.access32(0xe200, tlm::TLM_WRITE_COMMAND, 0xbe);
        initiator.access32(address, tlm::TLM_WRITE_COMMAND, value);
    };
    EXPECT_EQ(initiator.access32(0xffc8, tlm::TLM_READ_COMMAND), 12u);
    EXPECT_EQ(initiator.access32(0x20 + 11 * 0x40, tlm::TLM_READ_COMMAND), 0x61u);
    write(0xe210, 0x61);
    write(0xe204, 0x3ffffu);
    write(0xe208, 2);
    const uint64_t address = 0x10 + 11 * 0x40;
    const auto status = initiator.access32(address, tlm::TLM_READ_COMMAND);
    EXPECT_EQ((status >> 8) & 0xff, 2u);
    EXPECT_NE(status & STATUS_UE, 0u);
    write(address, status);
    const auto ack = initiator.access32(address, tlm::TLM_READ_COMMAND);
    EXPECT_EQ(ack & STATUS_V, 0u);
    EXPECT_EQ(ack & 1, 0u);
    EXPECT_EQ((ack >> 8) & 0xff, 2u);
}

TEST(ZenaLeafFmuTest, MhuNonSecureSparseAccessPreservesUnlock)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("mhu_ns_test"));
    broker.set_preset_cci_value("mhu_ns_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("mhu_ns_leaf");
    TlmInitiator initiator("mhu_ns_initiator");
    initiator.socket.bind(leaf.target_socket);
    initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
    uint32_t value = 0;
    unsigned char byte_enable = 0;
    RequestContext context;
    context.secure_valid = true;
    context.secure = false;
    RequestContextTlmExtension extension(context);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(8);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(&byte_enable);
    trans.set_byte_enable_length(1);
    trans.set_extension(&extension);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    leaf.b_transport(trans, delay);
    trans.clear_extension(&extension);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0xbeu);
    EXPECT_EQ(initiator.access32(8, tlm::TLM_READ_COMMAND), 3u);
    EXPECT_EQ((initiator.access32(4 * 0x40 + 0x10, tlm::TLM_READ_COMMAND) >> 8) & 0xff, 9u);
    leaf.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, MhuClearInsertionPreservesIndependentHardwareFault)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("mhu_clear_test"));
    broker.set_preset_cci_value("mhu_clear_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("mhu_clear_leaf");
    TlmInitiator initiator("mhu_clear_initiator");
    TlmInitiator source("mhu_clear_source");
    initiator.socket.bind(leaf.target_socket);
    source.socket.bind(leaf.error_socket);
    const uint32_t sec = (1u << 28) | (20u << 8);
    const auto clear_insertion = [&] {
        initiator.protected_write(0xf10, 2);
        initiator.protected_write(0xf0c, sec);
        EXPECT_EQ(initiator.access32(0xf18, tlm::TLM_READ_COMMAND), 2u);
    };
    initiator.protected_write(0xf04, sec);
    clear_insertion();
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    for (bool hardware_first : {false, true}) {
        if (hardware_first) source.access32(0, tlm::TLM_WRITE_COMMAND, sec);
        initiator.protected_write(0xf04, sec);
        if (!hardware_first) source.access32(0, tlm::TLM_WRITE_COMMAND, sec);
        clear_insertion();
        const auto status = initiator.access64(3 * 0x40 + 0x10, tlm::TLM_READ_COMMAND);
        EXPECT_NE(status & STATUS_V, 0u);
        EXPECT_EQ(status & (1ULL << 44), 0u);
        EXPECT_EQ((status >> 8) & 0xff, 20u);
        initiator.protected_write(3 * 0x40 + 0x10, STATUS_V | STATUS_UE | (1u << 27));
        EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    }
}

TEST(ZenaLeafFmuTest, MhuCrcTimeoutPageHasReadbackAndReset)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("mhu_timeout_test"));
    broker.set_preset_cci_value("mhu_timeout_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("mhu_timeout_leaf");
    TlmInitiator initiator("mhu_timeout_initiator");
    initiator.socket.bind(leaf.target_socket);
    const uint32_t crc_page1 = (1u << 28) | (8u << 8) | 1;
    initiator.protected_write(0xf10, 0x1234abcd);
    initiator.protected_write(0xf0c, crc_page1);
    EXPECT_EQ(initiator.access32(0xf18, tlm::TLM_READ_COMMAND), 0x1234abcdu);
    initiator.protected_write(0xf10, 0);
    initiator.protected_write(0xf14, crc_page1);
    EXPECT_EQ(initiator.access32(0xf18, tlm::TLM_READ_COMMAND), 0x1234abcdu);
    leaf.reset();
    initiator.protected_write(0xf14, crc_page1);
    EXPECT_EQ(initiator.access32(0xf18, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, MhuInvalidTransfersConsumeOnlySecureKeys)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("mhu_size_test"));
    broker.set_preset_cci_value("mhu_size_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("mhu_size_leaf");
    TlmInitiator initiator("mhu_size_initiator");
    initiator.socket.bind(leaf.target_socket);
    uint64_t value = (uint64_t((1u << 28) | (20u << 8)) << 32) | 1u;
    RequestContext context;
    context.secure_valid = true;
    context.secure = false;
    RequestContextTlmExtension extension(context);
    const auto invalid_write = [&](unsigned length, bool non_secure) {
        initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
        tlm::tlm_generic_payload trans;
        trans.set_address(0xf00);
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        if (non_secure) trans.set_extension(&extension);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        leaf.b_transport(trans, delay);
        if (non_secure) trans.clear_extension(&extension);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), non_secure ? 0xbeu : 0u);
        EXPECT_EQ(initiator.access32(3 * 0x40 + 0x10, tlm::TLM_READ_COMMAND), 1u);
    };
    invalid_write(1, false);
    invalid_write(2, false);
    invalid_write(8, false);
    invalid_write(2, true);
    invalid_write(8, true);
    initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
    initiator.access64(2 * 0x40 + 8, tlm::TLM_WRITE_COMMAND, 0xffffffff00002003ULL);
    EXPECT_EQ(initiator.access64(2 * 0x40 + 8, tlm::TLM_READ_COMMAND), 0x2003u);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, MhuKeyLifecycle)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("audit_key"));
    broker.set_preset_cci_value("audit_key_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("audit_key_leaf");
    TlmInitiator initiator("audit_key_initiator");
    initiator.socket.bind(leaf.target_socket);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
    initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0x123456be);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0xbeu);
    EXPECT_EQ(initiator.access32(0xfc8, tlm::TLM_READ_COMMAND), 6u);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0xbeu);
    initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
    initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
    initiator.access32(0x8c, tlm::TLM_WRITE_COMMAND, ~0u);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0xbeu);
    initiator.access32(0x88, tlm::TLM_WRITE_COMMAND, 0x2003);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
    initiator.access32(0x88, tlm::TLM_WRITE_COMMAND, 0);
    EXPECT_EQ(initiator.access32(0x88, tlm::TLM_READ_COMMAND), 0x2003u);
    initiator.access32(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
    leaf.reset();
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, AllEnabledByteStrobesMustUnlock)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("audit_strobe"));
    broker.set_preset_cci_value("audit_strobe_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("audit_strobe_leaf");
    TlmInitiator initiator("audit_strobe_initiator");
    initiator.socket.bind(leaf.target_socket);
    RequestContext context;
    context.secure_valid = true;
    context.secure = true;
    RequestContextTlmExtension extension(context);
    uint32_t value = 0xbe;
    unsigned char enables[4] = {0xff, 0xff, 0xff, 0xff};
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0xf20);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(enables);
    trans.set_byte_enable_length(4);
    trans.set_extension(&extension);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    leaf.b_transport(trans, delay);
    trans.clear_extension(&extension);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0xbeu);
}

TEST(ZenaLeafFmuTest, DifferentMechanismsMustSetOverflow)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("audit_overflow"));
    broker.set_preset_cci_value("audit_overflow_leaf.mhu_profile", cci::cci_value(true));
    zena_device_fmu leaf("audit_overflow_leaf");
    TlmInitiator initiator("audit_overflow_initiator");
    initiator.socket.bind(leaf.target_socket);
    initiator.protected_write(0xf04, (1u << 28) | (1u << 8));
    initiator.protected_write(0xf04, (1u << 28) | (2u << 8));
    const auto status = initiator.access64(2 * 0x40 + 0x10, tlm::TLM_READ_COMMAND);
    EXPECT_NE(status & (1ull << 46), 0u);
    EXPECT_EQ((status >> 8) & 0xff, 1u);
    EXPECT_NE(status & (1ull << 27), 0u);
}

TEST(ZenaLeafFmuTest, ProfilesKeepDistinctOverflowAndAcceptRepeatedFullStrobes)
{
    for (bool mhu : {false, true}) {
        auto broker = cci::cci_get_global_broker(cci::cci_originator("profile_strobe"));
        const std::string name = mhu ? "profile_mhu_leaf" : "profile_gic_leaf";
        broker.set_preset_cci_value(name + ".mhu_profile", cci::cci_value(mhu));
        zena_device_fmu leaf(name.c_str());
        TlmInitiator initiator((name + "_initiator").c_str());
        initiator.socket.bind(leaf.target_socket);
        for (unsigned int enable_length : {1u, 4u}) {
            uint32_t key = 0x123456be;
            unsigned char enables[4] = {0xff, 0xff, 0xff, 0xff};
            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(0xf20);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&key));
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(enables);
            trans.set_byte_enable_length(enable_length);
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            leaf.b_transport(trans, delay);
            EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
            EXPECT_EQ(initiator.access32(0xf20, tlm::TLM_READ_COMMAND), 0xbeu);
        }
        initiator.protected_write(0xf04, 1u << 8);
        initiator.protected_write(0xf04, 2u << 8);
        auto status = initiator.access64(0x10, tlm::TLM_READ_COMMAND);
        EXPECT_NE(status & (1ull << 46), 0u);
        EXPECT_EQ(bool(status & (1ull << 27)), mhu);
        initiator.protected_write(0x10, STATUS_V | STATUS_UE);
        EXPECT_EQ((initiator.access32(0x10, tlm::TLM_READ_COMMAND) >> 8) & 0xff,
                  mhu ? 1u : 2u);
        if (mhu) initiator.protected_write(0x10, STATUS_V | STATUS_UE | (1u << 27));
        initiator.protected_write(0x10, STATUS_V | STATUS_UE);
        EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    }
}

TEST(ZenaLeafFmuTest, WireAndPacketCanArriveSeparately)
{
    zena_device_fmu leaf("wire_leaf");
    TlmInitiator initiator("wire_initiator");
    TlmInitiator source("wire_source");
    initiator.socket.bind(leaf.target_socket);
    source.socket.bind(leaf.error_socket);
    source.access32(8, tlm::TLM_WRITE_COMMAND, 3);
    uint64_t status = initiator.access64(0x10, tlm::TLM_READ_COMMAND);
    EXPECT_NE(status & STATUS_V, 0u);
    EXPECT_NE(status & (1ull << 45), 0u);
    EXPECT_EQ(status & 0xff00, 0u);
    initiator.protected_write(0x10, STATUS_V | STATUS_UE);
    EXPECT_NE(initiator.access32(0x10, tlm::TLM_READ_COMMAND) & STATUS_V, 0u);
    source.access32(4, tlm::TLM_WRITE_COMMAND, 1u << 8);
    EXPECT_EQ((initiator.access32(0x10, tlm::TLM_READ_COMMAND) >> 8) & 0xff, 1u);
    source.access32(8, tlm::TLM_WRITE_COMMAND, 2);
    initiator.protected_write(0x10, STATUS_V | STATUS_UE);
    EXPECT_NE(initiator.access32(0x10, tlm::TLM_READ_COMMAND) & STATUS_V, 0u);
    initiator.protected_write(8, 1);
    initiator.protected_write(0x10, STATUS_V | STATUS_UE);
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    leaf.reset();
    source.access32(8, tlm::TLM_WRITE_COMMAND, 3);
    initiator.protected_write(8, 1);
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, ErrorUpdateStartsAtRequestedBlockAndStopsAtPowerBoundary)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("update_blocks"));
    broker.set_preset_cci_value("update_leaf.gci_count", cci::cci_value(3u));
    zena_device_fmu leaf("update_leaf");
    TlmInitiator initiator("update_initiator");
    initiator.socket.bind(leaf.target_socket);
    for (unsigned int block : {0u, 2u})
        initiator.protected_write(0xf04, (3u << 28) | (block << 16) | (1u << 8));
    const uint32_t status = 6 * 0x40 + 0x10;
    initiator.protected_write(status, STATUS_V | STATUS_UE);
    EXPECT_NE(initiator.access64(status, tlm::TLM_READ_COMMAND) & (1ull << 47), 0u);
    ASSERT_TRUE(leaf.runtime_set_block_response(3, 1, 1, false));
    initiator.protected_write(0xf28, (3u << 28) | 5u);
    EXPECT_EQ((initiator.access32(0xf1c, tlm::TLM_READ_COMMAND) >> 16) & 0xfff, 1u);
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    EXPECT_EQ(initiator.access64(status, tlm::TLM_READ_COMMAND) & (1ull << 47), 0u);
    initiator.protected_write(0xf28, (3u << 28) | (2u << 16) | 4u);
    EXPECT_EQ((initiator.access64(status, tlm::TLM_READ_COMMAND) >> 32) & 0xfff, 2u);
    EXPECT_NE(initiator.access32(0xe00, tlm::TLM_READ_COMMAND) & (1u << 6), 0u);
    initiator.protected_write(status, STATUS_V | STATUS_UE);
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
}

TEST(ZenaLeafFmuTest, IrqLevelsAndResetClearBothFmuStages)
{
    auto timed_broker = cci::cci_get_global_broker(cci::cci_originator("timed_fmu_test"));
    timed_broker.set_preset_cci_value("timed_leaf.clock_period_ns", cci::cci_value(uint64_t(8)));
    zena_device_fmu timed_leaf("timed_leaf");
    TlmInitiator timed("timed_initiator");
    timed.socket.bind(timed_leaf.target_socket);
    timed_broker.set_preset_cci_value("annotated_leaf.clock_period_ns", cci::cci_value(uint64_t(8)));
    zena_device_fmu annotated_leaf("annotated_leaf");
    sc_core::sc_signal<bool> annotated_irq("annotated_irq");
    AnnotatedInitiator annotated("annotated_initiator", annotated_irq, annotated_leaf);
    annotated.socket.bind(annotated_leaf.target_socket);
    annotated_leaf.critical_irq.bind(annotated_irq);
    {
        uint32_t value = 0;
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(0xf1c);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        sc_core::sc_time delay(1, sc_core::SC_NS);
        annotated_leaf.b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
        EXPECT_EQ(delay, sc_core::sc_time(1, sc_core::SC_NS));
    }
    gs::ModuleFactory::ContainerBase binder("fault_binding", true);
    WideSignalPair wide("wide_signals");
    ASSERT_TRUE(binder.try_bind_all<DEFAULT_TLM_BUSWIDTH>(&wide.output, &wide.input));
    ASSERT_TRUE(binder.try_bind_all<DEFAULT_TLM_BUSWIDTH>(&wide.optional_input, &wide.optional_output));
    EXPECT_EQ(wide.input.read(), 0u);
    auto broker = cci::cci_get_global_broker(cci::cci_originator("route_fmu_test"));
    broker.set_preset_cci_value("route_leaf.parent_critical_record", cci::cci_value(204u));
    broker.set_preset_cci_value("route_leaf.parent_non_critical_record", cci::cci_value(203u));
    broker.set_preset_cci_value("route_parent.critical_records", cci::cci_value(std::string("0:6,4:204")));
    broker.set_preset_cci_value("irq_parent.critical_records", cci::cci_value(std::string("0:6,4:0")));
    zena_fmu route_parent("route_parent");
    zena_device_fmu route_leaf("route_leaf");
    TlmInitiator route_initiator("route_initiator");
    TlmInitiator route_parent_initiator("route_parent_initiator");
    route_initiator.socket.bind(route_leaf.target_socket);
    route_parent_initiator.socket.bind(route_parent.target_socket);
    ASSERT_TRUE(binder.try_bind_all<DEFAULT_TLM_BUSWIDTH>(
        &route_leaf.fault_socket, &route_parent.device_fault_socket));
    zena_fmu parent("irq_parent");
    zena_device_fmu leaf("irq_leaf");
    TlmInitiator initiator("irq_initiator");
    TlmInitiator parent_initiator("irq_parent_initiator");
    initiator.socket.bind(leaf.target_socket);
    parent_initiator.socket.bind(parent.target_socket);
    ASSERT_TRUE(binder.try_bind_all<DEFAULT_TLM_BUSWIDTH>(
        &leaf.fault_socket, &parent.device_fault_socket));
    sc_core::sc_signal<bool> leaf_cri("leaf_cri");
    sc_core::sc_signal<bool> root_cri("root_cri");
    leaf.critical_irq.bind(leaf_cri);
    parent.critical_irq.bind(root_cri);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    wide.output->write((1ull << 32) | 104);
    wide.optional_output->write((1ull << 40) | 105);
    EXPECT_EQ(wide.input.read(), (1ull << 32) | 104);
    EXPECT_EQ(wide.optional_input.read(), (1ull << 40) | 105);
    {
        auto& initiator = route_initiator;
        auto& parent = route_parent;
    const uint32_t protection = (1u << 28) | (2u << 8);
    initiator.protected_write(2 * 0x40 + 8, 3u | (1u << 13));
    initiator.protected_write(3 * 0x40 + 8, 3u | (1u << 2));
    initiator.protected_write(0xf00, protection | 1u);
    initiator.protected_write(0xf08, protection | 1u);
    initiator.protected_write(0xf04, protection);

    EXPECT_EQ(initiator.access64(0xe00, tlm::TLM_READ_COMMAND), 1ull << 2);
    EXPECT_EQ(parent_read32(
                  parent, 4 * BANK_BYTES + 204 * RECORD_STRIDE + ERR_STATUS) &
                  (STATUS_V | STATUS_CI),
              STATUS_V | STATUS_CI);

    initiator.protected_write(0x10 + 2 * 0x40, STATUS_V | STATUS_UE);
    initiator.protected_write(0xf08, protection);
    initiator.protected_write(0xf04, protection);
    EXPECT_EQ(initiator.access64(0xe00, tlm::TLM_READ_COMMAND), 1ull << 3);
    EXPECT_EQ(parent_read32(
                  parent, 4 * BANK_BYTES + 203 * RECORD_STRIDE + ERR_STATUS) &
                  (STATUS_V | STATUS_UE),
              STATUS_V | STATUS_UE);
    }
    initiator.protected_write(8, 0x2003);
    initiator.protected_write(0xf04, 3u << 8);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_TRUE(leaf_cri.read());
    EXPECT_TRUE(root_cri.read());
    parent_initiator.access32(4 * BANK_BYTES + 0x8bfc, tlm::TLM_WRITE_COMMAND, 0xbe);
    parent_initiator.access32(4 * BANK_BYTES + ERR_STATUS,
                             tlm::TLM_WRITE_COMMAND, STATUS_V | STATUS_CI);
    EXPECT_NE(parent_initiator.access32(4 * BANK_BYTES + ERR_STATUS,
                                        tlm::TLM_READ_COMMAND) & STATUS_V, 0u);
    initiator.protected_write(0x10, STATUS_V | STATUS_UE);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(leaf_cri.read());
    EXPECT_TRUE(root_cri.read());
    parent_initiator.access32(4 * BANK_BYTES + 0x8bfc, tlm::TLM_WRITE_COMMAND, 0xbe);
    parent_initiator.access32(4 * BANK_BYTES + ERR_STATUS,
                             tlm::TLM_WRITE_COMMAND, STATUS_V | STATUS_CI);
    parent_initiator.access32(0x8bfc, tlm::TLM_WRITE_COMMAND, 0xbe);
    parent_initiator.access32(6 * RECORD_STRIDE + ERR_STATUS,
                             tlm::TLM_WRITE_COMMAND, STATUS_V | STATUS_CI);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(root_cri.read());
    initiator.protected_write(0xf04, 3u << 8);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_TRUE(root_cri.read());
    leaf.reset_in->write(true);
    parent.reset_in->write(true);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(leaf_cri.read());
    EXPECT_FALSE(root_cri.read());
    EXPECT_EQ(initiator.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    EXPECT_EQ(parent_initiator.access32(4 * BANK_BYTES + ERR_STATUS,
                                       tlm::TLM_READ_COMMAND), 0u);

    timed.protected_write(0xf04, 1u << 8);
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND) & 1, 1u);
    EXPECT_EQ(timed.access32(0x10, tlm::TLM_READ_COMMAND) & STATUS_V, 0u);
    timed.protected_write(0xf04, 2u << 8);
    EXPECT_EQ((timed.access32(10 * 0x40 + 0x10, tlm::TLM_READ_COMMAND) >> 8) & 0xff, 13u);
    sc_core::sc_start(sc_core::sc_time(9, sc_core::SC_NS));
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND) & 0x3f, 0u);
    EXPECT_EQ((timed.access32(0x10, tlm::TLM_READ_COMMAND) >> 8) & 0xff, 1u);

    timed_leaf.reset();
    timed.protected_write(0xf24, 1);
    ASSERT_TRUE(timed_leaf.runtime_set_block_response(0, 0, 3, true));
    timed.protected_write(0xf14, 1u << 8);
    sc_core::sc_start(sc_core::sc_time(9, sc_core::SC_NS));
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND) & 3, 2u);
    ASSERT_TRUE(timed_leaf.runtime_set_block_response(0, 0, 1, true));
    timed.protected_write(0xf14, 2u << 8);
    sc_core::sc_start(sc_core::sc_time(9, sc_core::SC_NS));
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND) & 0x3f, 0u);
    EXPECT_EQ((timed.access32(0xf1c, tlm::TLM_READ_COMMAND) >> 8) & 0xff, 2u);
    sc_core::sc_start(sc_core::sc_time(7, sc_core::SC_NS));
    EXPECT_NE(timed.access32(0xf1c, tlm::TLM_READ_COMMAND) & (1u << 5), 0u);
    EXPECT_EQ((timed.access32(0xf1c, tlm::TLM_READ_COMMAND) >> 8) & 0xff, 1u);

    timed_leaf.reset();
    timed.protected_write(0xf24, 1);
    ASSERT_TRUE(timed_leaf.runtime_set_block_response(0, 0, 3, true));
    timed.protected_write(0xf14, 1u << 8);
    sc_core::sc_start(sc_core::sc_time(9, sc_core::SC_NS));
    timed.protected_write(0xf24, 8);
    ASSERT_TRUE(timed_leaf.runtime_set_block_response(3, 0, 4, true));
    timed.protected_write(0xf14, (3u << 28) | (2u << 8));
    sc_core::sc_start(sc_core::sc_time(16, sc_core::SC_NS));
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND), (1u << 8) | (1u << 5) | 1u);
    sc_core::sc_start(sc_core::sc_time(17, sc_core::SC_NS));
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND), (3u << 28) | (2u << 8));

    timed_leaf.reset();
    ASSERT_TRUE(timed_leaf.runtime_set_block_response(0, 0, 1, false));
    timed.protected_write(0xf04, 1u << 8);
    sc_core::sc_start(sc_core::sc_time(9, sc_core::SC_NS));
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND) & 0x3f, 4u);
    EXPECT_EQ(timed.access32(0xe00, tlm::TLM_READ_COMMAND), 0u);
    timed_leaf.reset();
    timed.protected_write(0xf04, 1u << 8);
    timed_leaf.reset();
    sc_core::sc_start(sc_core::sc_time(9, sc_core::SC_NS));
    EXPECT_EQ(timed.access32(0xf1c, tlm::TLM_READ_COMMAND), 0u);
    sc_core::sc_start(sc_core::sc_time(120, sc_core::SC_NS));
    EXPECT_TRUE(annotated.done);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
