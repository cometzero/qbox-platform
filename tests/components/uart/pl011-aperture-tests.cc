#include <systemc.h>

#include <cci_configuration>
#include <libgsutils.h>

#include <char_backend_file.h>
#include "uart-pl011.h"

#include <ports/target-signal-socket.h>
#include <tests/initiator-tester.h>
#include <tests/test-bench.h>

class Pl011ApertureBench : public TestBench
{
public:
    Uart uart;
    char_backend_file backend;
    InitiatorTester initiator;
    TargetSignalSocket<bool> irq;

    Pl011ApertureBench(const sc_core::sc_module_name& n)
        : TestBench(n)
        , uart("uart")
        , backend("backend")
        , initiator("initiator")
        , irq("irq")
    {
        uart.backend_socket.bind(backend.socket);
        initiator.socket.bind(uart.socket);
        uart.irq.bind(irq);
    }
};

TEST_BENCH(Pl011ApertureBench, Pl011Aperture)
{
    uint32_t value = 0;

    ASSERT_EQ(initiator.do_read(0xfe0, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x11u);

    value = 0;
    ASSERT_EQ(initiator.do_read(0xfe8, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x34u);

    value = 0;
    ASSERT_EQ(initiator.do_read(0xffe0, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x11u);

    value = 0;
    ASSERT_EQ(initiator.do_read(0xffe8, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x34u);

    value = 0;
    ASSERT_EQ(initiator.do_read(0xf018, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x90u);

    value = 0x7fu;
    ASSERT_EQ(initiator.do_write(0x702c, value), tlm::TLM_OK_RESPONSE);

    value = 0;
    ASSERT_EQ(initiator.do_read(0x2c, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x7fu);

    sc_core::wait(1, sc_core::SC_NS);
    sc_core::sc_stop();
}

int sc_main(int argc, char* argv[])
{
#ifdef _WIN32
    const char* null_device = "NUL";
#else
    const char* null_device = "/dev/null";
#endif

    gs::ConfigurableBroker broker({
        { "Pl011Aperture.backend.read_file",
          cci::cci_value(std::string(null_device)) },
        { "Pl011Aperture.backend.write_file",
          cci::cci_value(std::string(null_device)) },
        { "Pl011Aperture.backend.baudrate", cci::cci_value(0) },
        { "Pl011Aperture.uart.revision", cci::cci_value(3) },
    });

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
