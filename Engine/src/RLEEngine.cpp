#include "RLEEngine.h"
#include <algorithm>

namespace pixelforge {

std::vector<uint8_t> RLEEngine::encode(const std::vector<uint8_t>& input) const {
    return encode(input.data(), input.size());
}

std::vector<uint8_t> RLEEngine::encode(const uint8_t* data, size_t size) const {
    std::vector<uint8_t> output;
    if (!data || size == 0) {
        return output;
    }

    output.reserve(size);
    size_t i = 0;

    while (i < size) {
        // 1. Look ahead to see if we have a run of at least 3 identical bytes
        size_t run_len = 1;
        while (i + run_len < size && data[i + run_len] == data[i] && run_len < 130) {
            run_len++;
        }

        if (run_len >= 3) {
            // Encode Run Packet: MSB=1, bits 0..6 = (run_len - 3)
            uint8_t ctrl = static_cast<uint8_t>(0x80 | (run_len - 3));
            output.push_back(ctrl);
            output.push_back(data[i]);
            i += run_len;
        } else {
            // 2. Accumulate literal non-repeating bytes until next run or max literal chunk (128)
            size_t lit_start = i;
            size_t lit_len = 0;

            while (i < size && lit_len < 128) {
                // If we encounter a run of 3 or more bytes, stop the literal packet
                if (i + 2 < size && data[i] == data[i + 1] && data[i] == data[i + 2]) {
                    break;
                }
                lit_len++;
                i++;
            }

            // Encode Literal Packet: MSB=0, bits 0..6 = (lit_len - 1)
            uint8_t ctrl = static_cast<uint8_t>(lit_len - 1);
            output.push_back(ctrl);
            for (size_t k = 0; k < lit_len; ++k) {
                output.push_back(data[lit_start + k]);
            }
        }
    }

    return output;
}

std::vector<uint8_t> RLEEngine::decode(const std::vector<uint8_t>& input) const {
    return decode(input.data(), input.size());
}

std::vector<uint8_t> RLEEngine::decode(const uint8_t* data, size_t size) const {
    std::vector<uint8_t> output;
    if (!data || size == 0) {
        return output;
    }

    output.reserve(size * 2);
    size_t i = 0;

    while (i < size) {
        uint8_t ctrl = data[i++];
        if (ctrl & 0x80) {
            // Run packet
            size_t run_len = static_cast<size_t>(ctrl & 0x7F) + 3;
            if (i >= size) {
                throw std::runtime_error("RLEEngine::decode: Truncated run packet in stream.");
            }
            uint8_t value = data[i++];
            output.insert(output.end(), run_len, value);
        } else {
            // Literal packet
            size_t lit_len = static_cast<size_t>(ctrl) + 1;
            if (i + lit_len > size) {
                throw std::runtime_error("RLEEngine::decode: Truncated literal packet in stream.");
            }
            output.insert(output.end(), data + i, data + i + lit_len);
            i += lit_len;
        }
    }

    return output;
}

} // namespace pixelforge
