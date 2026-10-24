#pragma once

#include <cstdint>
#include <cstddef>

namespace pixelforge {

/**
 * @brief Simple, fast Adler-32 checksum algorithm implemented from scratch.
 * Used for verifying integrity of original and reconstructed image payloads.
 */
class Checksum {
public:
    static uint32_t adler32(const uint8_t* data, size_t length) noexcept {
        constexpr uint32_t MOD_ADLER = 65521;
        uint32_t a = 1;
        uint32_t b = 0;

        for (size_t i = 0; i < length; ++i) {
            a = (a + data[i]) % MOD_ADLER;
            b = (b + a) % MOD_ADLER;
        }

        return (b << 16) | a;
    }
};

} // namespace pixelforge
