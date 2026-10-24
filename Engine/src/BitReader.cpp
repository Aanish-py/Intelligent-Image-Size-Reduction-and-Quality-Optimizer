#include "BitReader.h"

namespace pixelforge {

BitReader::BitReader(const std::vector<uint8_t>& buffer)
    : data_(buffer.data()), size_(buffer.size()), byte_pos_(0), bit_pos_(7) {}

BitReader::BitReader(const uint8_t* data, size_t size)
    : data_(data), size_(size), byte_pos_(0), bit_pos_(7) {}

int BitReader::readBit() {
    if (byte_pos_ >= size_) {
        throw std::runtime_error("BitReader: Unexpected end of bitstream (EOF).");
    }

    int bit = (data_[byte_pos_] >> bit_pos_) & 1;
    bit_pos_--;
    if (bit_pos_ < 0) {
        bit_pos_ = 7;
        byte_pos_++;
    }
    return bit;
}

uint32_t BitReader::readBits(int num_bits) {
    if (num_bits <= 0 || num_bits > 32) {
        throw std::invalid_argument("BitReader: num_bits must be between 1 and 32.");
    }

    uint32_t result = 0;
    for (int i = 0; i < num_bits; ++i) {
        result = (result << 1) | readBit();
    }
    return result;
}

bool BitReader::hasMoreBits() const noexcept {
    return byte_pos_ < size_;
}

void BitReader::alignToByte() noexcept {
    if (bit_pos_ != 7) {
        bit_pos_ = 7;
        byte_pos_++;
    }
}

} // namespace pixelforge
