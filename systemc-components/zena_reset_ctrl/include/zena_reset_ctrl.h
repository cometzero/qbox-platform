/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class zena_reset_ctrl : public sc_core::sc_module
{
    static constexpr uint32_t RGM_CTRL = 0x010;
    static constexpr uint32_t RGM_RST_SYNDROME = 0x020;
    static constexpr uint32_t RGM_RST_MASK = 0x030;
    static constexpr uint32_t TRI_REDT_INTR = 0xe00;
    static constexpr uint32_t PIK_CONFIG = 0xfc0;
    static constexpr uint32_t SID_PID_4 = 0xfd0;
    static constexpr uint32_t SID_PID_0 = 0xfe0;
    static constexpr uint32_t SID_PID_1 = 0xfe4;
    static constexpr uint32_t SID_PID_2 = 0xfe8;
    static constexpr uint32_t SID_PID_3 = 0xfec;
    static constexpr uint32_t COMPID0 = 0xff0;
    static constexpr uint32_t COMPID1 = 0xff4;
    static constexpr uint32_t COMPID2 = 0xff8;
    static constexpr uint32_t COMPID3 = 0xffc;
    static constexpr uint32_t CLKFORCE_STATUS = 0xa00;
    static constexpr uint32_t CLKFORCE_SET = 0xa04;
    static constexpr uint32_t CLKFORCE_CLR = 0xa08;
    static constexpr uint32_t SYS_PWR_REQ_ST = 0xc00;
    static constexpr uint32_t SYS_PWR_ACK = 0xc04;
    static constexpr uint32_t SYS_RST_REQ_ST = 0xc08;
    static constexpr uint32_t SYS_RST_ACK = 0xc0c;
    static constexpr uint32_t RGM_EXP0RESET = 1u << 24;
    static constexpr uint32_t RGM_RSECOLDRESET = 1u << 8;
    static constexpr uint32_t RGM_EXTCOLDRESET = 1u << 5;
    static constexpr uint64_t REG_BYTES = 0x1000;

    uint32_t m_rgm_syndrome = 0;
    uint32_t m_rgm_mask = 0;
    uint32_t m_clkforce = 0;
    uint32_t m_power_request = 0;
    uint32_t m_power_ack = 0;
    uint32_t m_reset_request = 0;
    uint32_t m_reset_ack = 0;
    bool m_ap_watchdog_reset = false;
    bool m_ap_power_reset = false;
    bool m_safety_fault_reset = false;
    std::array<uint32_t, 16> m_clock_ctrl{};

    static uint32_t identity(uint32_t offset)
    {
        switch (offset) {
        case PIK_CONFIG:
            return 0x00000001;
        case SID_PID_4:
            return 0x00000002;
        case SID_PID_0:
            return 0x00000000;
        case SID_PID_1:
            return 0x000b0000;
        case SID_PID_2:
            return 0x0000000b;
        case SID_PID_3:
            return 0x00000000;
        case COMPID0:
            return 0x0000000d;
        case COMPID1:
            return 0x000000f0;
        case COMPID2:
            return 0x00000005;
        case COMPID3:
            return 0x000000b1;
        default:
            return 0;
        }
    }

    static int clock_ctrl_index(uint32_t offset)
    {
        static constexpr std::array<uint32_t, 16> offsets = {
            0x800, 0x804, 0x808, 0x80c, 0x840, 0x850, 0x854, 0x858,
            0x85c, 0x860, 0x870, 0x874, 0x880, 0x890, 0x8a0, 0x8b0,
        };
        for (std::size_t index = 0; index < offsets.size(); ++index) {
            if (offsets[index] == offset) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    uint32_t read_rgm(uint32_t offset) const
    {
        switch (offset) {
        case RGM_CTRL:
            return 0;
        case RGM_RST_SYNDROME:
            return m_rgm_syndrome;
        case RGM_RST_MASK:
            return m_rgm_mask;
        case TRI_REDT_INTR:
            return 0;
        default:
            return identity(offset);
        }
    }

    void write_rgm(uint32_t offset, uint32_t value)
    {
        if (offset == RGM_RST_SYNDROME) {
            m_rgm_syndrome = value &
                (RGM_EXP0RESET | RGM_RSECOLDRESET | RGM_EXTCOLDRESET);
        } else if (offset == RGM_RST_MASK) {
            m_rgm_mask = value & RGM_EXP0RESET;
            update_ap_reset();
        }
    }

    uint32_t read_pik(uint32_t offset) const
    {
        const int index = clock_ctrl_index(offset);
        if (index >= 0) {
            return m_clock_ctrl[static_cast<std::size_t>(index)];
        }
        switch (offset) {
        case CLKFORCE_STATUS:
            return m_clkforce;
        case SYS_PWR_REQ_ST:
            return m_power_request;
        case SYS_PWR_ACK:
            return m_power_ack;
        case SYS_RST_REQ_ST:
            return m_reset_request;
        case SYS_RST_ACK:
            return m_reset_ack;
        default:
            return identity(offset);
        }
    }

    void write_pik(uint32_t offset, uint32_t value)
    {
        const int index = clock_ctrl_index(offset);
        if (index >= 0) {
            m_clock_ctrl[static_cast<std::size_t>(index)] = value;
            return;
        }
        switch (offset) {
        case CLKFORCE_SET:
            m_clkforce |= value;
            break;
        case CLKFORCE_CLR:
            m_clkforce &= ~value;
            break;
        case SYS_PWR_ACK:
            m_power_ack = value;
            if (power_ack.size() != 0) {
                power_ack->write(value != 0);
            }
            break;
        case SYS_RST_ACK:
            m_reset_ack = value;
            if (reset_ack.size() != 0) {
                reset_ack->write(value != 0);
            }
            break;
        default:
            break;
        }
    }

    bool access(tlm::tlm_generic_payload& trans, bool pik)
    {
        const uint64_t offset = trans.get_address();
        uint8_t* data = trans.get_data_ptr();
        if (data == nullptr || trans.get_data_length() != sizeof(uint32_t) ||
            (offset & 0x3u) != 0 || offset + sizeof(uint32_t) > REG_BYTES) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        uint32_t value = 0;
        if (trans.is_read()) {
            value = pik ? read_pik(static_cast<uint32_t>(offset))
                        : read_rgm(static_cast<uint32_t>(offset));
            std::memcpy(data, &value, sizeof(value));
        } else if (trans.is_write()) {
            std::memcpy(&value, data, sizeof(value));
            if (pik) {
                write_pik(static_cast<uint32_t>(offset), value);
            } else {
                write_rgm(static_cast<uint32_t>(offset), value);
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    void update_ap_reset()
    {
        const bool safety_reset = m_safety_fault_reset &&
            (m_rgm_mask & RGM_EXP0RESET) != 0;
        if (ap_reset.size() != 0) {
            ap_reset->write(m_ap_watchdog_reset || m_ap_power_reset ||
                            safety_reset);
        }
    }

    void ap_watchdog_changed(bool asserted)
    {
        if (asserted) {
            m_rgm_syndrome |= RGM_EXTCOLDRESET;
        }
        m_ap_watchdog_reset = asserted;
        update_ap_reset();
    }

    void ap_power_changed(bool asserted)
    {
        m_ap_power_reset = asserted;
        update_ap_reset();
    }

    void safety_fault_changed(bool asserted)
    {
        if (asserted) {
            m_rgm_syndrome |= RGM_EXP0RESET;
        }
        m_safety_fault_reset = asserted;
        update_ap_reset();
    }

    void si_watchdog_changed(bool asserted)
    {
        if (asserted) {
            m_rgm_syndrome |= RGM_EXP0RESET;
        }
        const bool enabled = (m_rgm_mask & RGM_EXP0RESET) != 0;
        if (si_reset.size() != 0) {
            si_reset->write(asserted && enabled);
        }
    }

    void rse_watchdog_changed(bool asserted)
    {
        if (asserted) {
            m_rgm_syndrome |= RGM_RSECOLDRESET;
        }
        if (rse_reset.size() != 0) {
            rse_reset->write(asserted);
        }
    }

public:
    tlm_utils::simple_target_socket<zena_reset_ctrl, DEFAULT_TLM_BUSWIDTH> rgm;
    tlm_utils::simple_target_socket<zena_reset_ctrl, DEFAULT_TLM_BUSWIDTH> pik;
    TargetSignalSocket<bool> ap_ns_watchdog_reset;
    TargetSignalSocket<bool> ap_s_watchdog_reset;
    TargetSignalSocket<bool> ap_power_reset;
    TargetSignalSocket<bool> si_watchdog_reset;
    TargetSignalSocket<bool> safety_fault_reset;
    TargetSignalSocket<bool> rse_watchdog_reset;
    TargetSignalSocket<bool> power_request;
    TargetSignalSocket<bool> reset_request;
    InitiatorSignalSocket<bool> ap_reset;
    InitiatorSignalSocket<bool> si_reset;
    InitiatorSignalSocket<bool> rse_reset;
    InitiatorSignalSocket<bool> power_ack;
    InitiatorSignalSocket<bool> reset_ack;

    explicit zena_reset_ctrl(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , rgm("rgm")
        , pik("pik")
        , ap_ns_watchdog_reset("ap_ns_watchdog_reset")
        , ap_s_watchdog_reset("ap_s_watchdog_reset")
        , ap_power_reset("ap_power_reset")
        , si_watchdog_reset("si_watchdog_reset")
        , safety_fault_reset("safety_fault_reset")
        , rse_watchdog_reset("rse_watchdog_reset")
        , power_request("power_request")
        , reset_request("reset_request")
        , ap_reset("ap_reset")
        , si_reset("si_reset")
        , rse_reset("rse_reset")
        , power_ack("power_ack")
        , reset_ack("reset_ack")
    {
        m_clock_ctrl.fill(0x20000101);
        m_clock_ctrl[11] = 0x20183101;
        m_clock_ctrl[14] = 0x20000202;
        rgm.register_b_transport(this, &zena_reset_ctrl::rgm_b_transport);
        pik.register_b_transport(this, &zena_reset_ctrl::pik_b_transport);
        ap_ns_watchdog_reset.register_value_changed_cb(
            [this](bool value) { ap_watchdog_changed(value); });
        ap_s_watchdog_reset.register_value_changed_cb(
            [this](bool value) { ap_watchdog_changed(value); });
        ap_power_reset.register_value_changed_cb(
            [this](bool value) { ap_power_changed(value); });
        si_watchdog_reset.register_value_changed_cb(
            [this](bool value) { si_watchdog_changed(value); });
        safety_fault_reset.register_value_changed_cb(
            [this](bool value) { safety_fault_changed(value); });
        rse_watchdog_reset.register_value_changed_cb(
            [this](bool value) { rse_watchdog_changed(value); });
        power_request.register_value_changed_cb([this](bool value) {
            m_power_request = value ? 1u : 0u;
        });
        reset_request.register_value_changed_cb([this](bool value) {
            m_reset_request = value ? 1u : 0u;
        });
    }

    void rgm_b_transport(tlm::tlm_generic_payload& trans,
                         sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access(trans, false);
    }

    void pik_b_transport(tlm::tlm_generic_payload& trans,
                         sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access(trans, true);
    }
};

extern "C" void module_register();
