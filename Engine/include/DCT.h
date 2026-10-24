#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cmath>

namespace pixelforge {

using Block8x8 = std::array<std::array<double, 8>, 8>;
using QuantBlock8x8 = std::array<std::array<int16_t, 8>, 8>;
using ZigzagArray = std::array<int16_t, 64>;

/**
 * @brief Mathematical 2D Discrete Cosine Transform (2D-DCT-II) and Quantization.
 * Implements forward/inverse DCT, quality scaling, and zigzag scanning from scratch.
 */
class DCT {
public:
    DCT();

    /**
     * @brief Computes Forward 2D-DCT on an 8x8 block (level-shifted by -128).
     * Converts spatial domain pixel values into frequency domain coefficients.
     */
    Block8x8 forwardDCT(const Block8x8& input) const;

    /**
     * @brief Computes Inverse 2D-DCT on an 8x8 block (level-shifted back by +128).
     * Converts frequency coefficients back into spatial domain pixel values.
     */
    Block8x8 inverseDCT(const Block8x8& input) const;

    /**
     * @brief Quantizes an 8x8 DCT block using quality-scaled quantization matrix.
     */
    QuantBlock8x8 quantize(const Block8x8& dct_block, int quality, bool is_chroma) const;

    /**
     * @brief Dequantizes an 8x8 quantized block back to floating-point DCT coefficients.
     */
    Block8x8 dequantize(const QuantBlock8x8& q_block, int quality, bool is_chroma) const;

    /**
     * @brief Traverses 8x8 2D block in zigzag diagonal order into 64-element 1D array.
     */
    static ZigzagArray toZigzag(const QuantBlock8x8& block) noexcept;

    /**
     * @brief Inverses 64-element 1D zigzag array back into 8x8 2D block.
     */
    static QuantBlock8x8 fromZigzag(const ZigzagArray& zigzag) noexcept;

    /**
     * @brief Scales a base 8x8 quantization matrix by quality parameter Q in [10, 100].
     */
    static std::array<std::array<int, 8>, 8> getScaledQuantTable(int quality, bool is_chroma);

private:
    std::array<std::array<double, 8>, 8> dct_matrix_; // Orthonormal DCT basis matrix A
    std::array<std::array<double, 8>, 8> idct_matrix_; // A^T
};

} // namespace pixelforge
