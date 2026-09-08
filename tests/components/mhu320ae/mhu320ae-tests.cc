/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>
#include <tlm-extensions/request-context.h>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <mhu320ae.h>
#include <ports/target-signal-socket.h>

namespace {

constexpr uint64_t SHMEM_BASE = 0x4000040000000;
constexpr uint64_t SHMEM_SIZE = 0x400;
constexpr uint64_t SCMI_STATUS = 0x04;
constexpr uint64_t SCMI_LENGTH = 0x14;
constexpr uint64_t SCMI_HEADER = 0x18;
constexpr uint64_t SCMI_PAYLOAD = 0x1c;
constexpr uint64_t CTRL_FEAT_SPT0 = 0x010;
constexpr uint64_t CTRL_FEAT_SPT1 = 0x014;
constexpr uint64_t CTRL_IIDR = 0xfc8;
constexpr uint64_t CTRL_AIDR = 0xfcc;
constexpr uint64_t CTRL_DBCH_INT_ST0 = 0x400;
constexpr uint64_t CTRL = 0x100;
constexpr uint64_t RPMSG_TEST_VRING = SHMEM_BASE + 0x100;
constexpr uint64_t RPMSG_TEST_BUFFER = SHMEM_BASE + 0x200;
constexpr uint64_t RPMSG_TEST_AVAIL = RPMSG_TEST_VRING + 0x20;
constexpr uint64_t RPMSG_TEST_USED = RPMSG_TEST_VRING + 0x30;
constexpr uint64_t DBCH_CFG0 = 0x20;
constexpr uint64_t DBCW_SET = 0x100c;
constexpr uint64_t DBCW_ST = 0x1000;
constexpr uint64_t DBCW_ST_MSK = 0x1004;
constexpr uint64_t DBCW_CLR = 0x1008;
constexpr uint64_t DBCW_INT_ST = 0x1010;
constexpr uint64_t DBCW_INT_CLR = 0x1014;
constexpr uint64_t DBCW_INT_EN = 0x1018;
constexpr uint64_t DBCW_CTRL = 0x101c;
constexpr uint64_t DBCW_STRIDE = 0x20;
constexpr uint32_t MHU_NOTIFY_VALUE = 1234;
constexpr uint32_t SCMI_PROTOCOL_BASE = 0x10;
constexpr uint32_t SCMI_PROTOCOL_POWER_DOMAIN = 0x11;
constexpr uint32_t SCMI_PROTOCOL_SYS_POWER = 0x12;
constexpr uint32_t SCMI_PROTOCOL_PERFORMANCE = 0x13;
constexpr uint32_t SCMI_PROTOCOL_PFDI_MONITOR = 0x90;
constexpr uint32_t SCMI_MESSAGE_PROTOCOL_VERSION = 0x0;
constexpr uint32_t SCMI_MESSAGE_PROTOCOL_ATTRIBUTES = 0x1;
constexpr uint32_t SCMI_MESSAGE_PROTOCOL_MESSAGE_ATTRIBUTES = 0x2;
constexpr uint32_t SCMI_MESSAGE_DISCOVER_LIST_PROTOCOLS = 0x6;
constexpr uint32_t SCMI_MESSAGE_POWER_STATE_SET = 0x4;
constexpr uint32_t SCMI_MESSAGE_POWER_STATE_GET = 0x5;
constexpr uint32_t SCMI_MESSAGE_SYS_POWER_STATE_SET = 0x3;
constexpr uint32_t SCMI_MESSAGE_SYS_POWER_STATE_NOTIFY = 0x5;
constexpr uint32_t SCMI_MESSAGE_PERF_DOMAIN_ATTRIBUTES = 0x3;
constexpr uint32_t SCMI_MESSAGE_PERF_DESCRIBE_LEVELS = 0x4;
constexpr uint32_t SCMI_MESSAGE_PERF_LIMITS_SET = 0x5;
constexpr uint32_t SCMI_MESSAGE_PERF_LIMITS_GET = 0x6;
constexpr uint32_t SCMI_MESSAGE_PERF_LEVEL_SET = 0x7;
constexpr uint32_t SCMI_MESSAGE_PERF_LEVEL_GET = 0x8;
constexpr uint32_t SCMI_SYS_POWER_COLD_RESET = 0x1;
constexpr uint32_t SCMI_PFDI_MONITOR_VERSION = 0x00020000;
constexpr uint32_t SCMI_PROTOCOL_ERROR = 0xfffffff6u;
constexpr uint8_t RSE_COMMS_PROTOCOL_EMBED = 0;
constexpr uint8_t RSE_COMMS_PROTOCOL_POINTER_ACCESS = 1;
constexpr uint32_t TFM_PROTECTED_STORAGE_SERVICE_HANDLE = 0x40000101;
constexpr uint32_t TFM_PS_SET = 1001;
constexpr uint32_t TFM_PS_GET = 1002;
constexpr uint32_t TFM_PS_GET_INFO = 1003;
constexpr uint32_t TFM_PS_REMOVE = 1004;
constexpr uint32_t TFM_MEASURED_BOOT_HANDLE = 0x40000110;
constexpr uint32_t TFM_MEASURED_BOOT_EXTEND = 1002;
constexpr uint32_t TFM_FIRMWARE_UPDATE_SERVICE_HANDLE = 0x40000104;
constexpr uint32_t TFM_FWU_QUERY = 1010;
constexpr uint32_t PSA_FWU_COMPONENT_INFO_SIZE = 44;
constexpr uint32_t PSA_ERROR_DOES_NOT_EXIST = 0xffffff74u;

uint32_t scmi_header(uint32_t protocol, uint32_t message)
{
    return (protocol << 10) | message;
}

class TestMemory : public sc_core::sc_module
{
    std::array<uint8_t, SHMEM_SIZE> m_mem {};

public:
    tlm_utils::simple_target_socket<TestMemory, DEFAULT_TLM_BUSWIDTH> target_socket;

    explicit TestMemory(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , target_socket("target_socket")
    {
        target_socket.register_b_transport(this, &TestMemory::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        const uint64_t address = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (address < SHMEM_BASE || address + len > SHMEM_BASE + m_mem.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        const auto offset = static_cast<size_t>(address - SHMEM_BASE);
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &m_mem[offset], len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&m_mem[offset], data, len);
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    uint32_t read32(uint64_t offset) const
    {
        uint32_t value = 0;
        std::memcpy(&value, &m_mem[offset], sizeof(value));
        return value;
    }

    uint16_t read16(uint64_t offset) const
    {
        uint16_t value = 0;
        std::memcpy(&value, &m_mem[offset], sizeof(value));
        return value;
    }

    uint8_t read8(uint64_t offset) const
    {
        return m_mem[offset];
    }

    std::vector<uint8_t> read_bytes(uint64_t offset, size_t len) const
    {
        std::vector<uint8_t> out(len);
        std::memcpy(out.data(), &m_mem[offset], len);
        return out;
    }

    void write_bytes(uint64_t offset, const std::vector<uint8_t>& value)
    {
        std::memcpy(&m_mem[offset], value.data(), value.size());
    }

    void write32(uint64_t offset, uint32_t value)
    {
        std::memcpy(&m_mem[offset], &value, sizeof(value));
    }

    void write16(uint64_t offset, uint16_t value)
    {
        std::memcpy(&m_mem[offset], &value, sizeof(value));
    }

    void write64(uint64_t offset, uint64_t value)
    {
        std::memcpy(&m_mem[offset], &value, sizeof(value));
    }
};

class TestInitiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<TestInitiator, DEFAULT_TLM_BUSWIDTH> initiator_socket;

    explicit TestInitiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , initiator_socket("initiator_socket")
    {
    }
};

class ResetSink : public sc_core::sc_module
{
    std::mutex m_race_mutex;
    std::condition_variable m_race_cv;
    bool m_race_armed = false;
    bool m_assert_callback_entered = false;
    bool m_clear_started = false;
    bool m_clear_returned = false;
    bool m_clear_returned_during_assert = false;

public:
    TargetSignalSocket<bool> reset;
    unsigned int write_count = 0;
    bool saw_asserted = false;

    explicit ResetSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , reset("reset")
    {
        reset.register_value_changed_cb([this](const bool& value) {
            ++write_count;
            saw_asserted = saw_asserted || value;
            std::unique_lock<std::mutex> lock(m_race_mutex);
            if (!value || !m_race_armed) {
                return;
            }
            m_assert_callback_entered = true;
            m_race_cv.notify_all();
            if (m_race_cv.wait_for(lock, std::chrono::seconds(1),
                                   [this] { return m_clear_started; })) {
                m_clear_returned_during_assert = m_race_cv.wait_for(
                    lock, std::chrono::milliseconds(50),
                    [this] { return m_clear_returned; });
            }
            m_race_armed = false;
        });
    }

    void arm_assert_clear_race()
    {
        std::lock_guard<std::mutex> lock(m_race_mutex);
        m_race_armed = true;
        m_assert_callback_entered = false;
        m_clear_started = false;
        m_clear_returned = false;
        m_clear_returned_during_assert = false;
    }

    bool wait_for_assert_callback()
    {
        std::unique_lock<std::mutex> lock(m_race_mutex);
        return m_race_cv.wait_for(lock, std::chrono::seconds(1),
                                  [this] { return m_assert_callback_entered; });
    }

    void mark_clear_started()
    {
        std::lock_guard<std::mutex> lock(m_race_mutex);
        m_clear_started = true;
        m_race_cv.notify_all();
    }

    void mark_clear_returned()
    {
        std::lock_guard<std::mutex> lock(m_race_mutex);
        m_clear_returned = true;
        m_race_cv.notify_all();
    }

    bool clear_returned_during_assert()
    {
        std::lock_guard<std::mutex> lock(m_race_mutex);
        return m_clear_returned_during_assert;
    }
};

class SignalSource : public sc_core::sc_module
{
public:
    InitiatorSignalSocket<bool> signal;

    explicit SignalSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
    {
    }

    void write(bool value) { signal->write(value); }
};

class TestableMhu320ae : public mhu320ae
{
public:
    using mhu320ae::mhu320ae;

    void queue_power_on_reset(bool asserted)
    {
        schedule_power_on_reset(asserted);
    }
};

class StartAction : public sc_core::sc_module
{
    std::function<void()> m_action;

public:
    StartAction(sc_core::sc_module_name name, std::function<void()> action)
        : sc_core::sc_module(name)
        , m_action(std::move(action))
    {
    }

    void start_of_simulation() override { m_action(); }
};

uint32_t access32(TestInitiator& initiator, uint64_t offset, tlm::tlm_command command,
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
    initiator.initiator_socket->b_transport(trans, delay);

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

uint32_t read32(TestInitiator& initiator, uint64_t offset)
{
    return access32(initiator, offset, tlm::TLM_READ_COMMAND);
}

void write32(TestInitiator& initiator, uint64_t offset, uint32_t value)
{
    (void)access32(initiator, offset, tlm::TLM_WRITE_COMMAND, value);
}

void write32_as_requester(TestInitiator& initiator, uint64_t offset,
                          uint32_t value, uint32_t requester)
{
    tlm::tlm_generic_payload trans;
    auto data = value;
    RequestContext context;
    context.requester_id = requester;
    context.requester_valid = true;
    RequestContextTlmExtension extension(context);

    trans.set_address(offset);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_extension(&extension);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    initiator.initiator_socket->b_transport(trans, delay);
    trans.clear_extension(&extension);

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
}

uint32_t read_le32(const std::vector<uint8_t>& in, size_t offset)
{
    return static_cast<uint32_t>(in[offset]) |
           (static_cast<uint32_t>(in[offset + 1]) << 8) |
           (static_cast<uint32_t>(in[offset + 2]) << 16) |
           (static_cast<uint32_t>(in[offset + 3]) << 24);
}

void append_le16(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(value & 0xffu);
    out.push_back((value >> 8) & 0xffu);
}

void append_le32(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(value & 0xffu);
    out.push_back((value >> 8) & 0xffu);
    out.push_back((value >> 16) & 0xffu);
    out.push_back((value >> 24) & 0xffu);
}

void append_le64(std::vector<uint8_t>& out, uint64_t value)
{
    append_le32(out, static_cast<uint32_t>(value));
    append_le32(out, static_cast<uint32_t>(value >> 32));
}

std::vector<uint8_t> le64_bytes(uint64_t value)
{
    std::vector<uint8_t> out;
    append_le64(out, value);
    return out;
}

std::vector<uint8_t> le32_bytes(uint32_t value)
{
    std::vector<uint8_t> out;
    append_le32(out, value);
    return out;
}

uint32_t rse_ctrl(uint32_t type, uint32_t in_len, uint32_t out_len)
{
    return (type & 0xffffu) | ((out_len & 0x7u) << 16) |
           ((in_len & 0x7u) << 24);
}

std::vector<uint8_t> build_embed_msg(
    uint32_t handle,
    uint8_t seq, uint32_t type, const std::vector<std::vector<uint8_t>>& in_vecs,
    const std::vector<uint16_t>& out_sizes)
{
    std::vector<uint8_t> msg = {RSE_COMMS_PROTOCOL_EMBED, seq, 1, 0};
    append_le32(msg, handle);
    append_le32(msg, rse_ctrl(type, in_vecs.size(), out_sizes.size()));

    for (const auto& in_vec : in_vecs) {
        append_le16(msg, static_cast<uint16_t>(in_vec.size()));
    }
    for (auto out_size : out_sizes) {
        append_le16(msg, out_size);
    }
    while (msg.size() < 20) {
        append_le16(msg, 0);
    }
    for (const auto& in_vec : in_vecs) {
        msg.insert(msg.end(), in_vec.begin(), in_vec.end());
    }
    return msg;
}

std::vector<uint8_t> build_embed_ps_msg(
    uint8_t seq, uint32_t type, const std::vector<std::vector<uint8_t>>& in_vecs,
    const std::vector<uint16_t>& out_sizes)
{
    return build_embed_msg(TFM_PROTECTED_STORAGE_SERVICE_HANDLE, seq, type,
                           in_vecs, out_sizes);
}

std::vector<uint8_t> build_pointer_msg(
    uint32_t handle,
    uint8_t seq, uint32_t type, const std::vector<uint32_t>& io_sizes,
    const std::vector<uint64_t>& host_ptrs, uint32_t in_len, uint32_t out_len)
{
    std::vector<uint8_t> msg = {RSE_COMMS_PROTOCOL_POINTER_ACCESS, seq, 1, 0};
    append_le32(msg, handle);
    append_le32(msg, rse_ctrl(type, in_len, out_len));
    for (unsigned int i = 0; i < 4; ++i) {
        append_le32(msg, i < io_sizes.size() ? io_sizes[i] : 0);
    }
    for (unsigned int i = 0; i < 4; ++i) {
        append_le64(msg, i < host_ptrs.size() ? host_ptrs[i] : 0);
    }
    return msg;
}

std::vector<uint8_t> build_pointer_ps_msg(
    uint8_t seq, uint32_t type, const std::vector<uint32_t>& io_sizes,
    const std::vector<uint64_t>& host_ptrs, uint32_t in_len, uint32_t out_len)
{
    return build_pointer_msg(TFM_PROTECTED_STORAGE_SERVICE_HANDLE, seq, type,
                             io_sizes, host_ptrs, in_len, out_len);
}

void write_doorbell_message(TestInitiator& initiator, uint32_t notify_channel,
                            const std::vector<uint8_t>& msg)
{
    write32(initiator, DBCW_SET, msg.size());
    for (size_t offset = 0, channel = 1; offset < msg.size() && channel < notify_channel;
         offset += sizeof(uint32_t), ++channel) {
        uint32_t word = 0;
        const size_t chunk = std::min<size_t>(sizeof(word), msg.size() - offset);
        for (size_t byte = 0; byte < chunk; ++byte) {
            word |= static_cast<uint32_t>(msg[offset + byte]) << (byte * 8);
        }
        write32(initiator, DBCW_SET + (channel * DBCW_STRIDE), word);
    }
    write32(initiator, 0x1000 + (notify_channel * DBCW_STRIDE) +
                           (DBCW_SET - DBCW_ST),
            MHU_NOTIFY_VALUE);
}

std::vector<uint8_t> read_doorbell_message(TestInitiator& initiator,
                                           uint32_t notify_channel)
{
    const uint32_t length = read32(initiator, DBCW_ST);
    std::vector<uint8_t> msg;
    msg.reserve(length);
    for (size_t channel = 1; msg.size() < length && channel < notify_channel;
         ++channel) {
        const uint32_t word = read32(initiator, DBCW_ST + (channel * DBCW_STRIDE));
        for (size_t byte = 0; byte < sizeof(word) && msg.size() < length; ++byte) {
            msg.push_back((word >> (byte * 8)) & 0xffu);
        }
    }
    return msg;
}

} // namespace

TEST(Mhu320aeFrameModelTest, PbxAndMbxDoorbellRegistersAreReusable)
{
    mhu320ae::mhu320ae_frame_model pbx;
    mhu320ae::mhu320ae_frame_model mbx;

    pbx.configure(false, 4, 0x55aau, 0xaa55u, 0x11223344u, 0x55667788u);
    mbx.configure(true, 4, 0x55aau, 0xaa55u, 0x11223344u, 0x55667788u);

    EXPECT_EQ(pbx.read32(DBCH_CFG0), 3u);
    EXPECT_EQ(pbx.read32(CTRL_FEAT_SPT0), 0x55aau);
    EXPECT_EQ(pbx.read32(CTRL_FEAT_SPT1), 0xaa55u);
    EXPECT_EQ(pbx.read32(CTRL_IIDR), 0x11223344u);
    EXPECT_EQ(pbx.read32(CTRL_AIDR), 0x55667788u);
    EXPECT_EQ(mbx.read32(DBCH_CFG0), 3u);

    pbx.set_status_bits(0, 0x3u);
    pbx.set_int_enable(0, 0x1u);
    pbx.set_int_status_bits(0, 0x1u);
    pbx.refresh_combined_irq_regs();

    EXPECT_EQ(pbx.read32(DBCW_ST), 0x3u);
    EXPECT_EQ(pbx.read32(DBCW_INT_ST), 0x1u);
    EXPECT_EQ(pbx.read32(DBCW_INT_EN), 0x1u);
    EXPECT_EQ(pbx.read32(CTRL_DBCH_INT_ST0), 0x1u);

    mbx.set_status_bits(0, 0x3u);
    mbx.refresh_combined_irq_regs();
    EXPECT_EQ(mbx.read32(DBCW_ST), 0x3u);
    EXPECT_EQ(mbx.read32(DBCW_ST_MSK), 0x3u);
    EXPECT_EQ(mbx.read32(DBCW_CTRL), 0x1u);
    EXPECT_EQ(mbx.read32(CTRL_DBCH_INT_ST0), 0x1u);

    mbx.set_ctrl(0, 0u);
    mbx.refresh_combined_irq_regs();
    EXPECT_EQ(mbx.read32(CTRL_DBCH_INT_ST0), 0u);

    mbx.set_ctrl(0, 1u);
    mbx.refresh_combined_irq_regs();
    EXPECT_EQ(mbx.read32(CTRL_DBCH_INT_ST0), 0x1u);

    mbx.set_mask_bits(0, 0x3u);
    mbx.refresh_combined_irq_regs();
    EXPECT_EQ(mbx.read32(DBCW_ST_MSK), 0x0u);
    EXPECT_EQ(mbx.read32(CTRL_DBCH_INT_ST0), 0x0u);

    mbx.clear_mask_bits(0, 0x1u);
    mbx.refresh_combined_irq_regs();
    EXPECT_EQ(mbx.read32(DBCW_ST_MSK), 0x1u);
    EXPECT_EQ(mbx.read32(CTRL_DBCH_INT_ST0), 0x1u);

    mbx.clear_status_bits(0, 0x1u);
    mbx.refresh_combined_irq_regs();
    EXPECT_EQ(mbx.read32(DBCW_ST), 0x2u);
    EXPECT_EQ(mbx.read32(DBCW_ST_MSK), 0x0u);
    EXPECT_EQ(mbx.read32(CTRL_DBCH_INT_ST0), 0x0u);
}

TEST(Mhu320aeTest, RseBl2PowerDomainTransportRespondsAndSignalsAckBit)
{
    const std::string bridge_trace_path =
        "/tmp/qbox-mhu320ae-channel-trace.log";
    std::remove(bridge_trace_path.c_str());

    auto broker = cci::cci_get_global_broker(cci::cci_originator("mhu320ae_test"));
    broker.set_preset_cci_value("rse_si_pbx.pair", cci::cci_value(std::string("rse_si_cl0")));
    broker.set_preset_cci_value("rse_si_pbx.frame", cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("rse_si_pbx.tx_shmem", cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("rse_si_pbx.ack_bit", cci::cci_value(1u));
    broker.set_preset_cci_value("rse_si_pbx.assert_power_on_reset", cci::cci_value(true));
    broker.set_preset_cci_value("rse_si_pbx.power_domain_reset_count",
                                cci::cci_value(4u));
    broker.set_preset_cci_value("rse_si_pbx.power_domain_reset_pulse_on_power_on",
                                cci::cci_value(true));
    broker.set_preset_cci_value("rse_si_pbx.performance_domain_count",
                                cci::cci_value(4u));
    broker.set_preset_cci_value("rse_si_mbx.pair", cci::cci_value(std::string("rse_si_cl0")));
    broker.set_preset_cci_value("rse_si_mbx.frame", cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("ap_rse_pbx.pair", cci::cci_value(std::string("ap_rse")));
    broker.set_preset_cci_value("ap_rse_pbx.frame", cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("ap_rse_pbx.protocol", cci::cci_value(std::string("doorbell")));
    broker.set_preset_cci_value("ap_rse_pbx.direct_boot_compat",
                                cci::cci_value(true));
    broker.set_preset_cci_value("ap_rse_mbx.pair", cci::cci_value(std::string("ap_rse")));
    broker.set_preset_cci_value("ap_rse_mbx.frame", cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("ap_rse_mbx.protocol", cci::cci_value(std::string("doorbell")));
    broker.set_preset_cci_value("si_cl1_pbx.pair", cci::cci_value(std::string("ap_si_cl1")));
    broker.set_preset_cci_value("si_cl1_pbx.frame", cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("si_cl1_pbx.protocol", cci::cci_value(std::string("doorbell")));
    broker.set_preset_cci_value("si_cl1_pbx.channel_count",
                                cci::cci_value(32u));
    broker.set_preset_cci_value("si_cl1_pbx.doorbell_ack_trigger_channel",
                                cci::cci_value(0u));
    broker.set_preset_cci_value("si_cl1_pbx.doorbell_ack_trigger_value",
                                cci::cci_value(0x8u));
    broker.set_preset_cci_value("si_cl1_pbx.doorbell_ack_channel",
                                cci::cci_value(0u));
    broker.set_preset_cci_value("si_cl1_pbx.doorbell_ack_value",
                                cci::cci_value(0x4u));
    broker.set_preset_cci_value("si_cl1_pbx.doorbell_ack_seed_address",
                                cci::cci_value(SHMEM_BASE + 0x40));
    const std::vector<unsigned int> resource_table_seed = {
        0x00000001u, 0x00000001u,
        0x00000000u, 0x00000000u,
        0x00000014u, 0x00000003u,
        0x00000007u, 0x00000000u,
        0x00000001u, 0x00000000u,
        0x00000000u, 0x00000200u,
        0xffffffffu, 0x00000010u,
        0x00000020u, 0x00000000u,
        0x00000000u, 0xffffffffu,
        0x00000010u, 0x00000020u,
        0x00000001u, 0x00000000u,
    };
    for (size_t i = 0; i < resource_table_seed.size(); ++i) {
        broker.set_preset_cci_value(
            "si_cl1_pbx.doorbell_ack_seed_words." + std::to_string(i + 1),
            cci::cci_value(resource_table_seed[i]));
    }
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_enable",
                                cci::cci_value(true));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_name",
                                cci::cci_value(std::string("ethsi1")));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_remote_addr",
                                cci::cci_value(0x400u));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_vring_address",
                                cci::cci_value(RPMSG_TEST_VRING));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_vring_num",
                                cci::cci_value(2u));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_vring_align",
                                cci::cci_value(16u));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_signal_channel",
                                cci::cci_value(0u));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_signal_value",
                                cci::cci_value(0x1u));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_poll_period_ns",
                                cci::cci_value(1u));
    broker.set_preset_cci_value("si_cl1_pbx.rpmsg_ns_max_polls",
                                cci::cci_value(4u));
    broker.set_preset_cci_value("si_cl1_mbx.pair", cci::cci_value(std::string("ap_si_cl1")));
    broker.set_preset_cci_value("si_cl1_mbx.frame", cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("si_cl1_mbx.protocol", cci::cci_value(std::string("doorbell")));
    broker.set_preset_cci_value("si_cl1_mbx.channel_count",
                                cci::cci_value(32u));
    broker.set_preset_cci_value("deferred_pbx.pair", cci::cci_value(std::string("deferred")));
    broker.set_preset_cci_value("deferred_pbx.frame", cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("deferred_pbx.tx_shmem", cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("deferred_pbx.power_domain_reset_count",
                                cci::cci_value(2u));
    broker.set_preset_cci_value("deferred_pbx.power_domain_reset_assert_on_power_off",
                                cci::cci_value(false));
    broker.set_preset_cci_value("deferred_pbx.power_domain_reset_pulse_on_power_on",
                                cci::cci_value(true));
    broker.set_preset_cci_value("deferred_mbx.pair", cci::cci_value(std::string("deferred")));
    broker.set_preset_cci_value("deferred_mbx.frame", cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("bridge_ap_pbx.pair",
                                cci::cci_value(std::string("test_ap_to_rse")));
    broker.set_preset_cci_value("bridge_ap_pbx.frame", cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("bridge_ap_pbx.protocol",
                                cci::cci_value(std::string("doorbell-bridge")));
    broker.set_preset_cci_value("bridge_ap_pbx.tx_shmem",
                                cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("bridge_ap_pbx.scmi_channel_stride",
                                cci::cci_value(40u));
    broker.set_preset_cci_value("bridge_ap_pbx.scmi_channel_count",
                                cci::cci_value(4u));
    broker.set_preset_cci_value("bridge_ap_pbx.requester_hold_enable",
                                cci::cci_value(true));
    broker.set_preset_cci_value("bridge_ap_pbx.trace", cci::cci_value(true));
    broker.set_preset_cci_value("bridge_ap_pbx.trace_file",
                                cci::cci_value(bridge_trace_path));
    broker.set_preset_cci_value("bridge_rse_mbx.pair",
                                cci::cci_value(std::string("test_ap_to_rse")));
    broker.set_preset_cci_value("bridge_rse_mbx.frame", cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("bridge_rse_mbx.protocol",
                                cci::cci_value(std::string("doorbell-bridge")));
    broker.set_preset_cci_value("bridge_rse_mbx.doorbell_commit_on_notify",
                                cci::cci_value(true));
    broker.set_preset_cci_value("bridge_rse_mbx.tx_shmem",
                                cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("bridge_rse_mbx.scmi_channel_stride",
                                cci::cci_value(40u));
    broker.set_preset_cci_value("bridge_rse_mbx.scmi_channel_count",
                                cci::cci_value(4u));
    broker.set_preset_cci_value("bridge_rse_mbx.trace", cci::cci_value(true));
    broker.set_preset_cci_value("bridge_rse_mbx.trace_file",
                                cci::cci_value(bridge_trace_path));
    broker.set_preset_cci_value("bridge_rse_pbx.pair",
                                cci::cci_value(std::string("test_rse_to_ap")));
    broker.set_preset_cci_value("bridge_rse_pbx.frame", cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("bridge_rse_pbx.protocol",
                                cci::cci_value(std::string("doorbell-bridge")));
    broker.set_preset_cci_value("bridge_ap_mbx.pair",
                                cci::cci_value(std::string("test_rse_to_ap")));
    broker.set_preset_cci_value("bridge_ap_mbx.frame", cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("bridge_ap_mbx.protocol",
                                cci::cci_value(std::string("doorbell-bridge")));
    broker.set_preset_cci_value("limited_pbx.pair",
                                cci::cci_value(std::string("limited")));
    broker.set_preset_cci_value("limited_pbx.frame",
                                cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("limited_pbx.protocol",
                                cci::cci_value(std::string("doorbell-bridge")));
    broker.set_preset_cci_value("limited_pbx.channel_count",
                                cci::cci_value(4u));
    broker.set_preset_cci_value("limited_pbx.feat_spt0",
                                cci::cci_value(0x55aau));
    broker.set_preset_cci_value("limited_pbx.feat_spt1",
                                cci::cci_value(0xaa55u));
    broker.set_preset_cci_value("limited_pbx.iidr",
                                cci::cci_value(0x11223344u));
    broker.set_preset_cci_value("limited_pbx.aidr",
                                cci::cci_value(0x55667788u));
    broker.set_preset_cci_value("strict_pbx.pair",
                                cci::cci_value(std::string("strict")));
    broker.set_preset_cci_value("strict_pbx.frame",
                                cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("strict_pbx.protocol",
                                cci::cci_value(std::string("doorbell")));
    broker.set_preset_cci_value("strict_mbx.pair",
                                cci::cci_value(std::string("strict")));
    broker.set_preset_cci_value("strict_mbx.frame",
                                cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("strict_mbx.protocol",
                                cci::cci_value(std::string("doorbell")));
    broker.set_preset_cci_value("pfdi_pbx.pair",
                                cci::cci_value(std::string("pfdi")));
    broker.set_preset_cci_value("pfdi_pbx.frame",
                                cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("pfdi_pbx.protocol",
                                cci::cci_value(std::string("scmi")));
    broker.set_preset_cci_value("pfdi_pbx.scmi_transport",
                                cci::cci_value(std::string("pfdi-monitor")));
    broker.set_preset_cci_value("pfdi_pbx.tx_shmem",
                                cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("pfdi_pbx.rx_shmem",
                                cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("pfdi_pbx.scmi_channel_stride",
                                cci::cci_value(40u));
    broker.set_preset_cci_value("pfdi_pbx.scmi_channel_base_index",
                                cci::cci_value(2u));
    broker.set_preset_cci_value("pfdi_pbx.scmi_channel_count",
                                cci::cci_value(4u));
    broker.set_preset_cci_value("ps_pbx.pair", cci::cci_value(std::string("ps")));
    broker.set_preset_cci_value("ps_pbx.frame", cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("ps_pbx.protocol",
                                cci::cci_value(std::string("rse-ps-proxy")));
    broker.set_preset_cci_value("ps_mbx.pair", cci::cci_value(std::string("ps")));
    broker.set_preset_cci_value("ps_mbx.frame", cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("ps_mbx.protocol",
                                cci::cci_value(std::string("rse-ps-proxy")));
    broker.set_preset_cci_value("ordered_reset_pbx.pair",
                                cci::cci_value(std::string("ordered_reset")));
    broker.set_preset_cci_value("ordered_reset_pbx.frame",
                                cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("ordered_reset_pbx.init_shmem",
                                cci::cci_value(false));
    broker.set_preset_cci_value("ordered_reset_pbx.tx_shmem",
                                cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("ordered_reset_pbx.rx_shmem",
                                cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("ordered_reset_pbx.power_domain_reset_count",
                                cci::cci_value(1u));
    broker.set_preset_cci_value("ordered_reset_pbx.power_domain_reset_delay_ns",
                                cci::cci_value(uint64_t(0)));
    broker.set_preset_cci_value("ordered_reset_pbx.power_domain_reset_pulse_on_power_on",
                                cci::cci_value(true));
    broker.set_preset_cci_value("reset_test_pbx.pair",
                                cci::cci_value(std::string("reset_test")));
    broker.set_preset_cci_value("reset_test_pbx.frame",
                                cci::cci_value(std::string("pbx")));
    broker.set_preset_cci_value("reset_test_pbx.protocol",
                                cci::cci_value(std::string("doorbell-bridge")));
    broker.set_preset_cci_value("reset_test_pbx.requester_hold_enable",
                                cci::cci_value(true));
    broker.set_preset_cci_value("reset_test_pbx.tx_shmem",
                                cci::cci_value(SHMEM_BASE));
    broker.set_preset_cci_value("reset_test_mbx.pair",
                                cci::cci_value(std::string("reset_test")));
    broker.set_preset_cci_value("reset_test_mbx.frame",
                                cci::cci_value(std::string("mbx")));
    broker.set_preset_cci_value("reset_test_mbx.protocol",
                                cci::cci_value(std::string("doorbell-bridge")));

    TestMemory shmem("si_scmi_shmem");
    TestMemory unused("unused_shmem");
    TestMemory ap_pbx_unused("ap_pbx_unused_shmem");
    TestMemory ap_unused("ap_unused_shmem");
    TestMemory si_cl1_pbx_unused("si_cl1_pbx_unused_shmem");
    TestMemory si_cl1_unused("si_cl1_unused_shmem");
    TestMemory deferred_shmem("deferred_shmem");
    TestMemory deferred_unused("deferred_unused_shmem");
    TestMemory bridge_ap_pbx_unused("bridge_ap_pbx_unused_shmem");
    TestMemory bridge_rse_mbx_unused("bridge_rse_mbx_unused_shmem");
    TestMemory bridge_rse_pbx_unused("bridge_rse_pbx_unused_shmem");
    TestMemory bridge_ap_mbx_unused("bridge_ap_mbx_unused_shmem");
    TestMemory limited_unused("limited_unused_shmem");
    TestMemory strict_pbx_unused("strict_pbx_unused_shmem");
    TestMemory strict_mbx_unused("strict_mbx_unused_shmem");
    TestMemory pfdi_shmem("pfdi_shmem");
    TestMemory ps_shmem("ps_shmem");
    TestMemory ps_mbx_unused("ps_mbx_unused_shmem");
    TestMemory ordered_reset_unused("ordered_reset_unused_shmem");
    TestMemory reset_test_pbx_memory("reset_test_pbx_memory");
    TestMemory reset_test_mbx_memory("reset_test_mbx_memory");
    TestInitiator pbx_bus("pbx_bus");
    TestInitiator mbx_bus("mbx_bus");
    TestInitiator ap_pbx_bus("ap_pbx_bus");
    TestInitiator ap_mbx_bus("ap_mbx_bus");
    TestInitiator si_cl1_pbx_bus("si_cl1_pbx_bus");
    TestInitiator si_cl1_mbx_bus("si_cl1_mbx_bus");
    TestInitiator deferred_pbx_bus("deferred_pbx_bus");
    TestInitiator deferred_mbx_bus("deferred_mbx_bus");
    TestInitiator bridge_ap_pbx_bus("bridge_ap_pbx_bus");
    TestInitiator bridge_rse_mbx_bus("bridge_rse_mbx_bus");
    TestInitiator bridge_rse_pbx_bus("bridge_rse_pbx_bus");
    TestInitiator bridge_ap_mbx_bus("bridge_ap_mbx_bus");
    TestInitiator limited_bus("limited_bus");
    TestInitiator strict_pbx_bus("strict_pbx_bus");
    TestInitiator strict_mbx_bus("strict_mbx_bus");
    TestInitiator pfdi_bus("pfdi_bus");
    TestInitiator ps_pbx_bus("ps_pbx_bus");
    TestInitiator ps_mbx_bus("ps_mbx_bus");
    TestInitiator ordered_reset_bus("ordered_reset_bus");
    TestInitiator reset_test_pbx_bus("reset_test_pbx_bus");
    TestInitiator reset_test_mbx_bus("reset_test_mbx_bus");
    TestableMhu320ae pbx("rse_si_pbx");
    mhu320ae mbx("rse_si_mbx");
    mhu320ae ap_pbx("ap_rse_pbx");
    mhu320ae ap_mbx("ap_rse_mbx");
    mhu320ae si_cl1_pbx("si_cl1_pbx");
    mhu320ae si_cl1_mbx("si_cl1_mbx");
    mhu320ae deferred_pbx("deferred_pbx");
    mhu320ae deferred_mbx("deferred_mbx");
    mhu320ae bridge_ap_pbx("bridge_ap_pbx");
    mhu320ae bridge_rse_mbx("bridge_rse_mbx");
    mhu320ae bridge_rse_pbx("bridge_rse_pbx");
    mhu320ae bridge_ap_mbx("bridge_ap_mbx");
    mhu320ae limited_pbx("limited_pbx");
    mhu320ae strict_pbx("strict_pbx");
    mhu320ae strict_mbx("strict_mbx");
    mhu320ae pfdi_pbx("pfdi_pbx");
    mhu320ae ps_pbx("ps_pbx");
    mhu320ae ps_mbx("ps_mbx");
    TestableMhu320ae ordered_reset_pbx("ordered_reset_pbx");
    mhu320ae reset_test_pbx("reset_test_pbx");
    mhu320ae reset_test_mbx("reset_test_mbx");
    ResetSink ap_reset("ap_reset");
    ResetSink system_reset("system_reset");
    ResetSink ap_domain1_reset("ap_domain1_reset");
    ResetSink ap_domain2_reset("ap_domain2_reset");
    ResetSink ap_domain3_reset("ap_domain3_reset");
    ResetSink deferred_domain1_reset("deferred_domain1_reset");
    ResetSink bridge_rse_mbx_irq("bridge_rse_mbx_irq");
    SignalSource reset_test_pbx_reset("reset_test_pbx_reset");
    SignalSource reset_test_mbx_reset("reset_test_mbx_reset");
    SignalSource bridge_ap_pbx_reset("bridge_ap_pbx_reset");
    SignalSource bridge_rse_mbx_reset("bridge_rse_mbx_reset");
    ResetSink reset_test_requester_hold("reset_test_requester_hold");
    ResetSink bridge_ap_requester0_hold("bridge_ap_requester0_hold");
    ResetSink bridge_ap_requester1_hold("bridge_ap_requester1_hold");
    ResetSink bridge_ap_requester2_hold("bridge_ap_requester2_hold");
    ResetSink bridge_ap_requester3_hold("bridge_ap_requester3_hold");
    ResetSink si_cl1_pbx_irq("si_cl1_pbx_irq");
    ResetSink ordered_reset_sink("ordered_reset_sink");
    ResetSink ordered_domain_reset_sink("ordered_domain_reset_sink");
    StartAction queue_ordered_reset("queue_ordered_reset", [&] {
        ordered_reset_pbx.queue_power_on_reset(true);
        ordered_reset_pbx.queue_power_on_reset(false);
        ordered_reset_unused.write32(SCMI_STATUS, 0);
        ordered_reset_unused.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
        ordered_reset_unused.write32(
            SCMI_HEADER,
            scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                        SCMI_MESSAGE_POWER_STATE_SET));
        ordered_reset_unused.write32(SCMI_PAYLOAD + sizeof(uint32_t), 0);
        ordered_reset_unused.write32(
            SCMI_PAYLOAD + sizeof(uint32_t) * 2,
            0x00000111);
        write32(ordered_reset_bus, DBCW_SET, 1);
    });

    pbx_bus.initiator_socket.bind(pbx.target_socket);
    mbx_bus.initiator_socket.bind(mbx.target_socket);
    ap_pbx_bus.initiator_socket.bind(ap_pbx.target_socket);
    ap_mbx_bus.initiator_socket.bind(ap_mbx.target_socket);
    si_cl1_pbx_bus.initiator_socket.bind(si_cl1_pbx.target_socket);
    si_cl1_mbx_bus.initiator_socket.bind(si_cl1_mbx.target_socket);
    deferred_pbx_bus.initiator_socket.bind(deferred_pbx.target_socket);
    deferred_mbx_bus.initiator_socket.bind(deferred_mbx.target_socket);
    bridge_ap_pbx_bus.initiator_socket.bind(bridge_ap_pbx.target_socket);
    bridge_rse_mbx_bus.initiator_socket.bind(bridge_rse_mbx.target_socket);
    bridge_rse_pbx_bus.initiator_socket.bind(bridge_rse_pbx.target_socket);
    bridge_ap_mbx_bus.initiator_socket.bind(bridge_ap_mbx.target_socket);
    limited_bus.initiator_socket.bind(limited_pbx.target_socket);
    strict_pbx_bus.initiator_socket.bind(strict_pbx.target_socket);
    strict_mbx_bus.initiator_socket.bind(strict_mbx.target_socket);
    pfdi_bus.initiator_socket.bind(pfdi_pbx.target_socket);
    ps_pbx_bus.initiator_socket.bind(ps_pbx.target_socket);
    ps_mbx_bus.initiator_socket.bind(ps_mbx.target_socket);
    ordered_reset_bus.initiator_socket.bind(ordered_reset_pbx.target_socket);
    reset_test_pbx_bus.initiator_socket.bind(reset_test_pbx.target_socket);
    reset_test_mbx_bus.initiator_socket.bind(reset_test_mbx.target_socket);
    pbx.initiator_socket.bind(shmem.target_socket);
    mbx.initiator_socket.bind(unused.target_socket);
    ap_pbx.initiator_socket.bind(ap_pbx_unused.target_socket);
    ap_mbx.initiator_socket.bind(ap_unused.target_socket);
    si_cl1_pbx.initiator_socket.bind(si_cl1_pbx_unused.target_socket);
    si_cl1_mbx.initiator_socket.bind(si_cl1_unused.target_socket);
    deferred_pbx.initiator_socket.bind(deferred_shmem.target_socket);
    deferred_mbx.initiator_socket.bind(deferred_unused.target_socket);
    bridge_ap_pbx.initiator_socket.bind(bridge_ap_pbx_unused.target_socket);
    bridge_rse_mbx.initiator_socket.bind(bridge_rse_mbx_unused.target_socket);
    bridge_rse_pbx.initiator_socket.bind(bridge_rse_pbx_unused.target_socket);
    bridge_ap_mbx.initiator_socket.bind(bridge_ap_mbx_unused.target_socket);
    limited_pbx.initiator_socket.bind(limited_unused.target_socket);
    strict_pbx.initiator_socket.bind(strict_pbx_unused.target_socket);
    strict_mbx.initiator_socket.bind(strict_mbx_unused.target_socket);
    pfdi_pbx.initiator_socket.bind(pfdi_shmem.target_socket);
    ps_pbx.initiator_socket.bind(ps_shmem.target_socket);
    ps_mbx.initiator_socket.bind(ps_mbx_unused.target_socket);
    ordered_reset_pbx.initiator_socket.bind(ordered_reset_unused.target_socket);
    reset_test_pbx.initiator_socket.bind(reset_test_pbx_memory.target_socket);
    reset_test_mbx.initiator_socket.bind(reset_test_mbx_memory.target_socket);
    pbx.power_on_reset.bind(ap_reset.reset);
    pbx.system_reset.bind(system_reset.reset);
    pbx.power_domain_reset[1].bind(ap_domain1_reset.reset);
    pbx.power_domain_reset[2].bind(ap_domain2_reset.reset);
    pbx.power_domain_reset[3].bind(ap_domain3_reset.reset);
    deferred_pbx.power_domain_reset[1].bind(deferred_domain1_reset.reset);
    bridge_rse_mbx.irq.bind(bridge_rse_mbx_irq.reset);
    reset_test_pbx_reset.signal.bind(reset_test_pbx.reset);
    reset_test_mbx_reset.signal.bind(reset_test_mbx.reset);
    bridge_ap_pbx_reset.signal.bind(bridge_ap_pbx.reset);
    bridge_rse_mbx_reset.signal.bind(bridge_rse_mbx.reset);
    reset_test_pbx.requester_hold[0].bind(reset_test_requester_hold.reset);
    bridge_ap_pbx.requester_hold[0].bind(bridge_ap_requester0_hold.reset);
    bridge_ap_pbx.requester_hold[1].bind(bridge_ap_requester1_hold.reset);
    bridge_ap_pbx.requester_hold[2].bind(bridge_ap_requester2_hold.reset);
    bridge_ap_pbx.requester_hold[3].bind(bridge_ap_requester3_hold.reset);
    si_cl1_pbx.irq.bind(si_cl1_pbx_irq.reset);
    ordered_reset_pbx.power_on_reset.bind(ordered_reset_sink.reset);
    ordered_reset_pbx.power_domain_reset[0].bind(
        ordered_domain_reset_sink.reset);

    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_TRUE(ap_reset.reset.read());
    EXPECT_TRUE(ordered_reset_sink.saw_asserted);
    EXPECT_FALSE(ordered_reset_sink.reset.read());
    EXPECT_TRUE(ordered_domain_reset_sink.saw_asserted);
    EXPECT_FALSE(ordered_domain_reset_sink.reset.read());
    EXPECT_FALSE(system_reset.reset.read());
    EXPECT_GE(read32(pbx_bus, DBCH_CFG0) + 1u, 2u);
    EXPECT_GE(read32(ap_pbx_bus, DBCH_CFG0) + 1u, 2u);
    EXPECT_EQ(read32(ap_pbx_bus, DBCH_CFG0), read32(ap_mbx_bus, DBCH_CFG0));
    EXPECT_EQ(read32(limited_bus, DBCH_CFG0), 3u);
    EXPECT_EQ(read32(limited_bus, CTRL_FEAT_SPT0), 0x55aau);
    EXPECT_EQ(read32(limited_bus, CTRL_FEAT_SPT1), 0xaa55u);
    EXPECT_EQ(read32(limited_bus, CTRL_IIDR), 0x11223344u);
    EXPECT_EQ(read32(limited_bus, CTRL_AIDR), 0x55667788u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCH_CFG0), 31u);
    EXPECT_EQ(read32(si_cl1_mbx_bus, DBCH_CFG0), 31u);
    const uint64_t si_cl1_last_channel = 0x1000 + (31 * DBCW_STRIDE);
    const uint64_t si_cl1_out_of_model_channel = 0x1000 + (32 * DBCW_STRIDE);
    write32(si_cl1_pbx_bus,
            si_cl1_last_channel + (DBCW_INT_EN - DBCW_ST),
            0x1u);
    write32(si_cl1_pbx_bus,
            si_cl1_last_channel + (DBCW_SET - DBCW_ST),
            0x10u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, si_cl1_last_channel), 0x10u);
    write32(si_cl1_mbx_bus,
            si_cl1_last_channel + (DBCW_CLR - DBCW_ST),
            0x10u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, si_cl1_last_channel), 0u);
    EXPECT_EQ(read32(si_cl1_pbx_bus,
                     si_cl1_last_channel + (DBCW_INT_ST - DBCW_ST)) &
                  0x1u,
              0x1u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, CTRL_DBCH_INT_ST0), 0x80000000u);
    write32(si_cl1_pbx_bus,
            si_cl1_last_channel + (DBCW_INT_CLR - DBCW_ST),
            0x1u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, CTRL_DBCH_INT_ST0), 0u);
    write32(si_cl1_pbx_bus,
            si_cl1_out_of_model_channel + (DBCW_SET - DBCW_ST),
            0x20u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, si_cl1_out_of_model_channel), 0u);
    pfdi_shmem.write32(SCMI_STATUS, 0);
    pfdi_shmem.write32(SCMI_LENGTH, sizeof(uint32_t) - 1);
    pfdi_shmem.write32(SCMI_HEADER,
                       scmi_header(SCMI_PROTOCOL_PFDI_MONITOR,
                                   SCMI_MESSAGE_PROTOCOL_VERSION));
    write32(pfdi_bus, 0x1000 + (2 * DBCW_STRIDE) + (DBCW_SET - DBCW_ST),
            0x1u);
    EXPECT_EQ(pfdi_shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(pfdi_shmem.read32(SCMI_LENGTH), 8u);
    EXPECT_EQ(pfdi_shmem.read32(SCMI_HEADER),
              scmi_header(SCMI_PROTOCOL_PFDI_MONITOR,
                          SCMI_MESSAGE_PROTOCOL_VERSION));
    EXPECT_EQ(pfdi_shmem.read32(SCMI_PAYLOAD), SCMI_PROTOCOL_ERROR);
    EXPECT_EQ(read32(pfdi_bus, 0x1000 + (2 * DBCW_STRIDE)), 0u);

    pfdi_shmem.write32(SCMI_STATUS, 0);
    pfdi_shmem.write32(SCMI_LENGTH, sizeof(uint32_t));
    pfdi_shmem.write32(SCMI_HEADER,
                       scmi_header(SCMI_PROTOCOL_PFDI_MONITOR,
                                   SCMI_MESSAGE_PROTOCOL_VERSION));
    write32(pfdi_bus, 0x1000 + (2 * DBCW_STRIDE) + (DBCW_SET - DBCW_ST),
            0x1u);
    EXPECT_EQ(pfdi_shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(pfdi_shmem.read32(SCMI_LENGTH), 12u);
    EXPECT_EQ(pfdi_shmem.read32(SCMI_HEADER),
              scmi_header(SCMI_PROTOCOL_PFDI_MONITOR,
                          SCMI_MESSAGE_PROTOCOL_VERSION));
    EXPECT_EQ(pfdi_shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(pfdi_shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)),
              SCMI_PFDI_MONITOR_VERSION);
    const uint32_t strict_notify_channel = read32(strict_pbx_bus, DBCH_CFG0);
    const uint64_t strict_notify_base = 0x1000 +
                                        (strict_notify_channel * DBCW_STRIDE);
    write32(strict_pbx_bus, strict_notify_base + (DBCW_SET - 0x1000),
            MHU_NOTIFY_VALUE);
    EXPECT_EQ(read32(strict_pbx_bus, strict_notify_base), MHU_NOTIFY_VALUE);
    EXPECT_EQ(read32(strict_mbx_bus, DBCW_ST), 0u);
    EXPECT_EQ(read32(strict_mbx_bus, strict_notify_base), 0u);

    write32(limited_bus, DBCW_SET, 0x1u);
    EXPECT_EQ(read32(limited_bus, DBCW_ST), 0x1u);
    EXPECT_EQ(read32(strict_mbx_bus, DBCW_ST), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t));
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                           SCMI_MESSAGE_PROTOCOL_VERSION));

    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 12u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 0x00020000u);
    EXPECT_EQ(read32(mbx_bus, DBCW_ST), 2u);
    EXPECT_EQ(read32(ap_mbx_bus, DBCW_ST), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t));
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_BASE,
                                           SCMI_MESSAGE_PROTOCOL_ATTRIBUTES));
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 12u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read8(SCMI_PAYLOAD + sizeof(uint32_t)), 4u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_BASE,
                                           SCMI_MESSAGE_DISCOVER_LIST_PROTOCOLS));
    shmem.write32(SCMI_PAYLOAD, 0);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 16u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 4u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2), 0x13121110u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t));
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PROTOCOL_VERSION));
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 12u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 0x00040000u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t));
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PROTOCOL_ATTRIBUTES));
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 24u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read16(SCMI_PAYLOAD + sizeof(uint32_t)), 4u);
    EXPECT_EQ(shmem.read16(SCMI_PAYLOAD + sizeof(uint32_t) + sizeof(uint16_t)), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 3), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 4), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PROTOCOL_MESSAGE_ATTRIBUTES));
    shmem.write32(SCMI_PAYLOAD, SCMI_MESSAGE_PERF_DESCRIBE_LEVELS);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 12u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_DOMAIN_ATTRIBUTES));
    shmem.write32(SCMI_PAYLOAD, 0);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 40u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 0xf0000000u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 3), 2500000u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 4), 2500u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 3);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_DESCRIBE_LEVELS));
    shmem.write32(SCMI_PAYLOAD, 0);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 0);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 72u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read16(SCMI_PAYLOAD + sizeof(uint32_t)), 3u);
    EXPECT_EQ(shmem.read16(SCMI_PAYLOAD + sizeof(uint32_t) + sizeof(uint16_t)), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2), 1800u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2 + 12), 1800000u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2 + 16), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2 + 40), 2500u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2 + 56), 2u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LIMITS_GET));
    shmem.write32(SCMI_PAYLOAD, 0);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 16u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 2500u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t) * 2), 1800u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 3);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LEVEL_SET));
    shmem.write32(SCMI_PAYLOAD, 0);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 2000);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 8u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LEVEL_GET));
    shmem.write32(SCMI_PAYLOAD, 0);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 12u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 2000u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 3);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LEVEL_SET));
    shmem.write32(SCMI_PAYLOAD, 3);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 1800);
    write32(pbx_bus, DBCW_SET, 2);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LEVEL_GET));
    shmem.write32(SCMI_PAYLOAD, 3);
    write32(pbx_bus, DBCW_SET, 2);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 1800u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LIMITS_SET));
    shmem.write32(SCMI_PAYLOAD, 3);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 2000);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 1800);
    write32(pbx_bus, DBCW_SET, 2);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LIMITS_SET));
    shmem.write32(SCMI_PAYLOAD, 3);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 1800);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 2500);
    write32(pbx_bus, DBCW_SET, 2);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0xfffffffeu);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 3);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LEVEL_SET));
    shmem.write32(SCMI_PAYLOAD, 3);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 2500);
    write32(pbx_bus, DBCW_SET, 2);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0xfffffffeu);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_PERFORMANCE,
                                           SCMI_MESSAGE_PERF_LEVEL_GET));
    shmem.write32(SCMI_PAYLOAD, 0);
    write32(pbx_bus, DBCW_SET, 2);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 2000u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                           SCMI_MESSAGE_POWER_STATE_SET));
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 2);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 0x00000111);
    write32(pbx_bus, DBCW_SET, 2);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_FALSE(ap_reset.reset.read());
    EXPECT_FALSE(ap_domain1_reset.reset.read());
    EXPECT_FALSE(ap_domain2_reset.reset.read());
    EXPECT_FALSE(ap_domain3_reset.reset.read());

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                           SCMI_MESSAGE_POWER_STATE_GET));
    shmem.write32(SCMI_PAYLOAD, 2);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 0x00000111u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_SYS_POWER,
                                           SCMI_MESSAGE_PROTOCOL_MESSAGE_ATTRIBUTES));
    shmem.write32(SCMI_PAYLOAD, SCMI_MESSAGE_SYS_POWER_STATE_NOTIFY);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 12u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD + sizeof(uint32_t)), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 2);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_SYS_POWER,
                                           SCMI_MESSAGE_SYS_POWER_STATE_NOTIFY));
    shmem.write32(SCMI_PAYLOAD, 1);
    write32(pbx_bus, DBCW_SET, 2);

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 8u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 3);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_SYS_POWER,
                                           SCMI_MESSAGE_SYS_POWER_STATE_SET));
    shmem.write32(SCMI_PAYLOAD, 1);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), SCMI_SYS_POWER_COLD_RESET);
    write32(pbx_bus, DBCW_SET, 2);
    sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_NS));

    EXPECT_EQ(shmem.read32(SCMI_STATUS), 1u);
    EXPECT_EQ(shmem.read32(SCMI_LENGTH), 8u);
    EXPECT_EQ(shmem.read32(SCMI_PAYLOAD), 0u);
    EXPECT_FALSE(system_reset.reset.read());
    EXPECT_TRUE(system_reset.saw_asserted);
    EXPECT_GE(system_reset.write_count, 2u);

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                           SCMI_MESSAGE_POWER_STATE_SET));
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 2);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 0);
    write32(pbx_bus, DBCW_SET, 2);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_TRUE(ap_domain2_reset.reset.read());

    shmem.write32(SCMI_STATUS, 0);
    shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
    shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                           SCMI_MESSAGE_POWER_STATE_SET));
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 2);
    shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 0x00000111);
    write32(pbx_bus, DBCW_SET, 2);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_FALSE(ap_domain2_reset.reset.read());

    ap_domain1_reset.write_count = 0;
    ap_domain1_reset.saw_asserted = false;
    ap_domain2_reset.write_count = 0;
    ap_domain2_reset.saw_asserted = false;
    ap_domain3_reset.write_count = 0;
    ap_domain3_reset.saw_asserted = false;
    for (uint32_t domain_id = 1; domain_id <= 3; ++domain_id) {
        shmem.write32(SCMI_STATUS, 0);
        shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
        shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                               SCMI_MESSAGE_POWER_STATE_SET));
        shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), domain_id);
        shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 0x00000111);
        write32(pbx_bus, DBCW_SET, 2);
    }
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_TRUE(ap_domain1_reset.saw_asserted);
    EXPECT_TRUE(ap_domain2_reset.saw_asserted);
    EXPECT_TRUE(ap_domain3_reset.saw_asserted);
    EXPECT_EQ(ap_domain1_reset.write_count, 2u);
    EXPECT_EQ(ap_domain2_reset.write_count, 2u);
    EXPECT_EQ(ap_domain3_reset.write_count, 2u);
    EXPECT_FALSE(ap_domain1_reset.reset.read());
    EXPECT_FALSE(ap_domain2_reset.reset.read());
    EXPECT_FALSE(ap_domain3_reset.reset.read());

    deferred_shmem.write32(SCMI_STATUS, 0);
    deferred_shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
    deferred_shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                                    SCMI_MESSAGE_POWER_STATE_SET));
    deferred_shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 1);
    deferred_shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 0);
    write32(deferred_pbx_bus, DBCW_SET, 1);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_FALSE(deferred_domain1_reset.reset.read());
    EXPECT_FALSE(deferred_domain1_reset.saw_asserted);

    deferred_shmem.write32(SCMI_STATUS, 0);
    deferred_shmem.write32(SCMI_LENGTH, sizeof(uint32_t) * 4);
    deferred_shmem.write32(SCMI_HEADER, scmi_header(SCMI_PROTOCOL_POWER_DOMAIN,
                                                    SCMI_MESSAGE_POWER_STATE_SET));
    deferred_shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t), 1);
    deferred_shmem.write32(SCMI_PAYLOAD + sizeof(uint32_t) * 2, 0x00010011);
    write32(deferred_pbx_bus, DBCW_SET, 1);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_FALSE(deferred_domain1_reset.reset.read());
    EXPECT_TRUE(deferred_domain1_reset.saw_asserted);
    EXPECT_GE(deferred_domain1_reset.write_count, 2u);

    const uint32_t notify_channel = read32(ap_pbx_bus, DBCH_CFG0);
    const uint64_t notify_base = 0x1000 + (notify_channel * DBCW_STRIDE);

    write32(ap_pbx_bus, DBCW_SET, 12);
    write32(ap_pbx_bus, DBCW_SET + DBCW_STRIDE, 0x12340700);
    write32(ap_pbx_bus, DBCW_SET + (2 * DBCW_STRIDE), 0x11223344);
    write32(ap_pbx_bus, DBCW_SET + (3 * DBCW_STRIDE), 0x1);
    write32(ap_pbx_bus, notify_base + (DBCW_SET - 0x1000), MHU_NOTIFY_VALUE);

    EXPECT_EQ(read32(ap_pbx_bus, notify_base), 0u);
    EXPECT_EQ(read32(ap_mbx_bus, DBCW_ST), 16u);
    EXPECT_EQ(read32(ap_mbx_bus, DBCW_ST + DBCW_STRIDE), 0x12340700u);
    EXPECT_EQ(read32(ap_mbx_bus, DBCW_ST + (2 * DBCW_STRIDE)), 0u);
    EXPECT_EQ(read32(ap_mbx_bus, DBCW_ST + (3 * DBCW_STRIDE)), 0u);
    EXPECT_EQ(read32(ap_mbx_bus, DBCW_ST + (4 * DBCW_STRIDE)), 0u);
    EXPECT_EQ(read32(ap_mbx_bus, notify_base), MHU_NOTIFY_VALUE);

    write32(ap_mbx_bus, notify_base + (DBCW_CLR - 0x1000), MHU_NOTIFY_VALUE);
    EXPECT_EQ(read32(ap_mbx_bus, notify_base), 0u);

    const uint32_t ps_notify_channel = read32(ps_pbx_bus, DBCH_CFG0);
    const uint64_t ps_uid = 0x1122334455667788ULL;
    const auto ps_uid_bytes = le64_bytes(ps_uid);
    const auto ps_flags = le32_bytes(0);
    const std::vector<uint8_t> ps_data = {0x10, 0x21, 0x32, 0x43};

    std::vector<uint8_t> measurement_metadata(44, 0);
    measurement_metadata[0] = 8;
    measurement_metadata[1] = 1;
    measurement_metadata[4] = 0x09;
    measurement_metadata[5] = 0x00;
    measurement_metadata[6] = 0x00;
    measurement_metadata[7] = 0x02;
    const std::string sw_type = "FW_CONFIG";
    std::copy(sw_type.begin(), sw_type.end(), measurement_metadata.begin() + 8);
    measurement_metadata[40] = static_cast<uint8_t>(sw_type.size());
    write_doorbell_message(
        ps_pbx_bus, ps_notify_channel,
        build_embed_msg(TFM_MEASURED_BOOT_HANDLE, 0x40,
                        TFM_MEASURED_BOOT_EXTEND,
                        {measurement_metadata, std::vector<uint8_t>(32, 0x5a),
                         {}, std::vector<uint8_t>(32, 0xa5)},
                        {}));
    auto ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 16u);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);

    write_doorbell_message(
        ps_pbx_bus, ps_notify_channel,
        build_embed_msg(TFM_FIRMWARE_UPDATE_SERVICE_HANDLE, 0x51,
                        TFM_FWU_QUERY, {std::vector<uint8_t>{3}},
                        {PSA_FWU_COMPONENT_INFO_SIZE}));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 16u + PSA_FWU_COMPONENT_INFO_SIZE);
    EXPECT_EQ(ps_reply[0], RSE_COMMS_PROTOCOL_EMBED);
    EXPECT_EQ(ps_reply[1], 0x51u);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);
    EXPECT_EQ(read_le32(ps_reply, 8) & 0xffffu, PSA_FWU_COMPONENT_INFO_SIZE);
    EXPECT_EQ(ps_reply[16], 0u);
    EXPECT_EQ(read_le32(ps_reply, 16 + 16), 0x00240000u);
    EXPECT_EQ(read_le32(ps_reply, 16 + 20), 0x1u);
    EXPECT_EQ(read_le32(ps_reply, 16 + 24), 0x00007000u);
    EXPECT_EQ(read_le32(ps_reply, 16 + 28), 0x1u);
    EXPECT_EQ(read_le32(ps_reply, 16 + 32), 0x01000000u);

    write_doorbell_message(
        ps_pbx_bus, ps_notify_channel,
        build_embed_msg(TFM_FIRMWARE_UPDATE_SERVICE_HANDLE, 0x52,
                        TFM_FWU_QUERY, {std::vector<uint8_t>{5}},
                        {PSA_FWU_COMPONENT_INFO_SIZE}));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 16u);
    EXPECT_EQ(ps_reply[1], 0x52u);
    EXPECT_EQ(read_le32(ps_reply, 4), PSA_ERROR_DOES_NOT_EXIST);

    ps_shmem.write_bytes(0x240, std::vector<uint8_t>{4});
    write_doorbell_message(
        ps_pbx_bus, ps_notify_channel,
        build_pointer_msg(TFM_FIRMWARE_UPDATE_SERVICE_HANDLE, 0x53,
                          TFM_FWU_QUERY, {1, PSA_FWU_COMPONENT_INFO_SIZE, 0, 0},
                          {SHMEM_BASE + 0x240, SHMEM_BASE + 0x260, 0, 0},
                          1, 1));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 24u);
    EXPECT_EQ(ps_reply[0], RSE_COMMS_PROTOCOL_POINTER_ACCESS);
    EXPECT_EQ(ps_reply[1], 0x53u);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);
    EXPECT_EQ(read_le32(ps_reply, 8), PSA_FWU_COMPONENT_INFO_SIZE);
    EXPECT_EQ(ps_shmem.read8(0x260), 0u);
    EXPECT_EQ(ps_shmem.read32(0x260 + 16), 0x00100000u);
    EXPECT_EQ(ps_shmem.read32(0x260 + 24), 0x00167000u);
    EXPECT_EQ(ps_shmem.read32(0x260 + 28), 0x1u);

    write_doorbell_message(ps_pbx_bus, ps_notify_channel,
                           build_embed_ps_msg(0x41, TFM_PS_REMOVE,
                                              {ps_uid_bytes}, {}));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 16u);
    EXPECT_EQ(ps_reply[0], RSE_COMMS_PROTOCOL_EMBED);
    EXPECT_EQ(ps_reply[1], 0x41u);
    EXPECT_EQ(read_le32(ps_reply, 4), PSA_ERROR_DOES_NOT_EXIST);

    write_doorbell_message(ps_pbx_bus, ps_notify_channel,
                           build_embed_ps_msg(0x42, TFM_PS_SET,
                                              {ps_uid_bytes, ps_data, ps_flags}, {}));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 16u);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);

    write_doorbell_message(ps_pbx_bus, ps_notify_channel,
                           build_embed_ps_msg(0x43, TFM_PS_GET_INFO,
                                              {ps_uid_bytes}, {12}));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 28u);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);
    EXPECT_EQ(read_le32(ps_reply, 8), 12u);
    EXPECT_EQ(read_le32(ps_reply, 16), ps_data.size());
    EXPECT_EQ(read_le32(ps_reply, 20), ps_data.size());
    EXPECT_EQ(read_le32(ps_reply, 24), 0u);

    write_doorbell_message(ps_pbx_bus, ps_notify_channel,
                           build_embed_ps_msg(0x44, TFM_PS_GET,
                                              {ps_uid_bytes, le32_bytes(0)}, {32}));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 20u);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);
    EXPECT_EQ(read_le32(ps_reply, 8), ps_data.size());
    EXPECT_EQ(std::vector<uint8_t>(ps_reply.begin() + 16,
                                   ps_reply.begin() + 16 + ps_data.size()),
              ps_data);

    const std::vector<uint8_t> pointer_data(64, 0xa5);
    ps_shmem.write_bytes(0x100, ps_uid_bytes);
    ps_shmem.write_bytes(0x120, pointer_data);
    ps_shmem.write_bytes(0x180, ps_flags);
    write_doorbell_message(
        ps_pbx_bus, ps_notify_channel,
        build_pointer_ps_msg(0x45, TFM_PS_SET,
                             {8, static_cast<uint32_t>(pointer_data.size()), 4, 0},
                             {SHMEM_BASE + 0x100, SHMEM_BASE + 0x120,
                              SHMEM_BASE + 0x180, 0},
                             3, 0));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 24u);
    EXPECT_EQ(ps_reply[0], RSE_COMMS_PROTOCOL_POINTER_ACCESS);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);

    ps_shmem.write32(0x1c0, 0);
    write_doorbell_message(
        ps_pbx_bus, ps_notify_channel,
        build_pointer_ps_msg(0x46, TFM_PS_GET,
                             {8, 4, static_cast<uint32_t>(pointer_data.size()), 0},
                             {SHMEM_BASE + 0x100, SHMEM_BASE + 0x1c0,
                              SHMEM_BASE + 0x200, 0},
                             2, 1));
    ps_reply = read_doorbell_message(ps_mbx_bus, ps_notify_channel);
    ASSERT_GE(ps_reply.size(), 24u);
    EXPECT_EQ(read_le32(ps_reply, 4), 0u);
    EXPECT_EQ(read_le32(ps_reply, 8), pointer_data.size());
    EXPECT_EQ(ps_shmem.read_bytes(0x200, pointer_data.size()), pointer_data);

    const uint32_t bridge_notify_channel = read32(bridge_ap_pbx_bus, DBCH_CFG0);
    const uint64_t bridge_notify_base = 0x1000 + (bridge_notify_channel * DBCW_STRIDE);

    EXPECT_FALSE(bridge_rse_mbx.runtime_drop_next_doorbell(0));
    EXPECT_FALSE(bridge_ap_pbx.runtime_drop_next_doorbell(
        read32(bridge_ap_pbx_bus, DBCH_CFG0) + 1u));
    EXPECT_TRUE(bridge_ap_pbx.runtime_drop_next_doorbell(1));
    bridge_ap_pbx.runtime_clear_doorbell_fault();
    EXPECT_FALSE(bridge_ap_pbx.runtime_doorbell_fault_snapshot().armed);
    EXPECT_TRUE(bridge_ap_pbx.runtime_drop_next_doorbell(2));
    write32(bridge_ap_pbx_bus, DBCW_SET + (2 * DBCW_STRIDE), 0x20u);
    auto doorbell_fault = bridge_ap_pbx.runtime_doorbell_fault_snapshot();
    EXPECT_FALSE(doorbell_fault.armed);
    EXPECT_EQ(doorbell_fault.channel, 2u);
    EXPECT_EQ(doorbell_fault.match_count, 1u);
    EXPECT_EQ(read32(bridge_ap_pbx_bus, DBCW_ST + (2 * DBCW_STRIDE)),
              0x20u);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST + (2 * DBCW_STRIDE)), 0u);

    bridge_ap_pbx_reset.write(true);
    bridge_rse_mbx_reset.write(true);
    bridge_ap_pbx_reset.write(false);
    bridge_rse_mbx_reset.write(false);
    write32(bridge_ap_pbx_bus, DBCW_SET + (2 * DBCW_STRIDE), 0x40u);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST + (2 * DBCW_STRIDE)),
              0x40u);
    write32(bridge_rse_mbx_bus, DBCW_CLR + (2 * DBCW_STRIDE), 0xffffffffu);

    EXPECT_TRUE(bridge_ap_pbx.runtime_drop_next_doorbell(3));
    bridge_ap_pbx_reset.write(true);
    doorbell_fault = bridge_ap_pbx.runtime_doorbell_fault_snapshot();
    EXPECT_FALSE(doorbell_fault.armed);
    EXPECT_EQ(doorbell_fault.match_count, 1u);
    bridge_ap_pbx_reset.write(false);
    write32(bridge_ap_pbx_bus, DBCW_SET + (3 * DBCW_STRIDE), 0x80u);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST + (3 * DBCW_STRIDE)),
              0x80u);
    write32(bridge_rse_mbx_bus, DBCW_CLR + (3 * DBCW_STRIDE), 0xffffffffu);
    bridge_ap_pbx_reset.write(true);
    bridge_rse_mbx_reset.write(true);
    bridge_ap_pbx_reset.write(false);
    bridge_rse_mbx_reset.write(false);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_FALSE(bridge_rse_mbx_irq.reset.read());

    const auto bridge_irq_write_count = bridge_rse_mbx_irq.write_count;

    bridge_ap_pbx_unused.write32(40 + SCMI_STATUS, 0x55aau);
    bridge_rse_mbx_unused.write32(40 + SCMI_STATUS, 0x1u);
    write32(bridge_ap_pbx_bus, DBCW_SET, 8);
    write32_as_requester(bridge_ap_pbx_bus, DBCW_SET + DBCW_STRIDE,
                         0x44556677, 3);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(bridge_rse_mbx_irq.write_count, bridge_irq_write_count);
    EXPECT_FALSE(bridge_rse_mbx_irq.reset.read());

    write32(bridge_ap_pbx_bus, bridge_notify_base + (DBCW_SET - 0x1000),
            MHU_NOTIFY_VALUE);

    EXPECT_EQ(bridge_rse_mbx_irq.write_count, bridge_irq_write_count);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST), 8u);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST + DBCW_STRIDE), 0x44556677u);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, bridge_notify_base), MHU_NOTIFY_VALUE);
    EXPECT_EQ(read32(bridge_ap_mbx_bus, DBCW_ST), 0u);
    EXPECT_TRUE(bridge_rse_mbx_irq.reset.read());
    const auto irq_write_count = bridge_rse_mbx_irq.write_count;
    write32(bridge_rse_mbx_bus, CTRL, 1u);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(bridge_rse_mbx_irq.write_count, irq_write_count);

    write32(bridge_rse_mbx_bus, DBCW_CLR, 0xffffffffu);
    write32(bridge_rse_mbx_bus, DBCW_CLR + DBCW_STRIDE, 0xffffffffu);
    write32(bridge_rse_mbx_bus, bridge_notify_base + (DBCW_CLR - 0x1000),
            0xffffffffu);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST), 0u);
    EXPECT_EQ(read32(bridge_ap_pbx_bus, DBCW_ST), 0u);
    EXPECT_EQ(read32(bridge_ap_pbx_bus, bridge_notify_base), 0u);
    EXPECT_FALSE(bridge_rse_mbx_irq.reset.read())
        << "mailbox clear must deassert IRQ before its MMIO write returns";

    EXPECT_FALSE(bridge_ap_requester1_hold.saw_asserted);
    EXPECT_TRUE(bridge_ap_requester3_hold.saw_asserted);
    EXPECT_TRUE(bridge_ap_requester3_hold.reset.read());
    const auto requester_hold_writes = bridge_ap_requester3_hold.write_count;
    sc_core::sc_start(sc_core::sc_time(20, sc_core::SC_US));
    EXPECT_EQ(bridge_ap_requester3_hold.write_count,
              requester_hold_writes);
    bridge_ap_pbx_unused.write32(40 + SCMI_STATUS, 0x1u);
    sc_core::sc_start(sc_core::sc_time(20, sc_core::SC_US));
    EXPECT_FALSE(bridge_ap_requester3_hold.reset.read());
    EXPECT_GT(bridge_ap_requester3_hold.write_count,
              requester_hold_writes);

    std::ifstream bridge_trace(bridge_trace_path);
    ASSERT_TRUE(bridge_trace);
    std::ostringstream bridge_trace_text;
    bridge_trace_text << bridge_trace.rdbuf();
    EXPECT_NE(bridge_trace_text.str().find(
                  "shmem=0x4000040000028 status=0x55aa"),
              std::string::npos);
    EXPECT_NE(bridge_trace_text.str().find(
                  "channel=1 requester=3 value=0x44556677"),
              std::string::npos);
    EXPECT_NE(bridge_trace_text.str().find(
                  "channel=1 mask=0xffffffff status=0x0 "
                  "shmem=0x4000040000028 channel_status=0x1"),
              std::string::npos);

    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_FALSE(bridge_rse_mbx_irq.reset.read());
    bridge_rse_mbx_irq.write_count = 0;
    bridge_rse_mbx_irq.saw_asserted = false;

    write32(bridge_ap_pbx_bus, bridge_notify_base + (DBCW_SET - 0x1000),
            MHU_NOTIFY_VALUE);
    write32(bridge_rse_mbx_bus, bridge_notify_base + (DBCW_CLR - 0x1000),
            MHU_NOTIFY_VALUE);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, bridge_notify_base), 0u);
    EXPECT_FALSE(bridge_rse_mbx_irq.reset.read());

    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_FALSE(bridge_rse_mbx_irq.reset.read());
    EXPECT_FALSE(bridge_rse_mbx_irq.saw_asserted);
    EXPECT_LE(bridge_rse_mbx_irq.write_count, 1u);

    bridge_rse_mbx_irq.arm_assert_clear_race();
    std::atomic<bool> assert_callback_seen(false);
    std::thread irq_clearer([&] {
        assert_callback_seen.store(
            bridge_rse_mbx_irq.wait_for_assert_callback(),
            std::memory_order_release);
        if (!assert_callback_seen.load(std::memory_order_acquire)) {
            return;
        }
        bridge_rse_mbx_irq.mark_clear_started();
        write32(bridge_rse_mbx_bus,
                bridge_notify_base + (DBCW_CLR - 0x1000),
                MHU_NOTIFY_VALUE);
        bridge_rse_mbx_irq.mark_clear_returned();
    });
    write32(bridge_ap_pbx_bus,
            bridge_notify_base + (DBCW_SET - 0x1000), MHU_NOTIFY_VALUE);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    irq_clearer.join();
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_TRUE(assert_callback_seen.load(std::memory_order_acquire));
    EXPECT_FALSE(bridge_rse_mbx_irq.clear_returned_during_assert())
        << "mailbox clear returned while an older IRQ assertion was in flight";
    EXPECT_FALSE(bridge_rse_mbx_irq.reset.read());

    std::atomic<bool> start_concurrent_access(false);
    std::thread postbox_writer([&] {
        while (!start_concurrent_access.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (unsigned int i = 0; i < 1024; ++i) {
            access32(bridge_ap_pbx_bus, DBCW_SET, tlm::TLM_WRITE_COMMAND,
                     0x1u);
        }
    });
    std::thread mailbox_clearer([&] {
        while (!start_concurrent_access.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (unsigned int i = 0; i < 1024; ++i) {
            access32(bridge_rse_mbx_bus, DBCW_CLR, tlm::TLM_WRITE_COMMAND,
                     0x1u);
        }
    });
    start_concurrent_access.store(true, std::memory_order_release);
    postbox_writer.join();
    mailbox_clearer.join();
    write32(bridge_rse_mbx_bus, DBCW_CLR, 0x1u);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST) & 0x1u, 0u);
    EXPECT_EQ(read32(bridge_ap_pbx_bus, DBCW_ST) & 0x1u, 0u);

    write32(bridge_rse_pbx_bus, DBCW_SET, 4);
    write32(bridge_rse_pbx_bus, DBCW_SET + DBCW_STRIDE, 0x89abcdef);
    write32(bridge_rse_pbx_bus, bridge_notify_base + (DBCW_SET - 0x1000),
            MHU_NOTIFY_VALUE);

    EXPECT_EQ(read32(bridge_ap_mbx_bus, DBCW_ST), 4u);
    EXPECT_EQ(read32(bridge_ap_mbx_bus, DBCW_ST + DBCW_STRIDE), 0x89abcdefu);
    EXPECT_EQ(read32(bridge_ap_mbx_bus, bridge_notify_base), MHU_NOTIFY_VALUE);
    EXPECT_EQ(read32(bridge_rse_mbx_bus, DBCW_ST), 0u);

    si_cl1_pbx_unused.write64(0x100, RPMSG_TEST_BUFFER);
    si_cl1_pbx_unused.write32(0x108, 0x100);
    si_cl1_pbx_unused.write16(0x10c, 0x2);
    si_cl1_pbx_unused.write16(0x10e, 0);
    si_cl1_pbx_unused.write16(0x122, 1);
    si_cl1_pbx_unused.write16(0x124, 0);

    write32(si_cl1_pbx_bus, DBCW_INT_EN, 0x1);
    EXPECT_FALSE(si_cl1_pbx_irq.reset.read());
    write32(si_cl1_mbx_bus, DBCW_CLR, 0xffffffffu);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0u);
    EXPECT_FALSE(si_cl1_pbx_irq.reset.read());

    write32(si_cl1_pbx_bus, DBCW_SET, 0x8);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_ST) & 0x8u, 0x8u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0u);
    EXPECT_FALSE(si_cl1_pbx_irq.reset.read());
    EXPECT_EQ(read32(si_cl1_mbx_bus, DBCW_ST) & 0x4u, 0x4u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x40), 0x00000001u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x44), 0x00000001u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x54), 0x00000003u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x58), 0x00000007u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x6c), 0x00000200u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x78), 0x00000020u);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_US));
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_ST) & 0x8u, 0u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0x1u);
    EXPECT_TRUE(si_cl1_pbx_irq.reset.read());
    EXPECT_TRUE(si_cl1_pbx_irq.saw_asserted);
    EXPECT_EQ(read32(si_cl1_mbx_bus, DBCW_ST) & 0x1u, 0u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x200), 0u);

    write32(si_cl1_pbx_bus, DBCW_INT_CLR, 0x1u);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0u);
    EXPECT_FALSE(si_cl1_pbx_irq.reset.read());

    si_cl1_pbx_unused.write16(0x124, 2);
    write32(si_cl1_pbx_bus, DBCW_SET, 0x1);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_ST) & 0x1u, 0x1u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0u);
    EXPECT_FALSE(si_cl1_pbx_irq.reset.read());
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_US));
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_ST) & 0x1u, 0u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0x1u);
    EXPECT_TRUE(si_cl1_pbx_irq.reset.read());
    EXPECT_EQ(read32(si_cl1_mbx_bus, DBCW_ST) & 0x1u, 0u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x200), 0u);

    write32(si_cl1_pbx_bus, DBCW_INT_CLR, 0x1u);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0u);
    EXPECT_FALSE(si_cl1_pbx_irq.reset.read());

    si_cl1_pbx_unused.write16(0x124, 0);
    write32(si_cl1_pbx_bus, DBCW_SET, 0x1);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_US));
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_ST) & 0x1u, 0u);
    EXPECT_EQ(read32(si_cl1_pbx_bus, DBCW_INT_ST) & 0x1u, 0x1u);
    EXPECT_TRUE(si_cl1_pbx_irq.reset.read());
    EXPECT_EQ(read32(si_cl1_mbx_bus, DBCW_ST) & 0x1u, 0x1u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x200), 0x400u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x204), 53u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x20c), 40u);
    EXPECT_EQ(si_cl1_pbx_unused.read8(0x210), 'e');
    EXPECT_EQ(si_cl1_pbx_unused.read8(0x211), 't');
    EXPECT_EQ(si_cl1_pbx_unused.read8(0x212), 'h');
    EXPECT_EQ(si_cl1_pbx_unused.read8(0x213), 's');
    EXPECT_EQ(si_cl1_pbx_unused.read8(0x214), 'i');
    EXPECT_EQ(si_cl1_pbx_unused.read8(0x215), '1');
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x230), 0x400u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x234), 0u);
    EXPECT_EQ(si_cl1_pbx_unused.read16(0x132), 1u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x134), 0u);
    EXPECT_EQ(si_cl1_pbx_unused.read32(0x138), 56u);
    write32(si_cl1_mbx_bus, DBCW_CLR, 0x5);
    EXPECT_EQ(read32(si_cl1_mbx_bus, DBCW_ST), 0u);

    reset_test_mbx_reset.write(true);
    reset_test_pbx_memory.write32(SCMI_STATUS, 0u);
    write32_as_requester(reset_test_pbx_bus, DBCW_SET, 0x1u, 0);
    EXPECT_EQ(read32(reset_test_pbx_bus, DBCW_ST), 0x1u);
    EXPECT_EQ(read32(reset_test_mbx_bus, DBCW_ST), 0u);
    EXPECT_TRUE(reset_test_requester_hold.reset.read());

    reset_test_mbx_reset.write(false);
    write32_as_requester(reset_test_pbx_bus, DBCW_SET, 0x1u, 0);
    EXPECT_EQ(read32(reset_test_mbx_bus, DBCW_ST), 0x1u);
    write32(reset_test_mbx_bus, DBCW_CLR, 0x1u);
    reset_test_pbx_memory.write32(SCMI_STATUS, 1u);
    sc_core::sc_start(sc_core::sc_time(20, sc_core::SC_US));
    EXPECT_EQ(read32(reset_test_pbx_bus, DBCW_ST), 0u);
    EXPECT_FALSE(reset_test_requester_hold.reset.read());

    reset_test_pbx_memory.write32(SCMI_STATUS, 0u);
    write32_as_requester(reset_test_pbx_bus, DBCW_SET, 0x2u, 0);
    EXPECT_EQ(read32(reset_test_pbx_bus, DBCW_ST), 0x2u);
    reset_test_pbx_reset.write(true);
    EXPECT_EQ(read32(reset_test_pbx_bus, DBCW_ST), 0u);
    EXPECT_FALSE(reset_test_requester_hold.reset.read());
    reset_test_pbx_reset.write(false);
    std::remove(bridge_trace_path.c_str());
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
