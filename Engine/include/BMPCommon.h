#pragma once

#include <cstdint>
#include <cstddef>

namespace pixelforge {

#pragma pack(push, 1)

/**
 * @brief Represents the standard 14-byte BMP File Header.
 * Packed with 1-byte alignment to prevent compiler-added padding.
 */
struct BMPFileHeader {
    uint16_t file_type{0x4D42}; // 'BM' in little-endian (0x42, 0x4D)
    uint32_t file_size{0};      // File size in bytes
    uint16_t reserved1{0};      // Reserved, must be 0
    uint16_t reserved2{0};      // Reserved, must be 0
    uint32_t offset_data{54};   // Offset to start of pixel data (14 + 40 = 54)
};

/**
 * @brief Represents the standard 40-byte BMP Information Header (BITMAPINFOHEADER).
 */
struct BMPInfoHeader {
    uint32_t header_size{40};    // Size of this header in bytes (40)
    int32_t  width{0};           // Width of image in pixels
    int32_t  height{0};          // Height of image in pixels (+ = bottom-up, - = top-down)
    uint16_t planes{1};          // Number of color planes (must be 1)
    uint16_t bit_count{24};      // Bits per pixel (24 for uncompressed RGB)
    uint32_t compression{0};    // 0 = BI_RGB (uncompressed)
    uint32_t size_image{0};      // Size of pixel data with padding (can be 0 for BI_RGB)
    int32_t  x_pixels_per_meter{2835}; // ~72 DPI
    int32_t  y_pixels_per_meter{2835}; // ~72 DPI
    uint32_t colors_used{0};     // Number of colors in palette (0 for 24-bit)
    uint32_t colors_important{0};// Number of important colors (0 = all)
};

#pragma pack(pop)

static_assert(sizeof(BMPFileHeader) == 14, "BMPFileHeader must be exactly 14 bytes");
static_assert(sizeof(BMPInfoHeader) == 40, "BMPInfoHeader must be exactly 40 bytes");

/**
 * @brief Computes the row stride in bytes for a given width and 24-bit depth.
 * Every row in a BMP must be padded to a 4-byte boundary.
 */
inline size_t computeBMPRowStride(int width) noexcept {
    return static_cast<size_t>((width * 3 + 3) / 4) * 4;
}

inline size_t computeBMPPadding(int width) noexcept {
    return (4 - (static_cast<size_t>(width) * 3) % 4) % 4;
}

} // namespace pixelforge
