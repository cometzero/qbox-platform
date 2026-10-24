/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <zena_fmu.h>
#include <zena_reset_ctrl.h>
#include <zena_ssu.h>

namespace {

constexpr uint64_t RECORD_STRIDE = 0x40;
constexpr uint64_t BANK_BYTES = 0x10000;
constexpr uint64_t ERR_CTLR = 0x008;
constexpr uint64_t ERR_STATUS = 0x010;
constexpr uint64_t ERR_IMPDEF = 0x8000;
constexpr uint64_t SYS_KEY = 0x8bfc;
constexpr uint64_t ERRGSR0_L = 0xe000;
constexpr uint64_t PIDR4 = 0xffd0;
constexpr uint64_t PIDR0 = 0xffe0;
constexpr uint64_t PIDR1 = 0xffe4;
constexpr uint64_t PIDR2 = 0xffe8;
constexpr uint64_t PIDR3 = 0xffec;
constexpr uint64_t CIDR0 = 0xfff0;
constexpr uint64_t CIDR1 = 0xfff4;
constexpr uint64_t CIDR2 = 0xfff8;
constexpr uint64_t CIDR3 = 0xfffc;

constexpr uint32_t SYS_KEY_VALUE = 0xbe;
constexpr uint32_t CTLR_RW_MASK = 0x00002119;
constexpr uint32_t STATUS_V = 1u << 30;
constexpr uint32_t STATUS_UE = 1u << 29;
constexpr uint32_t STATUS_CI = 1u << 19;
constexpr uint32_t STATUS_IERR_ERR_IN = 1u << 12;
constexpr uint32_t IMPDEF_IE = (1u << 13) | 0x180;
constexpr uint32_t IMPDEF_IC = 0xcu;
constexpr uint32_t IMPDEF_UE = 1u;
constexpr uint32_t IMPDEF_COUNT_SHIFT = 16;
constexpr uint32_t IMPDEF_THRESHOLD_SHIFT = 24;
constexpr uint64_t SSU_ERR_STATUS = 0x010;
constexpr uint64_t SSU_SYS_KEY = 0x804;
constexpr uint64_t SSU_SYS_STATUS = 0x808;
constexpr uint32_t SSU_SYS_KEY_VALUE = 0xbe;
constexpr uint32_t SSU_STATUS_ERRC = 1u << 3;
constexpr uint32_t SSU_STATUS_SAFE = 1u << 1;

class FaultSource : public sc_core::sc_module
{
    bool m_pending = false;
    sc_core::sc_event m_event;

    void emit()
    {
        signal->write(m_pending);
    }

public:
    SC_HAS_PROCESS(FaultSource);

    InitiatorSignalSocket<bool> signal;

    explicit FaultSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
    {
        SC_METHOD(emit);
        sensitive << m_event;
        dont_initialize();
    }

    void write(bool value)
    {
        m_pending = value;
        m_event.notify(sc_core::sc_time(1, sc_core::SC_PS));
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

uint32_t access32(zena_fmu& dut, uint64_t offset, tlm::tlm_command command,
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
    dut.b_transport(trans, delay);

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

uint32_t read32(zena_fmu& dut, uint64_t offset)
{
    return access32(dut, offset, tlm::TLM_READ_COMMAND);
}

void write32(zena_fmu& dut, uint64_t offset, uint32_t value)
{
    (void)access32(dut, offset, tlm::TLM_WRITE_COMMAND, value);
}

void write32_keyed(zena_fmu& dut, uint64_t offset, uint32_t value)
{
    write32(dut, SYS_KEY, SYS_KEY_VALUE);
    write32(dut, offset, value);
}

void write32_keyed_bank(zena_fmu& dut, unsigned int bank, uint64_t offset,
                        uint32_t value)
{
    const uint64_t bank_offset = bank * BANK_BYTES;
    write32(dut, bank_offset + SYS_KEY, SYS_KEY_VALUE);
    write32(dut, bank_offset + offset, value);
}

uint32_t ssu_access32(zena_ssu& dut, uint64_t offset,
                      tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    auto data = value;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

uint32_t reset_access32(zena_reset_ctrl& dut, uint64_t offset,
                        tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    auto data = value;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.rgm_b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

uint32_t ssu_read32(zena_ssu& dut, uint64_t offset)
{
    return ssu_access32(dut, offset, tlm::TLM_READ_COMMAND);
}

void ssu_write32(zena_ssu& dut, uint64_t offset, uint32_t value)
{
    (void)ssu_access32(dut, offset, tlm::TLM_WRITE_COMMAND, value);
}

void ssu_write32_keyed(zena_ssu& dut, uint64_t offset, uint32_t value)
{
    ssu_write32(dut, SSU_SYS_KEY, SSU_SYS_KEY_VALUE);
    ssu_write32(dut, offset, value);
}

uint64_t record_offset(unsigned int index, uint64_t field)
{
    return index * RECORD_STRIDE + field;
}

uint64_t impdef_offset(unsigned int index)
{
    return ERR_IMPDEF + index * 0x8;
}

}

TEST(ZenaFmuTest, ResetValuesMatchDocumentedPcid)
{
    zena_fmu dut("fmu_reset");
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, PIDR4), 0x04u);
    EXPECT_EQ(read32(dut, PIDR0), 0xd0u);
    EXPECT_EQ(read32(dut, PIDR1), 0xb3u);
    EXPECT_EQ(read32(dut, PIDR2), 0x0bu);
    EXPECT_EQ(read32(dut, PIDR3), 0x00u);
    EXPECT_EQ(read32(dut, CIDR0), 0x0du);
    EXPECT_EQ(read32(dut, CIDR1), 0xf0u);
    EXPECT_EQ(read32(dut, CIDR2), 0x05u);
    EXPECT_EQ(read32(dut, CIDR3), 0xb1u);
    EXPECT_EQ(read32(dut, record_offset(0, ERR_STATUS)), 0u);
}

TEST(ZenaFmuTest, RegistersRequireSysKeyWhenConfigured)
{
    zena_fmu dut("fmu_key");
    dut.before_end_of_elaboration();

    write32(dut, record_offset(0, ERR_CTLR), 0);
    EXPECT_EQ(read32(dut, record_offset(0, ERR_CTLR)), 0x2001u);

    write32_keyed(dut, record_offset(0, ERR_CTLR), 0);
    EXPECT_EQ(read32(dut, record_offset(0, ERR_CTLR)), 0u);
}

TEST(ZenaFmuTest, StatusUsesWriteOneToClearBits)
{
    zena_fmu dut("fmu_w1c");
    dut.before_end_of_elaboration();

    write32_keyed(dut, impdef_offset(1), IMPDEF_IE);
    EXPECT_NE(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V, 0u);
    EXPECT_EQ(read32(dut, ERRGSR0_L) & (1u << 1), 1u << 1);

    write32_keyed(dut, record_offset(1, ERR_STATUS), STATUS_V | STATUS_UE);
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V, 0u);
    EXPECT_EQ(read32(dut, ERRGSR0_L) & (1u << 1), 0u);
}

TEST(ZenaFmuTest, InjectionClearDoesNotAcknowledgeLatchedStatus)
{
    zena_fmu dut("fmu_ack_syndrome");
    dut.before_end_of_elaboration();

    write32_keyed(dut, impdef_offset(1), IMPDEF_IE);
    write32_keyed(dut, impdef_offset(1), IMPDEF_IC);

    EXPECT_NE(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V, 0u);
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) &
                  STATUS_IERR_ERR_IN,
              STATUS_IERR_ERR_IN);
    write32_keyed(dut, record_offset(1, ERR_STATUS), STATUS_V | STATUS_UE);
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V, 0u);
}

TEST(ZenaFmuTest, UpgradeThresholdPromotesThirdInjection)
{
    zena_fmu dut("fmu_upgrade");
    dut.before_end_of_elaboration();
    const uint32_t configuration = 0x180 | IMPDEF_UE |
        (2u << IMPDEF_THRESHOLD_SHIFT);
    write32_keyed(dut, impdef_offset(1), configuration);

    for (uint32_t expected_count = 1; expected_count <= 3;
         ++expected_count) {
        write32_keyed(
            dut, impdef_offset(1),
            read32(dut, impdef_offset(1)) | IMPDEF_IE);
        const uint32_t status =
            read32(dut, record_offset(1, ERR_STATUS));
        EXPECT_EQ((read32(dut, impdef_offset(1)) >> IMPDEF_COUNT_SHIFT) &
                      0xffu,
                  expected_count);
        EXPECT_EQ((status & STATUS_CI) != 0, expected_count > 2);
        write32_keyed(
            dut, impdef_offset(1),
            read32(dut, impdef_offset(1)) | IMPDEF_IC);
        write32_keyed(dut, record_offset(1, ERR_STATUS), STATUS_V | STATUS_UE | STATUS_CI);
    }
}

TEST(ZenaFmuTest, ChildFaultLatchesRootSummaryBeforeIrq)
{
    zena_fmu dut("fmu_hierarchy");
    dut.before_end_of_elaboration();

    write32_keyed_bank(dut, 1, impdef_offset(210), IMPDEF_IE);

    EXPECT_EQ(read32(dut, BANK_BYTES + ERRGSR0_L + 3 * 8) & (1u << 18),
              1u << 18);
    EXPECT_EQ(read32(dut, ERRGSR0_L) & (1u << 1), 1u << 1);

    write32_keyed_bank(dut, 1, impdef_offset(210), 0x18cu);
    EXPECT_TRUE(read32(dut, BANK_BYTES + record_offset(210, ERR_STATUS)) & STATUS_V);
    write32_keyed_bank(dut, 1, record_offset(210, ERR_STATUS), STATUS_V | STATUS_UE);
    EXPECT_EQ(read32(dut, BANK_BYTES + ERRGSR0_L + 3 * 8) & (1u << 18),
              0u);
    EXPECT_EQ(read32(dut, ERRGSR0_L) & (1u << 1), 1u << 1);

    write32_keyed(dut, impdef_offset(1), 0x18cu);
    EXPECT_NE(read32(dut, ERRGSR0_L) & (1u << 1), 0u);
    write32_keyed(dut, record_offset(1, ERR_STATUS), STATUS_V | STATUS_UE);
    EXPECT_EQ(read32(dut, ERRGSR0_L) & (1u << 1), 0u);
}

TEST(ZenaFmuTest, FeaturesAndControlsMatchInputClass)
{
    zena_fmu dut("fmu_features");
    dut.before_end_of_elaboration();
    EXPECT_EQ(read32(dut, record_offset(0, 0)), 0x00800002u);
    EXPECT_EQ(read32(dut, record_offset(1, 0)), 0x00800282u);
    EXPECT_EQ(read32(dut, record_offset(383, 0)), 0x00800282u);
    EXPECT_EQ(read32(dut, record_offset(0, ERR_CTLR)), 0x2001u);
    EXPECT_EQ(read32(dut, record_offset(1, ERR_CTLR)), 0x2019u);
}

TEST(ZenaFmuTest, InjectionUsesSelectedCauseAndDetectionEnable)
{
    zena_fmu dut("fmu_causes");
    dut.before_end_of_elaboration();
    write32_keyed(dut, impdef_offset(1), 0x180 | (1u << 12));
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) & 0x1f00, 1u << 11);
    write32_keyed(dut, impdef_offset(1), 0x180 | (1u << 3));
    EXPECT_TRUE(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V);
    write32_keyed(dut, impdef_offset(1), 0x180 | (1u << 2));
    EXPECT_TRUE(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V);
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) & 0x1f00, 1u << 11);
    write32_keyed(dut, record_offset(1, ERR_STATUS), STATUS_V | STATUS_UE);
    write32_keyed(dut, impdef_offset(1), 1u << 13); // DE.ERR_IN disabled
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V, 0u);
    write32_keyed(dut, impdef_offset(1), 0x180 | (1u << 9)); // internal-only cause
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V, 0u);
    write32_keyed(dut, impdef_offset(383), 0x70 | (1u << 9));
    EXPECT_EQ(read32(dut, record_offset(383, ERR_STATUS)) & 0x1f00, 1u << 8);
    write32_keyed(dut, impdef_offset(383), 0x70 | IMPDEF_IC);
    EXPECT_TRUE(read32(dut, record_offset(383, ERR_STATUS)) & STATUS_V);
}

TEST(ZenaFmuTest, DisabledRootRecordDoesNotCaptureChildFault)
{
    zena_fmu dut("fmu_root_ed");
    dut.before_end_of_elaboration();
    write32_keyed(dut, record_offset(1, ERR_CTLR), 0x2018);
    write32_keyed_bank(dut, 1, impdef_offset(20), 0x180 | (1u << 13));
    EXPECT_TRUE(read32(dut, BANK_BYTES + record_offset(20, ERR_STATUS)) & STATUS_V);
    EXPECT_EQ(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_V, 0u);
}

TEST(ZenaFmuTest, SparseWriteConsumesKeyAndReportsApbAccessError)
{
    zena_fmu dut("fmu_bad_access");
    dut.before_end_of_elaboration();
    write32(dut, SYS_KEY, SYS_KEY_VALUE);
    uint8_t data = 0;
    tlm::tlm_generic_payload trans;
    trans.set_address(ERR_CTLR);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_ptr(&data);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);
    EXPECT_EQ(read32(dut, SYS_KEY), 0u);
    EXPECT_EQ(read32(dut, record_offset(383, ERR_STATUS)) & 0x1f00, 1u << 8);
}

TEST(ZenaFmuTest, CorrectedProfileReservesUnsupportedFields)
{
    zena_fmu dut("fmu_corrected");
    dut.p_critical_records.set_value("1:3,4:204");
    dut.p_corrected_records.set_value("1:4");
    dut.before_end_of_elaboration();
    EXPECT_EQ(read32(dut, BANK_BYTES + record_offset(3, 0)), 0x00800002u);
    EXPECT_EQ(read32(dut, BANK_BYTES + record_offset(4, 0)), 0x00800882u);
    EXPECT_EQ(read32(dut, BANK_BYTES + record_offset(4, ERR_CTLR)), 0x2109u);
    write32_keyed_bank(dut, 1, record_offset(4, ERR_CTLR), ~0u);
    EXPECT_EQ(read32(dut, BANK_BYTES + record_offset(4, ERR_CTLR)), 0x2109u);
    write32_keyed_bank(dut, 1, impdef_offset(3), ~0u & ~IMPDEF_IC);
    EXPECT_EQ(read32(dut, BANK_BYTES + impdef_offset(3)), 0x180u);
    EXPECT_EQ(read32(dut, BANK_BYTES + record_offset(3, ERR_STATUS)) &
              (STATUS_CI | STATUS_UE | (3u << 24)), STATUS_CI);
    write32_keyed_bank(dut, 1, impdef_offset(4), IMPDEF_IE);
    EXPECT_EQ(read32(dut, BANK_BYTES + record_offset(4, ERR_STATUS)) &
              (STATUS_CI | STATUS_UE | (3u << 24)), 2u << 24);
}

TEST(ZenaFmuTest, InputProfilesRejectInvalidOrConflictingPins)
{
    zena_fmu dut("fmu_bad_profile");
    const auto actions = sc_core::sc_report_handler::set_actions(sc_core::SC_FATAL, sc_core::SC_THROW);
    for (const char* invalid : {"1:384", "5:0", "1:383", "1:2,1:2", "1:2,", "1.2"}) {
        dut.p_critical_records.set_value(invalid);
        EXPECT_THROW(dut.before_end_of_elaboration(), sc_core::sc_report);
    }
    dut.p_critical_records.set_value("1:2");
    dut.p_corrected_records.set_value("1:2");
    EXPECT_THROW(dut.before_end_of_elaboration(), sc_core::sc_report);
    dut.p_critical_records.set_value("");
    dut.p_corrected_records.set_value("");
    sc_core::sc_report_handler::set_actions(sc_core::SC_FATAL, actions);
}

TEST(ZenaFmuTest, InternalDetectionAndOverflowUseRecord383Only)
{
    zena_fmu dut("fmu_internal");
    dut.before_end_of_elaboration();
    write32(dut, SYS_KEY, 0); // INC_SEQ
    EXPECT_EQ(read32(dut, record_offset(383, ERR_STATUS)) & 0x1f00, 1u << 9);
    write32_keyed(dut, impdef_offset(383), 0x70 | (1u << 11));
    EXPECT_TRUE(read32(dut, record_offset(383, ERR_STATUS)) & (1u << 27));
    EXPECT_EQ(read32(dut, record_offset(383, ERR_STATUS)) & 0x1f00, 1u << 9);
    write32_keyed(dut, record_offset(383, ERR_STATUS), STATUS_V | STATUS_UE | (1u << 27));
    write32_keyed(dut, impdef_offset(383), 0); // all internal DE disabled
    write32(dut, SYS_KEY, 0);
    EXPECT_FALSE(read32(dut, record_offset(383, ERR_STATUS)) & STATUS_V);
    dut.p_record_count.set_value(16);
    dut.before_end_of_elaboration();
    write32(dut, SYS_KEY, 0);
    EXPECT_EQ(read32(dut, record_offset(15, ERR_STATUS)), 0u);
}

TEST(ZenaFmuTest, SequenceAndAccessChecksConsumeKeyWithoutChangingTarget)
{
    zena_fmu dut("fmu_access_checks");
    dut.before_end_of_elaboration();
    RequestContext context;
    context.secure_valid = true;
    context.secure = false;
    RequestContextTlmExtension extension(context);
    uint32_t value = SYS_KEY_VALUE;
    tlm::tlm_generic_payload trans;
    trans.set_extension(&extension);
    trans.set_address(SYS_KEY);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    EXPECT_EQ(read32(dut, SYS_KEY), 0u);
    EXPECT_EQ(read32(dut, record_offset(383, ERR_STATUS)) & 0x1f00, 1u << 8);
    trans.clear_extension(&extension);
    for (unsigned int length : {1u, 2u, 8u}) {
        write32_keyed(dut, record_offset(383, ERR_STATUS), STATUS_V | STATUS_UE | (1u << 27));
        write32(dut, SYS_KEY, SYS_KEY_VALUE);
        trans.set_address(ERR_CTLR);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        dut.b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
        EXPECT_EQ(read32(dut, SYS_KEY), 0u);
        EXPECT_EQ(read32(dut, ERR_CTLR), 0x2001u);
        EXPECT_EQ(read32(dut, record_offset(383, ERR_STATUS)) & 0x1f00, 1u << 8);
    }
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    unsigned char strobes[] = {0xff, 0xff, 0, 0xff};
    trans.set_byte_enable_ptr(strobes);
    trans.set_byte_enable_length(4);
    write32(dut, SYS_KEY, SYS_KEY_VALUE);
    dut.b_transport(trans, delay);
    EXPECT_EQ(read32(dut, SYS_KEY), 0u);
    EXPECT_EQ(read32(dut, ERR_CTLR), 0x2001u);
}

TEST(ZenaFmuTest, UpgradeAtZeroAndSaturationThresholds)
{
    zena_fmu dut("fmu_threshold_edges");
    dut.before_end_of_elaboration();
    for (uint32_t count : {0u, 254u, 255u}) {
        write32_keyed(dut, impdef_offset(1), 0x181 | (count << 16) |
            (count << 24) | (1u << 13));
        EXPECT_TRUE(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_CI);
        EXPECT_EQ((read32(dut, impdef_offset(1)) >> 16) & 255,
            std::min(count + 1, 255u));
        write32_keyed(dut, impdef_offset(1), 0x18c);
        write32_keyed(dut, record_offset(1, ERR_STATUS), STATUS_V | STATUS_UE | STATUS_CI);
    }
}

TEST(ZenaFmuTest, FaultSignalsFollowCriticalAndNonCriticalStatus)
{
    const char* configured_log = std::getenv("QBOX_I5_EVENT_LOG");
    const std::string event_log = configured_log != nullptr
        ? configured_log
        : "/tmp/qbox-i5-fault-event-observer.json";
    std::remove(event_log.c_str());

    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("zena_fmu_test"));
    broker.set_preset_cci_value(
        "fmu_external.fault_input_enabled", cci::cci_value(true));
    broker.set_preset_cci_value(
        "fmu_external.fault_input_record", cci::cci_value(0u));
    broker.set_preset_cci_value(
        "fmu_external.fault_source",
        cci::cci_value(std::string("ap_smmu_0.irq_eventq")));
    broker.set_preset_cci_value(
        "fmu_external.fault_id",
        cci::cci_value(std::string("translation-fault")));
    broker.set_preset_cci_value(
        "fmu_external.fault_sink",
        cci::cci_value(std::string("critical_irq")));
    broker.set_preset_cci_value(
        "fmu_external.event_log", cci::cci_value(event_log));

    zena_fmu level_fmu("fmu_levels");
    level_fmu.p_critical_records.set_value("1:3");
    level_fmu.p_corrected_records.set_value("1:4");
    TlmBinder producer_a("producer_a"), producer_b("producer_b");
    producer_a.socket.bind(level_fmu.device_fault_socket);
    producer_b.socket.bind(level_fmu.device_fault_socket);
    SignalSink level_critical("level_critical"), level_nc("level_nc");
    level_fmu.critical_irq.bind(level_critical.signal);
    level_fmu.non_critical_irq.bind(level_nc.signal);
    FaultSource level_reset("level_reset");
    level_reset.signal.bind(level_fmu.reset_in);
    zena_fmu dut("fmu_signals");
    zena_fmu external_fmu("fmu_external");
    zena_fmu disabled_fmu("fmu_disabled");
    zena_ssu ssu("ssu_external");
    zena_reset_ctrl reset_ctrl("reset_ctrl_external");
    TlmBinder tlm_binder("tlm_binder");
    TlmBinder external_tlm_binder("external_tlm_binder");
    TlmBinder disabled_tlm_binder("disabled_tlm_binder");
    TlmBinder ssu_tlm_binder("ssu_tlm_binder");
    TlmBinder reset_rgm_tlm_binder("reset_rgm_tlm_binder");
    TlmBinder reset_pik_tlm_binder("reset_pik_tlm_binder");
    FaultSource external_source("external_source");
    FaultSource disabled_source("disabled_source");
    SignalSink critical("critical_sink");
    SignalSink non_critical("non_critical_sink");
    SignalSink external_critical("external_critical_sink");
    SignalSink disabled_critical("disabled_critical_sink");
    SignalSink safety("safety_sink");

    tlm_binder.socket.bind(dut.target_socket);
    external_tlm_binder.socket.bind(external_fmu.target_socket);
    disabled_tlm_binder.socket.bind(disabled_fmu.target_socket);
    ssu_tlm_binder.socket.bind(ssu.target_socket);
    reset_rgm_tlm_binder.socket.bind(reset_ctrl.rgm);
    reset_pik_tlm_binder.socket.bind(reset_ctrl.pik);
    dut.critical_irq.bind(critical.signal);
    dut.non_critical_irq.bind(non_critical.signal);
    external_source.signal.bind(external_fmu.fault_in);
    external_fmu.critical_irq.bind(external_critical.signal);
    external_fmu.critical_ssu.bind(ssu.critical_in);
    disabled_source.signal.bind(disabled_fmu.fault_in);
    disabled_fmu.critical_irq.bind(disabled_critical.signal);
    ssu.safety_status.bind(reset_ctrl.safety_fault_reset);
    reset_ctrl.ap_reset.bind(safety.signal);
    dut.before_end_of_elaboration();
    external_fmu.before_end_of_elaboration();
    disabled_fmu.before_end_of_elaboration();
    ssu.before_end_of_elaboration();
    (void)reset_access32(reset_ctrl, 0x030, tlm::TLM_WRITE_COMMAND,
                         1u << 24);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));

    auto send_level = [&](TlmBinder& producer, unsigned int record, uint32_t value) {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(BANK_BYTES + record_offset(record, 0));
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        producer.socket->b_transport(trans, delay);
        EXPECT_FALSE(trans.is_dmi_allowed());
        return trans.get_response_status();
    };
    auto ack = [&](unsigned int bank, unsigned int record) {
        write32_keyed_bank(level_fmu, bank, record_offset(record, ERR_STATUS),
                          STATUS_V | STATUS_UE | STATUS_CI | (3u << 24) | (1u << 27));
    };
    auto has_level = [](const SignalSink& sink) {
        return !sink.observed.empty() && sink.observed.back();
    };
    EXPECT_EQ(send_level(producer_a, 3, 3), tlm::TLM_OK_RESPONSE);
    EXPECT_TRUE(has_level(level_critical));
    EXPECT_EQ(send_level(producer_a, 3, 0), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ack(1, 3); // invalid deassert left the original source asserted
    EXPECT_TRUE(read32(level_fmu, BANK_BYTES + record_offset(3, ERR_STATUS)) & STATUS_V);
    EXPECT_EQ(send_level(producer_b, 3, 3), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(send_level(producer_a, 3, 2), tlm::TLM_OK_RESPONSE);
    ack(1, 3);
    EXPECT_TRUE(has_level(level_critical)); // second producer still holds the pin
    EXPECT_EQ(send_level(producer_b, 3, 2), tlm::TLM_OK_RESPONSE);
    write32_keyed_bank(level_fmu, 1, impdef_offset(3), 0x18c);
    EXPECT_TRUE(read32(level_fmu, BANK_BYTES + record_offset(3, ERR_STATUS)) & STATUS_V);
    EXPECT_TRUE(has_level(level_critical)); // IC cannot ACK a deasserted hardware fault
    ack(1, 3);
    EXPECT_TRUE(has_level(level_critical)); // root summary remains latched
    ack(0, 0);
    EXPECT_FALSE(has_level(level_critical));

    // Corrected interrupt requires FI AND CFI, uncorrected requires FI AND UE.
    for (unsigned int record : {4u, 5u}) {
        const uint32_t enable = record == 4 ? 0x100 : 0x10;
        write32_keyed_bank(level_fmu, 1, record_offset(record, ERR_CTLR), 0x2001 | enable);
        EXPECT_EQ(send_level(producer_a, record, 1), tlm::TLM_OK_RESPONSE);
        EXPECT_FALSE(has_level(level_nc));
        write32_keyed_bank(level_fmu, 1, record_offset(record, ERR_CTLR), 0x2009);
        EXPECT_FALSE(has_level(level_nc));
        write32_keyed_bank(level_fmu, 1, record_offset(record, ERR_CTLR), 0x2009 | enable);
        EXPECT_TRUE(has_level(level_nc));
        EXPECT_EQ((read32(level_fmu, BANK_BYTES + impdef_offset(record)) >> 16) & 255, 1u);
        const uint32_t syndrome = read32(level_fmu, BANK_BYTES + record_offset(record, ERR_STATUS));
        write32_keyed_bank(level_fmu, 1, impdef_offset(record),
            read32(level_fmu, BANK_BYTES + impdef_offset(record)) | IMPDEF_IC);
        EXPECT_EQ(read32(level_fmu, BANK_BYTES + record_offset(record, ERR_STATUS)), syndrome);
        for (unsigned int attempt = 0; attempt < 3; ++attempt) ack(1, record);
        EXPECT_EQ((read32(level_fmu, BANK_BYTES + impdef_offset(record)) >> 16) & 255, 1u);
        EXPECT_EQ(send_level(producer_a, record, 0), tlm::TLM_OK_RESPONSE);
        ack(1, record);
        ack(0, 1);
        EXPECT_FALSE(has_level(level_nc));
    }
    // A parity injection and a real ERR_IN share one architectural record.
    // IC acknowledges neither syndrome; W1C discards the completed pulse but
    // the held hardware input re-latches without counting an additional event.
    EXPECT_EQ(send_level(producer_a, 7, 1), tlm::TLM_OK_RESPONSE);
    write32_keyed_bank(level_fmu, 1, impdef_offset(7),
        read32(level_fmu, BANK_BYTES + impdef_offset(7)) | (1u << 12));
    EXPECT_EQ(read32(level_fmu, BANK_BYTES + record_offset(7, ERR_STATUS)) & 0x1f00, 0x1800u);
    write32_keyed_bank(level_fmu, 1, impdef_offset(7),
        read32(level_fmu, BANK_BYTES + impdef_offset(7)) | IMPDEF_IC);
    EXPECT_EQ(read32(level_fmu, BANK_BYTES + record_offset(7, ERR_STATUS)) & 0x1f00, 0x1800u);
    EXPECT_TRUE(read32(level_fmu, BANK_BYTES + record_offset(7, ERR_STATUS)) & STATUS_V);
    ack(1, 7);
    EXPECT_EQ(read32(level_fmu, BANK_BYTES + record_offset(7, ERR_STATUS)) & 0x1f00, 0x1000u);
    EXPECT_EQ((read32(level_fmu, BANK_BYTES + impdef_offset(7)) >> 16) & 255, 2u);
    EXPECT_EQ(send_level(producer_a, 7, 0), tlm::TLM_OK_RESPONSE);
    ack(1, 7); ack(0, 1);
    EXPECT_EQ(send_level(producer_a, 7, 1), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ((read32(level_fmu, BANK_BYTES + impdef_offset(7)) >> 16) & 255, 3u);
    EXPECT_EQ(send_level(producer_a, 7, 0), tlm::TLM_OK_RESPONSE);
    ack(1, 7); ack(0, 1);

    // A real pin edge obeys DE and escalation; ACK/re-latch never increments CNT.
    write32_keyed_bank(level_fmu, 1, impdef_offset(5), 0x81 | (1u << 24));
    EXPECT_EQ(send_level(producer_a, 5, 1), tlm::TLM_OK_RESPONSE);
    EXPECT_FALSE(read32(level_fmu, BANK_BYTES + record_offset(5, ERR_STATUS)) & STATUS_V);
    write32_keyed_bank(level_fmu, 1, impdef_offset(5), 0x181 | (1u << 24));
    EXPECT_TRUE(has_level(level_nc));
    ack(1, 5);
    EXPECT_FALSE(read32(level_fmu, BANK_BYTES + record_offset(5, ERR_STATUS)) & STATUS_CI);
    EXPECT_EQ(send_level(producer_a, 5, 0), tlm::TLM_OK_RESPONSE);
    ack(1, 5); ack(0, 1);
    EXPECT_EQ(send_level(producer_a, 5, 1), tlm::TLM_OK_RESPONSE);
    EXPECT_TRUE(has_level(level_critical));
    EXPECT_EQ((read32(level_fmu, BANK_BYTES + impdef_offset(5)) >> 16) & 255, 2u);
    EXPECT_EQ(send_level(producer_a, 5, 0), tlm::TLM_OK_RESPONSE);
    level_reset.write(true);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_PS));
    EXPECT_EQ(read32(level_fmu, BANK_BYTES + record_offset(5, ERR_STATUS)), 0u);
    EXPECT_FALSE(has_level(level_critical));
    EXPECT_FALSE(has_level(level_nc));
    level_reset.write(false);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_PS));

    write32_keyed(dut, impdef_offset(0), IMPDEF_IE);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    ASSERT_FALSE(critical.observed.empty());
    EXPECT_TRUE(critical.observed.back());
    EXPECT_TRUE(non_critical.observed.empty() || !non_critical.observed.back());
    EXPECT_NE(read32(dut, record_offset(0, ERR_STATUS)) & STATUS_CI, 0u);

    write32_keyed(dut, record_offset(0, ERR_STATUS), STATUS_V | STATUS_CI);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    ASSERT_FALSE(critical.observed.empty());
    EXPECT_FALSE(critical.observed.back());

    write32_keyed(dut, impdef_offset(1), IMPDEF_IE);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    ASSERT_FALSE(non_critical.observed.empty());
    EXPECT_TRUE(non_critical.observed.back());
    EXPECT_NE(read32(dut, record_offset(1, ERR_STATUS)) & STATUS_UE, 0u);

    external_source.write(true);
    disabled_source.write(true);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_PS));

    EXPECT_NE(read32(external_fmu, record_offset(0, ERR_STATUS)) & STATUS_V,
              0u);
    EXPECT_NE(read32(external_fmu, record_offset(0, ERR_STATUS)) & STATUS_CI,
              0u);
    ASSERT_FALSE(external_critical.observed.empty());
    EXPECT_TRUE(external_critical.observed.back());
    EXPECT_EQ(ssu_read32(ssu, SSU_SYS_STATUS), SSU_STATUS_ERRC);
    ASSERT_FALSE(safety.observed.empty());
    EXPECT_TRUE(safety.observed.back());
    EXPECT_NE(reset_access32(reset_ctrl, 0x020, tlm::TLM_READ_COMMAND) &
                  (1u << 24),
              0u);

    EXPECT_EQ(read32(disabled_fmu, record_offset(0, ERR_STATUS)), 0u);
    EXPECT_TRUE(disabled_critical.observed.empty());

    write32_keyed(external_fmu, record_offset(0, ERR_STATUS),
                  STATUS_V | STATUS_CI);
    EXPECT_EQ(read32(external_fmu, record_offset(0, ERR_STATUS)) & STATUS_V,
              0u);
    ASSERT_FALSE(external_critical.observed.empty());
    EXPECT_FALSE(external_critical.observed.back());

    ssu_write32_keyed(ssu, SSU_ERR_STATUS, STATUS_V);
    EXPECT_EQ(ssu_read32(ssu, SSU_SYS_STATUS), SSU_STATUS_SAFE);
    ASSERT_FALSE(safety.observed.empty());
    EXPECT_FALSE(safety.observed.back());

    std::ifstream input(event_log);
    ASSERT_TRUE(input.is_open());
    const std::string json((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    const std::size_t source_pos = json.find("\"phase\": \"source\"");
    const std::size_t record_pos = json.find("\"phase\": \"record\"");
    const std::size_t sink_assert_pos =
        json.find("\"phase\": \"sink_assert\"");
    const std::size_t clear_pos = json.find("\"phase\": \"clear\"");
    const std::size_t sink_deassert_pos =
        json.find("\"phase\": \"sink_deassert\"");
    const std::size_t recovery_pos = json.find("\"phase\": \"recovery\"");

    ASSERT_NE(source_pos, std::string::npos);
    ASSERT_LT(source_pos, record_pos);
    ASSERT_LT(record_pos, sink_assert_pos);
    ASSERT_LT(sink_assert_pos, clear_pos);
    ASSERT_LT(clear_pos, sink_deassert_pos);
    ASSERT_LT(sink_deassert_pos, recovery_pos);
    EXPECT_NE(json.find("\"source\": \"ap_smmu_0.irq_eventq\""),
              std::string::npos);
    EXPECT_NE(json.find("\"fault_id\": \"translation-fault\""),
              std::string::npos);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
