/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cci_configuration>
#include <cciutils.h>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/multiinitiator-signal-socket.h>
#include <scp/report.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class mhu320ae : public sc_core::sc_module
{
    SCP_LOGGER();

    static constexpr uint64_t REG_SPACE_SIZE = 0x30000;
    static constexpr uint64_t CTRL_BLK_ID = 0x000;
    static constexpr uint64_t CTRL_FEAT_SPT0 = 0x010;
    static constexpr uint64_t CTRL_FEAT_SPT1 = 0x014;
    static constexpr uint64_t CTRL_DBCH_CFG0 = 0x020;
    static constexpr uint64_t CTRL_DBCH_INT_ST0 = 0x400;
    static constexpr uint64_t CTRL_IIDR = 0xfc8;
    static constexpr uint64_t CTRL_AIDR = 0xfcc;

    static constexpr uint64_t DBCW0 = 0x1000;
    static constexpr uint64_t DBCW_STRIDE = 0x20;
    static constexpr unsigned int DBCH_CHANNELS = 128;
    static constexpr uint64_t DBCW_END = DBCW0 + (DBCW_STRIDE * DBCH_CHANNELS);
    static constexpr uint64_t DBCW_ST = 0x00;
    static constexpr uint64_t DBCW_ST_MSK = 0x04;
    static constexpr uint64_t DBCW_CLR = 0x08;
    static constexpr uint64_t DBCW_SET = 0x0c;
    static constexpr uint64_t DBCW_INT_ST = 0x10;
    static constexpr uint64_t DBCW_INT_CLR = 0x14;
    static constexpr uint64_t DBCW_INT_EN = 0x18;
    static constexpr uint64_t DBCW_CTRL = 0x1c;
    static constexpr unsigned int DBCW_NOTIFY_CHANNEL = DBCH_CHANNELS - 1;
    static constexpr uint32_t MHU_NOTIFY_VALUE = 1234;
    static constexpr uint64_t PBX_DBCW_ST = DBCW0 + 0x00;
    static constexpr uint64_t PBX_DBCW_SET = DBCW0 + 0x0c;
    static constexpr uint64_t PBX_DBCW_INT_ST = DBCW0 + 0x10;
    static constexpr uint64_t PBX_DBCW_INT_CLR = DBCW0 + 0x14;
    static constexpr uint64_t PBX_DBCW_INT_EN = DBCW0 + 0x18;
    static constexpr uint64_t PBX_DBCW_CTRL = DBCW0 + 0x1c;

    static constexpr uint64_t MBX_DBCW_ST = DBCW0 + 0x00;
    static constexpr uint64_t MBX_DBCW_ST_MSK = DBCW0 + 0x04;
    static constexpr uint64_t MBX_DBCW_CLR = DBCW0 + 0x08;
    static constexpr uint64_t MBX_DBCW_MSK_ST = DBCW0 + 0x10;
    static constexpr uint64_t MBX_DBCW_MSK_SET = DBCW0 + 0x14;
    static constexpr uint64_t MBX_DBCW_MSK_CLR = DBCW0 + 0x18;
    static constexpr uint64_t MBX_DBCW_CTRL = DBCW0 + 0x1c;

    static constexpr uint64_t SCMI_CHAN_STATUS = 0x04;
    static constexpr uint64_t SCMI_LENGTH = 0x14;
    static constexpr uint64_t SCMI_HEADER = 0x18;
    static constexpr uint64_t SCMI_PAYLOAD = 0x1c;
    static constexpr uint32_t SCMI_CHAN_FREE = 1u;
    static constexpr uint32_t SCMI_SUCCESS = 0;
    static constexpr uint32_t SCMI_ERR_SUPPORT = static_cast<uint32_t>(-1);
    static constexpr uint8_t SCMI_PROTOCOL_BASE = 0x10;
    static constexpr uint8_t SCMI_PROTOCOL_POWER_DOMAIN = 0x11;
    static constexpr uint8_t SCMI_PROTOCOL_SYS_POWER = 0x12;
    static constexpr uint8_t SCMI_PROTOCOL_PFDI_MONITOR = 0x90;
    static constexpr uint8_t SCMI_MSG_PROTOCOL_MESSAGE_ATTRIBUTES = 0x2;
    static constexpr uint8_t SCMI_SYS_POWER_STATE_SET = 0x3;
    static constexpr uint8_t SCMI_SYS_POWER_STATE_NOTIFY = 0x5;
    static constexpr uint32_t SCMI_SYS_POWER_SHUTDOWN = 0;
    static constexpr uint32_t SCMI_SYS_POWER_COLD_RESET = 1;
    static constexpr uint32_t SCMI_SYS_POWER_WARM_RESET = 2;
    static constexpr uint32_t SCMI_PFDI_MONITOR_VERSION = 0x00020000;
    static constexpr uint8_t RSE_COMMS_PROTOCOL_EMBED = 0;
    static constexpr uint8_t RSE_COMMS_PROTOCOL_POINTER_ACCESS = 1;
    static constexpr unsigned int PSA_MAX_IOVEC = 4;
    static constexpr uint32_t PSA_SUCCESS = 0;
    static constexpr uint32_t TFM_PROTECTED_STORAGE_SERVICE_HANDLE = 0x40000101;
    static constexpr uint32_t TFM_PS_SET = 1001;
    static constexpr uint32_t TFM_PS_GET = 1002;
    static constexpr uint32_t TFM_PS_GET_INFO = 1003;
    static constexpr uint32_t TFM_PS_REMOVE = 1004;
    static constexpr uint32_t TFM_PS_GET_SUPPORT = 1005;
    static constexpr uint32_t TFM_MEASURED_BOOT_HANDLE = 0x40000110;
    static constexpr uint32_t TFM_MEASURED_BOOT_READ = 1001;
    static constexpr uint32_t TFM_MEASURED_BOOT_EXTEND = 1002;
    static constexpr unsigned int TFM_MEASUREMENT_SLOT_COUNT = 32;
    static constexpr unsigned int TFM_MEASUREMENT_VALUE_MAX_SIZE = 64;
    static constexpr unsigned int TFM_SIGNER_ID_MAX_SIZE = 64;
    static constexpr unsigned int TFM_VERSION_MAX_SIZE = 14;
    static constexpr unsigned int TFM_SW_TYPE_MAX_SIZE = 32;
    static constexpr int32_t PSA_ERROR_NOT_SUPPORTED = -134;
    static constexpr int32_t PSA_ERROR_INVALID_ARGUMENT = -135;
    static constexpr int32_t PSA_ERROR_DOES_NOT_EXIST = -140;
    static constexpr int32_t PSA_ERROR_COMMUNICATION_FAILURE = -145;
    static constexpr uint16_t VRING_DESC_F_WRITE = 2;
    static constexpr uint64_t VRING_DESC_SIZE = 16;
    static constexpr uint64_t VRING_AVAIL_HEADER_SIZE = 4;
    static constexpr uint64_t VRING_USED_HEADER_SIZE = 4;
    static constexpr uint64_t VRING_USED_ELEM_SIZE = 8;
    static constexpr uint32_t RPMSG_NS_ADDR = 53;
    static constexpr uint32_t RPMSG_NS_CREATE = 0;
    static constexpr size_t RPMSG_NAME_SIZE = 32;
    static constexpr uint16_t RPMSG_NS_MSG_SIZE = RPMSG_NAME_SIZE + 8;
    static constexpr uint16_t RPMSG_HDR_SIZE = 16;

public:
    class mhu320ae_frame_model
    {
        std::array<uint8_t, REG_SPACE_SIZE> m_regs {};
        std::array<uint32_t, DBCH_CHANNELS> m_db_status {};
        std::array<uint32_t, DBCH_CHANNELS> m_db_mask {};
        std::array<uint32_t, DBCH_CHANNELS> m_db_int_status {};
        std::array<uint32_t, DBCH_CHANNELS> m_db_int_enable {};
        std::array<uint32_t, DBCH_CHANNELS> m_db_ctrl {};
        bool m_is_mbx = false;
        unsigned int m_channel_count = DBCH_CHANNELS;

    public:
        template <typename T>
        void store(uint64_t offset, T value)
        {
            if (offset + sizeof(T) <= m_regs.size()) {
                std::memcpy(&m_regs[offset], &value, sizeof(T));
            }
        }

        template <typename T>
        T load(uint64_t offset) const
        {
            T value {};
            if (offset + sizeof(T) <= m_regs.size()) {
                std::memcpy(&value, &m_regs[offset], sizeof(T));
            }
            return value;
        }

        void configure(bool is_mbx, unsigned int channel_count, uint32_t feat_spt0,
                       uint32_t feat_spt1, uint32_t iidr, uint32_t aidr)
        {
            m_is_mbx = is_mbx;
            m_channel_count = std::max(1u, std::min(channel_count, DBCH_CHANNELS));
            m_db_mask.fill(is_mbx ? 0xffffffffu : 0);

            store<uint32_t>(CTRL_BLK_ID, is_mbx ? 1 : 0);
            store<uint32_t>(CTRL_FEAT_SPT0, feat_spt0);
            store<uint32_t>(CTRL_FEAT_SPT1, feat_spt1);
            store<uint32_t>(CTRL_DBCH_CFG0, m_channel_count - 1);
            store<uint32_t>(CTRL_IIDR, iidr);
            store<uint32_t>(CTRL_AIDR, aidr);

            for (unsigned int channel = 0; channel < DBCH_CHANNELS; ++channel) {
                store_channel_regs(channel);
            }
            refresh_combined_irq_regs();
        }

        void copy_read(uint64_t offset, uint8_t* data, unsigned int len) const
        {
            std::memcpy(data, &m_regs[offset], len);
        }

        void copy_write(uint64_t offset, const uint8_t* data, unsigned int len)
        {
            std::memcpy(&m_regs[offset], data, len);
        }

        bool decode_dbch_offset(uint64_t offset, unsigned int& channel,
                                uint64_t& reg_offset) const
        {
            if (offset < DBCW0 || offset >= DBCW_END) {
                return false;
            }

            const uint64_t relative = offset - DBCW0;
            channel = relative / DBCW_STRIDE;
            reg_offset = relative % DBCW_STRIDE;
            return channel < m_channel_count;
        }

        uint32_t status(unsigned int channel) const { return m_db_status[channel]; }

        uint32_t channel_masked_status(unsigned int channel) const
        {
            return m_db_status[channel] & ~m_db_mask[channel];
        }

        void set_status(unsigned int channel, uint32_t value)
        {
            m_db_status[channel] = value;
            store_channel_regs(channel);
        }

        void set_status_bits(unsigned int channel, uint32_t value)
        {
            m_db_status[channel] |= value;
            store_channel_regs(channel);
        }

        void clear_status_bits(unsigned int channel, uint32_t mask)
        {
            m_db_status[channel] &= ~mask;
            store_channel_regs(channel);
        }

        void set_mask_bits(unsigned int channel, uint32_t mask)
        {
            m_db_mask[channel] |= mask;
            store_channel_regs(channel);
        }

        void clear_mask_bits(unsigned int channel, uint32_t mask)
        {
            m_db_mask[channel] &= ~mask;
            store_channel_regs(channel);
        }

        void set_int_status_bits(unsigned int channel, uint32_t value)
        {
            m_db_int_status[channel] |= value;
            store_channel_regs(channel);
        }

        void clear_int_status_bits(unsigned int channel, uint32_t mask)
        {
            m_db_int_status[channel] &= ~mask;
            store_channel_regs(channel);
        }

        void set_int_enable(unsigned int channel, uint32_t value)
        {
            m_db_int_enable[channel] = value;
            store_channel_regs(channel);
        }

        void set_ctrl(unsigned int channel, uint32_t value)
        {
            m_db_ctrl[channel] = value;
            store_channel_regs(channel);
        }

        void clear_all_status()
        {
            for (unsigned int channel = 0; channel < DBCH_CHANNELS; ++channel) {
                m_db_status[channel] = 0;
                store_channel_regs(channel);
            }
        }

        void store_channel_regs(unsigned int channel)
        {
            const uint64_t base = DBCW0 + (DBCW_STRIDE * channel);

            store<uint32_t>(base + DBCW_ST, m_db_status[channel]);
            if (m_is_mbx) {
                store<uint32_t>(base + DBCW_ST_MSK, channel_masked_status(channel));
                store<uint32_t>(base + DBCW_INT_ST, m_db_mask[channel]);
            } else {
                store<uint32_t>(base + DBCW_INT_ST, m_db_int_status[channel]);
                store<uint32_t>(base + DBCW_INT_EN, m_db_int_enable[channel]);
            }
            store<uint32_t>(base + DBCW_CTRL, m_db_ctrl[channel]);
        }

        uint32_t combined_dbch_int_st(unsigned int word) const
        {
            uint32_t status = 0;
            const unsigned int first = word * 32;
            const unsigned int last = std::min(first + 32, m_channel_count);

            for (unsigned int channel = first; channel < last; ++channel) {
                const bool asserted =
                    m_is_mbx ? (channel_masked_status(channel) != 0)
                             : ((m_db_int_status[channel] &
                                 m_db_int_enable[channel]) != 0);
                if (asserted) {
                    status |= 1u << (channel - first);
                }
            }

            return status;
        }

        void refresh_combined_irq_regs()
        {
            for (unsigned int word = 0; word < 4; ++word) {
                store<uint32_t>(CTRL_DBCH_INT_ST0 + (word * sizeof(uint32_t)),
                                combined_dbch_int_st(word));
            }
        }

        bool any_combined_irq() const
        {
            for (unsigned int word = 0; word < 4; ++word) {
                if (combined_dbch_int_st(word) != 0) {
                    return true;
                }
            }
            return false;
        }

        uint32_t read32(uint64_t offset) const
        {
            if (offset >= CTRL_DBCH_INT_ST0 && offset < CTRL_DBCH_INT_ST0 + 16 &&
                ((offset - CTRL_DBCH_INT_ST0) % sizeof(uint32_t)) == 0) {
                return combined_dbch_int_st((offset - CTRL_DBCH_INT_ST0) /
                                            sizeof(uint32_t));
            }

            unsigned int channel = 0;
            uint64_t reg_offset = 0;
            if (!decode_dbch_offset(offset, channel, reg_offset)) {
                return load<uint32_t>(offset);
            }

            if (m_is_mbx) {
                switch (reg_offset) {
                case DBCW_ST:
                    return m_db_status[channel];
                case DBCW_ST_MSK:
                    return channel_masked_status(channel);
                case DBCW_INT_ST:
                    return m_db_mask[channel];
                case DBCW_CTRL:
                    return m_db_ctrl[channel];
                default:
                    return load<uint32_t>(offset);
                }
            }

            switch (reg_offset) {
            case DBCW_ST:
                return m_db_status[channel];
            case DBCW_INT_ST:
                return m_db_int_status[channel];
            case DBCW_INT_EN:
                return m_db_int_enable[channel];
            case DBCW_CTRL:
                return m_db_ctrl[channel];
            default:
                return load<uint32_t>(offset);
            }
        }
    };

private:
    static inline mhu320ae* s_mbx = nullptr;
    static inline mhu320ae* s_pbx = nullptr;
    static inline std::unordered_map<std::string, mhu320ae*> s_mbx_by_pair;
    static inline std::unordered_map<std::string, mhu320ae*> s_pbx_by_pair;

    cci::cci_param<std::string> p_pair;
    cci::cci_param<std::string> p_protocol;
    cci::cci_param<std::string> p_frame;
    cci::cci_param<unsigned int> p_channel_count;
    cci::cci_param<uint32_t> p_feat_spt0;
    cci::cci_param<uint32_t> p_feat_spt1;
    cci::cci_param<uint32_t> p_iidr;
    cci::cci_param<uint32_t> p_aidr;
    cci::cci_param<bool> p_direct_boot_compat;
    cci::cci_param<std::string> p_scmi_transport;
    cci::cci_param<uint64_t> p_tx_shmem;
    cci::cci_param<uint64_t> p_rx_shmem;
    cci::cci_param<uint64_t> p_scmi_channel_stride;
    cci::cci_param<unsigned int> p_scmi_channel_base_index;
    cci::cci_param<unsigned int> p_scmi_channel_count;
    cci::cci_param<bool> p_init_shmem;
    cci::cci_param<unsigned int> p_ack_bit;
    cci::cci_param<uint32_t> p_power_domain_version;
    cci::cci_param<uint32_t> p_sys_power_version;
    cci::cci_param<uint32_t> p_power_domain_attributes;
    cci::cci_param<std::string> p_power_domain_name;
    cci::cci_param<bool> p_assert_power_on_reset;
    cci::cci_param<unsigned int> p_power_domain_reset_count;
    cci::cci_param<uint64_t> p_power_domain_reset_delay_ns;
    cci::cci_param<bool> p_power_domain_reset_assert_on_power_off;
    cci::cci_param<bool> p_power_domain_reset_pulse_on_power_on;
    cci::cci_param<uint64_t> p_system_power_reset_delay_ns;
    cci::cci_param<uint64_t> p_system_power_reset_pulse_width_ns;
    cci::cci_param<unsigned int> p_doorbell_ack_trigger_channel;
    cci::cci_param<uint32_t> p_doorbell_ack_trigger_value;
    cci::cci_param<unsigned int> p_doorbell_ack_channel;
    cci::cci_param<uint32_t> p_doorbell_ack_value;
    cci::cci_param<uint64_t> p_doorbell_ack_seed_address;
    cci::cci_param<std::vector<unsigned int>> p_doorbell_ack_seed_words;
    cci::cci_param<bool> p_rpmsg_ns_enable;
    cci::cci_param<std::string> p_rpmsg_ns_name;
    cci::cci_param<uint32_t> p_rpmsg_ns_remote_addr;
    cci::cci_param<uint64_t> p_rpmsg_ns_vring_address;
    cci::cci_param<unsigned int> p_rpmsg_ns_vring_num;
    cci::cci_param<unsigned int> p_rpmsg_ns_vring_align;
    cci::cci_param<unsigned int> p_rpmsg_ns_signal_channel;
    cci::cci_param<uint32_t> p_rpmsg_ns_signal_value;
    cci::cci_param<uint64_t> p_rpmsg_ns_signal_delay_ns;
    cci::cci_param<uint64_t> p_rpmsg_ns_poll_period_ns;
    cci::cci_param<unsigned int> p_rpmsg_ns_max_polls;
    cci::cci_param<uint64_t> p_synthetic_txdone_delay_ns;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    cci::cci_param<std::string> p_trace_file;

    struct ps_entry {
        std::vector<uint8_t> data;
        uint32_t flags = 0;
    };

    struct measurement_entry {
        bool valid = false;
        bool locked = false;
        uint32_t algorithm = 0;
        std::vector<uint8_t> sw_type;
        std::vector<uint8_t> version;
        std::vector<uint8_t> signer_id;
        std::vector<uint8_t> measurement;
    };

    struct rse_ps_request {
        uint8_t protocol = RSE_COMMS_PROTOCOL_EMBED;
        uint8_t seq = 0;
        uint16_t client_id = 0;
        uint32_t handle = 0;
        uint32_t ctrl = 0;
        uint32_t type = 0;
        unsigned int in_len = 0;
        unsigned int out_len = 0;
        std::array<std::vector<uint8_t>, PSA_MAX_IOVEC> inputs;
        std::array<uint32_t, PSA_MAX_IOVEC> out_caps {};
        std::array<uint64_t, PSA_MAX_IOVEC> out_ptrs {};
    };

    struct synthetic_postbox_completion {
        unsigned int channel;
        uint32_t mask;
        std::string reason;
    };

    mhu320ae_frame_model m_frame;
    std::array<uint32_t, 256> m_power_domain_states {};
    uint32_t m_power_domain_state = 0;
    bool m_pending_power_on_reset = false;
    sc_core::sc_event m_power_on_reset_event;
    bool m_pending_power_domain_reset_valid = false;
    uint32_t m_pending_power_domain_reset_id = 0;
    uint32_t m_pending_power_domain_reset_state = 0;
    sc_core::sc_event m_power_domain_reset_event;
    bool m_pending_system_power_reset_valid = false;
    uint32_t m_pending_system_power_state = 0;
    sc_core::sc_event m_system_power_reset_event;
    bool m_rpmsg_ns_pending = false;
    bool m_rpmsg_ns_sent = false;
    uint16_t m_rpmsg_ns_last_avail_idx = 0;
    unsigned int m_rpmsg_ns_poll_count = 0;
    sc_core::sc_event m_rpmsg_ns_event;
    unsigned int m_trace_count = 0;
    std::vector<unsigned int> m_doorbell_ack_seed_words;
    std::deque<synthetic_postbox_completion> m_synthetic_postbox_completions;
    sc_core::sc_event m_synthetic_postbox_completion_event;
    std::ofstream m_trace_stream;
    std::mutex m_trace_lock;
    std::unordered_map<uint64_t, ps_entry> m_ps_store;
    std::array<measurement_entry, TFM_MEASUREMENT_SLOT_COUNT> m_measurements {};

    bool is_mbx() const { return p_frame.get_value() == "mbx"; }

    unsigned int channel_count() const
    {
        return std::max(1u, std::min(p_channel_count.get_value(), DBCH_CHANNELS));
    }

    unsigned int notify_channel() const { return channel_count() - 1; }

    bool scmi_channel_in_range(unsigned int channel) const
    {
        const unsigned int base = p_scmi_channel_base_index.get_value();
        return channel >= base &&
               (channel - base) < p_scmi_channel_count.get_value();
    }

    void trace_event(const char* event, const std::string& detail = "")
    {
        if (!p_trace.get_value()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_trace_lock);
        if (m_trace_count >= p_trace_limit.get_value()) {
            return;
        }

        std::ostream* out = &std::cerr;
        const std::string trace_file = p_trace_file.get_value();
        if (!trace_file.empty()) {
            if (!m_trace_stream.is_open()) {
                m_trace_stream.open(trace_file, std::ios::out | std::ios::app);
                if (!m_trace_stream) {
                    std::cerr << name() << " mhu_trace_error file=" << trace_file
                              << std::endl;
                    return;
                }
            }
            out = &m_trace_stream;
        }

        ++m_trace_count;
        *out << name()
             << " mhu_trace event=" << event
             << " frame=" << p_frame.get_value()
             << " pair=" << p_pair.get_value()
             << " protocol=" << p_protocol.get_value()
             << " sc_time=" << sc_core::sc_time_stamp();
        if (!detail.empty()) {
            *out << " " << detail;
        }
        *out << std::endl;
    }

    template <typename T>
    void store(uint64_t offset, T value)
    {
        m_frame.store(offset, value);
    }

    template <typename T>
    T load(uint64_t offset) const
    {
        return m_frame.load<T>(offset);
    }

    bool mem_access(tlm::tlm_command command, uint64_t address, void* data, unsigned int len)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(static_cast<unsigned char*>(data));
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_dmi_allowed(false);
        initiator_socket->b_transport(trans, delay);
        return trans.is_response_ok();
    }

    uint32_t mem_read32(uint64_t address)
    {
        uint32_t value = 0;
        mem_access(tlm::TLM_READ_COMMAND, address, &value, sizeof(value));
        return value;
    }

    uint16_t mem_read16(uint64_t address)
    {
        uint16_t value = 0;
        mem_access(tlm::TLM_READ_COMMAND, address, &value, sizeof(value));
        return value;
    }

    uint64_t mem_read64(uint64_t address)
    {
        uint64_t value = 0;
        mem_access(tlm::TLM_READ_COMMAND, address, &value, sizeof(value));
        return value;
    }

    void mem_write(uint64_t address, const void* data, unsigned int len)
    {
        std::vector<uint8_t> tmp(len);
        std::memcpy(tmp.data(), data, len);
        mem_access(tlm::TLM_WRITE_COMMAND, address, tmp.data(), len);
    }

    void mem_write32(uint64_t address, uint32_t value)
    {
        mem_write(address, &value, sizeof(value));
    }

    void mem_write16(uint64_t address, uint16_t value)
    {
        mem_write(address, &value, sizeof(value));
    }

    void write_power_on_reset(bool asserted)
    {
        if (power_on_reset.size() != 0) {
            power_on_reset->write(asserted);
        }
    }

    void write_power_domain_reset(uint32_t domain_id, bool asserted)
    {
        if (domain_id < power_domain_reset.size() &&
            power_domain_reset[domain_id].size() != 0) {
            power_domain_reset[domain_id]->write(asserted);
        }
    }

    void write_system_reset(bool asserted)
    {
        system_reset.async_write_vector({ asserted });
    }

    static bool power_state_is_on(uint32_t power_state)
    {
        return (power_state & 0xfu) == 1u;
    }

    static bool power_state_is_off(uint32_t power_state)
    {
        return (power_state & 0xfu) == 0u;
    }

    static bool sys_power_state_is_reset(uint32_t system_state)
    {
        return system_state == SCMI_SYS_POWER_COLD_RESET ||
               system_state == SCMI_SYS_POWER_WARM_RESET;
    }

    void drive_power_domain_reset(uint32_t domain_id, uint32_t power_state)
    {
        if (power_state_is_on(power_state)) {
            if (p_power_domain_reset_pulse_on_power_on.get_value()) {
                write_power_domain_reset(domain_id, true);
            }
            write_power_domain_reset(domain_id, false);
            write_power_on_reset(false);
        } else if (power_state_is_off(power_state)) {
            if (p_power_domain_reset_assert_on_power_off.get_value()) {
                write_power_domain_reset(domain_id, true);
                write_power_on_reset(true);
            } else {
                std::ostringstream detail;
                detail << "domain=" << domain_id
                       << " power_state=0x" << std::hex << power_state << std::dec;
                trace_event("power-domain-reset-off-assert-skipped", detail.str());
            }
        }
    }

    void apply_power_domain_state(uint32_t domain_id, uint32_t power_state)
    {
        {
            std::ostringstream detail;
            detail << "domain=" << domain_id
                   << " power_state=0x" << std::hex << power_state << std::dec;
            trace_event("power-domain-apply", detail.str());
        }

        if (domain_id < m_power_domain_states.size()) {
            m_power_domain_states[domain_id] = power_state;
        }
        m_power_domain_state = power_state;

        m_pending_power_domain_reset_id = domain_id;
        m_pending_power_domain_reset_state = power_state;
        m_pending_power_domain_reset_valid = true;
        const uint64_t reset_delay_ns = p_power_domain_reset_delay_ns.get_value();
        if (reset_delay_ns == 0) {
            m_power_domain_reset_event.notify(sc_core::SC_ZERO_TIME);
        } else {
            m_power_domain_reset_event.notify(
                sc_core::sc_time(reset_delay_ns, sc_core::SC_NS));
        }

        {
            std::ostringstream detail;
            detail << "domain=" << domain_id
                   << " stored_state=0x" << std::hex
                   << read_power_domain_state(domain_id) << std::dec
                   << " reset_delay_ns=" << reset_delay_ns;
            trace_event("power-domain-reset-scheduled", detail.str());
        }
    }

    uint32_t read_power_domain_state(uint32_t domain_id) const
    {
        if (domain_id < m_power_domain_states.size()) {
            return m_power_domain_states[domain_id];
        }

        return m_power_domain_state;
    }

    void schedule_power_on_reset(bool asserted)
    {
        m_pending_power_on_reset = asserted;
        m_power_on_reset_event.notify(sc_core::SC_ZERO_TIME);
    }

    void emit_power_on_reset() { write_power_on_reset(m_pending_power_on_reset); }

    void emit_power_domain_reset()
    {
        for (;;) {
            wait(m_power_domain_reset_event);
            if (!m_pending_power_domain_reset_valid) {
                continue;
            }

            const uint32_t domain_id = m_pending_power_domain_reset_id;
            const uint32_t power_state = m_pending_power_domain_reset_state;
            m_pending_power_domain_reset_valid = false;

            {
                std::ostringstream detail;
                detail << "domain=" << domain_id
                       << " power_state=0x" << std::hex << power_state << std::dec;
                trace_event("power-domain-reset-drive", detail.str());
            }
            drive_power_domain_reset(domain_id, power_state);
            trace_event("power-domain-reset-driven");
        }
    }

    void schedule_system_power_reset(uint32_t system_state)
    {
        if (!sys_power_state_is_reset(system_state)) {
            std::ostringstream detail;
            detail << "system_state=0x" << std::hex << system_state << std::dec;
            trace_event("system-power-reset-skipped", detail.str());
            return;
        }

        m_pending_system_power_state = system_state;
        m_pending_system_power_reset_valid = true;
        const uint64_t reset_delay_ns = p_system_power_reset_delay_ns.get_value();
        if (reset_delay_ns == 0) {
            m_system_power_reset_event.notify(sc_core::SC_ZERO_TIME);
        } else {
            m_system_power_reset_event.notify(
                sc_core::sc_time(reset_delay_ns, sc_core::SC_NS));
        }

        std::ostringstream detail;
        detail << "system_state=0x" << std::hex << system_state
               << std::dec << " reset_delay_ns=" << reset_delay_ns;
        trace_event("system-power-reset-scheduled", detail.str());
    }

    void emit_system_power_reset()
    {
        for (;;) {
            wait(m_system_power_reset_event);
            if (!m_pending_system_power_reset_valid) {
                continue;
            }

            const uint32_t system_state = m_pending_system_power_state;
            m_pending_system_power_reset_valid = false;

            {
                std::ostringstream detail;
                detail << "system_state=0x" << std::hex << system_state
                       << std::dec;
                trace_event("system-power-reset-drive", detail.str());
            }

            write_system_reset(true);
            const uint64_t pulse_width_ns = p_system_power_reset_pulse_width_ns.get_value();
            if (pulse_width_ns == 0) {
                wait(sc_core::SC_ZERO_TIME);
            } else {
                wait(sc_core::sc_time(pulse_width_ns, sc_core::SC_NS));
            }
            write_system_reset(false);
            trace_event("system-power-reset-driven");
        }
    }

    static uint8_t msg_id(uint32_t header) { return header & 0xffu; }
    static uint8_t protocol_id(uint32_t header) { return (header >> 10) & 0xffu; }

    mhu320ae* paired_mbx() const
    {
        const auto it = s_mbx_by_pair.find(p_pair.get_value());
        if (it != s_mbx_by_pair.end()) {
            return it->second;
        }

        if (!p_pair.get_value().empty()) {
            return nullptr;
        }

        return s_mbx;
    }

    mhu320ae* paired_pbx() const
    {
        const auto it = s_pbx_by_pair.find(p_pair.get_value());
        if (it != s_pbx_by_pair.end()) {
            return it->second;
        }

        if (!p_pair.get_value().empty()) {
            return nullptr;
        }

        return s_pbx;
    }

    bool decode_dbch_offset(uint64_t offset, unsigned int& channel,
                            uint64_t& reg_offset) const
    {
        return m_frame.decode_dbch_offset(offset, channel, reg_offset);
    }

    uint32_t channel_masked_status(unsigned int channel) const
    {
        return m_frame.channel_masked_status(channel);
    }

    void store_channel_regs(unsigned int channel)
    {
        m_frame.store_channel_regs(channel);
    }

    uint32_t combined_dbch_int_st(unsigned int word) const
    {
        return m_frame.combined_dbch_int_st(word);
    }

    void update_combined_irq()
    {
        m_frame.refresh_combined_irq_regs();

        if (irq.size() != 0) {
            irq->write(m_frame.any_combined_irq());
        }
    }

    uint32_t read32(uint64_t offset) const
    {
        return m_frame.read32(offset);
    }

    static uint32_t read_le32(const std::vector<uint8_t>& in, size_t offset)
    {
        if (offset + sizeof(uint32_t) > in.size()) {
            return 0;
        }

        return static_cast<uint32_t>(in[offset]) |
               (static_cast<uint32_t>(in[offset + 1]) << 8) |
               (static_cast<uint32_t>(in[offset + 2]) << 16) |
               (static_cast<uint32_t>(in[offset + 3]) << 24);
    }

    static uint16_t read_le16(const std::vector<uint8_t>& in, size_t offset)
    {
        if (offset + sizeof(uint16_t) > in.size()) {
            return 0;
        }

        return static_cast<uint16_t>(in[offset]) |
               (static_cast<uint16_t>(in[offset + 1]) << 8);
    }

    static uint64_t read_le64(const std::vector<uint8_t>& in, size_t offset)
    {
        if (offset + sizeof(uint64_t) > in.size()) {
            return 0;
        }

        uint64_t value = 0;
        for (unsigned int byte = 0; byte < sizeof(value); ++byte) {
            value |= static_cast<uint64_t>(in[offset + byte]) << (byte * 8);
        }
        return value;
    }

    bool mem_read_bytes(uint64_t address, std::vector<uint8_t>& out)
    {
        if (out.empty()) {
            return true;
        }
        return mem_access(tlm::TLM_READ_COMMAND, address, out.data(), out.size());
    }

    bool mem_write_bytes(uint64_t address, const std::vector<uint8_t>& in)
    {
        if (in.empty()) {
            return true;
        }
        return mem_access(tlm::TLM_WRITE_COMMAND, address,
                          const_cast<uint8_t*>(in.data()), in.size());
    }

    static unsigned int rse_ctrl_type(uint32_t ctrl)
    {
        return ctrl & 0xffffu;
    }

    static unsigned int rse_ctrl_out_len(uint32_t ctrl)
    {
        return (ctrl >> 16) & 0x7u;
    }

    static unsigned int rse_ctrl_in_len(uint32_t ctrl)
    {
        return (ctrl >> 24) & 0x7u;
    }

    uint64_t scmi_shmem(unsigned int channel) const
    {
        const unsigned int base = p_scmi_channel_base_index.get_value();
        const unsigned int slot = channel >= base ? channel - base : channel;

        return p_tx_shmem.get_value() + (p_scmi_channel_stride.get_value() * slot);
    }

    void write_scmi_response(unsigned int channel, uint32_t header, uint32_t status,
                             const std::vector<uint8_t>& payload)
    {
        const uint64_t shmem = scmi_shmem(channel);
        const uint32_t length = 8 + payload.size();

        {
            std::ostringstream detail;
            detail << "channel=" << channel
                   << " shmem=0x" << std::hex << shmem
                   << " header=0x" << header
                   << " status=0x" << status
                   << std::dec << " length=" << length
                   << " payload_len=" << payload.size();
            trace_event("scmi-write-response", detail.str());
        }

        mem_write32(shmem + SCMI_LENGTH, length);
        mem_write32(shmem + SCMI_HEADER, header);
        mem_write32(shmem + SCMI_PAYLOAD, status);
        if (!payload.empty()) {
            mem_write(shmem + SCMI_PAYLOAD + sizeof(status), payload.data(), payload.size());
        }
        mem_write32(shmem + SCMI_CHAN_STATUS, SCMI_CHAN_FREE);
        trace_event("scmi-response-written");
    }

    std::vector<uint8_t> read_scmi_request_payload(unsigned int channel, uint32_t length)
    {
        const uint64_t shmem = scmi_shmem(channel);
        if (length <= sizeof(uint32_t)) {
            return {};
        }

        std::vector<uint8_t> payload(length - sizeof(uint32_t));
        mem_access(tlm::TLM_READ_COMMAND, shmem + SCMI_PAYLOAD, payload.data(),
                   payload.size());
        return payload;
    }

    static void append_u32(std::vector<uint8_t>& out, uint32_t value)
    {
        out.push_back(value & 0xffu);
        out.push_back((value >> 8) & 0xffu);
        out.push_back((value >> 16) & 0xffu);
        out.push_back((value >> 24) & 0xffu);
    }

    static void append_u16(std::vector<uint8_t>& out, uint16_t value)
    {
        out.push_back(value & 0xffu);
        out.push_back((value >> 8) & 0xffu);
    }

    static uint64_t align_up(uint64_t value, uint64_t align)
    {
        if (align == 0) {
            return value;
        }

        return (value + align - 1) & ~(align - 1);
    }

    static std::vector<uint8_t> fixed_string(const char* value)
    {
        std::vector<uint8_t> out(16, 0);
        std::strncpy(reinterpret_cast<char*>(out.data()), value, out.size() - 1);
        return out;
    }

    std::vector<uint8_t> fixed_param_string(const std::string& value)
    {
        std::vector<uint8_t> out(16, 0);
        std::strncpy(reinterpret_cast<char*>(out.data()), value.c_str(), out.size() - 1);
        return out;
    }

    void respond_scmi_base(uint32_t header, uint32_t& status, std::vector<uint8_t>& payload)
    {
        switch (msg_id(header)) {
        case 0x0:
            append_u32(payload, 0x00020001);
            break;
        case 0x1:
            payload = {1, 1, 0, 0};
            break;
        case 0x2:
            append_u32(payload, 0);
            break;
        case 0x3:
            payload = fixed_string("QBox");
            break;
        case 0x4:
            payload = fixed_string("RD-Aspen");
            break;
        case 0x5:
            append_u32(payload, 1);
            break;
        case 0x6:
            append_u32(payload, 1);
            payload.push_back(SCMI_PROTOCOL_BASE);
            payload.insert(payload.end(), 3, 0);
            break;
        case 0x7:
            append_u32(payload, 0);
            {
                auto name = fixed_string("platform");
                payload.insert(payload.end(), name.begin(), name.end());
            }
            break;
        case 0x8:
            break;
        default:
            status = SCMI_ERR_SUPPORT;
            break;
        }
    }

    void respond_scmi_power_domain(uint32_t header, const std::vector<uint8_t>& request,
                                   uint32_t& status, std::vector<uint8_t>& payload)
    {
        switch (msg_id(header)) {
        case 0x0:
            append_u32(payload, p_power_domain_version.get_value());
            break;
        case 0x2:
            switch (read_le32(request, 0)) {
            case 0x4:
            case 0x5:
                append_u32(payload, 0);
                break;
            default:
                status = SCMI_ERR_SUPPORT;
                append_u32(payload, 0);
                break;
            }
            break;
        case 0x3: {
            append_u32(payload, p_power_domain_attributes.get_value());
            auto name = fixed_param_string(p_power_domain_name.get_value());
            payload.insert(payload.end(), name.begin(), name.end());
            break;
        }
        case 0x4:
            {
                std::ostringstream detail;
                detail << "domain=" << read_le32(request, sizeof(uint32_t))
                       << " flags=0x" << std::hex << read_le32(request, 0)
                       << " power_state=0x"
                       << read_le32(request, sizeof(uint32_t) * 2)
                       << std::dec;
                trace_event("scmi-power-state-set", detail.str());
            }
            apply_power_domain_state(read_le32(request, sizeof(uint32_t)),
                                     read_le32(request, sizeof(uint32_t) * 2));
            break;
        case 0x5:
            {
                std::ostringstream detail;
                detail << "domain=" << read_le32(request, 0)
                       << " power_state=0x" << std::hex
                       << read_power_domain_state(read_le32(request, 0))
                       << std::dec;
                trace_event("scmi-power-state-get", detail.str());
            }
            append_u32(payload, read_power_domain_state(read_le32(request, 0)));
            break;
        default:
            status = SCMI_ERR_SUPPORT;
            break;
        }
    }

    void respond_scmi_sys_power(uint32_t header, const std::vector<uint8_t>& request,
                                uint32_t& status,
                                std::vector<uint8_t>& payload)
    {
        switch (msg_id(header)) {
        case 0x0:
            append_u32(payload, p_sys_power_version.get_value());
            break;
        case SCMI_MSG_PROTOCOL_MESSAGE_ATTRIBUTES:
            switch (read_le32(request, 0)) {
            case SCMI_SYS_POWER_STATE_SET:
            case SCMI_SYS_POWER_STATE_NOTIFY:
                break;
            default:
                status = SCMI_ERR_SUPPORT;
                break;
            }
            append_u32(payload, 0);
            break;
        case SCMI_SYS_POWER_STATE_SET:
            {
                const uint32_t flags = read_le32(request, 0);
                const uint32_t system_state = read_le32(request, sizeof(uint32_t));
                std::ostringstream detail;
                detail << "flags=0x" << std::hex << flags
                       << " system_state=0x" << system_state << std::dec;
                trace_event("scmi-sys-power-state-set", detail.str());
                schedule_system_power_reset(system_state);
            }
            break;
        case SCMI_SYS_POWER_STATE_NOTIFY:
            {
                std::ostringstream detail;
                detail << "notify_enable=" << read_le32(request, 0);
                trace_event("scmi-sys-power-state-notify", detail.str());
            }
            break;
        default:
            status = SCMI_ERR_SUPPORT;
            break;
        }
    }

    void respond_scmi_pfdi_monitor(uint32_t header, uint32_t& status,
                                   std::vector<uint8_t>& payload)
    {
        switch (msg_id(header)) {
        case 0x0:
            append_u32(payload, SCMI_PFDI_MONITOR_VERSION);
            break;
        case 0x3:
        case 0x4:
            break;
        default:
            status = SCMI_ERR_SUPPORT;
            break;
        }
    }

    void respond_scmi(unsigned int channel)
    {
        const uint64_t shmem = scmi_shmem(channel);
        const uint32_t length = mem_read32(shmem + SCMI_LENGTH);
        const uint32_t header = mem_read32(shmem + SCMI_HEADER);
        const std::vector<uint8_t> request = read_scmi_request_payload(channel, length);
        std::vector<uint8_t> payload;
        uint32_t status = SCMI_SUCCESS;

        {
            std::ostringstream detail;
            detail << "channel=" << channel
                   << " shmem=0x" << std::hex << shmem
                   << " header=0x" << header
                   << " protocol=0x" << static_cast<unsigned int>(protocol_id(header))
                   << " msg=0x" << static_cast<unsigned int>(msg_id(header))
                   << std::dec << " length=" << length
                   << " request_len=" << request.size();
            trace_event("scmi-request", detail.str());
        }

        switch (protocol_id(header)) {
        case SCMI_PROTOCOL_BASE:
            respond_scmi_base(header, status, payload);
            break;
        case SCMI_PROTOCOL_POWER_DOMAIN:
            respond_scmi_power_domain(header, request, status, payload);
            break;
        case SCMI_PROTOCOL_SYS_POWER:
            respond_scmi_sys_power(header, request, status, payload);
            break;
        case SCMI_PROTOCOL_PFDI_MONITOR:
            if (p_scmi_transport.get_value() == "pfdi-monitor") {
                respond_scmi_pfdi_monitor(header, status, payload);
            } else {
                status = SCMI_ERR_SUPPORT;
            }
            break;
        default:
            status = SCMI_ERR_SUPPORT;
            break;
        }

        {
            std::ostringstream detail;
            detail << "channel=" << channel
                   << " header=0x" << std::hex << header
                   << " status=0x" << status
                   << std::dec << " payload_len=" << payload.size();
            trace_event("scmi-response", detail.str());
        }
        write_scmi_response(channel, header, status, payload);
        if (auto mbx = paired_mbx()) {
            std::ostringstream detail;
            detail << "ack_bit=" << p_ack_bit.get_value()
                   << " mbx=" << mbx->name();
            trace_event("scmi-ack-signal", detail.str());
            mbx->signal_doorbell(p_ack_bit.get_value());
            trace_event("scmi-ack-signaled");
        }
        if (protocol_id(header) == SCMI_PROTOCOL_POWER_DOMAIN &&
            msg_id(header) == 0x5 && m_pending_power_domain_reset_valid) {
            trace_event("power-domain-reset-reschedule-after-state-get");
            m_power_domain_reset_event.notify(sc_core::SC_ZERO_TIME);
        }
    }

    void signal_doorbell(unsigned int bit)
    {
        if (bit >= 32) {
            return;
        }

        signal_doorbell_channel(0, 1u << bit);
    }

    void signal_doorbell_channel(unsigned int channel, uint32_t value)
    {
        if (channel >= channel_count()) {
            return;
        }

        m_frame.set_status_bits(channel, value);
        update_combined_irq();

        std::ostringstream detail;
        detail << "channel=" << channel
               << " value=0x" << std::hex << value
               << " status=0x" << m_frame.status(channel) << std::dec;
        trace_event("doorbell-signal", detail.str());
    }

    void clear_postbox_doorbell_channel(unsigned int channel, uint32_t mask)
    {
        if (channel >= channel_count()) {
            return;
        }

        const bool had_pending_transfer = (m_frame.status(channel) & mask) != 0;
        const bool ack_empty_transfer = p_pair.get_value() != "ap_si_cl1" ||
                                        p_protocol.get_value() != "doorbell";
        m_frame.clear_status_bits(channel, mask);
        if (had_pending_transfer || ack_empty_transfer) {
            m_frame.set_int_status_bits(channel, 1);
        }
        update_combined_irq();
    }

    void clear_mbx_doorbell_channel(unsigned int channel, uint32_t mask)
    {
        if (channel >= channel_count()) {
            return;
        }

        m_frame.clear_status_bits(channel, mask);
        update_combined_irq();

        std::ostringstream detail;
        detail << "channel=" << channel
               << " mask=0x" << std::hex << mask
               << " status=0x" << m_frame.status(channel) << std::dec;
        trace_event("doorbell-clear", detail.str());

        if (auto pbx = paired_pbx()) {
            pbx->clear_postbox_doorbell_channel(channel, mask);
        }
    }

    void seed_doorbell_ack_memory()
    {
        const auto& words = m_doorbell_ack_seed_words;
        if (words.empty()) {
            return;
        }

        std::vector<uint8_t> data;
        data.reserve(words.size() * sizeof(uint32_t));
        for (const auto word : words) {
            append_u32(data, static_cast<uint32_t>(word));
        }

        const uint64_t address = p_doorbell_ack_seed_address.get_value();
        const bool ok = mem_access(tlm::TLM_WRITE_COMMAND, address, data.data(),
                                   data.size());

        std::ostringstream detail;
        detail << "address=0x" << std::hex << address
               << " bytes=0x" << data.size()
               << " status=" << (ok ? "ok" : "error") << std::dec;
        trace_event("doorbell-ack-seed-memory", detail.str());
    }

    std::vector<unsigned int> load_doorbell_ack_seed_words()
    {
        const auto direct_words = p_doorbell_ack_seed_words.get_value();
        if (!direct_words.empty()) {
            return direct_words;
        }

        const std::string base = std::string(name()) + ".doorbell_ack_seed_words";
        const std::string prefix = base + ".";
        std::vector<std::pair<unsigned int, unsigned int>> indexed_words;
        const auto broker = cci::cci_get_broker();

        auto presets = broker.get_unconsumed_preset_values(
            [&prefix](const std::pair<std::string, cci::cci_value>& value) {
                return value.first.find(prefix) == 0;
            });
        for (const auto& preset : presets) {
            const auto child = preset.first.substr(prefix.size());
            if (child.find('.') != std::string::npos) {
                continue;
            }
            if (child.empty() ||
                !std::all_of(child.begin(), child.end(), [](unsigned char c) {
                    return std::isdigit(c);
                })) {
                continue;
            }

            unsigned int value = 0;
            if (!gs::cci_get<unsigned int>(broker, base + "." + child, value)) {
                continue;
            }

            indexed_words.emplace_back(static_cast<unsigned int>(std::stoul(child)),
                                       value);
        }

        std::sort(indexed_words.begin(), indexed_words.end(),
                  [](const auto& left, const auto& right) {
                      return left.first < right.first;
                  });

        std::vector<unsigned int> words;
        words.reserve(indexed_words.size());
        for (const auto& word : indexed_words) {
            words.push_back(word.second);
        }
        return words;
    }

    uint64_t rpmsg_ns_avail_address() const
    {
        return p_rpmsg_ns_vring_address.get_value() +
               (p_rpmsg_ns_vring_num.get_value() * VRING_DESC_SIZE);
    }

    uint64_t rpmsg_ns_used_address() const
    {
        const uint64_t avail_end = rpmsg_ns_avail_address() +
                                   VRING_AVAIL_HEADER_SIZE +
                                   ((p_rpmsg_ns_vring_num.get_value() + 1) *
                                    sizeof(uint16_t));
        return align_up(avail_end, p_rpmsg_ns_vring_align.get_value());
    }

    std::vector<uint8_t> build_rpmsg_ns_message() const
    {
        std::vector<uint8_t> msg;
        const uint32_t remote_addr = p_rpmsg_ns_remote_addr.get_value();
        const uint32_t dst = RPMSG_NS_ADDR;
        const std::string name = p_rpmsg_ns_name.get_value();

        append_u32(msg, remote_addr);
        append_u32(msg, dst);
        append_u32(msg, 0);
        append_u16(msg, RPMSG_NS_MSG_SIZE);
        append_u16(msg, 0);

        const size_t name_start = msg.size();
        msg.insert(msg.end(), RPMSG_NAME_SIZE, 0);
        std::strncpy(reinterpret_cast<char*>(msg.data() + name_start),
                     name.c_str(), RPMSG_NAME_SIZE - 1);
        append_u32(msg, remote_addr);
        append_u32(msg, RPMSG_NS_CREATE);

        return msg;
    }

    bool inject_rpmsg_ns_once()
    {
        const unsigned int vring_num = p_rpmsg_ns_vring_num.get_value();
        if (!p_rpmsg_ns_enable.get_value() || m_rpmsg_ns_sent ||
            vring_num == 0) {
            return true;
        }

        const uint64_t vring = p_rpmsg_ns_vring_address.get_value();
        const uint64_t avail = rpmsg_ns_avail_address();
        const uint64_t used = rpmsg_ns_used_address();
        const uint16_t avail_idx = mem_read16(avail + sizeof(uint16_t));

        if (avail_idx == m_rpmsg_ns_last_avail_idx) {
            ++m_rpmsg_ns_poll_count;
            if (m_rpmsg_ns_poll_count == 1 ||
                (m_rpmsg_ns_poll_count % 1024) == 0) {
                std::ostringstream detail;
                detail << "poll=" << m_rpmsg_ns_poll_count
                       << " avail_idx=" << avail_idx
                       << " last_avail_idx=" << m_rpmsg_ns_last_avail_idx;
                trace_event("rpmsg-ns-wait-rx-buffer", detail.str());
            }
            return false;
        }

        m_rpmsg_ns_poll_count = 0;

        const uint16_t slot = m_rpmsg_ns_last_avail_idx % vring_num;
        const uint16_t desc_id = mem_read16(avail + VRING_AVAIL_HEADER_SIZE +
                                            (slot * sizeof(uint16_t)));
        if (desc_id >= vring_num) {
            std::ostringstream detail;
            detail << "desc_id=" << desc_id
                   << " vring_num=" << vring_num;
            trace_event("rpmsg-ns-invalid-desc", detail.str());
            return false;
        }

        const uint64_t desc = vring + (desc_id * VRING_DESC_SIZE);
        const uint64_t buffer_addr = mem_read64(desc);
        const uint32_t buffer_len = mem_read32(desc + sizeof(uint64_t));
        const uint16_t flags = mem_read16(desc + sizeof(uint64_t) +
                                          sizeof(uint32_t));
        const auto message = build_rpmsg_ns_message();
        if ((flags & VRING_DESC_F_WRITE) == 0 || buffer_len < message.size()) {
            std::ostringstream detail;
            detail << "desc_id=" << desc_id
                   << " buffer=0x" << std::hex << buffer_addr
                   << " buffer_len=0x" << buffer_len
                   << " flags=0x" << flags
                   << " msg_len=0x" << message.size() << std::dec;
            trace_event("rpmsg-ns-desc-not-ready", detail.str());
            return false;
        }

        const bool ok = mem_access(tlm::TLM_WRITE_COMMAND, buffer_addr,
                                   const_cast<uint8_t*>(message.data()),
                                   message.size());
        if (!ok) {
            std::ostringstream detail;
            detail << "buffer=0x" << std::hex << buffer_addr
                   << " msg_len=0x" << message.size() << std::dec;
            trace_event("rpmsg-ns-message-write-error", detail.str());
            return false;
        }

        const uint16_t used_idx = mem_read16(used + sizeof(uint16_t));
        const uint64_t used_elem = used + VRING_USED_HEADER_SIZE +
                                   ((used_idx % vring_num) * VRING_USED_ELEM_SIZE);
        mem_write32(used_elem, desc_id);
        mem_write32(used_elem + sizeof(uint32_t), message.size());
        mem_write16(used + sizeof(uint16_t), used_idx + 1);
        m_rpmsg_ns_last_avail_idx = avail_idx;
        m_rpmsg_ns_sent = true;

        std::ostringstream detail;
        detail << "name=" << p_rpmsg_ns_name.get_value()
               << " remote_addr=0x" << std::hex << p_rpmsg_ns_remote_addr.get_value()
               << " vring=0x" << vring
               << " desc_id=" << std::dec << desc_id
               << " buffer=0x" << std::hex << buffer_addr
               << " used_idx=" << std::dec << used_idx + 1
               << " msg_len=" << message.size();
        trace_event("rpmsg-ns-injected", detail.str());

        if (auto mbx = paired_mbx()) {
            const uint64_t signal_delay_ns = p_rpmsg_ns_signal_delay_ns.get_value();
            if (signal_delay_ns != 0) {
                std::ostringstream signal_detail;
                signal_detail << "delay_ns=" << signal_delay_ns;
                trace_event("rpmsg-ns-signal-delay", signal_detail.str());
                wait(sc_core::sc_time(signal_delay_ns, sc_core::SC_NS));
            }
            mbx->signal_doorbell_channel(p_rpmsg_ns_signal_channel.get_value(),
                                          p_rpmsg_ns_signal_value.get_value());
            trace_event("rpmsg-ns-signaled");
        }

        return true;
    }

    void schedule_rpmsg_ns_injection()
    {
        if (!p_rpmsg_ns_enable.get_value() || m_rpmsg_ns_sent) {
            return;
        }

        m_rpmsg_ns_pending = true;
        m_rpmsg_ns_event.notify(sc_core::SC_ZERO_TIME);
        trace_event("rpmsg-ns-scheduled");
    }

    void rpmsg_ns_worker()
    {
        for (;;) {
            wait(m_rpmsg_ns_event);
            if (!m_rpmsg_ns_pending) {
                continue;
            }

            while (m_rpmsg_ns_pending && !m_rpmsg_ns_sent) {
                m_rpmsg_ns_pending = false;
                for (unsigned int poll = 0; poll < p_rpmsg_ns_max_polls.get_value();
                     ++poll) {
                    if (inject_rpmsg_ns_once()) {
                        break;
                    }
                    wait(sc_core::sc_time(p_rpmsg_ns_poll_period_ns.get_value(),
                                          sc_core::SC_NS));
                }

                if (!m_rpmsg_ns_sent) {
                    trace_event("rpmsg-ns-poll-timeout");
                }
            }
        }
    }

    void set_mbx_mask(unsigned int channel, uint32_t mask)
    {
        m_frame.set_mask_bits(channel, mask);
        update_combined_irq();
    }

    void clear_mbx_mask(unsigned int channel, uint32_t mask)
    {
        m_frame.clear_mask_bits(channel, mask);
        update_combined_irq();
    }

    void write_postbox_doorbell(unsigned int channel, uint32_t value)
    {
        m_frame.set_status_bits(channel, value);
        update_combined_irq();

        std::ostringstream detail;
        detail << "channel=" << channel
               << " value=0x" << std::hex << value
               << " status=0x" << m_frame.status(channel) << std::dec;
        trace_event("postbox-doorbell-write", detail.str());

        if (p_protocol.get_value() == "rse-ps-proxy") {
            if (channel == notify_channel() && value == MHU_NOTIFY_VALUE) {
                const auto request = read_doorbell_message();
                std::ostringstream ps_detail;
                ps_detail << "bytes=" << request.size();
                trace_event("rse-ps-request", ps_detail.str());

                const auto reply = build_rse_ps_reply(request);
                clear_postbox_transfer();
                if (auto mbx = paired_mbx()) {
                    mbx->load_mbx_message(reply);
                }
            }
            return;
        }

        if (p_protocol.get_value() != "doorbell") {
            if (auto mbx = paired_mbx()) {
                mbx->signal_doorbell_channel(channel, value);
            }
        } else if (p_doorbell_ack_trigger_value.get_value() != 0 &&
                   channel == p_doorbell_ack_trigger_channel.get_value() &&
                   (value & p_doorbell_ack_trigger_value.get_value()) ==
                       p_doorbell_ack_trigger_value.get_value()) {
            if (auto mbx = paired_mbx()) {
                const unsigned int ack_channel = p_doorbell_ack_channel.get_value();
                const uint32_t ack_value = p_doorbell_ack_value.get_value();

                std::ostringstream ack_detail;
                ack_detail << "trigger_channel=" << channel
                           << " trigger_value=0x" << std::hex
                           << p_doorbell_ack_trigger_value.get_value()
                           << " ack_channel=" << std::dec << ack_channel
                           << " ack_value=0x" << std::hex << ack_value
                           << " mbx=" << mbx->name() << std::dec;
                trace_event("doorbell-auto-ack", ack_detail.str());
                seed_doorbell_ack_memory();
                mbx->signal_doorbell_channel(ack_channel, ack_value);
                complete_synthetic_postbox_transfer(channel, value,
                                                    "doorbell-auto-ack");
                trace_event("rpmsg-ns-defer-until-host-kick");
            }
        } else if (p_protocol.get_value() == "doorbell") {
            schedule_rpmsg_ns_injection();
            if (p_rpmsg_ns_enable.get_value()) {
                complete_synthetic_postbox_transfer(channel, value,
                                                    "rpmsg-ns-scheduled");
            }
        }
    }

    void clear_postbox_interrupt(unsigned int channel, uint32_t mask)
    {
        m_frame.clear_int_status_bits(channel, mask);
        update_combined_irq();
    }

    void complete_synthetic_postbox_transfer(unsigned int channel,
                                             uint32_t mask,
                                             const char* reason)
    {
        if (channel >= channel_count() || mask == 0) {
            return;
        }

        m_synthetic_postbox_completions.push_back(
            { channel, mask, std::string(reason) });

        std::ostringstream detail;
        detail << "channel=" << channel
               << " mask=0x" << std::hex << mask
               << " reason=" << reason
               << std::dec << " delay_ns="
               << p_synthetic_txdone_delay_ns.get_value();
        trace_event("postbox-synthetic-tx-complete-scheduled", detail.str());

        const uint64_t delay_ns = p_synthetic_txdone_delay_ns.get_value();
        m_synthetic_postbox_completion_event.notify(
            delay_ns == 0 ? sc_core::SC_ZERO_TIME
                          : sc_core::sc_time(delay_ns, sc_core::SC_NS));
    }

    void emit_synthetic_postbox_completions()
    {
        while (!m_synthetic_postbox_completions.empty()) {
            const auto completion = m_synthetic_postbox_completions.front();
            m_synthetic_postbox_completions.pop_front();

            std::ostringstream detail;
            detail << "channel=" << completion.channel
                   << " mask=0x" << std::hex << completion.mask
                   << " reason=" << completion.reason << std::dec;
            trace_event("postbox-synthetic-tx-complete", detail.str());

            clear_postbox_doorbell_channel(completion.channel,
                                           completion.mask);
        }
    }

    std::vector<uint8_t> read_doorbell_message() const
    {
        const uint32_t length = m_frame.status(0);
        std::vector<uint8_t> msg;
        const unsigned int notify = notify_channel();

        msg.reserve(length);
        for (unsigned int channel = 1; channel < notify && msg.size() < length;
             ++channel) {
            uint32_t word = m_frame.status(channel);
            for (unsigned int byte = 0; byte < sizeof(word) && msg.size() < length;
                 ++byte) {
                msg.push_back((word >> (byte * 8)) & 0xffu);
            }
        }

        return msg;
    }

    static std::vector<uint8_t> le32_bytes(uint32_t value)
    {
        std::vector<uint8_t> out;
        append_u32(out, value);
        return out;
    }

    bool parse_rse_ps_embed_request(const std::vector<uint8_t>& request,
                                    rse_ps_request& out,
                                    int32_t& error_status)
    {
        if (request.size() < 4 + sizeof(uint32_t) * 2 +
                                 sizeof(uint16_t) * PSA_MAX_IOVEC) {
            error_status = PSA_ERROR_INVALID_ARGUMENT;
            return false;
        }

        out.handle = read_le32(request, 4);
        out.ctrl = read_le32(request, 8);
        out.type = rse_ctrl_type(out.ctrl);
        out.in_len = rse_ctrl_in_len(out.ctrl);
        out.out_len = rse_ctrl_out_len(out.ctrl);
        if (out.in_len > PSA_MAX_IOVEC || out.out_len > PSA_MAX_IOVEC ||
            out.in_len + out.out_len > PSA_MAX_IOVEC) {
            error_status = PSA_ERROR_INVALID_ARGUMENT;
            return false;
        }

        std::array<uint32_t, PSA_MAX_IOVEC> io_sizes {};
        size_t offset = 12;
        for (unsigned int i = 0; i < PSA_MAX_IOVEC; ++i) {
            io_sizes[i] = read_le16(request, offset);
            offset += sizeof(uint16_t);
        }

        for (unsigned int i = 0; i < out.in_len; ++i) {
            if (offset + io_sizes[i] > request.size()) {
                error_status = PSA_ERROR_INVALID_ARGUMENT;
                return false;
            }
            out.inputs[i].assign(request.begin() + offset,
                                 request.begin() + offset + io_sizes[i]);
            offset += io_sizes[i];
        }
        for (unsigned int i = 0; i < out.out_len; ++i) {
            out.out_caps[i] = io_sizes[out.in_len + i];
        }

        return true;
    }

    bool parse_rse_ps_pointer_request(const std::vector<uint8_t>& request,
                                      rse_ps_request& out,
                                      int32_t& error_status)
    {
        const size_t min_size = 4 + sizeof(uint32_t) * 2 +
                                sizeof(uint32_t) * PSA_MAX_IOVEC +
                                sizeof(uint64_t) * PSA_MAX_IOVEC;
        if (request.size() < min_size) {
            error_status = PSA_ERROR_INVALID_ARGUMENT;
            return false;
        }

        out.handle = read_le32(request, 4);
        out.ctrl = read_le32(request, 8);
        out.type = rse_ctrl_type(out.ctrl);
        out.in_len = rse_ctrl_in_len(out.ctrl);
        out.out_len = rse_ctrl_out_len(out.ctrl);
        if (out.in_len > PSA_MAX_IOVEC || out.out_len > PSA_MAX_IOVEC ||
            out.in_len + out.out_len > PSA_MAX_IOVEC) {
            error_status = PSA_ERROR_INVALID_ARGUMENT;
            return false;
        }

        std::array<uint32_t, PSA_MAX_IOVEC> io_sizes {};
        std::array<uint64_t, PSA_MAX_IOVEC> host_ptrs {};
        size_t offset = 12;
        for (unsigned int i = 0; i < PSA_MAX_IOVEC; ++i) {
            io_sizes[i] = read_le32(request, offset);
            offset += sizeof(uint32_t);
        }
        for (unsigned int i = 0; i < PSA_MAX_IOVEC; ++i) {
            host_ptrs[i] = read_le64(request, offset);
            offset += sizeof(uint64_t);
        }

        for (unsigned int i = 0; i < out.in_len; ++i) {
            out.inputs[i].resize(io_sizes[i]);
            if (!mem_read_bytes(host_ptrs[i], out.inputs[i])) {
                error_status = PSA_ERROR_COMMUNICATION_FAILURE;
                return false;
            }
        }
        for (unsigned int i = 0; i < out.out_len; ++i) {
            const unsigned int source = out.in_len + i;
            out.out_caps[i] = io_sizes[source];
            out.out_ptrs[i] = host_ptrs[source];
        }

        return true;
    }

    bool parse_rse_ps_request(const std::vector<uint8_t>& request,
                              rse_ps_request& out,
                              int32_t& error_status)
    {
        if (request.size() < 4) {
            error_status = PSA_ERROR_INVALID_ARGUMENT;
            return false;
        }

        out.protocol = request[0];
        out.seq = request[1];
        out.client_id = static_cast<uint16_t>(request[2]) |
                        (static_cast<uint16_t>(request[3]) << 8);

        if (out.protocol == RSE_COMMS_PROTOCOL_EMBED) {
            return parse_rse_ps_embed_request(request, out, error_status);
        }
        if (out.protocol == RSE_COMMS_PROTOCOL_POINTER_ACCESS) {
            return parse_rse_ps_pointer_request(request, out, error_status);
        }

        error_status = PSA_ERROR_NOT_SUPPORTED;
        return false;
    }

    int32_t set_ps_output(const rse_ps_request& request,
                          std::array<std::vector<uint8_t>, PSA_MAX_IOVEC>& outputs,
                          std::array<uint32_t, PSA_MAX_IOVEC>& out_sizes,
                          unsigned int index,
                          const std::vector<uint8_t>& value) const
    {
        if (index >= request.out_len || index >= PSA_MAX_IOVEC) {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
        if (value.size() > request.out_caps[index]) {
            return PSA_ERROR_INVALID_ARGUMENT;
        }

        outputs[index] = value;
        out_sizes[index] = static_cast<uint32_t>(value.size());
        return PSA_SUCCESS;
    }

    static std::vector<uint8_t> bounded_bytes(const std::vector<uint8_t>& in,
                                              size_t offset,
                                              size_t size,
                                              size_t max_size)
    {
        if (offset >= in.size()) {
            return {};
        }
        const size_t available = in.size() - offset;
        const size_t count = std::min(std::min(size, max_size), available);
        return std::vector<uint8_t>(in.begin() + offset,
                                    in.begin() + offset + count);
    }

    int32_t handle_rse_measured_boot_request(
        const rse_ps_request& request,
        std::array<std::vector<uint8_t>, PSA_MAX_IOVEC>& outputs,
        std::array<uint32_t, PSA_MAX_IOVEC>& out_sizes)
    {
        switch (request.type) {
        case TFM_MEASURED_BOOT_EXTEND: {
            if (request.in_len < 4 || request.inputs[0].size() < 41) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }

            const uint8_t index = request.inputs[0][0];
            if (index >= m_measurements.size()) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }

            measurement_entry& entry = m_measurements[index];
            entry.valid = true;
            entry.locked = request.inputs[0][1] != 0;
            entry.algorithm = read_le32(request.inputs[0], 4);
            const uint8_t sw_type_size = request.inputs[0][40];
            entry.sw_type = bounded_bytes(request.inputs[0], 8, sw_type_size,
                                          TFM_SW_TYPE_MAX_SIZE);
            entry.signer_id = bounded_bytes(request.inputs[1], 0,
                                            request.inputs[1].size(),
                                            TFM_SIGNER_ID_MAX_SIZE);
            entry.version = bounded_bytes(request.inputs[2], 0,
                                          request.inputs[2].size(),
                                          TFM_VERSION_MAX_SIZE);
            entry.measurement = bounded_bytes(request.inputs[3], 0,
                                              request.inputs[3].size(),
                                              TFM_MEASUREMENT_VALUE_MAX_SIZE);
            return PSA_SUCCESS;
        }

        case TFM_MEASURED_BOOT_READ: {
            if (request.in_len < 1 || request.out_len < 1 ||
                request.inputs[0].empty()) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }

            const uint8_t index = request.inputs[0][0];
            if (index >= m_measurements.size() || !m_measurements[index].valid) {
                return PSA_ERROR_DOES_NOT_EXIST;
            }

            const measurement_entry& entry = m_measurements[index];
            std::vector<uint8_t> metadata(56, 0);
            metadata[0] = entry.locked ? 1 : 0;
            metadata[4] = entry.algorithm & 0xffu;
            metadata[5] = (entry.algorithm >> 8) & 0xffu;
            metadata[6] = (entry.algorithm >> 16) & 0xffu;
            metadata[7] = (entry.algorithm >> 24) & 0xffu;
            std::copy(entry.sw_type.begin(), entry.sw_type.end(),
                      metadata.begin() + 8);
            metadata[40] = static_cast<uint8_t>(entry.sw_type.size());
            std::copy(entry.version.begin(), entry.version.end(),
                      metadata.begin() + 41);
            metadata[55] = static_cast<uint8_t>(entry.version.size());

            int32_t status = set_ps_output(request, outputs, out_sizes, 0, metadata);
            if (status != static_cast<int32_t>(PSA_SUCCESS) || request.out_len < 2) {
                return status;
            }
            status = set_ps_output(request, outputs, out_sizes, 1,
                                   entry.measurement);
            if (status != static_cast<int32_t>(PSA_SUCCESS) || request.out_len < 3) {
                return status;
            }
            return set_ps_output(request, outputs, out_sizes, 2, entry.signer_id);
        }

        default:
            return PSA_ERROR_NOT_SUPPORTED;
        }
    }

    int32_t handle_rse_ps_request(
        const rse_ps_request& request,
        std::array<std::vector<uint8_t>, PSA_MAX_IOVEC>& outputs,
        std::array<uint32_t, PSA_MAX_IOVEC>& out_sizes)
    {
        if (request.handle == TFM_MEASURED_BOOT_HANDLE) {
            return handle_rse_measured_boot_request(request, outputs, out_sizes);
        }

        if (request.handle != TFM_PROTECTED_STORAGE_SERVICE_HANDLE) {
            return PSA_ERROR_NOT_SUPPORTED;
        }

        switch (request.type) {
        case TFM_PS_SET:
            if (request.in_len < 3 || request.inputs[0].size() < sizeof(uint64_t) ||
                request.inputs[2].size() < sizeof(uint32_t)) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            m_ps_store[read_le64(request.inputs[0], 0)] = {
                request.inputs[1],
                read_le32(request.inputs[2], 0)
            };
            return PSA_SUCCESS;

        case TFM_PS_GET: {
            if (request.in_len < 2 || request.out_len < 1 ||
                request.inputs[0].size() < sizeof(uint64_t) ||
                request.inputs[1].size() < sizeof(uint32_t)) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            const uint64_t uid = read_le64(request.inputs[0], 0);
            const uint32_t offset = read_le32(request.inputs[1], 0);
            const auto it = m_ps_store.find(uid);
            if (it == m_ps_store.end()) {
                return PSA_ERROR_DOES_NOT_EXIST;
            }
            if (offset > it->second.data.size()) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }

            const size_t available = it->second.data.size() - offset;
            const size_t copy_len =
                std::min<size_t>(available, request.out_caps[0]);
            std::vector<uint8_t> out(it->second.data.begin() + offset,
                                     it->second.data.begin() + offset + copy_len);
            return set_ps_output(request, outputs, out_sizes, 0, out);
        }

        case TFM_PS_GET_INFO: {
            if (request.in_len < 1 || request.out_len < 1 ||
                request.inputs[0].size() < sizeof(uint64_t)) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            const uint64_t uid = read_le64(request.inputs[0], 0);
            const auto it = m_ps_store.find(uid);
            if (it == m_ps_store.end()) {
                return PSA_ERROR_DOES_NOT_EXIST;
            }

            std::vector<uint8_t> info;
            const auto size = static_cast<uint32_t>(it->second.data.size());
            append_u32(info, size);
            append_u32(info, size);
            append_u32(info, it->second.flags);
            return set_ps_output(request, outputs, out_sizes, 0, info);
        }

        case TFM_PS_REMOVE:
            if (request.in_len < 1 || request.inputs[0].size() < sizeof(uint64_t)) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            if (m_ps_store.erase(read_le64(request.inputs[0], 0)) == 0) {
                return PSA_ERROR_DOES_NOT_EXIST;
            }
            return PSA_SUCCESS;

        case TFM_PS_GET_SUPPORT:
            if (request.out_len < 1) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            return set_ps_output(request, outputs, out_sizes, 0, le32_bytes(0));

        default:
            return PSA_ERROR_NOT_SUPPORTED;
        }
    }

    std::vector<uint8_t> build_rse_ps_reply(const std::vector<uint8_t>& request)
    {
        rse_ps_request parsed;
        int32_t status = PSA_SUCCESS;
        std::array<std::vector<uint8_t>, PSA_MAX_IOVEC> outputs;
        std::array<uint32_t, PSA_MAX_IOVEC> out_sizes {};

        if (parse_rse_ps_request(request, parsed, status)) {
            status = handle_rse_ps_request(parsed, outputs, out_sizes);
        } else if (request.size() >= 4) {
            parsed.protocol = request[0];
            parsed.seq = request[1];
            parsed.client_id = static_cast<uint16_t>(request[2]) |
                               (static_cast<uint16_t>(request[3]) << 8);
        }

        if (status == static_cast<int32_t>(PSA_SUCCESS) &&
            parsed.protocol == RSE_COMMS_PROTOCOL_POINTER_ACCESS) {
            for (unsigned int i = 0; i < parsed.out_len; ++i) {
                if (!mem_write_bytes(parsed.out_ptrs[i], outputs[i])) {
                    status = PSA_ERROR_COMMUNICATION_FAILURE;
                    out_sizes.fill(0);
                    break;
                }
            }
        }

        std::ostringstream detail;
        detail << "protocol=" << static_cast<unsigned int>(parsed.protocol)
               << " seq=" << static_cast<unsigned int>(parsed.seq)
               << " handle=0x" << std::hex << parsed.handle
               << " type=0x" << parsed.type
               << " status=0x" << static_cast<uint32_t>(status)
               << std::dec << " in_len=" << parsed.in_len
               << " out_len=" << parsed.out_len;
        trace_event("rse-ps-response", detail.str());

        std::vector<uint8_t> reply;
        reply.push_back(parsed.protocol);
        reply.push_back(parsed.seq);
        append_u16(reply, parsed.client_id);
        append_u32(reply, static_cast<uint32_t>(status));
        if (parsed.protocol == RSE_COMMS_PROTOCOL_POINTER_ACCESS) {
            for (unsigned int i = 0; i < PSA_MAX_IOVEC; ++i) {
                append_u32(reply, out_sizes[i]);
            }
        } else {
            for (unsigned int i = 0; i < PSA_MAX_IOVEC; ++i) {
                append_u16(reply, static_cast<uint16_t>(out_sizes[i]));
            }
            if (status == static_cast<int32_t>(PSA_SUCCESS)) {
                for (unsigned int i = 0; i < parsed.out_len; ++i) {
                    reply.insert(reply.end(), outputs[i].begin(), outputs[i].end());
                }
            }
        }

        return reply;
    }

    std::vector<uint8_t> build_rse_success_reply(const std::vector<uint8_t>& request) const
    {
        std::vector<uint8_t> reply;
        const uint8_t protocol = request.size() >= 1 ? request[0] : RSE_COMMS_PROTOCOL_EMBED;
        const uint8_t seq = request.size() >= 2 ? request[1] : 0;
        const uint8_t client_lo = request.size() >= 3 ? request[2] : 0;
        const uint8_t client_hi = request.size() >= 4 ? request[3] : 0;

        reply.push_back(protocol);
        reply.push_back(seq);
        reply.push_back(client_lo);
        reply.push_back(client_hi);

        append_u32(reply, PSA_SUCCESS);
        if (protocol == RSE_COMMS_PROTOCOL_POINTER_ACCESS) {
            for (unsigned int i = 0; i < PSA_MAX_IOVEC; ++i) {
                append_u32(reply, 0);
            }
        } else {
            for (unsigned int i = 0; i < PSA_MAX_IOVEC; ++i) {
                append_u16(reply, 0);
            }
        }

        return reply;
    }

    void clear_postbox_transfer()
    {
        m_frame.clear_all_status();
        const unsigned int notify = notify_channel();
        m_frame.set_int_status_bits(notify, 1);
        update_combined_irq();
    }

    void load_mbx_message(const std::vector<uint8_t>& msg)
    {
        m_frame.clear_all_status();

        m_frame.set_status(0, msg.size());
        unsigned int channel = 1;
        const unsigned int notify = notify_channel();
        for (size_t offset = 0; offset < msg.size() && channel < notify;
             offset += sizeof(uint32_t), ++channel) {
            uint32_t word = 0;
            const size_t chunk = std::min<size_t>(sizeof(word), msg.size() - offset);
            for (size_t byte = 0; byte < chunk; ++byte) {
                word |= static_cast<uint32_t>(msg[offset + byte]) << (byte * 8);
            }
            m_frame.set_status(channel, word);
        }
        m_frame.set_status(notify, MHU_NOTIFY_VALUE);
        update_combined_irq();
    }

    void respond_rse_doorbell()
    {
        const auto request = read_doorbell_message();
        const auto reply = build_rse_success_reply(request);

        clear_postbox_transfer();
        if (auto mbx = paired_mbx()) {
            mbx->load_mbx_message(reply);
        }
    }

    void write32(uint64_t offset, uint32_t value)
    {
        unsigned int channel = 0;
        uint64_t reg_offset = 0;
        if (decode_dbch_offset(offset, channel, reg_offset)) {
            if (!is_mbx()) {
                if (reg_offset == DBCW_SET) {
                    if (p_protocol.get_value() == "scmi" &&
                        scmi_channel_in_range(channel)) {
                        respond_scmi(channel);
                    } else {
                        write_postbox_doorbell(channel, value);
                        if (p_direct_boot_compat.get_value() &&
                            p_protocol.get_value() == "doorbell" &&
                            channel == notify_channel() &&
                            value == MHU_NOTIFY_VALUE) {
                            respond_rse_doorbell();
                        }
                    }
                    return;
                }

                if (reg_offset == DBCW_INT_CLR) {
                    clear_postbox_interrupt(channel, value);
                    return;
                }

                if (reg_offset == DBCW_INT_EN) {
                    m_frame.set_int_enable(channel, value);
                    update_combined_irq();
                    return;
                }

                if (reg_offset == DBCW_CTRL) {
                    m_frame.set_ctrl(channel, value);
                    update_combined_irq();
                    return;
                }
            } else {
                if (reg_offset == DBCW_CLR) {
                    clear_mbx_doorbell_channel(channel, value);
                    return;
                }

                if (reg_offset == DBCW_INT_CLR) {
                    set_mbx_mask(channel, value);
                    return;
                }

                if (reg_offset == DBCW_INT_EN) {
                    clear_mbx_mask(channel, value);
                    return;
                }

                if (reg_offset == DBCW_CTRL) {
                    m_frame.set_ctrl(channel, value);
                    update_combined_irq();
                    return;
                }
            }
        }

        store<uint32_t>(offset, value);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        trans.set_dmi_allowed(false);
        if (offset + len > REG_SPACE_SIZE || (len != 1 && len != 2 && len != 4 && len != 8)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            if (len == 4) {
                const uint32_t value = read32(offset);
                std::memcpy(data, &value, sizeof(value));
            } else {
                m_frame.copy_read(offset, data, len);
            }
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            if (len == 4) {
                uint32_t value;
                std::memcpy(&value, data, sizeof(value));
                write32(offset, value);
            } else {
                m_frame.copy_write(offset, data, len);
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

protected:
    void end_of_elaboration() override
    {
        const bool mbx = is_mbx();

        m_frame.configure(mbx, channel_count(), p_feat_spt0.get_value(),
                          p_feat_spt1.get_value(), p_iidr.get_value(),
                          p_aidr.get_value());
        if (mbx) {
            s_mbx = this;
            s_mbx_by_pair[p_pair.get_value()] = this;
        } else {
            s_pbx = this;
            s_pbx_by_pair[p_pair.get_value()] = this;
        }
        update_combined_irq();
    }

    void start_of_simulation() override
    {
        if (!is_mbx() && p_init_shmem.get_value()) {
            for (unsigned int channel = 0; channel < p_scmi_channel_count.get_value();
                 ++channel) {
                mem_write32(p_tx_shmem.get_value() +
                                (p_scmi_channel_stride.get_value() * channel) +
                                SCMI_CHAN_STATUS,
                            SCMI_CHAN_FREE);
            }
            mem_write32(p_rx_shmem.get_value() + SCMI_CHAN_STATUS, SCMI_CHAN_FREE);
        }
        if (!is_mbx() && p_assert_power_on_reset.get_value()) {
            schedule_power_on_reset(true);
        }
    }

public:
    SC_HAS_PROCESS(mhu320ae);

    tlm_utils::simple_target_socket<mhu320ae, DEFAULT_TLM_BUSWIDTH> target_socket;
    tlm_utils::simple_initiator_socket<mhu320ae, DEFAULT_TLM_BUSWIDTH> initiator_socket;
    InitiatorSignalSocket<bool> irq;
    InitiatorSignalSocket<bool> power_on_reset;
    MultiInitiatorSignalSocket<> system_reset;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> power_domain_reset;

    explicit mhu320ae(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_pair("pair", std::string(""))
        , p_protocol("protocol", std::string("scmi"))
        , p_frame("frame", std::string("pbx"))
        , p_channel_count("channel_count", DBCH_CHANNELS)
        , p_feat_spt0("feat_spt0", 1)
        , p_feat_spt1("feat_spt1", 0)
        , p_iidr("iidr", 0x0000043b)
        , p_aidr("aidr", 0x20)
        , p_direct_boot_compat("direct_boot_compat", false)
        , p_scmi_transport("scmi_transport", std::string("mailbox"))
        , p_tx_shmem("tx_shmem", 0x00180000)
        , p_rx_shmem("rx_shmem", 0x00180100)
        , p_scmi_channel_stride("scmi_channel_stride", 0)
        , p_scmi_channel_base_index("scmi_channel_base_index", 0)
        , p_scmi_channel_count("scmi_channel_count", 1)
        , p_init_shmem("init_shmem", true)
        , p_ack_bit("ack_bit", 0)
        , p_power_domain_version("power_domain_version", 0x00020000)
        , p_sys_power_version("sys_power_version", 0x00020000)
        , p_power_domain_attributes("power_domain_attributes", 0)
        , p_power_domain_name("power_domain_name", std::string("AP"))
        , p_assert_power_on_reset("assert_power_on_reset", false)
        , p_power_domain_reset_count("power_domain_reset_count", 0)
        , p_power_domain_reset_delay_ns("power_domain_reset_delay_ns", 1)
        , p_power_domain_reset_assert_on_power_off("power_domain_reset_assert_on_power_off", true)
        , p_power_domain_reset_pulse_on_power_on("power_domain_reset_pulse_on_power_on", false)
        , p_system_power_reset_delay_ns("system_power_reset_delay_ns", 1)
        , p_system_power_reset_pulse_width_ns("system_power_reset_pulse_width_ns", 1)
        , p_doorbell_ack_trigger_channel("doorbell_ack_trigger_channel", 0)
        , p_doorbell_ack_trigger_value("doorbell_ack_trigger_value", 0)
        , p_doorbell_ack_channel("doorbell_ack_channel", 0)
        , p_doorbell_ack_value("doorbell_ack_value", 0)
        , p_doorbell_ack_seed_address("doorbell_ack_seed_address", 0)
        , p_doorbell_ack_seed_words("doorbell_ack_seed_words", std::vector<unsigned int>())
        , p_rpmsg_ns_enable("rpmsg_ns_enable", false)
        , p_rpmsg_ns_name("rpmsg_ns_name", std::string(""))
        , p_rpmsg_ns_remote_addr("rpmsg_ns_remote_addr", 1024)
        , p_rpmsg_ns_vring_address("rpmsg_ns_vring_address", 0)
        , p_rpmsg_ns_vring_num("rpmsg_ns_vring_num", 32)
        , p_rpmsg_ns_vring_align("rpmsg_ns_vring_align", 16)
        , p_rpmsg_ns_signal_channel("rpmsg_ns_signal_channel", 0)
        , p_rpmsg_ns_signal_value("rpmsg_ns_signal_value", 1)
        , p_rpmsg_ns_signal_delay_ns("rpmsg_ns_signal_delay_ns", 0)
        , p_rpmsg_ns_poll_period_ns("rpmsg_ns_poll_period_ns", 100000)
        , p_rpmsg_ns_max_polls("rpmsg_ns_max_polls", 1000)
        , p_synthetic_txdone_delay_ns("synthetic_txdone_delay_ns", 1000)
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 256)
        , p_trace_file("trace_file", std::string(""))
        , target_socket("target_socket")
        , initiator_socket("initiator_socket")
        , irq("irq")
        , power_on_reset("power_on_reset")
        , system_reset("system_reset")
        , power_domain_reset("power_domain_reset", p_power_domain_reset_count.get_value(),
                             [](const char* n, size_t) { return new InitiatorSignalSocket<bool>(n); })
    {
        m_doorbell_ack_seed_words = load_doorbell_ack_seed_words();

        SC_METHOD(emit_power_on_reset);
        sensitive << m_power_on_reset_event;
        dont_initialize();

        SC_METHOD(emit_synthetic_postbox_completions);
        sensitive << m_synthetic_postbox_completion_event;
        dont_initialize();

        SC_THREAD(emit_power_domain_reset);
        SC_THREAD(emit_system_power_reset);
        SC_THREAD(rpmsg_ns_worker);

        target_socket.register_b_transport(this, &mhu320ae::b_transport);
    }
};

extern "C" void module_register();
