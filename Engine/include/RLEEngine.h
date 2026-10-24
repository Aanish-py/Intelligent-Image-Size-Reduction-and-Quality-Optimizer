#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

namespace pixelforge {

/**
 * @brief Custom Chunked Run-Length Encoder and Decoder.
 * Uses an unambiguous control-byte scheme distinguishing run-packets from literal-packets:
 * - Run packet: MSB = 1. Length = (ctrl & 0x7F) + 3 (range 3..130), followed by 1 byte value.
 * - Literal packet: MSB = 0. Length = ctrl + 1 (range 1..128), followed by Length bytes.
 */
class RLEEngine {
public:
    RLEEngine() = default;

    /**
     * @brief Compresses arbitrary byte data using custom chunked RLE.
     * @param input Raw input byte buffer.
     * @return Compressed byte vector.
     */
    std::vector<uint8_t> encode(const std::vector<uint8_t>& input) const;
    std::vector<uint8_t> encode(const uint8_t* data, size_t size) const;

    /**
     * @brief Decompresses RLE-encoded byte data.
     * @param input RLE compressed buffer.
     * @return Decompressed raw byte vector.
     * @throws std::runtime_error if encoded stream is truncated or malformed.
     */
    std::vector<uint8_t> decode(const std::vector<uint8_t>& input) const;
    std::vector<uint8_t> decode(const uint8_t* data, size_t size) const;
};

} // namespace pixelforge
