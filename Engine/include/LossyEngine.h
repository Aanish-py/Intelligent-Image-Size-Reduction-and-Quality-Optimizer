#pragma once

#include "Image.h"
#include "ColorSpace.h"
#include "DCT.h"
#include "HuffmanTree.h"
#include "Metrics.h"
#include <vector>
#include <cstdint>
#include <string>

namespace pixelforge {

struct LossyStats {
    size_t original_bytes{0};
    size_t compressed_bytes{0};
    double compression_ratio{0.0};
    double percentage_reduction{0.0};
    double compression_time_ms{0.0};
    double decompression_time_ms{0.0};
    int quality{75};
    double mse{0.0};
    double psnr{0.0};
};

/**
 * @brief Custom Lossy Compression & Decompression Engine based on 2D-DCT.
 * Implements BT.601 YCbCr color conversion, 8x8 block partitioning with edge padding,
 * forward/inverse 2D-DCT, quality-scaled quantization (Q in [10, 100]),
 * zigzag coefficient scanning, zero-run coding, and Huffman entropy encoding.
 */
class LossyEngine {
public:
    LossyEngine() = default;

    /**
     * @brief Compresses an Image into custom .pfz lossy binary format.
     * @param image Input 24-bit image.
     * @param quality Quality parameter in range [10, 100]. Higher = better fidelity, larger size.
     * @param out_stats Optional output pointer for metrics and timings.
     * @return Serialized .pfz binary file buffer.
     */
    std::vector<uint8_t> compress(const Image& image, 
                                  int quality = 75,
                                  LossyStats* out_stats = nullptr) const;

    /**
     * @brief Decompresses a .pfz binary stream back into a 24-bit Image.
     * @param pfz_data Raw .pfz byte vector.
     * @param out_stats Optional output pointer for timings.
     * @return Reconstructed 24-bit Image.
     */
    Image decompress(const std::vector<uint8_t>& pfz_data, 
                     LossyStats* out_stats = nullptr) const;

    bool saveToFile(const Image& image, const std::string& filepath, int quality = 75) const;
    Image loadFromFile(const std::string& filepath) const;

private:
    DCT dct_;

    std::vector<uint8_t> encodeCoefficients(const std::vector<ZigzagArray>& blocks) const;
    std::vector<ZigzagArray> decodeCoefficients(const std::vector<uint8_t>& stream, size_t num_blocks) const;
};

} // namespace pixelforge
