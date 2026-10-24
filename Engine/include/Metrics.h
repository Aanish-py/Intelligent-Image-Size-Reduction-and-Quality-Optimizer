#pragma once

#include "Image.h"
#include <cmath>
#include <limits>
#include <string>

namespace pixelforge {

struct QualityReport {
    double mse{0.0};
    double psnr{0.0};
    bool is_lossless{false};
    double max_error{0.0};
};

/**
 * @brief Computes mathematical objective image quality metrics (MSE and PSNR).
 */
class Metrics {
public:
    /**
     * @brief Computes full QualityReport comparing original vs reconstructed image.
     */
    static QualityReport evaluate(const Image& original, const Image& reconstructed);

    /**
     * @brief Computes Mean Squared Error (MSE) across R, G, B channels.
     */
    static double computeMSE(const Image& original, const Image& reconstructed);

    /**
     * @brief Computes Peak Signal-to-Noise Ratio (PSNR) in decibels (dB) from MSE.
     */
    static double computePSNR(double mse) noexcept;
};

} // namespace pixelforge
