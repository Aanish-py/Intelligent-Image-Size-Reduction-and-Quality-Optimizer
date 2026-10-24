#pragma once

#include "Image.h"
#include "BMPCommon.h"
#include <string>
#include <vector>

namespace pixelforge {

/**
 * @brief Custom writer for standard 24-bit uncompressed Windows BMP images.
 * Implements strict adherence to BMP specifications including BGR channel
 * ordering, bottom-up scanlines, and 4-byte row alignment padding.
 */
class BMPWriter {
public:
    BMPWriter() = default;

    /**
     * @brief Writes an Image object to disk as a standard 24-bit BMP file.
     * @param image The image to write.
     * @param filepath Destination path on disk.
     * @return true on success.
     * @throws std::runtime_error on I/O failure or invalid image.
     */
    bool writeToFile(const Image& image, const std::string& filepath) const;

    /**
     * @brief Serializes an Image object into a standard BMP byte buffer in memory.
     * Essential for serving reconstructed images directly to the browser via HTTP.
     * @param image The image to serialize.
     * @return Byte vector containing complete BMP file bytes.
     */
    std::vector<uint8_t> writeToMemory(const Image& image) const;
};

} // namespace pixelforge
