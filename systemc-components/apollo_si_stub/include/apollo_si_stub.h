/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <vector>
#include <async_event.h>
#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

/* Functional SI firmware substitute, deliberately not a CPU/Zephyr model.
 * AP MHUv3 PBX at offset 0, MBX at 0x30000. Shared resource table and
 * split virtqueues use the Apollo SI1 reserved-memory contract. Only direct,
 * single-buffer RPMsg descriptors are supported. No DMI or host pointers.
 */
class apollo_si_stub : public sc_core::sc_module
{
    static constexpr unsigned N = 32;
    static constexpr uint64_t RX = 0x120000, TX = 0x140000;
    static constexpr uint64_t AVAIL = N * 16, USED = 592;
    struct channel {
        uint32_t status = 0, mask = UINT32_MAX, ctrl = 0;
        uint32_t int_status = 0, int_enable = 0;
    };
    std::array<channel, N> m_pbx {}, m_mbx {};
    std::mutex m_lock;
    gs::async_event m_work, m_irq_event;
    bool m_attached = false, m_ns_sent = false;
    bool m_irq_pbx = false, m_irq_mbx = false;
    uint16_t m_rx_avail = 0, m_tx_avail = 0;
    std::deque<std::vector<uint8_t>> m_replies;
    uint64_t m_attach_count = 0, m_tx_count = 0, m_rx_count = 0;
    uint64_t m_error_count = 0, m_drop_count = 0;

    static uint64_t le(const uint8_t* p, unsigned n)
    {
        uint64_t v = 0;
        for (unsigned i = 0; i < n; ++i) v |= uint64_t(p[i]) << (8 * i);
        return v;
    }
    static void put(uint8_t* p, uint64_t v, unsigned n)
    {
        for (unsigned i = 0; i < n; ++i) p[i] = uint8_t(v >> (8 * i));
    }
    static uint16_t be16(const uint8_t* p) { return (unsigned(p[0]) << 8) | p[1]; }
    static void putbe(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
    static uint16_t checksum(const uint8_t* p, size_t n)
    {
        uint32_t sum = 0;
        while (n >= 2) { sum += be16(p); p += 2; n -= 2; }
        if (n) sum += unsigned(*p) << 8;
        while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
        return uint16_t(~sum);
    }
    void trace(const char* event)
    {
        if (p_trace.get_value())
            std::cout << name() << ": " << event << " attach=" << m_attach_count
                      << " tx=" << m_tx_count << " rx=" << m_rx_count
                      << " errors=" << m_error_count << " drops=" << m_drop_count << std::endl;
    }
    bool memory(uint64_t address, uint8_t* data, unsigned size, bool write)
    {
        // Restrict every DMA transaction, including malformed guest descriptors.
        if (address < 0x100000 || address >= 0x180000 ||
            size > 0x180000 - address || !initiator_socket.size()) return false;
        tlm::tlm_generic_payload t;
        t.set_address(address);
        t.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        t.set_data_ptr(data);
        t.set_data_length(size);
        t.set_streaming_width(size);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        initiator_socket->b_transport(t, delay);
        // This helper is called exclusively by the SC_THREAD worker. Payload
        // and backing bytes remain alive through transport and annotated delay.
        if (delay != sc_core::SC_ZERO_TIME) wait(delay);
        return t.is_response_ok();
    }
    bool read(uint64_t a, uint64_t& value, unsigned n)
    {
        uint8_t bytes[8] {};
        if (!memory(a, bytes, n, false)) return false;
        value = le(bytes, n);
        return true;
    }
    bool write(uint64_t a, uint64_t value, unsigned n)
    {
        uint8_t bytes[8]; put(bytes, value, n);
        return memory(a, bytes, n, true);
    }
    uint32_t combined(bool mbx)
    {
        uint32_t result = 0;
        const auto& bank = mbx ? m_mbx : m_pbx;
        for (unsigned i = 0; i < N; ++i)
            if ((bank[i].ctrl & 1) && (mbx ? (bank[i].status & ~bank[i].mask)
                                                   : (bank[i].int_status & bank[i].int_enable & 1)))
                result |= uint32_t(1) << i;
        return result;
    }
    // Called with m_lock. Deassert synchronously on guest acknowledgement,
    // as a delayed deassertion can spuriously retrigger the GIC level IRQ.
    void update_irqs()
    {
        if (!combined(false) && m_irq_pbx) {
            if (irq_pbx.size()) irq_pbx->write(false);
            m_irq_pbx = false;
        }
        if (!combined(true) && m_irq_mbx) {
            if (irq_mbx.size()) irq_mbx->write(false);
            m_irq_mbx = false;
        }
        m_irq_event.notify(sc_core::SC_ZERO_TIME);
    }
    void emit_irqs()
    {
        std::lock_guard<std::mutex> lock(m_lock);
        bool pbx = combined(false), mbx = combined(true);
        if (pbx != m_irq_pbx && irq_pbx.size()) irq_pbx->write(pbx);
        if (mbx != m_irq_mbx && irq_mbx.size()) irq_mbx->write(mbx);
        m_irq_pbx = pbx; m_irq_mbx = mbx;
    }
    void signal(uint32_t bits)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_mbx[0].status |= bits;
        update_irqs();
    }
    bool seed()
    {
        const uint32_t words[] = {
            1, 1, 0, 0, 20, 3, 7, 0, 1, 0, 0, 0x200,
            0xffffffff, 16, N, 0, 0, 0xffffffff, 16, N, 1, 0
        };
        std::vector<uint8_t> table(sizeof(words));
        for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
            put(table.data() + i * 4, words[i], 4);
        return memory(0x100000, table.data(), table.size(), true);
    }
    static std::vector<uint8_t> message(uint32_t dst, const std::vector<uint8_t>& payload)
    {
        std::vector<uint8_t> out(16 + payload.size());
        put(out.data(), 1024, 4); put(out.data() + 4, dst, 4);
        put(out.data() + 12, payload.size(), 2);
        std::copy(payload.begin(), payload.end(), out.begin() + 16);
        return out;
    }
    struct descriptor { uint64_t address; uint32_t length; uint16_t id; };
    // Return 0: no buffer, 1: valid, -1: invalid consumed entry.
    int next(uint64_t ring, uint16_t& cursor, bool writable, descriptor& d)
    {
        uint64_t idx = 0, id = 0;
        if (!read(ring + AVAIL + 2, idx, 2)) { ++m_error_count; return 0; }
        uint16_t pending = uint16_t(idx - cursor);
        if (!pending) return 0;
        if (pending > N) { cursor = uint16_t(idx); ++m_error_count; return 0; }
        if (!read(ring + AVAIL + 4 + (cursor % N) * 2, id, 2)) return 0;
        ++cursor;
        d = {0, 0, uint16_t(id)};
        if (id >= N) { ++m_error_count; return -1; }
        uint8_t bytes[16];
        if (!memory(ring + id * 16, bytes, 16, false)) { ++m_error_count; return -1; }
        d.address = le(bytes, 8); d.length = le(bytes + 8, 4);
        const unsigned flags = le(bytes + 12, 2);
        if ((flags & ~2u) || bool(flags & 2) != writable || d.length < 16 ||
            d.length > 512 || d.address < 0x160000 || d.address >= 0x180000 ||
            d.length > 0x180000 - d.address) { ++m_error_count; return -1; }
        return 1;
    }
    bool complete(uint64_t ring, unsigned id, unsigned length)
    {
        uint64_t idx;
        if (!read(ring + USED + 2, idx, 2) ||
            !write(ring + USED + 4 + (idx % N) * 8, id, 4) ||
            !write(ring + USED + 8 + (idx % N) * 8, length, 4) ||
            !write(ring + USED + 2, uint16_t(idx + 1), 2)) {
            ++m_error_count; return false;
        }
        signal(ring == RX ? 1 : 2);
        return true;
    }
    std::vector<uint8_t> network(const uint8_t* bytes, size_t length)
    {
        const uint8_t mac[] = {0, 1, 2, 3, 4, 6}, ip[] = {192, 168, 1, 1};
        if (length < 14) return {};
        std::vector<uint8_t> out(bytes, bytes + length);
        size_t offset = 14;
        uint16_t type = be16(bytes + 12);
        if (type == 0x8100) {
            if (length < 18) return {};
            type = be16(bytes + 16); offset = 18;
        }
        std::copy(bytes + 6, bytes + 12, out.begin());
        std::copy(mac, mac + 6, out.begin() + 6);
        if (type == 0x806) {
            if (length < offset + 28) return {};
            const auto* arp = bytes + offset;
            if (be16(arp) != 1 || be16(arp + 2) != 0x800 || arp[4] != 6 ||
                arp[5] != 4 || be16(arp + 6) != 1 || !std::equal(ip, ip + 4, arp + 24))
                return {};
            auto* reply = out.data() + offset;
            putbe(reply + 6, 2);
            std::copy(arp + 8, arp + 18, reply + 18);
            std::copy(mac, mac + 6, reply + 8);
            std::copy(ip, ip + 4, reply + 14);
            return out;
        }
        if (type != 0x800 || length < offset + 20) return {};
        const auto* hdr = bytes + offset;
        const unsigned ihl = (hdr[0] & 15) * 4, total = be16(hdr + 2);
        if ((hdr[0] >> 4) != 4 || ihl < 20 || total < ihl + 8 ||
            offset + total > length || hdr[9] != 1 || (be16(hdr + 6) & 0x3fff) ||
            !std::equal(ip, ip + 4, hdr + 16) || checksum(hdr, ihl) != 0)
            return {};
        const auto* icmp = hdr + ihl;
        if (icmp[0] != 8 || icmp[1] != 0 || checksum(icmp, total - ihl) != 0) return {};
        auto* reply = out.data() + offset;
        std::copy(hdr + 12, hdr + 16, reply + 16);
        std::copy(ip, ip + 4, reply + 12);
        reply[8] = 64; putbe(reply + 10, 0); putbe(reply + 10, checksum(reply, ihl));
        reply[ihl] = 0; putbe(reply + ihl + 2, 0);
        putbe(reply + ihl + 2, checksum(reply + ihl, total - ihl));
        out.resize(offset + total);
        return out;
    }
    void receive_tx()
    {
        for (unsigned budget = 0; budget < N; ++budget) {
            descriptor d;
            const int status = next(TX, m_tx_avail, false, d);
            if (!status) break;
            if (status < 0) { if (d.id < N) complete(TX, d.id, 0); continue; }
            std::vector<uint8_t> bytes(d.length);
            if (!memory(d.address, bytes.data(), bytes.size(), false)) { ++m_error_count; break; }
            const unsigned length = le(bytes.data() + 12, 2);
            if (length > d.length - 16 || le(bytes.data() + 4, 4) != 1024 ||
                le(bytes.data() + 14, 2)) ++m_error_count;
            else if (!(length == 1 && bytes[16] == 0)) {
                auto reply = network(bytes.data() + 16, length);
                if (!reply.empty() && m_replies.size() < N)
                    m_replies.push_back(message(le(bytes.data(), 4), reply));
                else ++m_drop_count;
            }
            ++m_tx_count;
            complete(TX, d.id, 0);
        }
    }
    void transmit_rx()
    {
        for (unsigned budget = 0; budget < N && !m_replies.empty(); ++budget) {
            descriptor d;
            const int status = next(RX, m_rx_avail, true, d);
            if (!status) break;
            if (status < 0) { if (d.id < N) complete(RX, d.id, 0); continue; }
            auto& bytes = m_replies.front();
            if (d.length < bytes.size()) { ++m_error_count; complete(RX, d.id, 0); continue; }
            if (!memory(d.address, bytes.data(), bytes.size(), true)) { ++m_error_count; break; }
            if (!complete(RX, d.id, bytes.size())) break;
            m_replies.pop_front(); ++m_rx_count;
        }
    }
    void worker()
    {
        while (true) {
            if (m_attach_count) wait(sc_core::sc_time(100, sc_core::SC_US), m_work);
            else wait(m_work);
            uint32_t bits;
            {
                std::lock_guard<std::mutex> lock(m_lock);
                bits = m_pbx[0].status;
                for (auto& ch : m_pbx) {
                    if (ch.status) { ch.status = 0; ch.int_status |= 1; }
                }
                update_irqs();
            }
            if (bits & 4) {
                m_attached = false; m_ns_sent = false;
                m_replies.clear(); m_rx_avail = m_tx_avail = 0;
                trace("detach");
                // Detach dominates kicks sampled from the old queue session.
                if (!(bits & 8)) continue;
            }
            if (bits & 8) {
                m_replies.clear(); m_rx_avail = m_tx_avail = 0;
                m_ns_sent = false; m_attached = seed();
                if (m_attached) { ++m_attach_count; signal(4); trace("attach"); }
                else ++m_error_count;
            }
            // Linux restores the pristine resource table (status zero) before
            // detach, but .attach() is a no-op on sysfs restart. Its initial
            // kick may coalesce with detach. DRIVER_OK is the shared-memory
            // ownership handoff: keep bounded polling after detach and check
            // readiness before each service batch. In-flight DMA cancellation
            // during concurrent detach is outside this simple model's contract.
            uint64_t status = 0;
            if (!m_attach_count || !read(0x10002c, status, 1) || !(status & 4))
                continue;
            m_attached = true;
            // Host initializes RX descriptors before publishing DRIVER_OK.
            uint64_t available = 0;
            if (!m_ns_sent && read(RX + AVAIL + 2, available, 2) && available) {
                std::vector<uint8_t> ns(40);
                const char name[] = "ethsi1";
                std::copy(name, name + sizeof(name), ns.begin());
                put(ns.data() + 32, 1024, 4);
                m_replies.push_back(message(53, ns)); m_ns_sent = true;
                trace("name-service");
            }
            if (!m_ns_sent) continue;
            receive_tx(); transmit_rx();
            if (bits & 3) trace("vring-service");
        }
    }
    uint32_t read_register(bool mbx, uint64_t offset)
    {
        if (offset == 0) return mbx ? 1 : 0;
        if (offset == 0x10 || offset == 0x14) return 1;
        if (offset == 0x20) return N - 1;
        if (offset == 0x100) return 1;
        if (offset == 0x400) return combined(mbx);
        if (offset == 0xfc8) return 0x0760043b;
        if (offset == 0xfcc) return 0x20;
        if (offset >= 0x1000 && offset < 0x1000 + N * 0x20) {
            auto& ch = (mbx ? m_mbx : m_pbx)[(offset - 0x1000) / 0x20];
            switch (offset % 0x20) {
            case 0: return ch.status;
            case 4: return mbx ? ch.status & ~ch.mask : 0;
            case 0x10: return mbx ? ch.mask : ch.int_status;
            case 0x18: return mbx ? 0 : ch.int_enable;
            case 0x1c: return ch.ctrl;
            default: return 0;
            }
        }
        return 0;
    }
    bool access(tlm::tlm_generic_payload& t, bool debug, const sc_core::sc_time& delay)
    {
        t.set_dmi_allowed(false);
        if (t.get_byte_enable_ptr()) { t.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return false; }
        if (t.get_data_length() != 4 || t.get_streaming_width() < 4) {
            t.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return false;
        }
        if (!t.get_data_ptr() || t.get_address() >= 0x60000 || (t.get_address() & 3)) {
            t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return false;
        }
        std::lock_guard<std::mutex> lock(m_lock);
        const bool mbx = t.get_address() >= 0x30000;
        const uint64_t offset = t.get_address() % 0x30000;
        if (t.is_read()) put(t.get_data_ptr(), read_register(mbx, offset), 4);
        else if (t.is_write()) {
            // Debug access is inspection-only; no DMA, doorbell or IRQ side effects.
            if (!debug && offset >= 0x1000 && offset < 0x1000 + N * 0x20) {
                auto& ch = (mbx ? m_mbx : m_pbx)[(offset - 0x1000) / 0x20];
                const uint32_t value = le(t.get_data_ptr(), 4);
                switch (offset % 0x20) {
                case 8: if (mbx) ch.status &= ~value; break;
                case 12: if (!mbx) { ch.status |= value; m_work.notify(delay); } break;
                case 20: if (mbx) ch.mask |= value; else ch.int_status &= ~value; break;
                case 24: if (mbx) ch.mask &= ~value; else ch.int_enable = value; break;
                case 28: ch.ctrl = value; break;
                default: break;
                }
                update_irqs();
            }
        } else { t.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return false; }
        t.set_response_status(tlm::TLM_OK_RESPONSE); return true;
    }
public:
    cci::cci_param<bool> p_trace;
    tlm_utils::simple_target_socket_b<apollo_si_stub, DEFAULT_TLM_BUSWIDTH,
        tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND> target_socket;
    tlm_utils::simple_initiator_socket<apollo_si_stub, DEFAULT_TLM_BUSWIDTH> initiator_socket;
    InitiatorSignalSocket<bool> irq_pbx, irq_mbx;

    SC_HAS_PROCESS(apollo_si_stub);
    explicit apollo_si_stub(sc_core::sc_module_name name)
        : sc_module(name), m_work(false), m_irq_event(false),
          p_trace("trace", false), target_socket("target_socket"),
          initiator_socket("initiator_socket"), irq_pbx("irq_pbx"), irq_mbx("irq_mbx")
    {
        target_socket.register_b_transport(this, &apollo_si_stub::b_transport);
        target_socket.register_transport_dbg(this, &apollo_si_stub::transport_dbg);
        SC_THREAD(worker);
        SC_METHOD(emit_irqs); sensitive << m_irq_event; dont_initialize();
    }
    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& delay) { access(t, false, delay); }
    unsigned transport_dbg(tlm::tlm_generic_payload& t)
    { return access(t, true, sc_core::SC_ZERO_TIME) ? t.get_data_length() : 0; }
    uint64_t attach_count() const { return m_attach_count; }
    uint64_t tx_count() const { return m_tx_count; }
    uint64_t rx_count() const { return m_rx_count; }
    uint64_t error_count() const { return m_error_count; }
    uint64_t drop_count() const { return m_drop_count; }
};

extern "C" void module_register();
