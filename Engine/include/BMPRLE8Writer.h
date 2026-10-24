#pragma once

#include "Image.h"
#include <vector>
#include <cstdint>
#include <string>

namespace pixelforge {

/**
 * @brief Writer for standard Windows 8-bit RLE Compressed BMP files (BI_RLE8).
 * Uses adaptive Median Cut 256-color palette quantization and standard Windows Run-Length Encoding.
 * Produces genuine, viewable .BMP files on disk with high visual fidelity and 85-98% size reduction.
 */
class BMPRLE8Writer {
public:
    BMPRLE8Writer() = default;

    /**
     * @brief Serializes an Image into a standard 8-bit RLE compressed BMP byte buffer.
     * @param image Source 24-bit RGB image.
     * @return Byte vector containing a fully valid BI_RLE8 compressed BMP file.
     */
    std::vector<uint8_t> writeToMemory(const Image& image) const;

    /**
     * @brief Writes the 8-bit RLE compressed BMP directly to a file on disk.
     */
    bool writeToFile(const Image& image, const std::string& filepath) const;

private:
    struct RGBQuad {
        uint8_t b{0};
        uint8_t g{0};
        uint8_t r{0};
        uint8_t reserved{0};
    };

    void buildAdaptivePalette(const Image& image, RGBQuad palette[256], int& active_colors) const;
};

} // namespace pixelforge
