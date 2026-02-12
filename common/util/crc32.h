#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>   // memcpy

#include "common/common_types.h"  // assuming this defines u8, u32, etc.

#if defined(__SSE4_2__) && !defined(NO_HARDWARE_CRC32)
#include <immintrin.h>
#define HAS_SSE42_CRC 1
#elif defined(__ARM_FEATURE_CRC32) && !defined(NO_HARDWARE_CRC32)
#include <arm_acle.h>
#define HAS_ARM_CRC 1
#endif

inline u32 crc32(const u8* data, size_t size) {
    u32 crc = 0xFFFFFFFF;

#if defined(HAS_SSE42_CRC)
    // x86 SSE4.2 hardware CRC32
    while (size >= 4) {
        u32 word;
        std::memcpy(&word, data, 4);
        crc = _mm_crc32_u32(crc, word);
        data += 4;
        size -= 4;
    }
    while (size > 0) {
        crc = _mm_crc32_u8(crc, *data);
        ++data;
        --size;
    }

#elif defined(HAS_ARM_CRC)
    // ARMv8 CRC32 hardware acceleration
    while (size >= 4) {
        u32 word;
        std::memcpy(&word, data, 4);
        crc = __crc32w(crc, word);
        data += 4;
        size -= 4;
    }
    while (size > 0) {
        crc = __crc32b(crc, *data);
        ++data;
        --size;
    }

#else
    // Software fallback (bit-by-bit) - slow but always works
    static const u32 crc32_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        // ... (full 256-entry CRC32 table here - omitted for brevity)
        // You can generate it or include from a header
    };

    while (size--) {
        crc = crc32_table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
    }
#endif

    return ~crc;
}