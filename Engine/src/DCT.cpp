#include "DCT.h"
#include <algorithm>

namespace pixelforge {

// Standard JPEG Luminance Quantization Table
static const int BASE_LUMINANCE_QUANT[8][8] = {
    {16, 11, 10, 16, 24, 40, 51, 61},
    {12, 12, 14, 19, 26, 58, 60, 55},
    {14, 13, 16, 24, 40, 57, 69, 56},
    {14, 17, 22, 29, 51, 87, 80, 62},
    {18, 22, 37, 56, 68, 109, 103, 77},
    {24, 35, 55, 64, 81, 104, 113, 92},
    {49, 64, 78, 87, 103, 121, 120, 101},
    {72, 92, 95, 98, 112, 100, 103, 99}
};

// Standard JPEG Chrominance Quantization Table
static const int BASE_CHROMINANCE_QUANT[8][8] = {
    {17, 18, 24, 47, 99, 99, 99, 99},
    {18, 21, 26, 66, 99, 99, 99, 99},
    {24, 26, 56, 99, 99, 99, 99, 99},
    {47, 66, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99}
};

// 64-element Zigzag Scan Order Coordinates [row, col]
static const int ZIGZAG_MAP[64][2] = {
    {0,0}, {0,1}, {1,0}, {2,0}, {1,1}, {0,2}, {0,3}, {1,2},
    {2,1}, {3,0}, {4,0}, {3,1}, {2,2}, {1,3}, {0,4}, {0,5},
    {1,4}, {2,3}, {3,2}, {4,1}, {5,0}, {6,0}, {5,1}, {4,2},
    {3,3}, {2,4}, {1,5}, {0,6}, {0,7}, {1,6}, {2,5}, {3,4},
    {4,3}, {5,2}, {6,1}, {7,0}, {7,1}, {6,2}, {5,3}, {4,4},
    {3,5}, {2,6}, {1,7}, {2,7}, {3,6}, {4,5}, {5,4}, {6,3},
    {7,2}, {7,3}, {6,4}, {5,5}, {4,6}, {3,7}, {4,7}, {5,6},
    {6,5}, {7,4}, {7,5}, {6,6}, {5,7}, {6,7}, {7,6}, {7,7}
};

DCT::DCT() {
    // Initialize the orthonormal 8x8 DCT matrix A
    constexpr double PI = 3.14159265358979323846;
    for (int u = 0; u < 8; ++u) {
        double alpha = (u == 0) ? (1.0 / std::sqrt(8.0)) : 0.5;
        for (int x = 0; x < 8; ++x) {
            double val = alpha * std::cos(((2.0 * x + 1.0) * u * PI) / 16.0);
            dct_matrix_[u][x] = val;
            idct_matrix_[x][u] = val; // Transpose
        }
    }
}

Block8x8 DCT::forwardDCT(const Block8x8& input) const {
    // Computes F = A * input * A^T
    // Step 1: temp = input * A^T
    Block8x8 temp{};
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += input[r][k] * idct_matrix_[k][c];
            }
            temp[r][c] = sum;
        }
    }

    // Step 2: F = A * temp
    Block8x8 output{};
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += dct_matrix_[r][k] * temp[k][c];
            }
            output[r][c] = sum;
        }
    }

    return output;
}

Block8x8 DCT::inverseDCT(const Block8x8& input) const {
    // Computes f = A^T * input * A
    // Step 1: temp = input * A
    Block8x8 temp{};
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += input[r][k] * dct_matrix_[k][c];
            }
            temp[r][c] = sum;
        }
    }

    // Step 2: f = A^T * temp
    Block8x8 output{};
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += idct_matrix_[r][k] * temp[k][c];
            }
            output[r][c] = sum;
        }
    }

    return output;
}

std::array<std::array<int, 8>, 8> DCT::getScaledQuantTable(int quality, bool is_chroma) {
    quality = std::clamp(quality, 10, 100);

    // Standard JPEG quality scaling factor
    int scale = (quality < 50) ? (5000 / quality) : (200 - 2 * quality);

    std::array<std::array<int, 8>, 8> scaled{};
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int base = is_chroma ? BASE_CHROMINANCE_QUANT[r][c] : BASE_LUMINANCE_QUANT[r][c];
            int val = (base * scale + 50) / 100;
            scaled[r][c] = std::clamp(val, 1, 255);
        }
    }

    return scaled;
}

QuantBlock8x8 DCT::quantize(const Block8x8& dct_block, int quality, bool is_chroma) const {
    auto q_table = getScaledQuantTable(quality, is_chroma);
    QuantBlock8x8 output{};

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double val = dct_block[r][c] / static_cast<double>(q_table[r][c]);
            output[r][c] = static_cast<int16_t>(std::round(val));
        }
    }

    return output;
}

Block8x8 DCT::dequantize(const QuantBlock8x8& q_block, int quality, bool is_chroma) const {
    auto q_table = getScaledQuantTable(quality, is_chroma);
    Block8x8 output{};

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            output[r][c] = static_cast<double>(q_block[r][c]) * q_table[r][c];
        }
    }

    return output;
}

ZigzagArray DCT::toZigzag(const QuantBlock8x8& block) noexcept {
    ZigzagArray zz{};
    for (int i = 0; i < 64; ++i) {
        int r = ZIGZAG_MAP[i][0];
        int c = ZIGZAG_MAP[i][1];
        zz[i] = block[r][c];
    }
    return zz;
}

QuantBlock8x8 DCT::fromZigzag(const ZigzagArray& zigzag) noexcept {
    QuantBlock8x8 block{};
    for (int i = 0; i < 64; ++i) {
        int r = ZIGZAG_MAP[i][0];
        int c = ZIGZAG_MAP[i][1];
        block[r][c] = zigzag[i];
    }
    return block;
}

} // namespace pixelforge
