#pragma once

#include "Image.h"
#include "BMPCommon.h"
#include <string>
#include <vector>

namespace pixelforge {

/**
 * @brief Custom parser for 24-bit uncompressed Windows BMP images.
 * Implements strict format validation, row-stride padding handling,
 * and support for both bottom-up and top-down pixel orientations.
 */
class BMPReader {
public:
    BMPReader() = default;

    /**
     * @brief Reads a 24-bit BMP image from a file on disk.
     * @param filepath Path to the .bmp file.
     * @return Decoded Image object.
     * @throws std::runtime_error on I/O error or invalid BMP format.
     */
    Image readFromFile(const std::string& filepath) const;

    /**
     * @brief Reads a 24-bit BMP image directly from an in-memory byte buffer.
     * Essential for web server integration (e.g., uploaded multipart files).
     * @param buffer Byte vector containing raw BMP file data.
     * @return Decoded Image object.
     */
    Image readFromMemory(const std::vector<uint8_t>& buffer) const;

    /**
     * @brief Reads a 24-bit BMP image from a raw pointer and size.
     */
    Image readFromMemory(const uint8_t* data, size_t size) const;

private:
    Image decodeBMPStream(const uint8_t* data, size_t size) const;
};

} // namespace pixelforge
