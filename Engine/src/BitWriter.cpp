#include "BitWriter.h"

namespace pixelforge {

BitWriter::BitWriter() {
    buffer_.reserve(1024);
}

void BitWriter::writeBit(int bit) {
    accumulator_ = static_cast<uint8_t>((accumulator_ << 1) | (bit ? 1 : 0));
    bit_count_++;
    total_bits_++;

    if (bit_count_ == 8) {
        buffer_.push_back(accumulator_);
        accumulator_ = 0;
        bit_count_ = 0;
    }
}

void BitWriter::writeBits(uint32_t value, int num_bits) {
    for (int i = num_bits - 1; i >= 0; --i) {
        writeBit((value >> i) & 1);
    }
}

void BitWriter::flush() {
    if (bit_count_ > 0) {
        accumulator_ = static_cast<uint8_t>(accumulator_ << (8 - bit_count_));
        buffer_.push_back(accumulator_);
        accumulator_ = 0;
        bit_count_ = 0;
    }
}

const std::vector<uint8_t>& BitWriter::getBuffer() {
    flush();
    return buffer_;
}

void BitWriter::clear() {
    buffer_.clear();
    accumulator_ = 0;
    bit_count_ = 0;
    total_bits_ = 0;
}

} // namespace pixelforge
