#pragma once

#include <cstdint>
#include <algorithm>

namespace pixelforge {

/**
 * @brief Represents a single 24-bit color pixel with Red, Green, and Blue channels.
 * Encapsulates color data and ensures safe channel operations.
 */
struct Pixel {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};

    constexpr Pixel() noexcept = default;
    constexpr Pixel(uint8_t red, uint8_t green, uint8_t blue) noexcept
        : r(red), g(green), b(blue) {}

    bool operator==(const Pixel& other) const noexcept {
        return r == other.r && g == other.g && b == other.b;
    }

    bool operator!=(const Pixel& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Utility function to clamp integer values to valid 8-bit unsigned range [0, 255].
     */
    static constexpr uint8_t clamp(int value) noexcept {
        if (value < 0) return 0;
        if (value > 255) return 255;
        return static_cast<uint8_t>(value);
    }
};

} // namespace pixelforge
