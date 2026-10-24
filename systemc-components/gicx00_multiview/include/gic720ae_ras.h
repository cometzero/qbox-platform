/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <array>
#include <cstdint>

// GIC-720AE Issue 0201-12 software-visible RAS records. Geometry describes
// logical diagnostic RAMs of this model, not the physical SRAM implementation.
// ERRINS is non-destructive: it exercises reporting without corrupting IRQ state.
class gic720ae_ras
{
public:
    struct Record {
        uint64_t control = 0;
        uint32_t status = 0;
        uint64_t address = 0;
        uint64_t misc = 0;
        uint64_t misc1 = 0;
    };
    struct Ram {
        uint64_t request = 0;
        unsigned status = 0;
    };
    static constexpr uint32_t V = 1u << 30;
    static constexpr uint32_t UE = 1u << 29;
    static constexpr uint32_t OF = 1u << 27;
    static constexpr uint32_t MV = 1u << 26;
    static constexpr uint32_t CE = 2u << 24;
    std::array<Record, 64> records {};
    std::array<Ram, 35> rams {}; // 16 distributor + 16 GCI + 3 ITS caches
    std::array<uint32_t, 2> irq_ids {};
    uint32_t sac = 0;

    void reset() { records = {}; rams = {}; irq_ids = {}; sac = 0; }
    static bool corrected(unsigned record) { return record && (record & 1) && record < 27; }
    static uint32_t feature(unsigned record)
    {
        return 1u | (corrected(record) ? 0x800u : 0xa0u) | (record == 0 ? 0x200u : 0);
    }
    void report(unsigned index, uint32_t data, bool injected = true,
                uint32_t syndrome = 0, uint64_t address = 0)
    {
        auto& rec = records[index];
        const bool valid = rec.status & V;
        const bool ce = corrected(index);
        uint32_t code = index == 0 ? syndrome : (injected ? 0x100u : 0) | (index >= 25 ? 6u : 7u);
        const bool different = !valid || static_cast<uint32_t>(rec.misc) != data ||
                               (rec.status & 0xffff) != code;
        unsigned count = (rec.misc >> 32) & 0xff;
        if (index && (ce || different)) {
            count = (count + 1) & 0xff;
            if (!count) { rec.misc |= 1ull << 40; rec.status |= OF; }
        }
        if (valid && !ce) rec.status |= OF;
        if (!valid) {
            rec.misc = (rec.misc & (1ull << 40)) | (uint64_t(count) << 32) | data;
            rec.status = (rec.status & OF) | V | MV | code |
                (ce ? CE : UE | (index == 0 || index == 26 ? 2u : 3u) << 20);
            if (index == 0) { rec.address = address; rec.status |= 1u << 31; }
        } else rec.misc = (rec.misc & ~(0xffull << 32)) | (uint64_t(count) << 32);
    }
    bool level(unsigned kind) const
    {
        for (unsigned index = 0; index < records.size(); ++index) {
            const auto& rec = records[index];
            if (!(rec.status & V)) continue;
            if (kind == 0 && ((corrected(index) && (rec.control & 0x100)) ||
                             (!corrected(index) && (rec.control & 8)))) return true;
            if (kind == 1 && !corrected(index) && (rec.control & 4)) return true;
        }
        return false;
    }
    uint64_t read(unsigned record, unsigned reg) const
    {
        const auto& rec = records[record];
        switch (reg) {
        case 0: return feature(record);
        case 8: return rec.control;
        case 16: return rec.status;
        case 24: return record == 0 ? rec.address : 0;
        case 32: return rec.misc;
        case 40: return rec.misc1;
        default: return 0;
        }
    }
    void write(unsigned record, unsigned reg, uint64_t value, uint64_t mask)
    {
        auto& rec = records[record];
        switch (reg) {
        case 8: {
            uint64_t allowed = corrected(record) ? 0x100 : 0xc;
            if (record == 0) allowed |= 0xff00000010ull;
            mask &= allowed;
            rec.control = (rec.control & ~mask) | (value & mask);
            break;
        }
        case 16:
            // Arm RAS status flags are W1C. Syndrome discovery remains readable.
            rec.status &= ~(value & mask & 0xff300000u);
            break;
        case 24:
            if (record == 0 && !(rec.status & (1u << 31)))
                rec.address = (rec.address & ~mask) | (value & mask);
            break;
        case 32:
            if (!(rec.status & MV)) {
                mask &= 0x3ffffffffffull;
                rec.misc = (rec.misc & ~mask) | (value & mask);
            }
            break;
        case 40:
            if (!(rec.status & MV)) rec.misc1 = (rec.misc1 & ~mask) | (value & mask);
            break;
        }
    }
};
