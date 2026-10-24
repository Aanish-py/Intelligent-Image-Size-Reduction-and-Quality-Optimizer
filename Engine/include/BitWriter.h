#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace pixelforge {

/**
 * @brief Utility class for bit-level output stream serialization.
 * Buffers bits into a byte accumulator and appends full bytes to a memory buffer.
 */
class BitWriter {
public:
    BitWriter();

    /**
     * @brief Writes a single bit (0 or 1) to the stream.
     */
    void writeBit(int bit);

    /**
     * @brief Writes up to 32 bits from a uint32_t value (written MSB first).
     * @param value The value whose bits to write.
     * @param num_bits The number of lower bits to write (1 to 32).
     */
    void writeBits(uint32_t value, int num_bits);

    /**
     * @brief Flushes any remaining bits in the accumulator to the buffer,
     * padding the remaining bits with zeros to complete the final byte.
     */
    void flush();

    /**
     * @brief Gets the underlying serialized byte buffer. Automatically flushes if needed.
     */
    const std::vector<uint8_t>& getBuffer();

    /**
     * @brief Clears the stream and resets all state.
     */
    void clear();

    /**
     * @brief Returns total number of full bytes written so far.
     */
    size_t sizeInBytes() const noexcept { return buffer_.size() + (bit_count_ > 0 ? 1 : 0); }

    /**
     * @brief Returns total number of bits written so far.
     */
    uint64_t totalBitsWritten() const noexcept { return total_bits_; }

private:
    std::vector<uint8_t> buffer_;
    uint8_t accumulator_{0};
    int bit_count_{0}; // Bits currently held in accumulator (0..7)
    uint64_t total_bits_{0};
};

} // namespace pixelforge
