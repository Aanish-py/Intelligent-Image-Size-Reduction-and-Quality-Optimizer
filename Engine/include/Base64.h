#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace pixelforge {

/**
 * @brief Base64 encoder and decoder implemented from scratch.
 * Used for JSON payload serialization of binary BMP, PFL, and PFZ images.
 */
class Base64 {
public:
    static std::string encode(const uint8_t* data, size_t length) {
        static constexpr char TABLE[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string encoded;
        encoded.reserve(((length + 2) / 3) * 4);

        for (size_t i = 0; i < length; i += 3) {
            uint32_t octet_a = data[i];
            uint32_t octet_b = (i + 1 < length) ? data[i + 1] : 0;
            uint32_t octet_c = (i + 2 < length) ? data[i + 2] : 0;

            uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

            encoded.push_back(TABLE[(triple >> 18) & 0x3F]);
            encoded.push_back(TABLE[(triple >> 12) & 0x3F]);
            encoded.push_back((i + 1 < length) ? TABLE[(triple >> 6) & 0x3F] : '=');
            encoded.push_back((i + 2 < length) ? TABLE[triple & 0x3F] : '=');
        }

        return encoded;
    }

    static std::string encode(const std::vector<uint8_t>& buffer) {
        return encode(buffer.data(), buffer.size());
    }

    static std::vector<uint8_t> decode(const std::string& input) {
        std::vector<uint8_t> decoded;
        if (input.empty()) return decoded;

        auto decodeChar = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };

        decoded.reserve((input.size() * 3) / 4);

        for (size_t i = 0; i < input.size(); ) {
            // Skip whitespace / newlines
            while (i < input.size() && (input[i] == '\r' || input[i] == '\n' || input[i] == ' ')) i++;
            if (i >= input.size()) break;

            int b0 = decodeChar(input[i++]);
            int b1 = (i < input.size()) ? decodeChar(input[i++]) : -1;
            int b2 = (i < input.size()) ? decodeChar(input[i++]) : -1;
            int b3 = (i < input.size()) ? decodeChar(input[i++]) : -1;

            if (b0 == -1 || b1 == -1) break;

            decoded.push_back(static_cast<uint8_t>((b0 << 2) | (b1 >> 4)));
            if (b2 != -1) {
                decoded.push_back(static_cast<uint8_t>(((b1 & 0x0F) << 4) | (b2 >> 2)));
            }
            if (b3 != -1) {
                decoded.push_back(static_cast<uint8_t>(((b2 & 0x03) << 6) | b3));
            }
        }

        return decoded;
    }
};

} // namespace pixelforge
