/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <host_ni710ae_nci.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm-extensions/request-context.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <zena_fmu.h>
#include <zena_reset_ctrl.h>
#include <zena_ssu.h>

namespace {

class TlmBinder : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<TlmBinder, DEFAULT_TLM_BUSWIDTH> socket;

    explicit TlmBinder(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }
};

class MemoryTarget : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<MemoryTarget, DEFAULT_TLM_BUSWIDTH> socket;

    explicit MemoryTarget(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &MemoryTarget::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
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
        signal.register_value_changed_cb([this](bool value) {
            observed.push_back(value);
        });
    }
};

uint32_t fmu_access32(zena_fmu& dut, uint64_t address,
                      tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(address);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

uint32_t direct32(sc_core::sc_module& module, uint64_t address,
                  tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(address);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (auto* ssu = dynamic_cast<zena_ssu*>(&module)) {
        ssu->b_transport(trans, delay);
    } else {
        dynamic_cast<zena_reset_ctrl&>(module).rgm_b_transport(trans, delay);
    }
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

}

TEST(ZenaSafetyVerticalTest, ApuDenialEscalatesAndRecovers)
{
    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("zena_safety_vertical_test"));
    broker.set_preset_cci_value(
        "fmu.fault_input_enabled", cci::cci_value(true));
    broker.set_preset_cci_value(
        "fmu.fault_input_record", cci::cci_value(0u));

    host_ni710ae_nci ni("ni");
    zena_fmu fmu("fmu");
    zena_ssu ssu("ssu");
    zena_reset_ctrl reset_ctrl("reset_ctrl");
    MemoryTarget memory("memory");
    TlmBinder ni_regs("ni_regs");
    TlmBinder fmu_regs("fmu_regs");
    TlmBinder ssu_regs("ssu_regs");
    TlmBinder rgm_regs("rgm_regs");
    TlmBinder pik_regs("pik_regs");
    SignalSink fmu_irq("fmu_irq");
    SignalSink reset("reset");

    ni.p_reset_owner_domain_id = 3u;
    ni_regs.socket.bind(ni.target_socket);
    ni.initiator_socket.bind(memory.socket);
    ni.apu_fault.bind(fmu.fault_in);
    fmu_regs.socket.bind(fmu.target_socket);
    fmu.critical_irq.bind(fmu_irq.signal);
    fmu.critical_ssu.bind(ssu.critical_in);
    ssu_regs.socket.bind(ssu.target_socket);
    ssu.safety_status.bind(reset_ctrl.safety_fault_reset);
    rgm_regs.socket.bind(reset_ctrl.rgm);
    pik_regs.socket.bind(reset_ctrl.pik);
    reset_ctrl.ap_reset.bind(reset.signal);

    ni.before_end_of_elaboration();
    fmu.before_end_of_elaboration();
    ssu.before_end_of_elaboration();
    (void)direct32(reset_ctrl, 0x030, tlm::TLM_WRITE_COMMAND, 1u << 24);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));

    uint32_t data = 0;
    tlm::tlm_generic_payload trans;
    RequestContext context = make_request_context(0x1000, 1, 0, 0);
    context.secure = true;
    context.secure_valid = true;
    RequestContextTlmExtension extension(context);
    trans.set_address(0x1000);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_extension(&extension);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    ni.protected_b_transport(trans, delay);
    trans.clear_extension<RequestContextTlmExtension>();

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(ni.denied_count(), 1u);
    EXPECT_NE(fmu_access32(fmu, 0x010, tlm::TLM_READ_COMMAND) &
                  (1u << 30),
              0u);
    EXPECT_NE(direct32(ssu, 0x808, tlm::TLM_READ_COMMAND) & (1u << 3), 0u);
    EXPECT_NE(direct32(reset_ctrl, 0x020, tlm::TLM_READ_COMMAND) &
                  (1u << 24),
              0u);
    ASSERT_FALSE(fmu_irq.observed.empty());
    EXPECT_TRUE(fmu_irq.observed.back());
    ASSERT_FALSE(reset.observed.empty());
    EXPECT_TRUE(reset.observed.back());

    (void)fmu_access32(fmu, 0x8bfc, tlm::TLM_WRITE_COMMAND, 0xbe);
    (void)fmu_access32(fmu, 0x010, tlm::TLM_WRITE_COMMAND,
                       (1u << 30) | (1u << 19));
    (void)direct32(ssu, 0x804, tlm::TLM_WRITE_COMMAND, 0xbe);
    (void)direct32(ssu, 0x010, tlm::TLM_WRITE_COMMAND, 1u << 30);
    EXPECT_FALSE(fmu_irq.observed.back());
    EXPECT_FALSE(reset.observed.back());
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
