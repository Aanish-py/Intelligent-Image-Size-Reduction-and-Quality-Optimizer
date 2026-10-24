#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

namespace pixelforge {

/**
 * @brief Utility class for bit-level input stream deserialization.
 * Reads bits MSB-first from a contiguous byte buffer.
 */
class BitReader {
public:
    BitReader(const std::vector<uint8_t>& buffer);
    BitReader(const uint8_t* data, size_t size);

    /**
     * @brief Reads a single bit from the stream.
     * @return 0 or 1.
     * @throws std::runtime_error on EOF.
     */
    int readBit();

    /**
     * @brief Reads up to 32 bits from the stream (MSB first).
     * @param num_bits Number of bits to read (1 to 32).
     * @return Resulting unsigned integer.
     */
    uint32_t readBits(int num_bits);

    /**
     * @brief Checks if there are more bits available to read.
     */
    bool hasMoreBits() const noexcept;

    /**
     * @brief Advances to the start of the next byte boundary.
     */
    void alignToByte() noexcept;

    /**
     * @brief Returns current byte index within the buffer.
     */
    size_t getBytePosition() const noexcept { return byte_pos_; }

private:
    const uint8_t* data_{nullptr};
    size_t size_{0};
    size_t byte_pos_{0};
    int bit_pos_{7}; // Current bit in current byte (7 down to 0)
};

} // namespace pixelforge
