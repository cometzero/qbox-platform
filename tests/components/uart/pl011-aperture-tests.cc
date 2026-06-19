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
    Uart uart_mirror;
    Uart uart_plain;
    char_backend_file mirror_backend;
    char_backend_file plain_backend;
    InitiatorTester mirror_initiator;
    InitiatorTester plain_initiator;
    TargetSignalSocket<bool> mirror_irq;
    TargetSignalSocket<bool> plain_irq;

    Pl011ApertureBench(const sc_core::sc_module_name& n)
        : TestBench(n)
        , uart_mirror("uart_mirror")
        , uart_plain("uart_plain")
        , mirror_backend("mirror_backend")
        , plain_backend("plain_backend")
        , mirror_initiator("mirror_initiator")
        , plain_initiator("plain_initiator")
        , mirror_irq("mirror_irq")
        , plain_irq("plain_irq")
    {
        uart_mirror.backend_socket.bind(mirror_backend.socket);
        uart_plain.backend_socket.bind(plain_backend.socket);
        mirror_initiator.socket.bind(uart_mirror.socket);
        plain_initiator.socket.bind(uart_plain.socket);
        uart_mirror.irq.bind(mirror_irq);
        uart_plain.irq.bind(plain_irq);
    }
};

TEST_BENCH(Pl011ApertureBench, Pl011Aperture)
{
    uint32_t value = 0;

    ASSERT_EQ(mirror_initiator.do_read(0xfe0, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x11u);

    value = 0;
    ASSERT_EQ(mirror_initiator.do_read(0xffe0, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0x11u);

    value = 0xffffffffu;
    ASSERT_EQ(mirror_initiator.do_read(0x10018, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0u);

    value = 0xffffffffu;
    ASSERT_EQ(plain_initiator.do_read(0xffe0, value), tlm::TLM_OK_RESPONSE);
    ASSERT_EQ(value, 0u);

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
        { "Pl011Aperture.uart_mirror.id_register_mirror_mask",
          cci::cci_value(uint64_t{ 0xfff }) },
        { "Pl011Aperture.mirror_backend.read_file",
          cci::cci_value(std::string(null_device)) },
        { "Pl011Aperture.mirror_backend.write_file",
          cci::cci_value(std::string(null_device)) },
        { "Pl011Aperture.mirror_backend.baudrate", cci::cci_value(0) },
        { "Pl011Aperture.plain_backend.read_file",
          cci::cci_value(std::string(null_device)) },
        { "Pl011Aperture.plain_backend.write_file",
          cci::cci_value(std::string(null_device)) },
        { "Pl011Aperture.plain_backend.baudrate", cci::cci_value(0) },
    });

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
