#pragma once

#include "Image.h"
#include "Predictor.h"
#include "RLEEngine.h"
#include "HuffmanTree.h"
#include "Checksum.h"
#include <vector>
#include <cstdint>
#include <string>

namespace pixelforge {

struct LosslessStats {
    size_t original_bytes{0};
    size_t compressed_bytes{0};
    double compression_ratio{0.0};
    double percentage_reduction{0.0};
    double compression_time_ms{0.0};
    double decompression_time_ms{0.0};
    PredictorType chosen_predictor{PredictorType::None};
    bool rle_applied{false};
    bool verified_exact{false};
};

/**
 * @brief Custom Lossless Compression & Decompression Engine.
 * Implements planed spatial prediction, modulo-256 residual mapping,
 * optional chunked RLE, canonical Huffman coding, and Adler32 checksum verification.
 */
class LosslessEngine {
public:
    LosslessEngine() = default;

    /**
     * @brief Compresses an Image into custom .pfl binary format.
     * @param image Input image.
     * @param predictor Preferred predictor, or adaptive if adaptive_selection is true.
     * @param adaptive_selection If true, evaluates all predictors and selects the smallest result.
     * @return Serialized .pfl binary file data.
     */
    std::vector<uint8_t> compress(const Image& image, 
                                  PredictorType predictor = PredictorType::Paeth,
                                  bool adaptive_selection = true,
                                  LosslessStats* out_stats = nullptr) const;

    /**
     * @brief Decompresses a .pfl binary stream back into a 24-bit Image.
     * @param pfl_data Raw .pfl byte vector.
     * @param out_stats Optional output pointer to receive timing and verification stats.
     * @return Fully reconstructed Image bit-for-bit identical to original.
     * @throws std::runtime_error on corrupted, truncated, or invalid .pfl data.
     */
    Image decompress(const std::vector<uint8_t>& pfl_data, 
                     LosslessStats* out_stats = nullptr) const;

    /**
     * @brief File helper functions for saving and loading .pfl files directly.
     */
    bool saveToFile(const Image& image, const std::string& filepath, bool adaptive = true) const;
    Image loadFromFile(const std::string& filepath) const;

private:
    std::vector<uint8_t> generateResiduals(const Image& image, const IPredictor& predictor) const;
    Image reconstructFromResiduals(int width, int height, 
                                   const std::vector<uint8_t>& residuals, 
                                   const IPredictor& predictor) const;
};

} // namespace pixelforge
