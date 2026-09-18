/* SPDX-License-Identifier: BSD-3-Clause */

#include <array>
#include <cstdint>
#include <vector>

#include <cci/utils/broker.h>
#include <dw-apb-i2s.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <ports/target-signal-socket.h>
#include <tests/test-bench.h>

namespace {

constexpr uint32_t DMA_ACTIVE = 1u << 2;

class I2sTestBench : public TestBench
{
protected:
    dw_apb_i2s playback;
    dw_apb_i2s capture;
    sc_core::sc_signal<bool> playback_irq;
    sc_core::sc_signal<bool> capture_irq;
    sc_core::sc_signal<uint32_t> playback_dma_tx_req;
    sc_core::sc_signal<uint32_t> playback_dma_rx_req;
    sc_core::sc_signal<uint32_t> capture_dma_tx_req;
    sc_core::sc_signal<uint32_t> capture_dma_rx_req;
    tlm_utils::simple_initiator_socket<I2sTestBench, DEFAULT_TLM_BUSWIDTH> playback_mmio;
    tlm_utils::simple_initiator_socket<I2sTestBench, DEFAULT_TLM_BUSWIDTH> capture_mmio;

    explicit I2sTestBench(const sc_core::sc_module_name& name)
        : TestBench(name)
        , playback("playback")
        , capture("capture")
        , playback_irq("playback_irq")
        , capture_irq("capture_irq")
        , playback_dma_tx_req("playback_dma_tx_req")
        , playback_dma_rx_req("playback_dma_rx_req")
        , capture_dma_tx_req("capture_dma_tx_req")
        , capture_dma_rx_req("capture_dma_rx_req")
        , playback_mmio("playback_mmio")
        , capture_mmio("capture_mmio")
    {
        playback.p_master_mode = true;
        playback.p_transmitter_enabled = true;
        playback.p_receiver_enabled = true;
        playback.p_access_latency_ns = 0;
        playback.p_frame_period_ns = 1;
        capture.p_master_mode = false;
        capture.p_transmitter_enabled = true;
        capture.p_receiver_enabled = true;
        capture.p_access_latency_ns = 0;
        capture.p_frame_period_ns = 1;

        playback.audio_socket.bind(capture.audio_socket);
        playback_mmio.bind(playback.target_socket);
        capture_mmio.bind(capture.target_socket);
        playback.irq.bind(playback_irq);
        capture.irq.bind(capture_irq);
        playback.dma_tx_req.bind(playback_dma_tx_req);
        playback.dma_rx_req.bind(playback_dma_rx_req);
        capture.dma_tx_req.bind(capture_dma_tx_req);
        capture.dma_rx_req.bind(capture_dma_rx_req);
    }

    void write(dw_apb_i2s& device, uint32_t offset, uint32_t value)
    {
        access(device, tlm::TLM_WRITE_COMMAND, offset, value);
    }

    uint32_t read(dw_apb_i2s& device, uint32_t offset)
    {
        uint32_t value = 0;
        access(device, tlm::TLM_READ_COMMAND, offset, value);
        return value;
    }

    void write16(dw_apb_i2s& device, uint32_t offset, uint16_t value)
    {
        access(device, tlm::TLM_WRITE_COMMAND, offset, value);
    }

    uint16_t read16(dw_apb_i2s& device, uint32_t offset)
    {
        uint16_t value = 0;
        access(device, tlm::TLM_READ_COMMAND, offset, value);
        return value;
    }

    static void drive(TargetSignalSocket<uint32_t>& socket, uint32_t value)
    {
        auto* signal = dynamic_cast<sc_core::sc_signal_inout_if<uint32_t>*>(socket.get_interface());
        ASSERT_NE(signal, nullptr);
        signal->write(value);
    }

    void acknowledge(TargetSignalSocket<uint32_t>& ack, sc_core::sc_signal<uint32_t>& request)
    {
        drive(ack, DMA_ACTIVE);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        EXPECT_EQ(request.read() & DMA_ACTIVE, 0u);
        drive(ack, 0);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
    }

    tlm::tlm_response_status send_frame_from(dw_apb_i2s& source, uint32_t left, uint32_t right)
    {
        std::array<uint32_t, 2> frame = { left, right };
        tlm::tlm_generic_payload trans;
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(frame.data()));
        trans.set_data_length(sizeof(frame));
        trans.set_streaming_width(sizeof(frame));
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        source.audio_socket.force_send(trans);
        return trans.get_response_status();
    }

    tlm::tlm_response_status send_frame(uint32_t left, uint32_t right)
    {
        return send_frame_from(playback, left, right);
    }

private:
    template <typename T>
    void access(dw_apb_i2s& device, tlm::tlm_command command, uint32_t offset, T& value)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        device.b_transport(trans, delay);
        ASSERT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        wait(delay);
    }
};

TEST_BENCH(I2sTestBench, TransfersPioAndCyclesDmaRequests)
{
    EXPECT_EQ(read(playback, dw_apb_i2s::COMP_PARAM_1), 0x4007eu);
    EXPECT_EQ(read(capture, dw_apb_i2s::COMP_PARAM_1), 0x4006eu);
    EXPECT_EQ(read(playback, dw_apb_i2s::COMP_PARAM_2), 4u);
    EXPECT_EQ(read(playback, dw_apb_i2s::COMP_TYPE), 0x445701a0u);
    EXPECT_EQ(read(playback, dw_apb_i2s::TCR0), 5u);
    EXPECT_EQ(read(capture, dw_apb_i2s::RCR0), 5u);
    write(playback, dw_apb_i2s::TCR0, 7);
    write(capture, dw_apb_i2s::RCR0, 6);
    EXPECT_EQ(read(playback, dw_apb_i2s::TCR0), 5u);
    EXPECT_EQ(read(capture, dw_apb_i2s::RCR0), 5u);

    for (uint32_t i = 0; i < dw_apb_i2s::FIFO_DEPTH; ++i) {
        write(playback, dw_apb_i2s::LRBR_LTHR0, i);
        write(playback, dw_apb_i2s::RRBR_RTHR0, ~i);
    }
    write(playback, dw_apb_i2s::LRBR_LTHR0, 0xdeadbeef);
    EXPECT_NE(read(playback, dw_apb_i2s::ISR0) & (1u << 5), 0u);
    EXPECT_EQ(read(playback, dw_apb_i2s::TOR0), 1u);
    EXPECT_EQ(read(playback, dw_apb_i2s::ISR0) & (1u << 5), 0u);
    write(playback, dw_apb_i2s::TXFFR, 1);
    write(playback, dw_apb_i2s::IMR0, 0x23);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(playback_irq.read());
    write(playback, dw_apb_i2s::IER, 1);
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_TRUE(playback_irq.read());
    write(playback, dw_apb_i2s::IER, 0);
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(playback_irq.read());

    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::CER, 1);
    write(playback, dw_apb_i2s::LRBR_LTHR0, 0xdeadbeef);
    write(playback, dw_apb_i2s::RRBR_RTHR0, 0xfeedface);
    wait(sc_core::sc_time(1500, sc_core::SC_PS));
    write(playback, dw_apb_i2s::IER, 0);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    wait(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(read(capture, dw_apb_i2s::ISR0) & 1u, 0u);
    write(capture, dw_apb_i2s::IER, 0);

    write(capture, dw_apb_i2s::RFCR0, 0);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::CER, 1);
    wait(sc_core::sc_time(1500, sc_core::SC_PS));
    write(playback, dw_apb_i2s::IER, 0);
    EXPECT_NE(read(capture, dw_apb_i2s::ISR0) & 1u, 0u);
    EXPECT_EQ(read(capture, dw_apb_i2s::LRBR_LTHR0), 0u);
    EXPECT_EQ(read(capture, dw_apb_i2s::RRBR_RTHR0), 0u);
    write(capture, dw_apb_i2s::IER, 0);

    write(capture, dw_apb_i2s::RFCR0, 15);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    write(capture, dw_apb_i2s::IMR0, 0x31);
    for (uint32_t i = 0; i <= dw_apb_i2s::FIFO_DEPTH; ++i)
        EXPECT_EQ(send_frame(0x1000 + i, 0x2000 + i), tlm::TLM_OK_RESPONSE);
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_NE(read(capture, dw_apb_i2s::ISR0) & (1u << 1), 0u);
    EXPECT_TRUE(capture_irq.read());
    write(capture, dw_apb_i2s::RFF0, 1);
    EXPECT_NE(read(capture, dw_apb_i2s::ISR0) & 1u, 0u);
    EXPECT_EQ(read(capture, dw_apb_i2s::ROR0), 1u);
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(capture_irq.read());
    write(capture, dw_apb_i2s::RER0, 0);
    write(capture, dw_apb_i2s::RFF0, 1);
    EXPECT_EQ(read(capture, dw_apb_i2s::ISR0) & 1u, 0u);
    write(capture, dw_apb_i2s::IER, 0);

    write(capture, dw_apb_i2s::RFCR0, 3);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    write(capture, dw_apb_i2s::IMR0, 0x32);

    write(playback, dw_apb_i2s::TFCR0, 2);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::IMR0, 0x23);
    write(playback, dw_apb_i2s::CER, 1);

    const std::array<std::array<uint32_t, 2>, 4> pio = { {
        { { 0x00112233, 0x00445566 } },
        { { 0x00778899, 0x00aabbcc } },
        { { 0x00ddeeff, 0x00010203 } },
        { { 0x00040506, 0x00070809 } },
    } };
    for (const auto& frame : pio) {
        write(playback, dw_apb_i2s::LRBR_LTHR0, frame[0]);
        write(playback, dw_apb_i2s::RRBR_RTHR0, frame[1]);
    }

    wait(sc_core::sc_time(4500, sc_core::SC_PS));
    EXPECT_NE(read(capture, dw_apb_i2s::ISR0) & 1u, 0u);
    EXPECT_TRUE(capture_irq.read());
    EXPECT_NE(read(playback, dw_apb_i2s::ISR0) & (1u << 4), 0u);
    EXPECT_TRUE(playback_irq.read());
    write(playback, dw_apb_i2s::IER, 0);
    for (const auto& frame : pio) {
        EXPECT_EQ(read(capture, dw_apb_i2s::LRBR_LTHR0), frame[0]);
        EXPECT_EQ(read(capture, dw_apb_i2s::RRBR_RTHR0), frame[1]);
    }
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(capture_irq.read());

    write(capture, dw_apb_i2s::IER, 0);
    write(capture, dw_apb_i2s::RFCR0, 0);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    write(capture, dw_apb_i2s::DMACR, 1u << 16);
    write(playback, dw_apb_i2s::TFCR0, 0);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::DMACR, 1u << 17);
    write(playback, dw_apb_i2s::CER, 1);
    wait(sc_core::sc_time(1, sc_core::SC_PS));

    const std::array<uint16_t, 8> dma = {
        0x100, 0x101, 0x200, 0x201, 0x300, 0x301, 0x400, 0x401,
    };
    for (uint16_t sample : dma) {
        ASSERT_NE(playback_dma_tx_req.read() & DMA_ACTIVE, 0u);
        write16(playback, dw_apb_i2s::TXDMA, sample);
        acknowledge(playback.dma_tx_ack, playback_dma_tx_req);
    }

    wait(sc_core::sc_time(4500, sc_core::SC_PS));
    write(playback, dw_apb_i2s::IER, 0);
    std::vector<uint16_t> recorded;
    for (size_t i = 0; i < dma.size(); ++i) {
        ASSERT_NE(capture_dma_rx_req.read() & DMA_ACTIVE, 0u);
        recorded.push_back(read16(capture, dw_apb_i2s::RXDMA));
        acknowledge(capture.dma_rx_ack, capture_dma_rx_req);
    }
    EXPECT_EQ(recorded, std::vector<uint16_t>(dma.begin(), dma.end()));
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_EQ(capture_dma_rx_req.read() & DMA_ACTIVE, 0u);

    write(playback, dw_apb_i2s::IER, 0);
    write(capture, dw_apb_i2s::IER, 0);
    playback.p_functional_pacing = true;
    capture.p_functional_pacing = true;
    wait(sc_core::sc_time(2, sc_core::SC_NS));
    write(capture, dw_apb_i2s::RFCR0, 0);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::CER, 1);
    wait(sc_core::sc_time(3, sc_core::SC_NS));
    EXPECT_EQ(read(capture, dw_apb_i2s::ISR0) & 1u, 0u);
    write(playback, dw_apb_i2s::LRBR_LTHR0, 0x1234);
    write(playback, dw_apb_i2s::RRBR_RTHR0, 0xabcd);
    wait(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_EQ(read(capture, dw_apb_i2s::LRBR_LTHR0), 0x1234u);
    EXPECT_EQ(read(capture, dw_apb_i2s::RRBR_RTHR0), 0xabcdu);

    write(playback, dw_apb_i2s::IER, 0);
    write(capture, dw_apb_i2s::IER, 0);
    write(capture, dw_apb_i2s::RFCR0, 15);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    for (uint32_t i = 0; i < dw_apb_i2s::FIFO_DEPTH; ++i)
        EXPECT_EQ(send_frame(0x3000 + i, 0x4000 + i), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(send_frame(0x5555, 0xaaaa), tlm::TLM_GENERIC_ERROR_RESPONSE);
    EXPECT_EQ(read(capture, dw_apb_i2s::ISR0) & (1u << 1), 0u);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::CER, 1);
    write(playback, dw_apb_i2s::LRBR_LTHR0, 0x5555);
    write(playback, dw_apb_i2s::RRBR_RTHR0, 0xaaaa);
    wait(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_EQ(read(capture, dw_apb_i2s::LRBR_LTHR0), 0x3000u);
    EXPECT_EQ(read(capture, dw_apb_i2s::RRBR_RTHR0), 0x4000u);
    wait(sc_core::sc_time(2, sc_core::SC_NS));
    for (uint32_t i = 1; i < dw_apb_i2s::FIFO_DEPTH; ++i) {
        EXPECT_EQ(read(capture, dw_apb_i2s::LRBR_LTHR0), 0x3000u + i);
        EXPECT_EQ(read(capture, dw_apb_i2s::RRBR_RTHR0), 0x4000u + i);
    }
    EXPECT_EQ(read(capture, dw_apb_i2s::LRBR_LTHR0), 0x5555u);
    EXPECT_EQ(read(capture, dw_apb_i2s::RRBR_RTHR0), 0xaaaau);

    write(playback, dw_apb_i2s::IER, 0);
    write(capture, dw_apb_i2s::IER, 0);
    write(playback, dw_apb_i2s::RER0, 1);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::IRER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::CER, 1);
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::TER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    write(capture, dw_apb_i2s::ITER, 1);
    write(capture, dw_apb_i2s::CER, 1);
    write(playback, dw_apb_i2s::LRBR_LTHR0, 0x11112222);
    write(playback, dw_apb_i2s::RRBR_RTHR0, 0x33334444);
    write(capture, dw_apb_i2s::LRBR_LTHR0, 0x55556666);
    write(capture, dw_apb_i2s::RRBR_RTHR0, 0x77778888);
    wait(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_EQ(read(capture, dw_apb_i2s::LRBR_LTHR0), 0x11112222u);
    EXPECT_EQ(read(capture, dw_apb_i2s::RRBR_RTHR0), 0x33334444u);
    EXPECT_EQ(read(playback, dw_apb_i2s::LRBR_LTHR0), 0x55556666u);
    EXPECT_EQ(read(playback, dw_apb_i2s::RRBR_RTHR0), 0x77778888u);

    write(playback, dw_apb_i2s::IER, 0);
    write(capture, dw_apb_i2s::IER, 0);
    write(playback, dw_apb_i2s::RER0, 1);
    write(playback, dw_apb_i2s::TER0, 1);
    write(playback, dw_apb_i2s::IER, 1);
    write(playback, dw_apb_i2s::IRER, 1);
    write(playback, dw_apb_i2s::ITER, 1);
    write(playback, dw_apb_i2s::DMACR, (1u << 16) | (1u << 17));
    write(capture, dw_apb_i2s::RER0, 1);
    write(capture, dw_apb_i2s::TER0, 1);
    write(capture, dw_apb_i2s::IER, 1);
    write(capture, dw_apb_i2s::IRER, 1);
    write(capture, dw_apb_i2s::ITER, 1);
    write(capture, dw_apb_i2s::DMACR, (1u << 16) | (1u << 17));
    EXPECT_EQ(send_frame_from(playback, 0x9000, 0x9001), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(send_frame_from(capture, 0xa000, 0xa001), tlm::TLM_OK_RESPONSE);
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_NE(playback_dma_tx_req.read() & DMA_ACTIVE, 0u);
    EXPECT_NE(playback_dma_rx_req.read() & DMA_ACTIVE, 0u);
    EXPECT_NE(capture_dma_tx_req.read() & DMA_ACTIVE, 0u);
    EXPECT_NE(capture_dma_rx_req.read() & DMA_ACTIVE, 0u);
    write(playback, dw_apb_i2s::IRER, 0);
    write(capture, dw_apb_i2s::ITER, 0);
    wait(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_NE(playback_dma_tx_req.read() & DMA_ACTIVE, 0u);
    EXPECT_EQ(playback_dma_rx_req.read() & DMA_ACTIVE, 0u);
    EXPECT_EQ(capture_dma_tx_req.read() & DMA_ACTIVE, 0u);
    EXPECT_NE(capture_dma_rx_req.read() & DMA_ACTIVE, 0u);

    sc_core::sc_stop();
}

} // namespace

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
