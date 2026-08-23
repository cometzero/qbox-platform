/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <cstring>
#include <array>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

#include <cc3xx_core.h>

namespace {

using Cc3xxCore = qbox::cc3xx::core;

constexpr uint64_t RNG_ISR = 0x104;
constexpr uint64_t SAMPLE_CNT1 = 0x130;
constexpr uint64_t PKA_SRAM_ADDR = 0x0d4;
constexpr uint64_t PKA_SRAM_WDATA = 0x0d8;
constexpr uint64_t PKA_SRAM_RDATA = 0x0dc;
constexpr uint64_t PKA_SRAM_RADDR = 0x0e4;
constexpr uint64_t AES_KEY_0 = 0x400;
constexpr uint64_t AES_IV_0 = 0x440;
constexpr uint64_t AES_CTR_0 = 0x460;
constexpr uint64_t AES_CMAC_INIT = 0x47c;
constexpr uint64_t AES_REMAINING_BYTES = 0x4bc;
constexpr uint64_t AES_CONTROL = 0x4c0;
constexpr uint64_t AES_HW_FLAGS = 0x4c8;
constexpr uint64_t AES_RBG_SEEDING_RDY = 0x4fc;
constexpr uint64_t HASH_H = 0x640;
constexpr uint64_t AUTO_HW_PADDING = 0x684;
constexpr uint64_t HASH_CONTROL = 0x7c0;
constexpr uint64_t HASH_PAD_CFG = 0x7c8;
constexpr uint64_t HASH_CUR_LEN0 = 0x7cc;
constexpr uint64_t HASH_CUR_LEN1 = 0x7d0;
constexpr uint64_t CRYPTO_CTL = 0x900;
constexpr uint64_t HOST_RGF_IRR = 0xa00;
constexpr uint64_t HOST_RGF_ICR = 0xa08;
constexpr uint64_t DIN_SRC_LLI_WORD0 = 0xc28;
constexpr uint64_t DIN_SRC_LLI_WORD1 = 0xc2c;
constexpr uint64_t DOUT_DST_LLI_WORD0 = 0xd28;
constexpr uint64_t DOUT_DST_LLI_WORD1 = 0xd2c;
constexpr uint64_t PIDR0 = 0xfe0;
constexpr uint64_t PIDR1 = 0xfe4;
constexpr uint64_t PIDR2 = 0xfe8;
constexpr uint64_t PIDR3 = 0xfec;
constexpr uint64_t CIDR0 = 0xff0;
constexpr uint64_t CIDR1 = 0xff4;
constexpr uint64_t LCS_REG = 0x1f14;

constexpr uint32_t SYM_DMA_COMPLETED = 1u << 11;
constexpr uint32_t DOUT_TO_MEM_INT = 1u << 7;
constexpr uint32_t CC3XX_HASH_ALG_SHA256 = 0x02;
constexpr uint32_t CC3XX_ENGINE_AES = 0x01;
constexpr uint32_t CC3XX_ENGINE_HASH = 0x07;
constexpr uint32_t CC3XX_AES_MODE_ECB = 0x00;
constexpr uint32_t CC3XX_AES_MODE_CBC = 0x01;
constexpr uint32_t CC3XX_AES_MODE_CTR = 0x02;
constexpr uint32_t CC3XX_AES_MODE_CMAC = 0x07;
constexpr uint32_t CC3XX_AES_KEYSIZE_128 = 0x00;

class TestMemory : public Cc3xxCore::memory_if
{
public:
    explicit TestMemory(size_t size): bytes(size, 0) {}

    bool read(uint64_t address, uint8_t* data, unsigned int len) override
    {
        if (data == nullptr || address + len > bytes.size()) {
            return false;
        }
        std::memcpy(data, bytes.data() + address, len);
        return true;
    }

    bool write(uint64_t address, const uint8_t* data, unsigned int len) override
    {
        if (data == nullptr || address + len > bytes.size()) {
            return false;
        }
        std::memcpy(bytes.data() + address, data, len);
        return true;
    }

    std::vector<uint8_t> bytes;
};

uint32_t read32(Cc3xxCore& dut, uint64_t offset, bool debug = false)
{
    uint32_t value = 0;
    const auto result = dut.read(offset, reinterpret_cast<uint8_t*>(&value),
                                 sizeof(value), debug);
    EXPECT_EQ(result.status, Cc3xxCore::access_status::ok);
    EXPECT_EQ(result.transferred, sizeof(value));
    return value;
}

void write32(Cc3xxCore& dut, uint64_t offset, uint32_t value, bool debug = false)
{
    const auto result = dut.write(offset, reinterpret_cast<const uint8_t*>(&value),
                                  sizeof(value), debug);
    EXPECT_EQ(result.status, Cc3xxCore::access_status::ok);
    EXPECT_EQ(result.transferred, sizeof(value));
}

void write_reg_bytes(Cc3xxCore& dut, uint64_t offset, const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; i += sizeof(uint32_t)) {
        uint32_t value = 0;
        std::memcpy(&value, data + i, std::min(sizeof(value), len - i));
        write32(dut, offset + i, value);
    }
}

void read_reg_bytes(Cc3xxCore& dut, uint64_t offset, uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; i += sizeof(uint32_t)) {
        const uint32_t value = read32(dut, offset + i);
        std::memcpy(data + i, &value, std::min(sizeof(value), len - i));
    }
}

} // namespace

TEST(Cc3xxCoreTest, AesCtrBufferHelperDecryptsNistVector)
{
    const std::array<uint8_t, 16> key = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    const std::array<uint8_t, 16> counter = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    };
    const std::array<uint8_t, 16> ciphertext = {
        0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
        0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce,
    };
    const std::array<uint8_t, 16> plaintext = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    };

    std::array<uint8_t, 16> output{};
    ASSERT_TRUE(Cc3xxCore::aes_ctr_xcrypt_buffer(
        key.data(), key.size(), counter.data(), ciphertext.data(),
        ciphertext.size(), 0, output.data()));
    EXPECT_EQ(output, plaintext);
}

TEST(Cc3xxCoreTest, AesCtrBufferHelperHonorsBlockOffset)
{
    const std::array<uint8_t, 16> key = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    const std::array<uint8_t, 16> counter = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    };
    const std::array<uint8_t, 16> ciphertext = {
        0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
        0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce,
    };
    const std::array<uint8_t, 16> plaintext = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    };

    std::array<uint8_t, 7> output{};
    ASSERT_TRUE(Cc3xxCore::aes_ctr_xcrypt_buffer(
        key.data(), key.size(), counter.data(), ciphertext.data() + 5,
        output.size(), 5, output.data()));
    EXPECT_EQ(0, std::memcmp(output.data(), plaintext.data() + 5,
                             output.size()));
}

TEST(Cc3xxCoreTest, ResetInitializesReadableStatus)
{
    Cc3xxCore dut("cc3xx_core");

    EXPECT_EQ(read32(dut, RNG_ISR), 0x1u);
    EXPECT_EQ(read32(dut, SAMPLE_CNT1), 0xffffu);
    EXPECT_EQ(read32(dut, AES_RBG_SEEDING_RDY), 0x1u);
    EXPECT_NE(read32(dut, AES_HW_FLAGS) & (1u << 0), 0u);
    EXPECT_EQ(read32(dut, LCS_REG), 0x5u);
}

TEST(Cc3xxCoreTest, IdentificationRegistersIgnoreWrites)
{
    Cc3xxCore dut("cc3xx_core");
    constexpr std::array<uint64_t, 6> offsets = {
        PIDR0, PIDR1, PIDR2, PIDR3, CIDR0, CIDR1,
    };
    constexpr std::array<uint32_t, 6> expected = {
        0xc1u, 0xb0u, 0x0bu, 0x00u, 0x0du, 0xf0u,
    };

    for (size_t i = 0; i < offsets.size(); ++i) {
        write32(dut, offsets[i], ~expected[i]);
        EXPECT_EQ(read32(dut, offsets[i]), expected[i]);
    }
}

TEST(Cc3xxCoreTest, UnsupportedAddressReturnsAddressError)
{
    Cc3xxCore dut("cc3xx_core");
    uint32_t value = 0;

    auto result = dut.read(0x10000, reinterpret_cast<uint8_t*>(&value),
                           sizeof(value), false);
    EXPECT_EQ(result.status, Cc3xxCore::access_status::address_error);

    result = dut.read(0, nullptr, sizeof(value), false);
    EXPECT_EQ(result.status, Cc3xxCore::access_status::address_error);
}

TEST(Cc3xxCoreTest, DebugReadDoesNotAdvancePkaReadCursor)
{
    Cc3xxCore dut("cc3xx_core");

    write32(dut, PKA_SRAM_ADDR, 0x18);
    write32(dut, PKA_SRAM_WDATA, 0x11223344);
    write32(dut, PKA_SRAM_WDATA, 0x55667788);
    write32(dut, PKA_SRAM_RADDR, 0x18);

    EXPECT_EQ(read32(dut, PKA_SRAM_RDATA, true), 0u);
    EXPECT_EQ(read32(dut, PKA_SRAM_RDATA), 0x11223344u);
    EXPECT_EQ(read32(dut, PKA_SRAM_RDATA), 0x55667788u);
}

TEST(Cc3xxCoreTest, HashEmptyFinalizesThroughStateRegisters)
{
    Cc3xxCore dut("cc3xx_core");

    write32(dut, HASH_CONTROL, CC3XX_HASH_ALG_SHA256);
    write32(dut, HASH_PAD_CFG, 0x4);

    EXPECT_EQ(read32(dut, HASH_H + 0x00), 0xe3b0c442u);
    EXPECT_EQ(read32(dut, HASH_H + 0x04), 0x98fc1c14u);
    EXPECT_EQ(read32(dut, HASH_H + 0x08), 0x9afbf4c8u);
    EXPECT_EQ(read32(dut, HASH_H + 0x0c), 0x996fb924u);
}

TEST(Cc3xxCoreTest, Sha224DmaMatchesKnownDigest)
{
    Cc3xxCore dut("cc3xx_sha224");
    TestMemory memory(0x100);
    dut.set_memory(&memory);

    const uint8_t message[] = {0x61, 0x62, 0x63};
    const uint32_t initial[] = {
        0xc1059ed8u, 0x367cd507u, 0x3070dd17u, 0xf70e5939u,
        0xffc00b31u, 0x68581511u, 0x64f98fa7u, 0xbefa4fa4u,
    };
    const uint32_t expected[] = {
        0x23097d22u, 0x3405d822u, 0x8642a477u, 0xbda255b3u,
        0x2aadbce4u, 0xbda0b3f7u, 0xe36c9da7u,
    };

    std::memcpy(memory.bytes.data() + 0x20, message, sizeof(message));
    write32(dut, HASH_CONTROL, CC3XX_HASH_ALG_SHA256);
    for (size_t index = 0; index < std::size(initial); ++index) {
        write32(dut, HASH_H + index * sizeof(uint32_t), initial[index]);
    }
    write32(dut, CRYPTO_CTL, CC3XX_ENGINE_HASH);
    write32(dut, AUTO_HW_PADDING, 1);
    write32(dut, DIN_SRC_LLI_WORD0, 0x20);
    write32(dut, DIN_SRC_LLI_WORD1, sizeof(message));

    for (size_t index = 0; index < std::size(expected); ++index) {
        EXPECT_EQ(read32(dut, HASH_H + index * sizeof(uint32_t)),
                  expected[index]);
    }
}

TEST(Cc3xxCoreTest, Sha224RestoresDriverOrderedMultipartState)
{
    Cc3xxCore dut("cc3xx_sha224_multipart");
    TestMemory memory(0x200);
    dut.set_memory(&memory);

    const uint32_t initial[] = {
        0xc1059ed8u, 0x367cd507u, 0x3070dd17u, 0xf70e5939u,
        0xffc00b31u, 0x68581511u, 0x64f98fa7u, 0xbefa4fa4u,
    };
    const uint32_t expected[] = {
        0x598e2d81u, 0xf19b0c1au, 0x21b1f269u, 0x6aacd6feu,
        0x837e5511u, 0x1713ce96u, 0x46cffeaau,
    };

    for (size_t index = 0; index < 64; ++index) {
        memory.bytes[0x20 + index] = static_cast<uint8_t>(index);
    }
    memory.bytes[0xa0] = 'a';
    memory.bytes[0xa1] = 'b';
    memory.bytes[0xa2] = 'c';

    write32(dut, HASH_CONTROL, CC3XX_HASH_ALG_SHA256);
    for (size_t index = 0; index < std::size(initial); ++index) {
        write32(dut, HASH_H + index * sizeof(uint32_t), initial[index]);
    }
    write32(dut, CRYPTO_CTL, CC3XX_ENGINE_HASH);
    write32(dut, DIN_SRC_LLI_WORD0, 0x20);
    write32(dut, DIN_SRC_LLI_WORD1, 64);

    uint32_t saved[8];
    for (size_t index = 0; index < std::size(saved); ++index) {
        saved[index] = read32(dut, HASH_H + index * sizeof(uint32_t));
    }
    const uint32_t saved_len = read32(dut, HASH_CUR_LEN0);

    write32(dut, CRYPTO_CTL, CC3XX_ENGINE_HASH);
    write32(dut, HASH_CUR_LEN0, saved_len);
    write32(dut, HASH_CUR_LEN1, 0);
    write32(dut, HASH_CONTROL, CC3XX_HASH_ALG_SHA256);
    for (size_t index = 0; index < std::size(saved); ++index) {
        write32(dut, HASH_H + index * sizeof(uint32_t), saved[index]);
    }
    write32(dut, AUTO_HW_PADDING, 1);
    write32(dut, DIN_SRC_LLI_WORD0, 0xa0);
    write32(dut, DIN_SRC_LLI_WORD1, 3);

    for (size_t index = 0; index < std::size(expected); ++index) {
        EXPECT_EQ(read32(dut, HASH_H + index * sizeof(uint32_t)),
                  expected[index]);
    }
}

TEST(Cc3xxCoreTest, AesCtrDmaUsesMemoryInterface)
{
    Cc3xxCore dut("cc3xx_core");
    TestMemory memory(0x100);
    dut.set_memory(&memory);

    const uint8_t key[] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    const uint8_t counter[] = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    };
    const uint8_t ciphertext[] = {
        0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
        0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce,
    };
    const uint8_t plaintext[] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    };

    std::memcpy(memory.bytes.data() + 0x20, ciphertext, sizeof(ciphertext));
    write_reg_bytes(dut, AES_KEY_0, key, sizeof(key));
    write_reg_bytes(dut, AES_CTR_0, counter, sizeof(counter));
    write32(dut, AES_CONTROL, (CC3XX_AES_KEYSIZE_128 << 12) |
                              (CC3XX_AES_MODE_CTR << 2));
    write32(dut, CRYPTO_CTL, CC3XX_ENGINE_AES);
    write32(dut, DOUT_DST_LLI_WORD0, 0x60);
    write32(dut, DOUT_DST_LLI_WORD1, sizeof(ciphertext));
    write32(dut, HOST_RGF_ICR, SYM_DMA_COMPLETED | DOUT_TO_MEM_INT);
    write32(dut, DIN_SRC_LLI_WORD0, 0x20);
    write32(dut, DIN_SRC_LLI_WORD1, sizeof(ciphertext));

    EXPECT_EQ(std::memcmp(memory.bytes.data() + 0x60, plaintext,
                          sizeof(plaintext)), 0);
}

TEST(Cc3xxCoreTest, AesEcbDmaUsesMemoryInterface)
{
    Cc3xxCore dut("cc3xx_core");
    TestMemory memory(0x100);
    dut.set_memory(&memory);

    const uint8_t key[] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    const uint8_t ciphertext[] = {
        0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
        0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97,
    };
    const uint8_t plaintext[] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    };

    std::memcpy(memory.bytes.data() + 0x20, ciphertext, sizeof(ciphertext));
    write_reg_bytes(dut, AES_KEY_0, key, sizeof(key));
    write32(dut, AES_CONTROL, (CC3XX_AES_KEYSIZE_128 << 12) |
                              (CC3XX_AES_MODE_ECB << 2) | 0x1u);
    write32(dut, CRYPTO_CTL, CC3XX_ENGINE_AES);
    write32(dut, DOUT_DST_LLI_WORD0, 0x60);
    write32(dut, DOUT_DST_LLI_WORD1, sizeof(ciphertext));
    write32(dut, DIN_SRC_LLI_WORD0, 0x20);
    write32(dut, DIN_SRC_LLI_WORD1, sizeof(ciphertext));

    EXPECT_EQ(std::memcmp(memory.bytes.data() + 0x60, plaintext,
                          sizeof(plaintext)), 0);
}

TEST(Cc3xxCoreTest, AesCbcDmaDecryptsNistVector)
{
    Cc3xxCore dut("cc3xx_cbc");
    TestMemory memory(0x100);
    dut.set_memory(&memory);

    const uint8_t key[] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    const uint8_t iv[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };
    const uint8_t ciphertext[] = {
        0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
        0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
    };
    const uint8_t plaintext[] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    };

    std::memcpy(memory.bytes.data() + 0x20, ciphertext, sizeof(ciphertext));
    write_reg_bytes(dut, AES_KEY_0, key, sizeof(key));
    write_reg_bytes(dut, AES_IV_0, iv, sizeof(iv));
    write32(dut, AES_CONTROL, (CC3XX_AES_KEYSIZE_128 << 12) |
                              (CC3XX_AES_MODE_CBC << 2) | 0x1u);
    write32(dut, CRYPTO_CTL, CC3XX_ENGINE_AES);
    write32(dut, DOUT_DST_LLI_WORD0, 0x60);
    write32(dut, DOUT_DST_LLI_WORD1, sizeof(ciphertext));
    write32(dut, DIN_SRC_LLI_WORD0, 0x20);
    write32(dut, DIN_SRC_LLI_WORD1, sizeof(ciphertext));

    EXPECT_EQ(std::memcmp(memory.bytes.data() + 0x60, plaintext,
                          sizeof(plaintext)), 0);
}

TEST(Cc3xxCoreTest, CmacFinishWritesTagToIvRegisters)
{
    Cc3xxCore dut("cc3xx_core");
    TestMemory memory(0x100);
    dut.set_memory(&memory);

    const uint8_t key[] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    const uint8_t message[] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    };
    const uint8_t expected_tag[] = {
        0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
        0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c,
    };
    uint8_t actual_tag[sizeof(expected_tag)] = {};

    std::memcpy(memory.bytes.data() + 0x20, message, sizeof(message));
    write_reg_bytes(dut, AES_KEY_0, key, sizeof(key));
    write32(dut, AES_CONTROL, (CC3XX_AES_KEYSIZE_128 << 12) |
                              (CC3XX_AES_MODE_CMAC << 2));
    write32(dut, AES_CMAC_INIT, 0x1);
    write32(dut, CRYPTO_CTL, CC3XX_ENGINE_AES);
    write32(dut, AES_REMAINING_BYTES, sizeof(message));
    write32(dut, DIN_SRC_LLI_WORD0, 0x20);
    write32(dut, DIN_SRC_LLI_WORD1, sizeof(message));
    write32(dut, AES_REMAINING_BYTES, 0x0);

    read_reg_bytes(dut, AES_IV_0, actual_tag, sizeof(actual_tag));
    EXPECT_EQ(std::memcmp(actual_tag, expected_tag, sizeof(expected_tag)), 0);
}

TEST(Cc3xxCoreTest, StatsJsonIncludesRegisterHistograms)
{
    Cc3xxCore dut("cc3xx_core");

    (void)read32(dut, RNG_ISR);
    write32(dut, HOST_RGF_ICR, SYM_DMA_COMPLETED);

    std::stringstream out;
    dut.write_stats_json(out, "cc3xx_core");
    const auto json = out.str();

    EXPECT_NE(json.find("\"module\": \"cc3xx_core\""), std::string::npos);
    EXPECT_NE(json.find("\"register_read_count\""), std::string::npos);
    EXPECT_NE(json.find("\"0x104\""), std::string::npos);
    EXPECT_NE(json.find("\"register_write_count\""), std::string::npos);
    EXPECT_NE(json.find("\"0xa08\""), std::string::npos);
}

extern "C" int sc_main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
