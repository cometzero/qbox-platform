#ifndef QBOX_PLATFORM_RSE_CPU_ACCEL_H
#define QBOX_PLATFORM_RSE_CPU_ACCEL_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <cci_configuration>

#include "cpu-pc-entry-observer.h"
#include "cpu-semantic-context.h"
#include "cc3xx_core.h"
#include "rse_lms_accel.h"
#include "rse_mcuboot_image.h"
#include "rse_p256_ecdsa.h"

namespace qbox {
namespace platform {
namespace rse_cpu_accel {

template <typename T>
class ConfigValue
{
public:
    ConfigValue() = default;
    explicit ConfigValue(T value): m_value(std::move(value)) {}
    const T& get_value() const { return m_value; }
    ConfigValue& operator=(T value)
    {
        m_value = std::move(value);
        return *this;
    }

private:
    T m_value{};
};

class RseCpuAccel : public QemuCpuPcEntryObserver
{
private:
    QemuCpuSemanticContext& m_context;
    cci::cci_broker_handle m_broker;
    std::string m_parameter_prefix;
    std::string m_cpu_name;
    ConfigValue<bool> p_hotpath_accel;
    ConfigValue<uint64_t> p_hotpath_memcpy_addr;
    ConfigValue<uint64_t> p_hotpath_memset_addr;
    ConfigValue<uint64_t> p_hotpath_max_bytes;
    ConfigValue<std::string> p_hotpath_profile_file;
    ConfigValue<uint64_t> p_hotpath_profile_interval;
    ConfigValue<bool> p_lms_accel;
    ConfigValue<uint64_t> p_lms_verify_addr;
    ConfigValue<uint64_t> p_lms_max_data_bytes;
    ConfigValue<bool> p_bl2_load_profile;
    ConfigValue<uint64_t> p_bl2_boot_go_for_image_id_addr;
    ConfigValue<uint64_t> p_bl2_boot_load_image_to_sram_addr;
    ConfigValue<uint64_t> p_bl2_boot_enc_load_addr;
    ConfigValue<uint64_t> p_bl2_boot_enc_decrypt_addr;
    ConfigValue<uint64_t> p_bl2_bootutil_img_validate_addr;
    ConfigValue<uint64_t> p_bl2_bootutil_img_hash_addr;
    ConfigValue<uint64_t> p_bl2_bootutil_verify_sig_addr;
    ConfigValue<uint64_t> p_bl2_boot_image_count;
    ConfigValue<uint64_t> p_bl2_boot_state_curr_img_offset;
    ConfigValue<uint64_t> p_bl2_boot_state_imgs_offset;
    ConfigValue<uint64_t> p_bl2_boot_state_image_stride;
    ConfigValue<uint64_t> p_bl2_boot_state_slot_stride;
    ConfigValue<uint64_t> p_bl2_boot_state_slot_usage_offset;
    ConfigValue<uint64_t> p_bl2_boot_state_slot_usage_stride;
    ConfigValue<uint64_t> p_bl2_boot_slot_usage_img_dst_offset;
    ConfigValue<uint64_t> p_bl2_boot_slot_usage_img_sz_offset;
    ConfigValue<bool> p_bl2_load_accel;
    ConfigValue<uint64_t> p_bl2_load_accel_max_bytes;
    ConfigValue<bool> p_bl2_boot_enc_accel;
    ConfigValue<uint64_t> p_bl2_boot_enc_set_key_addr;
    ConfigValue<uint64_t> p_bl2_boot_status_enckey_offset;
    ConfigValue<uint64_t> p_bl2_boot_enc_key_bytes;
    ConfigValue<uint64_t> p_bl2_boot_enc_key_stride;
    ConfigValue<uint64_t> p_bl2_boot_enc_slots;
    ConfigValue<uint64_t> p_bl2_boot_enc_max_bytes;
    ConfigValue<bool> p_bl2_img_hash_accel;
    ConfigValue<uint64_t> p_bl2_img_hash_max_bytes;
    ConfigValue<uint64_t> p_bl2_img_hash_max_seed_bytes;
    ConfigValue<bool> p_bl2_verify_sig_accel;
    ConfigValue<bool> p_bl2_verify_sig_skip;
    ConfigValue<uint64_t> p_bl2_bootutil_keys_addr;
    ConfigValue<uint64_t> p_bl2_bootutil_key_cnt_addr;
    ConfigValue<uint64_t> p_bl2_fih_success_addr;
    ConfigValue<uint64_t> p_bl2_verify_sig_max_key_bytes;
    ConfigValue<uint64_t> p_bl2_verify_sig_max_sig_bytes;
    ConfigValue<bool> p_bl2_delay_accel;
    ConfigValue<uint64_t> p_bl2_delay_cycles_addr;
    ConfigValue<uint64_t> p_bl2_delay_max_cycles;
    ConfigValue<uint64_t> p_bl2_delay_expected_hits;

    std::atomic<uint64_t> m_hotpath_memcpy_hits{ 0 };
    std::atomic<uint64_t> m_hotpath_memset_hits{ 0 };
    std::atomic<uint64_t> m_hotpath_fallbacks{ 0 };
    std::atomic<uint64_t> m_hotpath_pc_misses{ 0 };
    std::atomic<uint64_t> m_hotpath_too_large_fails{ 0 };
    std::atomic<uint64_t> m_hotpath_dmi_fails{ 0 };
    std::atomic<uint64_t> m_hotpath_stack_fails{ 0 };
    std::atomic<uint64_t> m_hotpath_state_fails{ 0 };
    std::atomic<uint64_t> m_lms_accel_hits{ 0 };
    std::atomic<uint64_t> m_lms_accel_pc_misses{ 0 };
    std::atomic<uint64_t> m_lms_accel_dmi_fails{ 0 };
    std::atomic<uint64_t> m_lms_accel_verify_fails{ 0 };
    std::atomic<uint64_t> m_lms_accel_state_fails{ 0 };
    std::atomic<uint64_t> m_lms_accel_unsupported{ 0 };
    std::atomic<bool> m_lms_accel_done{ false };
    std::atomic<uint32_t> m_lms_last_key_addr{ 0 };
    std::atomic<uint32_t> m_lms_last_key_size{ 0 };
    std::atomic<uint32_t> m_lms_last_data_addr{ 0 };
    std::atomic<uint32_t> m_lms_last_data_size{ 0 };
    std::atomic<uint32_t> m_lms_last_sig_addr{ 0 };
    std::atomic<uint32_t> m_lms_last_sig_size{ 0 };
    std::atomic<uint32_t> m_lms_last_unsupported_mask{ 0 };

    struct Bl2ProfileSample {
        std::atomic<uint32_t> pc{ 0 };
        std::atomic<uint32_t> r0{ 0 };
        std::atomic<uint32_t> r1{ 0 };
        std::atomic<uint32_t> r2{ 0 };
        std::atomic<uint32_t> r3{ 0 };
        std::atomic<uint32_t> sp{ 0 };
        std::atomic<uint32_t> lr{ 0 };
        std::atomic<uint32_t> stack0{ 0 };
        std::atomic<uint32_t> stack1{ 0 };
        std::atomic<uint32_t> stack2{ 0 };
        std::atomic<uint32_t> stack3{ 0 };
        std::atomic<uint32_t> stack_words{ 0 };
    };

    struct Bl2ProfileSite {
        std::atomic<uint64_t> hits{ 0 };
        Bl2ProfileSample last;
    };

    std::atomic<uint64_t> m_bl2_load_profile_pc_misses{ 0 };
    std::atomic<uint64_t> m_bl2_load_profile_stack_fails{ 0 };
    Bl2ProfileSite m_bl2_boot_go_for_image_id;
    Bl2ProfileSite m_bl2_boot_load_image_to_sram;
    Bl2ProfileSite m_bl2_boot_enc_load;
    Bl2ProfileSite m_bl2_boot_enc_decrypt;
    Bl2ProfileSite m_bl2_bootutil_img_validate;
    Bl2ProfileSite m_bl2_bootutil_img_hash;
    Bl2ProfileSite m_bl2_bootutil_verify_sig;

    struct Bl2RamLoadSnapshot {
        std::atomic<uint64_t> hits{ 0 };
        std::atomic<uint64_t> dmi_failures{ 0 };
        std::atomic<uint64_t> unsupported{ 0 };
        std::atomic<uint32_t> last_state{ 0 };
        std::atomic<uint32_t> last_curr_img{ 0 };
        std::atomic<uint32_t> last_active_slot{ 0 };
        std::atomic<uint32_t> last_hdr_addr{ 0 };
        std::atomic<uint32_t> last_hdr_magic{ 0 };
        std::atomic<uint32_t> last_load_addr{ 0 };
        std::atomic<uint32_t> last_hdr_size{ 0 };
        std::atomic<uint32_t> last_protect_tlv_size{ 0 };
        std::atomic<uint32_t> last_img_size{ 0 };
        std::atomic<uint32_t> last_flags{ 0 };
        std::atomic<uint32_t> last_hash_region_size{ 0 };
        std::atomic<uint32_t> last_slot_img_dst{ 0 };
        std::atomic<uint32_t> last_slot_img_sz{ 0 };
        std::atomic<uint32_t> last_unsupported_mask{ 0 };
    };

    static constexpr size_t BL2_RAM_LOAD_SNAPSHOT_IMAGE_SLOTS = 64;

    Bl2RamLoadSnapshot m_bl2_ram_load_snapshot;
    std::array<Bl2RamLoadSnapshot, BL2_RAM_LOAD_SNAPSHOT_IMAGE_SLOTS>
        m_bl2_ram_load_snapshot_by_image;

    std::array<std::atomic<bool>, BL2_RAM_LOAD_SNAPSHOT_IMAGE_SLOTS>
        m_bl2_load_accel_image_decrypted;
    std::atomic<uint64_t> m_bl2_load_accel_hits{ 0 };
    std::atomic<uint64_t> m_bl2_load_accel_skip_hits{ 0 };
    std::atomic<uint64_t> m_bl2_load_accel_bytes{ 0 };
    std::atomic<uint64_t> m_bl2_load_accel_key_misses{ 0 };
    std::atomic<uint64_t> m_bl2_load_accel_dmi_failures{ 0 };
    std::atomic<uint64_t> m_bl2_load_accel_state_failures{ 0 };
    std::atomic<uint64_t> m_bl2_load_accel_unsupported{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_curr_img{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_slot{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_off{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_size{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_blk_off{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_buf{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_img_size{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_flags{ 0 };
    std::atomic<uint32_t> m_bl2_load_accel_last_unsupported_mask{ 0 };

    struct Bl2EncKeyCacheEntry {
        bool valid = false;
        uint32_t key_size = 0;
        std::array<uint8_t, 32> key{};
    };

    std::mutex m_bl2_boot_enc_lock;
    std::map<uint64_t, Bl2EncKeyCacheEntry> m_bl2_boot_enc_keys;
    std::atomic<uint64_t> m_bl2_boot_enc_key_captures{ 0 };
    std::atomic<uint64_t> m_bl2_boot_enc_key_capture_failures{ 0 };
    std::atomic<uint64_t> m_bl2_boot_enc_decrypt_hits{ 0 };
    std::atomic<uint64_t> m_bl2_boot_enc_decrypt_bytes{ 0 };
    std::atomic<uint64_t> m_bl2_boot_enc_decrypt_key_misses{ 0 };
    std::atomic<uint64_t> m_bl2_boot_enc_decrypt_dmi_failures{ 0 };
    std::atomic<uint64_t> m_bl2_boot_enc_decrypt_state_failures{ 0 };
    std::atomic<uint64_t> m_bl2_boot_enc_decrypt_unsupported{ 0 };
    std::atomic<uint32_t> m_bl2_boot_enc_last_enc_state{ 0 };
    std::atomic<uint32_t> m_bl2_boot_enc_last_slot{ 0 };
    std::atomic<uint32_t> m_bl2_boot_enc_last_off{ 0 };
    std::atomic<uint32_t> m_bl2_boot_enc_last_size{ 0 };
    std::atomic<uint32_t> m_bl2_boot_enc_last_blk_off{ 0 };
    std::atomic<uint32_t> m_bl2_boot_enc_last_buf{ 0 };
    std::atomic<uint32_t> m_bl2_boot_enc_last_unsupported_mask{ 0 };
    std::atomic<uint64_t> m_bl2_img_hash_hits{ 0 };
    std::atomic<uint64_t> m_bl2_img_hash_bytes{ 0 };
    std::atomic<uint64_t> m_bl2_img_hash_dmi_failures{ 0 };
    std::atomic<uint64_t> m_bl2_img_hash_state_failures{ 0 };
    std::atomic<uint64_t> m_bl2_img_hash_unsupported{ 0 };
    std::atomic<uint32_t> m_bl2_img_hash_last_hdr{ 0 };
    std::atomic<uint32_t> m_bl2_img_hash_last_load_addr{ 0 };
    std::atomic<uint32_t> m_bl2_img_hash_last_hash_result{ 0 };
    std::atomic<uint32_t> m_bl2_img_hash_last_hash_size{ 0 };
    std::atomic<uint32_t> m_bl2_img_hash_last_seed{ 0 };
    std::atomic<uint32_t> m_bl2_img_hash_last_seed_len{ 0 };
    std::atomic<uint32_t> m_bl2_img_hash_last_unsupported_mask{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_matches{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_cache_hits{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_cache_misses{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_skip_hits{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_dmi_failures{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_verify_failures{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_state_failures{ 0 };
    std::atomic<uint64_t> m_bl2_verify_sig_unsupported{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_hash{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_hlen{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_sig{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_slen{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_key_id{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_key_ptr{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_key_len{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_fih_success{ 0 };
    std::atomic<uint32_t> m_bl2_verify_sig_last_unsupported_mask{ 0 };
    std::atomic<uint64_t> m_bl2_delay_hits{ 0 };
    std::atomic<uint64_t> m_bl2_delay_cycles{ 0 };
    std::atomic<uint64_t> m_bl2_delay_state_failures{ 0 };
    std::atomic<uint64_t> m_bl2_delay_unsupported{ 0 };
    std::atomic<uint32_t> m_bl2_delay_last_cycles{ 0 };
    std::atomic<uint32_t> m_bl2_delay_last_lr{ 0 };
    std::atomic<uint32_t> m_bl2_delay_last_unsupported_mask{ 0 };
    std::atomic<bool> m_bl2_delay_watch_active{ false };

    struct Bl2VerifySigCacheEntry {
        std::vector<uint8_t> public_key;
        std::vector<uint8_t> hash;
        std::vector<uint8_t> signature;
        bool verified = false;
    };

    std::mutex m_bl2_verify_sig_cache_lock;
    std::vector<Bl2VerifySigCacheEntry> m_bl2_verify_sig_cache;
    std::mutex m_hotpath_profile_lock;
    bool m_pc_entry_registered = false;

    enum class HotpathFailReason {
        PcMismatch,
        TooLarge,
        Dmi,
        Stack,
        State,
    };

    template <typename T>
    T param(const std::string& name, const T& default_value) const
    {
        const std::string full_name = m_parameter_prefix + name;
        auto handle = m_broker.get_param_handle(full_name);
        T value{};
        if (handle.is_valid() && handle.get_cci_value().try_get<T>(value)) {
            return value;
        }
        auto preset = m_broker.get_preset_cci_value(full_name);
        if (preset.template try_get<T>(value)) {
            return value;
        }
        return default_value;
    }

    void load_config()
    {
        p_hotpath_accel = param("hotpath_accel", false);
        p_hotpath_memcpy_addr = param<uint64_t>("hotpath_memcpy_addr", 0);
        p_hotpath_memset_addr = param<uint64_t>("hotpath_memset_addr", 0);
        p_hotpath_max_bytes = param<uint64_t>("hotpath_max_bytes", 16 * 1024 * 1024);
        p_hotpath_profile_file = param<std::string>("hotpath_profile_file", "");
        p_hotpath_profile_interval = param<uint64_t>("hotpath_profile_interval", 1024);
        p_lms_accel = param("lms_accel", false);
        p_lms_verify_addr = param<uint64_t>("lms_verify_addr", 0);
        p_lms_max_data_bytes = param<uint64_t>("lms_max_data_bytes", 4096);
        p_bl2_load_profile = param("bl2_load_profile", false);
        p_bl2_boot_go_for_image_id_addr = param<uint64_t>("bl2_boot_go_for_image_id_addr", 0);
        p_bl2_boot_load_image_to_sram_addr = param<uint64_t>("bl2_boot_load_image_to_sram_addr", 0);
        p_bl2_boot_enc_load_addr = param<uint64_t>("bl2_boot_enc_load_addr", 0);
        p_bl2_boot_enc_decrypt_addr = param<uint64_t>("bl2_boot_enc_decrypt_addr", 0);
        p_bl2_bootutil_img_validate_addr = param<uint64_t>("bl2_bootutil_img_validate_addr", 0);
        p_bl2_bootutil_img_hash_addr = param<uint64_t>("bl2_bootutil_img_hash_addr", 0);
        p_bl2_bootutil_verify_sig_addr = param<uint64_t>("bl2_bootutil_verify_sig_addr", 0);
        p_bl2_boot_image_count = param<uint64_t>("bl2_boot_image_count", 5);
        p_bl2_boot_state_curr_img_offset = param<uint64_t>("bl2_boot_state_curr_img_offset", 0x10c8);
        p_bl2_boot_state_imgs_offset = param<uint64_t>("bl2_boot_state_imgs_offset", 0);
        p_bl2_boot_state_image_stride = param<uint64_t>("bl2_boot_state_image_stride", 88);
        p_bl2_boot_state_slot_stride = param<uint64_t>("bl2_boot_state_slot_stride", 44);
        p_bl2_boot_state_slot_usage_offset = param<uint64_t>("bl2_boot_state_slot_usage_offset", 0x10d0);
        p_bl2_boot_state_slot_usage_stride = param<uint64_t>("bl2_boot_state_slot_usage_stride", 16);
        p_bl2_boot_slot_usage_img_dst_offset = param<uint64_t>("bl2_boot_slot_usage_img_dst_offset", 8);
        p_bl2_boot_slot_usage_img_sz_offset = param<uint64_t>("bl2_boot_slot_usage_img_sz_offset", 12);
        p_bl2_load_accel = param("bl2_load_accel", false);
        p_bl2_load_accel_max_bytes = param<uint64_t>("bl2_load_accel_max_bytes", 16 * 1024 * 1024);
        p_bl2_boot_enc_accel = param("bl2_boot_enc_accel", false);
        p_bl2_boot_enc_set_key_addr = param<uint64_t>("bl2_boot_enc_set_key_addr", 0);
        p_bl2_boot_status_enckey_offset = param<uint64_t>("bl2_boot_status_enckey_offset", 0x0c);
        p_bl2_boot_enc_key_bytes = param<uint64_t>("bl2_boot_enc_key_bytes", 16);
        p_bl2_boot_enc_key_stride = param<uint64_t>("bl2_boot_enc_key_stride", 16);
        p_bl2_boot_enc_slots = param<uint64_t>("bl2_boot_enc_slots", 2);
        p_bl2_boot_enc_max_bytes = param<uint64_t>("bl2_boot_enc_max_bytes", 4096);
        p_bl2_img_hash_accel = param("bl2_img_hash_accel", false);
        p_bl2_img_hash_max_bytes = param<uint64_t>("bl2_img_hash_max_bytes", 16 * 1024 * 1024);
        p_bl2_img_hash_max_seed_bytes = param<uint64_t>("bl2_img_hash_max_seed_bytes", 4096);
        p_bl2_verify_sig_accel = param("bl2_verify_sig_accel", false);
        p_bl2_verify_sig_skip = param("bl2_verify_sig_skip", false);
        p_bl2_bootutil_keys_addr = param<uint64_t>("bl2_bootutil_keys_addr", 0);
        p_bl2_bootutil_key_cnt_addr = param<uint64_t>("bl2_bootutil_key_cnt_addr", 0);
        p_bl2_fih_success_addr = param<uint64_t>("bl2_fih_success_addr", 0);
        p_bl2_verify_sig_max_key_bytes = param<uint64_t>("bl2_verify_sig_max_key_bytes", 512);
        p_bl2_verify_sig_max_sig_bytes = param<uint64_t>("bl2_verify_sig_max_sig_bytes", 128);
        p_bl2_delay_accel = param("bl2_delay_accel", false);
        p_bl2_delay_cycles_addr = param<uint64_t>("bl2_delay_cycles_addr", 0);
        p_bl2_delay_max_cycles = param<uint64_t>("bl2_delay_max_cycles", 50 * 1000 * 1000);
        p_bl2_delay_expected_hits = param<uint64_t>("bl2_delay_expected_hits", 3);
    }

    bool hotpath_dmi_ptr(uint64_t address, uint64_t size, bool need_read,
                         bool need_write, uint8_t*& ptr)
    {
        return m_context.guest_dmi_ptr(address, size, need_read, need_write, ptr);
    }

    static void add_pc_entry_watch(qemu::Cpu& cpu, bool enabled, uint64_t pc)
    {
        if (!enabled) {
            return;
        }

        const uint64_t entry = pc & ~1ull;
        if (entry != 0) {
            cpu.add_pc_entry_watch(entry);
        }
    }

    bool hotpath_read_u32(uint64_t address, uint32_t& value)
    {
        uint8_t* ptr = nullptr;
        if (hotpath_dmi_ptr(address, sizeof(value), true, false, ptr)) {
            std::memcpy(&value, ptr, sizeof(value));
            return true;
        }

        std::vector<uint8_t> bytes;
        if (!hotpath_read_bytes(address, sizeof(value), bytes) ||
            bytes.size() != sizeof(value)) {
            return false;
        }
        std::memcpy(&value, bytes.data(), sizeof(value));
        return true;
    }

    bool hotpath_read_u8(uint64_t address, uint8_t& value)
    {
        uint8_t* ptr = nullptr;
        if (hotpath_dmi_ptr(address, sizeof(value), true, false, ptr)) {
            value = *ptr;
            return true;
        }

        std::vector<uint8_t> bytes;
        if (!hotpath_read_bytes(address, sizeof(value), bytes) ||
            bytes.size() != sizeof(value)) {
            return false;
        }
        value = bytes[0];
        return true;
    }

    bool hotpath_read_bytes(uint64_t address, uint64_t size,
                            std::vector<uint8_t>& out)
    {
        return m_context.guest_read_bytes(address, size, out);
    }

    bool hotpath_write_bytes(uint64_t address, const uint8_t* data, uint64_t size)
    {
        return m_context.guest_write_bytes(address, data, size);
    }

    static std::string profile_hex_string(uint64_t value)
    {
        std::ostringstream ss;
        ss << "0x" << std::hex << value;
        return ss.str();
    }

    bool hotpath_memcpy_safe_pc(uint32_t pc, uint32_t base) const
    {
        return pc == base ||
               pc == base + 0x14 ||
               pc == base + 0x36 ||
               pc == base + 0x4a;
    }

    bool hotpath_memset_safe_pc(uint32_t pc, uint32_t base) const
    {
        return pc == base ||
               pc == base + 0x24 ||
               pc == base + 0x36;
    }

    uint64_t bl2_load_profile_events() const
    {
        return m_bl2_boot_go_for_image_id.hits.load(std::memory_order_relaxed) +
               m_bl2_boot_load_image_to_sram.hits.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_load.hits.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_decrypt.hits.load(std::memory_order_relaxed) +
               m_bl2_bootutil_img_validate.hits.load(std::memory_order_relaxed) +
               m_bl2_bootutil_img_hash.hits.load(std::memory_order_relaxed) +
               m_bl2_bootutil_verify_sig.hits.load(std::memory_order_relaxed) +
               m_bl2_load_profile_pc_misses.load(std::memory_order_relaxed) +
               m_bl2_load_profile_stack_fails.load(std::memory_order_relaxed);
    }

    uint64_t bl2_boot_enc_accel_events() const
    {
        return m_bl2_boot_enc_key_captures.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_key_capture_failures.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_decrypt_hits.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_decrypt_key_misses.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_decrypt_dmi_failures.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_decrypt_state_failures.load(std::memory_order_relaxed) +
               m_bl2_boot_enc_decrypt_unsupported.load(std::memory_order_relaxed);
    }

    uint64_t bl2_load_accel_events() const
    {
        return m_bl2_load_accel_hits.load(std::memory_order_relaxed) +
               m_bl2_load_accel_skip_hits.load(std::memory_order_relaxed) +
               m_bl2_load_accel_key_misses.load(std::memory_order_relaxed) +
               m_bl2_load_accel_dmi_failures.load(std::memory_order_relaxed) +
               m_bl2_load_accel_state_failures.load(std::memory_order_relaxed) +
               m_bl2_load_accel_unsupported.load(std::memory_order_relaxed);
    }

    uint64_t bl2_img_hash_accel_events() const
    {
        return m_bl2_img_hash_hits.load(std::memory_order_relaxed) +
               m_bl2_img_hash_dmi_failures.load(std::memory_order_relaxed) +
               m_bl2_img_hash_state_failures.load(std::memory_order_relaxed) +
               m_bl2_img_hash_unsupported.load(std::memory_order_relaxed);
    }

    uint64_t bl2_verify_sig_accel_events() const
    {
        return m_bl2_verify_sig_matches.load(std::memory_order_relaxed) +
               m_bl2_verify_sig_skip_hits.load(std::memory_order_relaxed) +
               m_bl2_verify_sig_dmi_failures.load(std::memory_order_relaxed) +
               m_bl2_verify_sig_verify_failures.load(std::memory_order_relaxed) +
               m_bl2_verify_sig_state_failures.load(std::memory_order_relaxed) +
               m_bl2_verify_sig_unsupported.load(std::memory_order_relaxed);
    }

    uint64_t bl2_delay_accel_events() const
    {
        return m_bl2_delay_hits.load(std::memory_order_relaxed) +
               m_bl2_delay_state_failures.load(std::memory_order_relaxed) +
               m_bl2_delay_unsupported.load(std::memory_order_relaxed);
    }

    void capture_bl2_profile_sample(Bl2ProfileSite& site, uint32_t pc)
    {
        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));

        site.hits.fetch_add(1, std::memory_order_relaxed);
        site.last.pc.store(pc, std::memory_order_relaxed);
        site.last.r0.store(static_cast<uint32_t>(m_context.get_v7m_state(Field::R0)),
                           std::memory_order_relaxed);
        site.last.r1.store(static_cast<uint32_t>(m_context.get_v7m_state(Field::R1)),
                           std::memory_order_relaxed);
        site.last.r2.store(static_cast<uint32_t>(m_context.get_v7m_state(Field::R2)),
                           std::memory_order_relaxed);
        site.last.r3.store(static_cast<uint32_t>(m_context.get_v7m_state(Field::R3)),
                           std::memory_order_relaxed);
        site.last.sp.store(sp, std::memory_order_relaxed);
        site.last.lr.store(static_cast<uint32_t>(m_context.get_v7m_state(Field::LR)),
                           std::memory_order_relaxed);

        std::atomic<uint32_t>* stack_words[] = {
            &site.last.stack0,
            &site.last.stack1,
            &site.last.stack2,
            &site.last.stack3,
        };
        uint32_t captured = 0;
        for (uint32_t index = 0; index < 4; ++index) {
            uint32_t value = 0;
            if (!hotpath_read_u32(sp + index * sizeof(value), value)) {
                m_bl2_load_profile_stack_fails.fetch_add(1, std::memory_order_relaxed);
                break;
            }
            stack_words[index]->store(value, std::memory_order_relaxed);
            ++captured;
        }
        site.last.stack_words.store(captured, std::memory_order_relaxed);
    }

    bool record_bl2_profile_site(uint32_t pc, uint64_t addr, Bl2ProfileSite& site)
    {
        const uint32_t entry = static_cast<uint32_t>(addr & ~1ull);
        if (entry == 0 || pc != entry) {
            return false;
        }

        capture_bl2_profile_sample(site, pc);
        return true;
    }

    static void store_bl2_ram_load_snapshot(
        Bl2RamLoadSnapshot& snapshot, uint32_t state, uint32_t curr_img,
        uint32_t active_slot, uint32_t hdr_addr, uint32_t hdr_magic,
        uint32_t load_addr, uint32_t hdr_size, uint32_t protect_tlv_size,
        uint32_t img_size, uint32_t flags, uint32_t hash_region_size,
        uint32_t slot_img_dst, uint32_t slot_img_sz, uint32_t unsupported_mask)
    {
        snapshot.last_state.store(state, std::memory_order_relaxed);
        snapshot.last_curr_img.store(curr_img, std::memory_order_relaxed);
        snapshot.last_active_slot.store(active_slot, std::memory_order_relaxed);
        snapshot.last_hdr_addr.store(hdr_addr, std::memory_order_relaxed);
        snapshot.last_hdr_magic.store(hdr_magic, std::memory_order_relaxed);
        snapshot.last_load_addr.store(load_addr, std::memory_order_relaxed);
        snapshot.last_hdr_size.store(hdr_size, std::memory_order_relaxed);
        snapshot.last_protect_tlv_size.store(
            protect_tlv_size, std::memory_order_relaxed);
        snapshot.last_img_size.store(img_size, std::memory_order_relaxed);
        snapshot.last_flags.store(flags, std::memory_order_relaxed);
        snapshot.last_hash_region_size.store(
            hash_region_size, std::memory_order_relaxed);
        snapshot.last_slot_img_dst.store(slot_img_dst, std::memory_order_relaxed);
        snapshot.last_slot_img_sz.store(slot_img_sz, std::memory_order_relaxed);
        snapshot.last_unsupported_mask.store(
            unsupported_mask, std::memory_order_relaxed);
    }

    void record_bl2_ram_load_snapshot(
        uint32_t state, uint32_t curr_img, uint32_t active_slot,
        uint32_t hdr_addr, uint32_t hdr_magic, uint32_t load_addr,
        uint32_t hdr_size, uint32_t protect_tlv_size, uint32_t img_size,
        uint32_t flags, uint32_t hash_region_size, uint32_t slot_img_dst,
        uint32_t slot_img_sz, uint32_t unsupported_mask)
    {
        store_bl2_ram_load_snapshot(
            m_bl2_ram_load_snapshot, state, curr_img, active_slot, hdr_addr,
            hdr_magic, load_addr, hdr_size, protect_tlv_size, img_size, flags,
            hash_region_size, slot_img_dst, slot_img_sz, unsupported_mask);
        if (curr_img < m_bl2_ram_load_snapshot_by_image.size()) {
            store_bl2_ram_load_snapshot(
                m_bl2_ram_load_snapshot_by_image[curr_img], state, curr_img,
                active_slot, hdr_addr, hdr_magic, load_addr, hdr_size,
                protect_tlv_size, img_size, flags, hash_region_size,
                slot_img_dst, slot_img_sz, unsupported_mask);
        }
    }

    void account_bl2_ram_load_snapshot(uint32_t curr_img,
                                       uint32_t unsupported_mask)
    {
        if (unsupported_mask != 0) {
            m_bl2_ram_load_snapshot.unsupported.fetch_add(
                1, std::memory_order_relaxed);
            if (curr_img < m_bl2_ram_load_snapshot_by_image.size()) {
                m_bl2_ram_load_snapshot_by_image[curr_img].unsupported.fetch_add(
                    1, std::memory_order_relaxed);
            }
        } else {
            m_bl2_ram_load_snapshot.hits.fetch_add(1, std::memory_order_relaxed);
            if (curr_img < m_bl2_ram_load_snapshot_by_image.size()) {
                m_bl2_ram_load_snapshot_by_image[curr_img].hits.fetch_add(
                    1, std::memory_order_relaxed);
                m_bl2_load_accel_image_decrypted[curr_img].store(
                    false, std::memory_order_relaxed);
            }
        }
    }

    bool capture_bl2_ram_load_snapshot()
    {
        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t state = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        m_bl2_ram_load_snapshot.last_state.store(state, std::memory_order_relaxed);

        uint32_t unsupported_mask = 0;
        if (state == 0) {
            unsupported_mask |= 1u;
        }

        const uint64_t image_count = p_bl2_boot_image_count.get_value();
        if (image_count == 0 || image_count > 64) {
            unsupported_mask |= 2u;
        }

        uint8_t curr_img_u8 = 0;
        uint32_t active_slot = 0;
        if (unsupported_mask == 0 &&
            !hotpath_read_u8(
                static_cast<uint64_t>(state) +
                    p_bl2_boot_state_curr_img_offset.get_value(),
                curr_img_u8)) {
            m_bl2_ram_load_snapshot.dmi_failures.fetch_add(
                1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        const uint32_t curr_img = curr_img_u8;
        m_bl2_ram_load_snapshot.last_curr_img.store(
            curr_img, std::memory_order_relaxed);
        if (unsupported_mask == 0 && curr_img >= image_count) {
            unsupported_mask |= 4u;
        }

        const uint64_t slot_usage_addr =
            static_cast<uint64_t>(state) +
            p_bl2_boot_state_slot_usage_offset.get_value() +
            static_cast<uint64_t>(curr_img) *
                p_bl2_boot_state_slot_usage_stride.get_value();
        if (unsupported_mask == 0 &&
            !hotpath_read_u32(slot_usage_addr, active_slot)) {
            m_bl2_ram_load_snapshot.dmi_failures.fetch_add(
                1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_ram_load_snapshot.last_active_slot.store(
            active_slot, std::memory_order_relaxed);
        if (unsupported_mask == 0 && active_slot > 1) {
            unsupported_mask |= 8u;
        }

        const uint64_t hdr_addr =
            static_cast<uint64_t>(state) +
            p_bl2_boot_state_imgs_offset.get_value() +
            static_cast<uint64_t>(curr_img) *
                p_bl2_boot_state_image_stride.get_value() +
            static_cast<uint64_t>(active_slot) *
                p_bl2_boot_state_slot_stride.get_value();
        m_bl2_ram_load_snapshot.last_hdr_addr.store(
            static_cast<uint32_t>(hdr_addr), std::memory_order_relaxed);

        std::vector<uint8_t> hdr_bytes;
        if (unsupported_mask == 0 &&
            !hotpath_read_bytes(hdr_addr,
                                qbox::rse_mcuboot_image::IMAGE_HEADER_SIZE,
                                hdr_bytes)) {
            m_bl2_ram_load_snapshot.dmi_failures.fetch_add(
                1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        qbox::rse_mcuboot_image::ImageHeader image_header;
        size_t hash_region_size = 0;
        if (unsupported_mask == 0 &&
            !qbox::rse_mcuboot_image::parse_header(
                hdr_bytes.data(), hdr_bytes.size(), image_header)) {
            unsupported_mask |= 16u;
        }
        if (unsupported_mask == 0 &&
            !qbox::rse_mcuboot_image::hash_region_size(
                image_header, hash_region_size)) {
            unsupported_mask |= 32u;
        }
        if (unsupported_mask == 0 &&
            hash_region_size > std::numeric_limits<uint32_t>::max()) {
            unsupported_mask |= 64u;
        }

        uint32_t slot_img_dst = 0;
        uint32_t slot_img_sz = 0;
        if (unsupported_mask == 0 &&
            (!hotpath_read_u32(
                 slot_usage_addr +
                     p_bl2_boot_slot_usage_img_dst_offset.get_value(),
                 slot_img_dst) ||
             !hotpath_read_u32(
                 slot_usage_addr +
                     p_bl2_boot_slot_usage_img_sz_offset.get_value(),
                 slot_img_sz))) {
            m_bl2_ram_load_snapshot.dmi_failures.fetch_add(
                1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        record_bl2_ram_load_snapshot(
            state, curr_img, active_slot, static_cast<uint32_t>(hdr_addr),
            image_header.magic, image_header.load_addr, image_header.hdr_size,
            image_header.protect_tlv_size, image_header.img_size,
            image_header.flags, static_cast<uint32_t>(hash_region_size),
            slot_img_dst, slot_img_sz, unsupported_mask);
        account_bl2_ram_load_snapshot(curr_img, unsupported_mask);

        if (unsupported_mask != 0) {
            write_hotpath_profile_file();
            return false;
        }

        return true;
    }

    bool try_bl2_load_profile_at_pc(uint32_t pc)
    {
        if (!p_bl2_load_profile.get_value() && !p_bl2_load_accel.get_value()) {
            return false;
        }

        bool matched = false;
        if (p_bl2_load_profile.get_value()) {
            matched |= record_bl2_profile_site(
                pc, p_bl2_boot_go_for_image_id_addr.get_value(),
                m_bl2_boot_go_for_image_id);
        }

        const uint32_t load_to_sram_entry =
            static_cast<uint32_t>(p_bl2_boot_load_image_to_sram_addr.get_value() & ~1ull);
        const bool load_to_sram_matched =
            load_to_sram_entry != 0 && pc == load_to_sram_entry;
        if (p_bl2_load_profile.get_value() && load_to_sram_matched) {
            capture_bl2_profile_sample(m_bl2_boot_load_image_to_sram, pc);
            m_bl2_boot_load_image_to_sram.hits.fetch_add(
                1, std::memory_order_relaxed);
        }
        matched |= load_to_sram_matched;

        if (p_bl2_load_profile.get_value()) {
            matched |= record_bl2_profile_site(
                pc, p_bl2_boot_enc_load_addr.get_value(),
                m_bl2_boot_enc_load);
            matched |= record_bl2_profile_site(
                pc, p_bl2_boot_enc_decrypt_addr.get_value(),
                m_bl2_boot_enc_decrypt);
            matched |= record_bl2_profile_site(
                pc, p_bl2_bootutil_img_validate_addr.get_value(),
                m_bl2_bootutil_img_validate);
            matched |= record_bl2_profile_site(
                pc, p_bl2_bootutil_img_hash_addr.get_value(),
                m_bl2_bootutil_img_hash);
            matched |= record_bl2_profile_site(
                pc, p_bl2_bootutil_verify_sig_addr.get_value(),
                m_bl2_bootutil_verify_sig);
        }

        if (matched) {
            if (load_to_sram_matched) {
                capture_bl2_ram_load_snapshot();
            }
            maybe_write_hotpath_profile_file();
        }
        return matched;
    }

    static bool bl2_boot_enc_valid_key_size(uint32_t key_size)
    {
        return key_size == 16 || key_size == 24 || key_size == 32;
    }

    static uint64_t bl2_boot_enc_cache_key(uint32_t enc_state, uint32_t slot)
    {
        return (static_cast<uint64_t>(enc_state) << 8) | (slot & 0xffu);
    }

    bool try_bl2_boot_enc_capture_key_at_pc(uint32_t pc)
    {
        if (!p_bl2_boot_enc_accel.get_value() && !p_bl2_load_accel.get_value()) {
            return false;
        }

        const uint32_t entry =
            static_cast<uint32_t>(p_bl2_boot_enc_set_key_addr.get_value() & ~1ull);
        if (entry == 0 || pc != entry) {
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t enc_state = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t slot = static_cast<uint32_t>(m_context.get_v7m_state(Field::R1));
        const uint32_t boot_status = static_cast<uint32_t>(m_context.get_v7m_state(Field::R2));
        const uint32_t slots = static_cast<uint32_t>(p_bl2_boot_enc_slots.get_value());
        const uint32_t key_size =
            static_cast<uint32_t>(p_bl2_boot_enc_key_bytes.get_value());
        const uint32_t key_stride =
            static_cast<uint32_t>(p_bl2_boot_enc_key_stride.get_value());
        const uint32_t enckey_offset =
            static_cast<uint32_t>(p_bl2_boot_status_enckey_offset.get_value());

        if (slots == 0 || slot >= slots ||
            !bl2_boot_enc_valid_key_size(key_size) || key_stride < key_size) {
            m_bl2_boot_enc_key_capture_failures.fetch_add(1, std::memory_order_relaxed);
            maybe_write_hotpath_profile_file();
            return false;
        }

        std::vector<uint8_t> key;
        const uint64_t key_addr =
            static_cast<uint64_t>(boot_status) + enckey_offset +
            static_cast<uint64_t>(slot) * key_stride;
        if (!hotpath_read_bytes(key_addr, key_size, key)) {
            m_bl2_boot_enc_key_capture_failures.fetch_add(1, std::memory_order_relaxed);
            maybe_write_hotpath_profile_file();
            return false;
        }

        Bl2EncKeyCacheEntry entry_data;
        entry_data.valid = true;
        entry_data.key_size = key_size;
        std::copy(key.begin(), key.end(), entry_data.key.begin());

        {
            std::lock_guard<std::mutex> lock(m_bl2_boot_enc_lock);
            m_bl2_boot_enc_keys[bl2_boot_enc_cache_key(enc_state, slot)] = entry_data;
        }

        m_bl2_boot_enc_key_captures.fetch_add(1, std::memory_order_relaxed);
        maybe_write_hotpath_profile_file();
        return false;
    }

    bool find_bl2_load_snapshot_for_decrypt(uint32_t off, uint32_t buf,
                                            uint32_t& curr_img,
                                            uint32_t& load_addr,
                                            uint32_t& hdr_size,
                                            uint32_t& img_size,
                                            uint32_t& flags)
    {
        const uint64_t image_count = std::min<uint64_t>(
            p_bl2_boot_image_count.get_value(),
            m_bl2_ram_load_snapshot_by_image.size());
        for (uint64_t image = 0; image < image_count; ++image) {
            const Bl2RamLoadSnapshot& snapshot =
                m_bl2_ram_load_snapshot_by_image[image];
            if (snapshot.hits.load(std::memory_order_relaxed) == 0 ||
                snapshot.last_unsupported_mask.load(std::memory_order_relaxed) != 0) {
                continue;
            }

            const uint32_t candidate_load_addr =
                snapshot.last_load_addr.load(std::memory_order_relaxed);
            const uint32_t candidate_hdr_size =
                snapshot.last_hdr_size.load(std::memory_order_relaxed);
            const uint32_t candidate_img_size =
                snapshot.last_img_size.load(std::memory_order_relaxed);
            const uint32_t candidate_flags =
                snapshot.last_flags.load(std::memory_order_relaxed);
            if (candidate_hdr_size == 0 || candidate_img_size == 0 ||
                off >= candidate_img_size) {
                continue;
            }
            const uint64_t expected_buf =
                static_cast<uint64_t>(candidate_load_addr) + candidate_hdr_size + off;
            if (expected_buf != buf) {
                continue;
            }

            curr_img = static_cast<uint32_t>(image);
            load_addr = candidate_load_addr;
            hdr_size = candidate_hdr_size;
            img_size = candidate_img_size;
            flags = candidate_flags;
            return true;
        }

        return false;
    }

    bool try_bl2_load_decrypt_accel_at_pc(uint32_t pc)
    {
        if (!p_bl2_load_accel.get_value()) {
            return false;
        }

        const uint32_t entry =
            static_cast<uint32_t>(p_bl2_boot_enc_decrypt_addr.get_value() & ~1ull);
        if (entry == 0 || pc != entry) {
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t enc_state = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t slot = static_cast<uint32_t>(m_context.get_v7m_state(Field::R1));
        const uint32_t off = static_cast<uint32_t>(m_context.get_v7m_state(Field::R2));
        const uint32_t size = static_cast<uint32_t>(m_context.get_v7m_state(Field::R3));
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));
        uint32_t blk_off = 0;
        uint32_t buf = 0;

        if (!hotpath_read_u32(sp, blk_off) ||
            !hotpath_read_u32(sp + sizeof(blk_off), buf)) {
            m_bl2_load_accel_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_load_accel_last_slot.store(slot, std::memory_order_relaxed);
        m_bl2_load_accel_last_off.store(off, std::memory_order_relaxed);
        m_bl2_load_accel_last_size.store(size, std::memory_order_relaxed);
        m_bl2_load_accel_last_blk_off.store(blk_off, std::memory_order_relaxed);
        m_bl2_load_accel_last_buf.store(buf, std::memory_order_relaxed);

        uint32_t curr_img = 0;
        uint32_t load_addr = 0;
        uint32_t hdr_size = 0;
        uint32_t img_size = 0;
        uint32_t flags = 0;
        uint32_t unsupported_mask = 0;
        if (!find_bl2_load_snapshot_for_decrypt(
                off, buf, curr_img, load_addr, hdr_size, img_size, flags)) {
            unsupported_mask |= 1u;
        }
        m_bl2_load_accel_last_curr_img.store(curr_img, std::memory_order_relaxed);
        m_bl2_load_accel_last_img_size.store(img_size, std::memory_order_relaxed);
        m_bl2_load_accel_last_flags.store(flags, std::memory_order_relaxed);

        const uint32_t slots = static_cast<uint32_t>(p_bl2_boot_enc_slots.get_value());
        if (slots == 0 || slot >= slots) {
            unsupported_mask |= 2u;
        }
        if (size > p_bl2_boot_enc_max_bytes.get_value()) {
            unsupported_mask |= 4u;
        }
        if (blk_off >= 16) {
            unsupported_mask |= 8u;
        }
        if (buf == 0 && size != 0) {
            unsupported_mask |= 16u;
        }
        if (img_size > p_bl2_load_accel_max_bytes.get_value()) {
            unsupported_mask |= 32u;
        }
        if (flags != (qbox::rse_mcuboot_image::IMAGE_F_RAM_LOAD |
                      qbox::rse_mcuboot_image::IMAGE_F_ENCRYPTED_AES128)) {
            unsupported_mask |= 128u;
        }

        if (unsupported_mask == 0 &&
            curr_img < m_bl2_load_accel_image_decrypted.size() &&
            m_bl2_load_accel_image_decrypted[curr_img].load(
                std::memory_order_relaxed)) {
            const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
            if (!m_context.set_v7m_state(Field::PC, return_pc)) {
                m_bl2_load_accel_state_failures.fetch_add(1, std::memory_order_relaxed);
                write_hotpath_profile_file();
                return false;
            }
            m_bl2_load_accel_skip_hits.fetch_add(1, std::memory_order_relaxed);
            m_bl2_load_accel_last_unsupported_mask.store(
                0, std::memory_order_relaxed);
            m_context.set_vcpu_dirty(true);
            maybe_write_hotpath_profile_file();
            return true;
        }

        if (off != 0 || blk_off != 0) {
            unsupported_mask |= 64u;
        }
        m_bl2_load_accel_last_unsupported_mask.store(
            unsupported_mask, std::memory_order_relaxed);
        if (unsupported_mask != 0) {
            m_bl2_load_accel_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        Bl2EncKeyCacheEntry key;
        {
            std::lock_guard<std::mutex> lock(m_bl2_boot_enc_lock);
            const auto it = m_bl2_boot_enc_keys.find(
                bl2_boot_enc_cache_key(enc_state, slot));
            if (it != m_bl2_boot_enc_keys.end()) {
                key = it->second;
            }
        }
        if (!key.valid || !bl2_boot_enc_valid_key_size(key.key_size)) {
            m_bl2_load_accel_key_misses.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        const uint64_t payload_addr = static_cast<uint64_t>(load_addr) + hdr_size;
        std::vector<uint8_t> input;
        std::vector<uint8_t> output(static_cast<size_t>(img_size));
        if (!hotpath_read_bytes(payload_addr, img_size, input)) {
            m_bl2_load_accel_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        std::array<uint8_t, 16> counter{};
        if (!qbox::cc3xx::core::aes_ctr_xcrypt_buffer(
                key.key.data(), key.key_size, counter.data(), input.data(),
                img_size, 0, output.data())) {
            m_bl2_load_accel_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        if (!hotpath_write_bytes(payload_addr, output.data(), img_size)) {
            m_bl2_load_accel_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }
        m_context.invalidate_guest_range(
            payload_addr, payload_addr + img_size - 1);
        m_bl2_load_accel_image_decrypted[curr_img].store(
            true, std::memory_order_relaxed);

        const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
        if (!m_context.set_v7m_state(Field::PC, return_pc)) {
            m_bl2_load_accel_state_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_load_accel_hits.fetch_add(1, std::memory_order_relaxed);
        m_bl2_load_accel_bytes.fetch_add(img_size, std::memory_order_relaxed);
        m_context.set_vcpu_dirty(true);
        maybe_write_hotpath_profile_file();
        return true;
    }

    bool try_bl2_delay_accel_at_pc(uint32_t pc)
    {
        if (!p_bl2_delay_accel.get_value()) {
            return false;
        }

        const uint32_t norm_pc = pc & ~1u;
        const uint32_t entry =
            static_cast<uint32_t>(p_bl2_delay_cycles_addr.get_value() & ~1ull);
        if (entry == 0 || norm_pc != entry) {
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t cycles = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
        m_bl2_delay_last_cycles.store(cycles, std::memory_order_relaxed);
        m_bl2_delay_last_lr.store(return_pc, std::memory_order_relaxed);

        uint32_t unsupported_mask = 0;
        if (cycles > p_bl2_delay_max_cycles.get_value()) {
            unsupported_mask |= 1u;
        }
        if (return_pc == 0) {
            unsupported_mask |= 2u;
        }
        m_bl2_delay_last_unsupported_mask.store(
            unsupported_mask, std::memory_order_relaxed);
        if (unsupported_mask != 0) {
            m_bl2_delay_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        if (!m_context.set_v7m_state(Field::R0, 0) ||
            !m_context.set_v7m_state(Field::PC, return_pc)) {
            m_bl2_delay_state_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        const uint64_t hits =
            m_bl2_delay_hits.fetch_add(1, std::memory_order_relaxed) + 1;
        m_bl2_delay_cycles.fetch_add(cycles, std::memory_order_relaxed);
        const uint64_t expected_hits = p_bl2_delay_expected_hits.get_value();
        if (expected_hits != 0 && hits >= expected_hits &&
            m_bl2_delay_watch_active.exchange(false, std::memory_order_relaxed)) {
            m_pc_entry_registered = false;
        }
        m_context.set_vcpu_dirty(true);
        maybe_write_hotpath_profile_file();
        return true;
    }

    void try_bl2_delay_accel()
    {
        if (!p_bl2_delay_accel.get_value()) {
            return;
        }

        if (try_bl2_delay_accel_at_pc(static_cast<uint32_t>(m_context.get_pc()))) {
            m_context.set_vcpu_dirty(true);
            m_context.kick_cpu();
        }
    }

    bool try_bl2_boot_enc_decrypt_accel_at_pc(uint32_t pc)
    {
        if (!p_bl2_boot_enc_accel.get_value()) {
            return false;
        }

        const uint32_t entry =
            static_cast<uint32_t>(p_bl2_boot_enc_decrypt_addr.get_value() & ~1ull);
        if (entry == 0 || pc != entry) {
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t enc_state = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t slot = static_cast<uint32_t>(m_context.get_v7m_state(Field::R1));
        const uint32_t off = static_cast<uint32_t>(m_context.get_v7m_state(Field::R2));
        const uint32_t size = static_cast<uint32_t>(m_context.get_v7m_state(Field::R3));
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));
        uint32_t blk_off = 0;
        uint32_t buf = 0;

        if (!hotpath_read_u32(sp, blk_off) ||
            !hotpath_read_u32(sp + sizeof(blk_off), buf)) {
            m_bl2_boot_enc_decrypt_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_boot_enc_last_enc_state.store(enc_state, std::memory_order_relaxed);
        m_bl2_boot_enc_last_slot.store(slot, std::memory_order_relaxed);
        m_bl2_boot_enc_last_off.store(off, std::memory_order_relaxed);
        m_bl2_boot_enc_last_size.store(size, std::memory_order_relaxed);
        m_bl2_boot_enc_last_blk_off.store(blk_off, std::memory_order_relaxed);
        m_bl2_boot_enc_last_buf.store(buf, std::memory_order_relaxed);

        uint32_t unsupported_mask = 0;
        const uint32_t slots = static_cast<uint32_t>(p_bl2_boot_enc_slots.get_value());
        if (slots == 0 || slot >= slots) {
            unsupported_mask |= 1u;
        }
        if (size > p_bl2_boot_enc_max_bytes.get_value()) {
            unsupported_mask |= 2u;
        }
        if (blk_off >= 16) {
            unsupported_mask |= 4u;
        }
        if (buf == 0 && size != 0) {
            unsupported_mask |= 8u;
        }
        m_bl2_boot_enc_last_unsupported_mask.store(unsupported_mask,
                                                   std::memory_order_relaxed);
        if (unsupported_mask != 0) {
            m_bl2_boot_enc_decrypt_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
        if (size == 0) {
            if (!m_context.set_v7m_state(Field::PC, return_pc)) {
                m_bl2_boot_enc_decrypt_state_failures.fetch_add(1, std::memory_order_relaxed);
                write_hotpath_profile_file();
                return false;
            }
            m_bl2_boot_enc_decrypt_hits.fetch_add(1, std::memory_order_relaxed);
            m_context.set_vcpu_dirty(true);
            maybe_write_hotpath_profile_file();
            return true;
        }

        Bl2EncKeyCacheEntry key;
        {
            std::lock_guard<std::mutex> lock(m_bl2_boot_enc_lock);
            const auto it = m_bl2_boot_enc_keys.find(bl2_boot_enc_cache_key(enc_state, slot));
            if (it != m_bl2_boot_enc_keys.end()) {
                key = it->second;
            }
        }
        if (!key.valid || !bl2_boot_enc_valid_key_size(key.key_size)) {
            m_bl2_boot_enc_decrypt_key_misses.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        std::vector<uint8_t> input;
        std::vector<uint8_t> output(static_cast<size_t>(size));
        if (!hotpath_read_bytes(buf, size, input)) {
            m_bl2_boot_enc_decrypt_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        std::array<uint8_t, 16> counter{};
        const uint32_t block = off >> 4;
        counter[12] = static_cast<uint8_t>(block >> 24);
        counter[13] = static_cast<uint8_t>(block >> 16);
        counter[14] = static_cast<uint8_t>(block >> 8);
        counter[15] = static_cast<uint8_t>(block);

        if (!qbox::cc3xx::core::aes_ctr_xcrypt_buffer(
                key.key.data(), key.key_size, counter.data(), input.data(), size,
                blk_off, output.data())) {
            m_bl2_boot_enc_decrypt_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        if (!hotpath_write_bytes(buf, output.data(), size)) {
            m_bl2_boot_enc_decrypt_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_context.invalidate_guest_range(buf, static_cast<uint64_t>(buf) + size - 1);
        if (!m_context.set_v7m_state(Field::PC, return_pc)) {
            m_bl2_boot_enc_decrypt_state_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_boot_enc_decrypt_hits.fetch_add(1, std::memory_order_relaxed);
        m_bl2_boot_enc_decrypt_bytes.fetch_add(size, std::memory_order_relaxed);
        m_context.set_vcpu_dirty(true);
        maybe_write_hotpath_profile_file();
        return true;
    }

    bool try_bl2_img_hash_accel_at_pc(uint32_t pc)
    {
        if (!p_bl2_img_hash_accel.get_value()) {
            return false;
        }

        const uint32_t entry =
            static_cast<uint32_t>(p_bl2_bootutil_img_hash_addr.get_value() & ~1ull);
        if (entry == 0 || pc != entry) {
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t hdr = static_cast<uint32_t>(m_context.get_v7m_state(Field::R1));
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));
        uint32_t hash_result = 0;
        uint32_t seed = 0;
        uint32_t seed_len = 0;
        if (!hotpath_read_u32(sp + sizeof(uint32_t), hash_result) ||
            !hotpath_read_u32(sp + 2 * sizeof(uint32_t), seed) ||
            !hotpath_read_u32(sp + 3 * sizeof(uint32_t), seed_len)) {
            m_bl2_img_hash_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_img_hash_last_hdr.store(hdr, std::memory_order_relaxed);
        m_bl2_img_hash_last_hash_result.store(hash_result, std::memory_order_relaxed);
        m_bl2_img_hash_last_seed.store(seed, std::memory_order_relaxed);
        m_bl2_img_hash_last_seed_len.store(seed_len, std::memory_order_relaxed);

        uint32_t unsupported_mask = 0;
        if (hdr == 0) {
            unsupported_mask |= 1u;
        }
        if (hash_result == 0) {
            unsupported_mask |= 2u;
        }
        if (seed_len > p_bl2_img_hash_max_seed_bytes.get_value()) {
            unsupported_mask |= 4u;
        }
        if (seed == 0 && seed_len != 0) {
            unsupported_mask |= 8u;
        }

        std::vector<uint8_t> hdr_bytes;
        if (unsupported_mask == 0 &&
            !hotpath_read_bytes(hdr, qbox::rse_mcuboot_image::IMAGE_HEADER_SIZE,
                                hdr_bytes)) {
            m_bl2_img_hash_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        qbox::rse_mcuboot_image::ImageHeader image_header;
        size_t hash_size = 0;
        if (unsupported_mask == 0 &&
            !qbox::rse_mcuboot_image::parse_header(
                hdr_bytes.data(), hdr_bytes.size(), image_header)) {
            unsupported_mask |= 16u;
        }
        if (unsupported_mask == 0 &&
            !qbox::rse_mcuboot_image::hash_region_size(image_header, hash_size)) {
            unsupported_mask |= 32u;
        }
        if (unsupported_mask == 0 &&
            hash_size > p_bl2_img_hash_max_bytes.get_value()) {
            unsupported_mask |= 64u;
        }

        m_bl2_img_hash_last_load_addr.store(image_header.load_addr,
                                            std::memory_order_relaxed);
        m_bl2_img_hash_last_hash_size.store(static_cast<uint32_t>(hash_size),
                                            std::memory_order_relaxed);
        m_bl2_img_hash_last_unsupported_mask.store(
            unsupported_mask, std::memory_order_relaxed);
        if (unsupported_mask != 0) {
            m_bl2_img_hash_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        std::vector<uint8_t> image;
        if (!hotpath_read_bytes(image_header.load_addr, hash_size, image)) {
            m_bl2_img_hash_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        std::vector<uint8_t> seed_bytes;
        if (seed_len != 0) {
            if (!hotpath_read_bytes(seed, seed_len, seed_bytes)) {
                m_bl2_img_hash_dmi_failures.fetch_add(1, std::memory_order_relaxed);
                write_hotpath_profile_file();
                return false;
            }
        }

        const auto digest = qbox::rse_mcuboot_image::sha256(
            seed_len == 0 ? nullptr : seed_bytes.data(), seed_bytes.size(),
            image.data(), image.size());
        if (!hotpath_write_bytes(hash_result, digest.data(), digest.size())) {
            m_bl2_img_hash_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
        if (!m_context.set_v7m_state(Field::R0, 0) ||
            !m_context.set_v7m_state(Field::PC, return_pc)) {
            m_bl2_img_hash_state_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_img_hash_hits.fetch_add(1, std::memory_order_relaxed);
        m_bl2_img_hash_bytes.fetch_add(hash_size, std::memory_order_relaxed);
        m_context.set_vcpu_dirty(true);
        maybe_write_hotpath_profile_file();
        return true;
    }

    bool try_bl2_verify_sig_accel_at_pc(uint32_t pc)
    {
        if (!p_bl2_verify_sig_accel.get_value()) {
            return false;
        }

        const uint32_t entry =
            static_cast<uint32_t>(p_bl2_bootutil_verify_sig_addr.get_value() & ~1ull);
        if (entry == 0 || pc != entry) {
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t hash = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t hlen = static_cast<uint32_t>(m_context.get_v7m_state(Field::R1));
        const uint32_t sig = static_cast<uint32_t>(m_context.get_v7m_state(Field::R2));
        const uint32_t slen = static_cast<uint32_t>(m_context.get_v7m_state(Field::R3));
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));
        uint32_t key_id = 0;
        if (!hotpath_read_u32(sp, key_id)) {
            m_bl2_verify_sig_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_verify_sig_last_hash.store(hash, std::memory_order_relaxed);
        m_bl2_verify_sig_last_hlen.store(hlen, std::memory_order_relaxed);
        m_bl2_verify_sig_last_sig.store(sig, std::memory_order_relaxed);
        m_bl2_verify_sig_last_slen.store(slen, std::memory_order_relaxed);
        m_bl2_verify_sig_last_key_id.store(key_id, std::memory_order_relaxed);

        uint32_t unsupported_mask = 0;
        if (hlen != 32) {
            unsupported_mask |= 1u;
        }
        if (slen == 0 || slen > p_bl2_verify_sig_max_sig_bytes.get_value()) {
            unsupported_mask |= 2u;
        }
        if (hash == 0 || sig == 0) {
            unsupported_mask |= 4u;
        }
        if (key_id > 0xffu) {
            unsupported_mask |= 8u;
        }
        const uint32_t keys_addr =
            static_cast<uint32_t>(p_bl2_bootutil_keys_addr.get_value());
        if (keys_addr == 0) {
            unsupported_mask |= 16u;
        }

        const uint32_t key_cnt_addr =
            static_cast<uint32_t>(p_bl2_bootutil_key_cnt_addr.get_value());
        if (key_cnt_addr != 0) {
            uint32_t key_cnt = 0;
            if (!hotpath_read_u32(key_cnt_addr, key_cnt)) {
                m_bl2_verify_sig_dmi_failures.fetch_add(1, std::memory_order_relaxed);
                write_hotpath_profile_file();
                return false;
            }
            if (key_id >= key_cnt) {
                unsupported_mask |= 32u;
            }
        }

        m_bl2_verify_sig_last_unsupported_mask.store(
            unsupported_mask, std::memory_order_relaxed);
        if (unsupported_mask != 0) {
            m_bl2_verify_sig_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        uint32_t key_ptr = 0;
        uint32_t key_len_ptr = 0;
        uint32_t key_len = 0;
        const uint32_t key_entry_addr = keys_addr + key_id * 8u;
        if (!hotpath_read_u32(key_entry_addr, key_ptr) ||
            !hotpath_read_u32(key_entry_addr + sizeof(key_ptr), key_len_ptr) ||
            !hotpath_read_u32(key_len_ptr, key_len)) {
            m_bl2_verify_sig_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_verify_sig_last_key_ptr.store(key_ptr, std::memory_order_relaxed);
        m_bl2_verify_sig_last_key_len.store(key_len, std::memory_order_relaxed);
        if (key_ptr == 0 || key_len == 0 ||
            key_len > p_bl2_verify_sig_max_key_bytes.get_value()) {
            m_bl2_verify_sig_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        std::vector<uint8_t> public_key;
        std::vector<uint8_t> hash_bytes;
        std::vector<uint8_t> signature;
        if (!hotpath_read_bytes(key_ptr, key_len, public_key) ||
            !hotpath_read_bytes(hash, hlen, hash_bytes) ||
            !hotpath_read_bytes(sig, slen, signature)) {
            m_bl2_verify_sig_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        bool verified = false;
        bool cached = false;
        {
            std::lock_guard<std::mutex> lock(m_bl2_verify_sig_cache_lock);
            for (const auto& entry : m_bl2_verify_sig_cache) {
                if (entry.public_key == public_key && entry.hash == hash_bytes &&
                    entry.signature == signature) {
                    verified = entry.verified;
                    cached = true;
                    break;
                }
            }
        }

        if (cached) {
            m_bl2_verify_sig_cache_hits.fetch_add(1, std::memory_order_relaxed);
        } else {
            m_bl2_verify_sig_cache_misses.fetch_add(1, std::memory_order_relaxed);
            verified = qbox::rse_p256_ecdsa::verify(public_key, hash_bytes, signature);

            Bl2VerifySigCacheEntry entry;
            entry.public_key = public_key;
            entry.hash = hash_bytes;
            entry.signature = signature;
            entry.verified = verified;
            {
                std::lock_guard<std::mutex> lock(m_bl2_verify_sig_cache_lock);
                static constexpr size_t MAX_VERIFY_SIG_CACHE_ENTRIES = 16;
                if (m_bl2_verify_sig_cache.size() >= MAX_VERIFY_SIG_CACHE_ENTRIES) {
                    m_bl2_verify_sig_cache.erase(m_bl2_verify_sig_cache.begin());
                }
                m_bl2_verify_sig_cache.push_back(entry);
            }
        }

        if (!verified) {
            m_bl2_verify_sig_verify_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_verify_sig_matches.fetch_add(1, std::memory_order_relaxed);
        if (!p_bl2_verify_sig_skip.get_value()) {
            maybe_write_hotpath_profile_file();
            return false;
        }

        uint32_t fih_success = 0;
        const uint32_t fih_success_addr =
            static_cast<uint32_t>(p_bl2_fih_success_addr.get_value());
        if (fih_success_addr == 0 ||
            !hotpath_read_u32(fih_success_addr, fih_success)) {
            m_bl2_verify_sig_dmi_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }
        m_bl2_verify_sig_last_fih_success.store(
            fih_success, std::memory_order_relaxed);

        const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
        if (!m_context.set_v7m_state(Field::R0, fih_success) ||
            !m_context.set_v7m_state(Field::PC, return_pc)) {
            m_bl2_verify_sig_state_failures.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_bl2_verify_sig_skip_hits.fetch_add(1, std::memory_order_relaxed);
        m_context.set_vcpu_dirty(true);
        maybe_write_hotpath_profile_file();
        return true;
    }

    void write_bl2_profile_site(std::ostream& out, const char* json_name,
                                uint64_t addr, const Bl2ProfileSite& site,
                                bool trailing_comma) const
    {
        const Bl2ProfileSample& last = site.last;
        out << "      \"" << json_name << "\": {\n"
            << "        \"addr\": \"" << profile_hex_string(addr) << "\",\n"
            << "        \"hits\": " << site.hits.load(std::memory_order_relaxed) << ",\n"
            << "        \"last\": {\n"
            << "          \"pc\": \"" << profile_hex_string(last.pc.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"r0\": \"" << profile_hex_string(last.r0.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"r1\": \"" << profile_hex_string(last.r1.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"r2\": \"" << profile_hex_string(last.r2.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"r3\": \"" << profile_hex_string(last.r3.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"sp\": \"" << profile_hex_string(last.sp.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"lr\": \"" << profile_hex_string(last.lr.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"stack_words\": " << last.stack_words.load(std::memory_order_relaxed) << ",\n"
            << "          \"stack0\": \"" << profile_hex_string(last.stack0.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"stack1\": \"" << profile_hex_string(last.stack1.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"stack2\": \"" << profile_hex_string(last.stack2.load(std::memory_order_relaxed)) << "\",\n"
            << "          \"stack3\": \"" << profile_hex_string(last.stack3.load(std::memory_order_relaxed)) << "\"\n"
            << "        }\n"
            << "      }" << (trailing_comma ? "," : "") << "\n";
    }

    static void write_bl2_ram_load_snapshot_values(
        std::ostream& out, const Bl2RamLoadSnapshot& snapshot,
        const char* indent)
    {
        out << indent << "\"state\": \""
            << profile_hex_string(snapshot.last_state.load(std::memory_order_relaxed)) << "\",\n"
            << indent << "\"curr_img\": "
            << snapshot.last_curr_img.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"active_slot\": "
            << snapshot.last_active_slot.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"hdr_addr\": \""
            << profile_hex_string(snapshot.last_hdr_addr.load(std::memory_order_relaxed)) << "\",\n"
            << indent << "\"hdr_magic\": \""
            << profile_hex_string(snapshot.last_hdr_magic.load(std::memory_order_relaxed)) << "\",\n"
            << indent << "\"load_addr\": \""
            << profile_hex_string(snapshot.last_load_addr.load(std::memory_order_relaxed)) << "\",\n"
            << indent << "\"hdr_size\": "
            << snapshot.last_hdr_size.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"protect_tlv_size\": "
            << snapshot.last_protect_tlv_size.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"img_size\": "
            << snapshot.last_img_size.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"flags\": \""
            << profile_hex_string(snapshot.last_flags.load(std::memory_order_relaxed)) << "\",\n"
            << indent << "\"hash_region_size\": "
            << snapshot.last_hash_region_size.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"slot_img_dst\": \""
            << profile_hex_string(snapshot.last_slot_img_dst.load(std::memory_order_relaxed)) << "\",\n"
            << indent << "\"slot_img_sz\": "
            << snapshot.last_slot_img_sz.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"unsupported_mask\": "
            << snapshot.last_unsupported_mask.load(std::memory_order_relaxed) << "\n";
    }

    static void write_bl2_ram_load_snapshot_object(
        std::ostream& out, const Bl2RamLoadSnapshot& snapshot,
        const char* indent, const char* value_indent)
    {
        out << indent << "\"hits\": "
            << snapshot.hits.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"dmi_failures\": "
            << snapshot.dmi_failures.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"unsupported\": "
            << snapshot.unsupported.load(std::memory_order_relaxed) << ",\n"
            << indent << "\"last\": {\n";
        write_bl2_ram_load_snapshot_values(out, snapshot, value_indent);
        out << indent << "}\n";
    }

    void write_hotpath_profile_file()
    {
        const std::string profile_file = p_hotpath_profile_file.get_value();
        if (profile_file.empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_hotpath_profile_lock);
        std::ofstream out(profile_file, std::ios::out | std::ios::trunc);
        if (!out) {
            std::cerr << m_cpu_name << " hotpath_profile_error file=" << profile_file
                      << std::endl;
            return;
        }

        const auto hex_string = [](uint64_t value) {
            return profile_hex_string(value);
        };

        out << "{\n"
            << "  \"name\": \"" << m_cpu_name << "\",\n"
            << "  \"enabled\": " << (p_hotpath_accel.get_value() ? "true" : "false") << ",\n"
            << "  \"memcpy_addr\": \"" << hex_string(p_hotpath_memcpy_addr.get_value()) << "\",\n"
            << "  \"memset_addr\": \"" << hex_string(p_hotpath_memset_addr.get_value()) << "\",\n"
            << "  \"max_bytes\": " << p_hotpath_max_bytes.get_value() << ",\n"
            << "  \"memcpy_hits\": " << m_hotpath_memcpy_hits.load(std::memory_order_relaxed) << ",\n"
            << "  \"memset_hits\": " << m_hotpath_memset_hits.load(std::memory_order_relaxed) << ",\n"
            << "  \"fallbacks\": " << m_hotpath_fallbacks.load(std::memory_order_relaxed) << ",\n"
            << "  \"pc_misses\": " << m_hotpath_pc_misses.load(std::memory_order_relaxed) << ",\n"
            << "  \"too_large_failures\": " << m_hotpath_too_large_fails.load(std::memory_order_relaxed) << ",\n"
            << "  \"dmi_failures\": " << m_hotpath_dmi_fails.load(std::memory_order_relaxed) << ",\n"
            << "  \"stack_failures\": " << m_hotpath_stack_fails.load(std::memory_order_relaxed) << ",\n"
            << "  \"state_failures\": " << m_hotpath_state_fails.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_accel_enabled\": " << (p_lms_accel.get_value() ? "true" : "false") << ",\n"
            << "  \"lms_verify_addr\": \"" << hex_string(p_lms_verify_addr.get_value()) << "\",\n"
            << "  \"lms_hits\": " << m_lms_accel_hits.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_pc_misses\": " << m_lms_accel_pc_misses.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_dmi_failures\": " << m_lms_accel_dmi_fails.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_verify_failures\": " << m_lms_accel_verify_fails.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_state_failures\": " << m_lms_accel_state_fails.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_unsupported\": " << m_lms_accel_unsupported.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_last_key_addr\": \"" << hex_string(m_lms_last_key_addr.load(std::memory_order_relaxed)) << "\",\n"
            << "  \"lms_last_key_size\": " << m_lms_last_key_size.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_last_data_addr\": \"" << hex_string(m_lms_last_data_addr.load(std::memory_order_relaxed)) << "\",\n"
            << "  \"lms_last_data_size\": " << m_lms_last_data_size.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_last_sig_addr\": \"" << hex_string(m_lms_last_sig_addr.load(std::memory_order_relaxed)) << "\",\n"
            << "  \"lms_last_sig_size\": " << m_lms_last_sig_size.load(std::memory_order_relaxed) << ",\n"
            << "  \"lms_last_unsupported_mask\": " << m_lms_last_unsupported_mask.load(std::memory_order_relaxed) << ",\n"
            << "  \"bl2_delay_accel\": {\n"
            << "    \"enabled\": " << (p_bl2_delay_accel.get_value() ? "true" : "false") << ",\n"
            << "    \"delay_cycles_addr\": \"" << hex_string(p_bl2_delay_cycles_addr.get_value()) << "\",\n"
            << "    \"max_cycles\": " << p_bl2_delay_max_cycles.get_value() << ",\n"
            << "    \"hits\": " << m_bl2_delay_hits.load(std::memory_order_relaxed) << ",\n"
            << "    \"cycles\": " << m_bl2_delay_cycles.load(std::memory_order_relaxed) << ",\n"
            << "    \"expected_hits\": " << p_bl2_delay_expected_hits.get_value() << ",\n"
            << "    \"state_failures\": " << m_bl2_delay_state_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"unsupported\": " << m_bl2_delay_unsupported.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_cycles\": " << m_bl2_delay_last_cycles.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_lr\": \"" << hex_string(m_bl2_delay_last_lr.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_unsupported_mask\": " << m_bl2_delay_last_unsupported_mask.load(std::memory_order_relaxed) << "\n"
            << "  },\n"
            << "  \"bl2_load_accel\": {\n"
            << "    \"enabled\": " << (p_bl2_load_accel.get_value() ? "true" : "false") << ",\n"
            << "    \"decrypt_addr\": \"" << hex_string(p_bl2_boot_enc_decrypt_addr.get_value()) << "\",\n"
            << "    \"max_bytes\": " << p_bl2_load_accel_max_bytes.get_value() << ",\n"
            << "    \"hits\": " << m_bl2_load_accel_hits.load(std::memory_order_relaxed) << ",\n"
            << "    \"skip_hits\": " << m_bl2_load_accel_skip_hits.load(std::memory_order_relaxed) << ",\n"
            << "    \"bytes\": " << m_bl2_load_accel_bytes.load(std::memory_order_relaxed) << ",\n"
            << "    \"key_misses\": " << m_bl2_load_accel_key_misses.load(std::memory_order_relaxed) << ",\n"
            << "    \"dmi_failures\": " << m_bl2_load_accel_dmi_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"state_failures\": " << m_bl2_load_accel_state_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"unsupported\": " << m_bl2_load_accel_unsupported.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_curr_img\": " << m_bl2_load_accel_last_curr_img.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_slot\": " << m_bl2_load_accel_last_slot.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_off\": \"" << hex_string(m_bl2_load_accel_last_off.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_size\": " << m_bl2_load_accel_last_size.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_blk_off\": " << m_bl2_load_accel_last_blk_off.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_buf\": \"" << hex_string(m_bl2_load_accel_last_buf.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_img_size\": " << m_bl2_load_accel_last_img_size.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_flags\": \"" << hex_string(m_bl2_load_accel_last_flags.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_unsupported_mask\": " << m_bl2_load_accel_last_unsupported_mask.load(std::memory_order_relaxed) << "\n"
            << "  },\n"
            << "  \"bl2_boot_enc_accel\": {\n"
            << "    \"enabled\": " << (p_bl2_boot_enc_accel.get_value() ? "true" : "false") << ",\n"
            << "    \"set_key_addr\": \"" << hex_string(p_bl2_boot_enc_set_key_addr.get_value()) << "\",\n"
            << "    \"decrypt_addr\": \"" << hex_string(p_bl2_boot_enc_decrypt_addr.get_value()) << "\",\n"
            << "    \"boot_status_enckey_offset\": \"" << hex_string(p_bl2_boot_status_enckey_offset.get_value()) << "\",\n"
            << "    \"key_bytes\": " << p_bl2_boot_enc_key_bytes.get_value() << ",\n"
            << "    \"key_stride\": " << p_bl2_boot_enc_key_stride.get_value() << ",\n"
            << "    \"slots\": " << p_bl2_boot_enc_slots.get_value() << ",\n"
            << "    \"max_bytes\": " << p_bl2_boot_enc_max_bytes.get_value() << ",\n"
            << "    \"key_captures\": " << m_bl2_boot_enc_key_captures.load(std::memory_order_relaxed) << ",\n"
            << "    \"key_capture_failures\": " << m_bl2_boot_enc_key_capture_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"decrypt_hits\": " << m_bl2_boot_enc_decrypt_hits.load(std::memory_order_relaxed) << ",\n"
            << "    \"decrypt_bytes\": " << m_bl2_boot_enc_decrypt_bytes.load(std::memory_order_relaxed) << ",\n"
            << "    \"decrypt_key_misses\": " << m_bl2_boot_enc_decrypt_key_misses.load(std::memory_order_relaxed) << ",\n"
            << "    \"decrypt_dmi_failures\": " << m_bl2_boot_enc_decrypt_dmi_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"decrypt_state_failures\": " << m_bl2_boot_enc_decrypt_state_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"decrypt_unsupported\": " << m_bl2_boot_enc_decrypt_unsupported.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_enc_state\": \"" << hex_string(m_bl2_boot_enc_last_enc_state.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_slot\": " << m_bl2_boot_enc_last_slot.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_off\": \"" << hex_string(m_bl2_boot_enc_last_off.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_size\": " << m_bl2_boot_enc_last_size.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_blk_off\": " << m_bl2_boot_enc_last_blk_off.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_buf\": \"" << hex_string(m_bl2_boot_enc_last_buf.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_unsupported_mask\": " << m_bl2_boot_enc_last_unsupported_mask.load(std::memory_order_relaxed) << "\n"
            << "  },\n"
            << "  \"bl2_img_hash_accel\": {\n"
            << "    \"enabled\": " << (p_bl2_img_hash_accel.get_value() ? "true" : "false") << ",\n"
            << "    \"img_hash_addr\": \"" << hex_string(p_bl2_bootutil_img_hash_addr.get_value()) << "\",\n"
            << "    \"max_bytes\": " << p_bl2_img_hash_max_bytes.get_value() << ",\n"
            << "    \"max_seed_bytes\": " << p_bl2_img_hash_max_seed_bytes.get_value() << ",\n"
            << "    \"hits\": " << m_bl2_img_hash_hits.load(std::memory_order_relaxed) << ",\n"
            << "    \"bytes\": " << m_bl2_img_hash_bytes.load(std::memory_order_relaxed) << ",\n"
            << "    \"dmi_failures\": " << m_bl2_img_hash_dmi_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"state_failures\": " << m_bl2_img_hash_state_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"unsupported\": " << m_bl2_img_hash_unsupported.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_hdr\": \"" << hex_string(m_bl2_img_hash_last_hdr.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_load_addr\": \"" << hex_string(m_bl2_img_hash_last_load_addr.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_hash_result\": \"" << hex_string(m_bl2_img_hash_last_hash_result.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_hash_size\": " << m_bl2_img_hash_last_hash_size.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_seed\": \"" << hex_string(m_bl2_img_hash_last_seed.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_seed_len\": " << m_bl2_img_hash_last_seed_len.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_unsupported_mask\": " << m_bl2_img_hash_last_unsupported_mask.load(std::memory_order_relaxed) << "\n"
            << "  },\n"
            << "  \"bl2_verify_sig_accel\": {\n"
            << "    \"enabled\": " << (p_bl2_verify_sig_accel.get_value() ? "true" : "false") << ",\n"
            << "    \"verify_sig_addr\": \"" << hex_string(p_bl2_bootutil_verify_sig_addr.get_value()) << "\",\n"
            << "    \"bootutil_keys_addr\": \"" << hex_string(p_bl2_bootutil_keys_addr.get_value()) << "\",\n"
            << "    \"bootutil_key_cnt_addr\": \"" << hex_string(p_bl2_bootutil_key_cnt_addr.get_value()) << "\",\n"
            << "    \"max_key_bytes\": " << p_bl2_verify_sig_max_key_bytes.get_value() << ",\n"
            << "    \"max_sig_bytes\": " << p_bl2_verify_sig_max_sig_bytes.get_value() << ",\n"
            << "    \"skip_enabled\": " << (p_bl2_verify_sig_skip.get_value() ? "true" : "false") << ",\n"
            << "    \"verify_matches\": " << m_bl2_verify_sig_matches.load(std::memory_order_relaxed) << ",\n"
            << "    \"cache_hits\": " << m_bl2_verify_sig_cache_hits.load(std::memory_order_relaxed) << ",\n"
            << "    \"cache_misses\": " << m_bl2_verify_sig_cache_misses.load(std::memory_order_relaxed) << ",\n"
            << "    \"skip_hits\": " << m_bl2_verify_sig_skip_hits.load(std::memory_order_relaxed) << ",\n"
            << "    \"dmi_failures\": " << m_bl2_verify_sig_dmi_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"verify_failures\": " << m_bl2_verify_sig_verify_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"state_failures\": " << m_bl2_verify_sig_state_failures.load(std::memory_order_relaxed) << ",\n"
            << "    \"unsupported\": " << m_bl2_verify_sig_unsupported.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_hash\": \"" << hex_string(m_bl2_verify_sig_last_hash.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_hlen\": " << m_bl2_verify_sig_last_hlen.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_sig\": \"" << hex_string(m_bl2_verify_sig_last_sig.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_slen\": " << m_bl2_verify_sig_last_slen.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_key_id\": " << m_bl2_verify_sig_last_key_id.load(std::memory_order_relaxed) << ",\n"
            << "    \"last_key_ptr\": \"" << hex_string(m_bl2_verify_sig_last_key_ptr.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_key_len\": " << m_bl2_verify_sig_last_key_len.load(std::memory_order_relaxed) << ",\n"
            << "    \"fih_success_addr\": \"" << hex_string(p_bl2_fih_success_addr.get_value()) << "\",\n"
            << "    \"last_fih_success\": \"" << hex_string(m_bl2_verify_sig_last_fih_success.load(std::memory_order_relaxed)) << "\",\n"
            << "    \"last_unsupported_mask\": " << m_bl2_verify_sig_last_unsupported_mask.load(std::memory_order_relaxed) << "\n"
            << "  },\n"
            << "  \"bl2_load_profile\": {\n"
            << "    \"enabled\": " << (p_bl2_load_profile.get_value() ? "true" : "false") << ",\n"
            << "    \"pc_misses\": " << m_bl2_load_profile_pc_misses.load(std::memory_order_relaxed) << ",\n"
            << "    \"stack_failures\": " << m_bl2_load_profile_stack_fails.load(std::memory_order_relaxed) << ",\n"
            << "    \"ram_load_snapshot\": {\n"
            << "      \"hits\": " << m_bl2_ram_load_snapshot.hits.load(std::memory_order_relaxed) << ",\n"
            << "      \"dmi_failures\": " << m_bl2_ram_load_snapshot.dmi_failures.load(std::memory_order_relaxed) << ",\n"
            << "      \"unsupported\": " << m_bl2_ram_load_snapshot.unsupported.load(std::memory_order_relaxed) << ",\n"
            << "      \"layout\": {\n"
            << "        \"image_count\": " << p_bl2_boot_image_count.get_value() << ",\n"
            << "        \"curr_img_offset\": \"" << hex_string(p_bl2_boot_state_curr_img_offset.get_value()) << "\",\n"
            << "        \"imgs_offset\": \"" << hex_string(p_bl2_boot_state_imgs_offset.get_value()) << "\",\n"
            << "        \"image_stride\": " << p_bl2_boot_state_image_stride.get_value() << ",\n"
            << "        \"slot_stride\": " << p_bl2_boot_state_slot_stride.get_value() << ",\n"
            << "        \"slot_usage_offset\": \"" << hex_string(p_bl2_boot_state_slot_usage_offset.get_value()) << "\",\n"
            << "        \"slot_usage_stride\": " << p_bl2_boot_state_slot_usage_stride.get_value() << ",\n"
            << "        \"slot_img_dst_offset\": \"" << hex_string(p_bl2_boot_slot_usage_img_dst_offset.get_value()) << "\",\n"
            << "        \"slot_img_sz_offset\": \"" << hex_string(p_bl2_boot_slot_usage_img_sz_offset.get_value()) << "\"\n"
            << "      },\n"
            << "      \"last\": {\n";
        write_bl2_ram_load_snapshot_values(out, m_bl2_ram_load_snapshot, "        ");
        out << "      },\n"
            << "      \"by_image\": {\n";
        const uint64_t snapshot_image_count = std::min<uint64_t>(
            p_bl2_boot_image_count.get_value(),
            m_bl2_ram_load_snapshot_by_image.size());
        bool first_snapshot = true;
        for (uint64_t image = 0; image < snapshot_image_count; ++image) {
            const Bl2RamLoadSnapshot& snapshot =
                m_bl2_ram_load_snapshot_by_image[image];
            const uint64_t events =
                snapshot.hits.load(std::memory_order_relaxed) +
                snapshot.dmi_failures.load(std::memory_order_relaxed) +
                snapshot.unsupported.load(std::memory_order_relaxed);
            if (events == 0) {
                continue;
            }
            if (!first_snapshot) {
                out << ",\n";
            }
            first_snapshot = false;
            out << "        \"" << image << "\": {\n";
            write_bl2_ram_load_snapshot_object(
                out, snapshot, "          ", "            ");
            out << "        }";
        }
        out << "\n"
            << "      }\n"
            << "    },\n"
            << "    \"sites\": {\n";
        write_bl2_profile_site(
            out, "boot_go_for_image_id", p_bl2_boot_go_for_image_id_addr.get_value(),
            m_bl2_boot_go_for_image_id, true);
        write_bl2_profile_site(
            out, "boot_load_image_to_sram", p_bl2_boot_load_image_to_sram_addr.get_value(),
            m_bl2_boot_load_image_to_sram, true);
        write_bl2_profile_site(
            out, "boot_enc_load", p_bl2_boot_enc_load_addr.get_value(),
            m_bl2_boot_enc_load, true);
        write_bl2_profile_site(
            out, "boot_enc_decrypt", p_bl2_boot_enc_decrypt_addr.get_value(),
            m_bl2_boot_enc_decrypt, true);
        write_bl2_profile_site(
            out, "bootutil_img_validate", p_bl2_bootutil_img_validate_addr.get_value(),
            m_bl2_bootutil_img_validate, true);
        write_bl2_profile_site(
            out, "bootutil_img_hash", p_bl2_bootutil_img_hash_addr.get_value(),
            m_bl2_bootutil_img_hash, true);
        write_bl2_profile_site(
            out, "bootutil_verify_sig", p_bl2_bootutil_verify_sig_addr.get_value(),
            m_bl2_bootutil_verify_sig, false);
        out << "    }\n"
            << "  }\n"
            << "}\n";
    }

    void maybe_write_hotpath_profile_file()
    {
        const uint64_t interval = p_hotpath_profile_interval.get_value();
        if (interval == 0) {
            return;
        }

        const uint64_t events =
            m_hotpath_memcpy_hits.load(std::memory_order_relaxed) +
            m_hotpath_memset_hits.load(std::memory_order_relaxed) +
            m_hotpath_fallbacks.load(std::memory_order_relaxed) +
            m_lms_accel_hits.load(std::memory_order_relaxed) +
            m_lms_accel_pc_misses.load(std::memory_order_relaxed) +
            m_lms_accel_dmi_fails.load(std::memory_order_relaxed) +
            m_lms_accel_verify_fails.load(std::memory_order_relaxed) +
            m_lms_accel_state_fails.load(std::memory_order_relaxed) +
            m_lms_accel_unsupported.load(std::memory_order_relaxed) +
            bl2_load_profile_events() +
            bl2_load_accel_events() +
            bl2_boot_enc_accel_events() +
            bl2_img_hash_accel_events() +
            bl2_verify_sig_accel_events() +
            bl2_delay_accel_events();
        if (events != 0 && (events % interval) == 0) {
            write_hotpath_profile_file();
        }
    }

    void record_hotpath_failure(HotpathFailReason reason)
    {
        m_hotpath_fallbacks.fetch_add(1, std::memory_order_relaxed);
        switch (reason) {
        case HotpathFailReason::PcMismatch:
            m_hotpath_pc_misses.fetch_add(1, std::memory_order_relaxed);
            break;
        case HotpathFailReason::TooLarge:
            m_hotpath_too_large_fails.fetch_add(1, std::memory_order_relaxed);
            break;
        case HotpathFailReason::Dmi:
            m_hotpath_dmi_fails.fetch_add(1, std::memory_order_relaxed);
            break;
        case HotpathFailReason::Stack:
            m_hotpath_stack_fails.fetch_add(1, std::memory_order_relaxed);
            break;
        case HotpathFailReason::State:
            m_hotpath_state_fails.fetch_add(1, std::memory_order_relaxed);
            break;
        }
        maybe_write_hotpath_profile_file();
    }

    bool hotpath_finish_stacked_lr(uint32_t& return_pc,
                                   HotpathFailReason& reason)
    {
        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));
        if (!hotpath_read_u32(sp, return_pc)) {
            reason = HotpathFailReason::Stack;
            return false;
        }
        if (!m_context.set_v7m_state(Field::SP, sp + sizeof(return_pc)) ||
            !m_context.set_v7m_state(Field::PC, return_pc)) {
            reason = HotpathFailReason::State;
            return false;
        }
        return true;
    }

    bool hotpath_finish_memset(uint32_t return_pc,
                               HotpathFailReason& reason)
    {
        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));
        uint32_t saved_r4 = 0;
        if (!hotpath_read_u32(sp, saved_r4)) {
            reason = HotpathFailReason::Stack;
            return false;
        }
        if (!m_context.set_v7m_state(Field::R4, saved_r4) ||
            !m_context.set_v7m_state(Field::SP, sp + sizeof(saved_r4)) ||
            !m_context.set_v7m_state(Field::PC, return_pc)) {
            reason = HotpathFailReason::State;
            return false;
        }
        return true;
    }

    bool hotpath_memcpy(uint32_t pc, uint32_t base, uint64_t max_bytes,
                        HotpathFailReason& reason)
    {
        if (!hotpath_memcpy_safe_pc(pc, base)) {
            reason = HotpathFailReason::PcMismatch;
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t dest = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t src = pc == base
                                 ? static_cast<uint32_t>(m_context.get_v7m_state(Field::R1))
                                 : static_cast<uint32_t>(m_context.get_v7m_state(Field::R1));
        const uint32_t remaining = pc == base
                                       ? static_cast<uint32_t>(m_context.get_v7m_state(Field::R2))
                                       : static_cast<uint32_t>(m_context.get_v7m_state(Field::R2));
        const uint32_t current_dest = pc == base
                                          ? dest
                                          : static_cast<uint32_t>(m_context.get_v7m_state(Field::R3));
        if (remaining == 0) {
            uint32_t return_pc = 0;
            if (pc == base) {
                return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
                if (!m_context.set_v7m_state(Field::PC, return_pc)) {
                    reason = HotpathFailReason::State;
                    return false;
                }
                return true;
            }
            return hotpath_finish_stacked_lr(return_pc, reason);
        }
        if (remaining > max_bytes) {
            reason = HotpathFailReason::TooLarge;
            return false;
        }

        uint8_t* src_ptr = nullptr;
        uint8_t* dst_ptr = nullptr;
        if (!hotpath_dmi_ptr(src, remaining, true, false, src_ptr) ||
            !hotpath_dmi_ptr(current_dest, remaining, false, true, dst_ptr)) {
            reason = HotpathFailReason::Dmi;
            return false;
        }

        std::memmove(dst_ptr, src_ptr, remaining);
        if (remaining != 0) {
            m_context.invalidate_guest_range(current_dest, current_dest + remaining - 1);
        }

        if (pc == base) {
            const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
            if (!m_context.set_v7m_state(Field::PC, return_pc)) {
                reason = HotpathFailReason::State;
                return false;
            }
        } else {
            uint32_t return_pc = 0;
            if (!hotpath_finish_stacked_lr(return_pc, reason)) {
                return false;
            }
        }
        m_hotpath_memcpy_hits.fetch_add(1, std::memory_order_relaxed);
        maybe_write_hotpath_profile_file();
        return true;
    }

    bool hotpath_memset(uint32_t pc, uint32_t base, uint64_t max_bytes,
                        HotpathFailReason& reason)
    {
        if (!hotpath_memset_safe_pc(pc, base)) {
            reason = HotpathFailReason::PcMismatch;
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t dest = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t current_dest = pc == base
                                          ? dest
                                          : static_cast<uint32_t>(m_context.get_v7m_state(Field::R3));
        const uint32_t remaining = static_cast<uint32_t>(m_context.get_v7m_state(Field::R2));
        const uint8_t value = static_cast<uint8_t>(m_context.get_v7m_state(Field::R1));
        if (remaining > max_bytes) {
            reason = HotpathFailReason::TooLarge;
            return false;
        }

        uint8_t* dst_ptr = nullptr;
        if (remaining != 0 &&
            !hotpath_dmi_ptr(current_dest, remaining, false, true, dst_ptr)) {
            reason = HotpathFailReason::Dmi;
            return false;
        }
        if (remaining != 0) {
            std::memset(dst_ptr, value, remaining);
            m_context.invalidate_guest_range(current_dest, current_dest + remaining - 1);
        }

        const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
        if (pc == base) {
            if (!m_context.set_v7m_state(Field::PC, return_pc)) {
                reason = HotpathFailReason::State;
                return false;
            }
        } else if (!hotpath_finish_memset(return_pc, reason)) {
            return false;
        }
        m_hotpath_memset_hits.fetch_add(1, std::memory_order_relaxed);
        maybe_write_hotpath_profile_file();
        return true;
    }

    void try_hotpath_accel()
    {
        if (!p_hotpath_accel.get_value()) {
            return;
        }

        const uint64_t max_bytes = p_hotpath_max_bytes.get_value();
        if (max_bytes == 0) {
            return;
        }

        const uint32_t pc = static_cast<uint32_t>(m_context.get_pc());
        const uint32_t memcpy_base = static_cast<uint32_t>(p_hotpath_memcpy_addr.get_value() & ~1ull);
        const uint32_t memset_base = static_cast<uint32_t>(p_hotpath_memset_addr.get_value() & ~1ull);
        bool handled = false;
        HotpathFailReason fail_reason = HotpathFailReason::PcMismatch;

        if (memcpy_base != 0 && hotpath_memcpy_safe_pc(pc, memcpy_base)) {
            handled = hotpath_memcpy(pc, memcpy_base, max_bytes, fail_reason);
        } else if (memset_base != 0 && hotpath_memset_safe_pc(pc, memset_base)) {
            handled = hotpath_memset(pc, memset_base, max_bytes, fail_reason);
        }

        if (!handled) {
            record_hotpath_failure(fail_reason);
        } else {
            m_context.set_vcpu_dirty(true);
            m_context.kick_cpu();
        }
    }

    bool try_lms_accel_at_pc(uint32_t pc)
    {
        if (!p_lms_accel.get_value() ||
            m_lms_accel_done.load(std::memory_order_relaxed)) {
            return false;
        }

        using Field = qemu::CpuArm::V7MStateField;
        const uint32_t verify_addr =
            static_cast<uint32_t>(p_lms_verify_addr.get_value() & ~1ull);
        if (verify_addr == 0 || pc != verify_addr) {
            m_lms_accel_pc_misses.fetch_add(1, std::memory_order_relaxed);
            maybe_write_hotpath_profile_file();
            return false;
        }

        const uint32_t key_addr = static_cast<uint32_t>(m_context.get_v7m_state(Field::R0));
        const uint32_t key_size = static_cast<uint32_t>(m_context.get_v7m_state(Field::R1));
        const uint32_t data_addr = static_cast<uint32_t>(m_context.get_v7m_state(Field::R2));
        const uint32_t data_size = static_cast<uint32_t>(m_context.get_v7m_state(Field::R3));
        const uint32_t sp = static_cast<uint32_t>(m_context.get_v7m_state(Field::SP));
        uint32_t sig_addr = 0;
        uint32_t sig_size = 0;
        if (!hotpath_read_u32(sp, sig_addr) ||
            !hotpath_read_u32(sp + sizeof(sig_addr), sig_size)) {
            m_lms_accel_dmi_fails.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }
        m_lms_last_key_addr.store(key_addr, std::memory_order_relaxed);
        m_lms_last_key_size.store(key_size, std::memory_order_relaxed);
        m_lms_last_data_addr.store(data_addr, std::memory_order_relaxed);
        m_lms_last_data_size.store(data_size, std::memory_order_relaxed);
        m_lms_last_sig_addr.store(sig_addr, std::memory_order_relaxed);
        m_lms_last_sig_size.store(sig_size, std::memory_order_relaxed);

        uint32_t unsupported_mask = 0;
        if (key_size != qbox::rse_lms_accel::LMS_PUBLIC_KEY_LEN) {
            unsupported_mask |= 1u;
        }
        if (sig_size != qbox::rse_lms_accel::LMS_SIG_LEN) {
            unsupported_mask |= 2u;
        }
        if (data_size > p_lms_max_data_bytes.get_value()) {
            unsupported_mask |= 4u;
        }
        m_lms_last_unsupported_mask.store(unsupported_mask, std::memory_order_relaxed);
        if (unsupported_mask != 0) {
            m_lms_accel_unsupported.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        std::vector<uint8_t> key;
        std::vector<uint8_t> data;
        std::vector<uint8_t> sig;
        if (!hotpath_read_bytes(key_addr, key_size, key) ||
            !hotpath_read_bytes(data_addr, data_size, data) ||
            !hotpath_read_bytes(sig_addr, sig_size, sig)) {
            m_lms_accel_dmi_fails.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        if (!qbox::rse_lms_accel::verify(key, data, sig)) {
            m_lms_accel_verify_fails.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        static constexpr uint32_t FIH_SUCCESS_VALUE = 0x1aaa555au;
        const uint32_t return_pc = static_cast<uint32_t>(m_context.get_v7m_state(Field::LR));
        if (!m_context.set_v7m_state(Field::R0, FIH_SUCCESS_VALUE) ||
            !m_context.set_v7m_state(Field::PC, return_pc)) {
            m_lms_accel_state_fails.fetch_add(1, std::memory_order_relaxed);
            write_hotpath_profile_file();
            return false;
        }

        m_lms_accel_hits.fetch_add(1, std::memory_order_relaxed);
        m_lms_accel_done.store(true, std::memory_order_relaxed);
        if (m_pc_entry_registered && !p_bl2_load_profile.get_value() &&
            !p_bl2_load_accel.get_value() &&
            !p_bl2_boot_enc_accel.get_value() &&
            !p_bl2_img_hash_accel.get_value() &&
            !p_bl2_verify_sig_accel.get_value() &&
            !p_bl2_delay_accel.get_value()) {
            m_pc_entry_registered = false;
        }
        write_hotpath_profile_file();
        return true;
    }

    bool try_rse_pc_entry(uintptr_t pc)
    {
        const uint32_t pc32 = static_cast<uint32_t>(pc);
        try_bl2_load_profile_at_pc(pc32);
        try_bl2_boot_enc_capture_key_at_pc(pc32);
        if (try_bl2_delay_accel_at_pc(pc32)) {
            return true;
        }
        if (try_bl2_load_decrypt_accel_at_pc(pc32)) {
            return true;
        }
        if (try_bl2_boot_enc_decrypt_accel_at_pc(pc32)) {
            return true;
        }
        if (try_bl2_img_hash_accel_at_pc(pc32)) {
            return true;
        }
        if (try_bl2_verify_sig_accel_at_pc(pc32)) {
            return true;
        }
        return try_lms_accel_at_pc(pc32);
    }

    void try_lms_accel()
    {
        if (try_lms_accel_at_pc(static_cast<uint32_t>(m_context.get_pc()))) {
            m_context.set_vcpu_dirty(true);
            m_context.kick_cpu();
        }
    }

public:
    RseCpuAccel(QemuCpuSemanticContext& context, std::string parameter_prefix,
                std::string cpu_name = {})
        : m_context(context)
        , m_broker(cci::cci_get_broker())
        , m_parameter_prefix(std::move(parameter_prefix))
        , m_cpu_name(std::move(cpu_name))
    {
        if (m_cpu_name.empty()) {
            m_cpu_name = m_parameter_prefix;
            if (!m_cpu_name.empty() && m_cpu_name.back() == '.') {
                m_cpu_name.pop_back();
            }
        }
        load_config();
        reset_runtime_state();
    }

    void reset_runtime_state()
    {
        m_lms_accel_done.store(false, std::memory_order_relaxed);
        for (auto& decrypted : m_bl2_load_accel_image_decrypted) {
            decrypted.store(false, std::memory_order_relaxed);
        }
        std::lock_guard<std::mutex> lock(m_bl2_boot_enc_lock);
        m_bl2_boot_enc_keys.clear();
    }

    bool enabled() const override
    {
        return p_hotpath_accel.get_value() || p_lms_accel.get_value() ||
               p_bl2_load_profile.get_value() || p_bl2_load_accel.get_value() ||
               p_bl2_boot_enc_accel.get_value() || p_bl2_img_hash_accel.get_value() ||
               p_bl2_verify_sig_accel.get_value() || p_bl2_delay_accel.get_value();
    }

    bool needs_pc_entry_callback() const override
    {
        return p_lms_accel.get_value() || p_bl2_load_profile.get_value() ||
               p_bl2_load_accel.get_value() || p_bl2_boot_enc_accel.get_value() ||
               p_bl2_img_hash_accel.get_value() || p_bl2_verify_sig_accel.get_value() ||
               p_bl2_delay_accel.get_value();
    }

    void configure_pc_watches(qemu::Cpu& cpu) override
    {
        if (!needs_pc_entry_callback()) {
            return;
        }

        add_pc_entry_watch(cpu, p_lms_accel.get_value(),
                           p_lms_verify_addr.get_value());

        add_pc_entry_watch(cpu, p_bl2_load_profile.get_value(),
                           p_bl2_boot_go_for_image_id_addr.get_value());
        add_pc_entry_watch(cpu,
                           p_bl2_load_profile.get_value() ||
                               p_bl2_load_accel.get_value(),
                           p_bl2_boot_load_image_to_sram_addr.get_value());
        add_pc_entry_watch(cpu, p_bl2_load_profile.get_value(),
                           p_bl2_boot_enc_load_addr.get_value());
        add_pc_entry_watch(cpu,
                           p_bl2_load_profile.get_value() ||
                               p_bl2_load_accel.get_value() ||
                               p_bl2_boot_enc_accel.get_value(),
                           p_bl2_boot_enc_decrypt_addr.get_value());
        add_pc_entry_watch(cpu, p_bl2_load_profile.get_value(),
                           p_bl2_bootutil_img_validate_addr.get_value());
        add_pc_entry_watch(cpu,
                           p_bl2_load_profile.get_value() ||
                               p_bl2_img_hash_accel.get_value(),
                           p_bl2_bootutil_img_hash_addr.get_value());
        add_pc_entry_watch(cpu,
                           p_bl2_load_profile.get_value() ||
                               p_bl2_verify_sig_accel.get_value(),
                           p_bl2_bootutil_verify_sig_addr.get_value());
        add_pc_entry_watch(cpu,
                           p_bl2_load_accel.get_value() ||
                               p_bl2_boot_enc_accel.get_value(),
                           p_bl2_boot_enc_set_key_addr.get_value());

        if (p_bl2_delay_accel.get_value() &&
            p_bl2_delay_cycles_addr.get_value() != 0) {
            add_pc_entry_watch(cpu, true, p_bl2_delay_cycles_addr.get_value());
            m_bl2_delay_watch_active.store(true, std::memory_order_relaxed);
        }
        m_pc_entry_registered = true;
    }

    bool on_pc_entry(uint64_t pc) override
    {
        return try_rse_pc_entry(static_cast<uintptr_t>(pc));
    }

    void on_cpu_sync() override
    {
        if (!m_pc_entry_registered) {
            try_lms_accel();
        }
        try_bl2_delay_accel();
        try_hotpath_accel();
    }

    void end_of_simulation() override
    {
        if (p_hotpath_accel.get_value() || p_lms_accel.get_value() ||
            p_bl2_load_profile.get_value() || p_bl2_load_accel.get_value() ||
            p_bl2_boot_enc_accel.get_value() || p_bl2_img_hash_accel.get_value() ||
            p_bl2_verify_sig_accel.get_value() || p_bl2_delay_accel.get_value()) {
            write_hotpath_profile_file();
        }
    }
};

}  // namespace rse_cpu_accel
}  // namespace platform
}  // namespace qbox

#endif
