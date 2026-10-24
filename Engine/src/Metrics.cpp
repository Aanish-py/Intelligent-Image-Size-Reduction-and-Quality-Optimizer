#include "Metrics.h"
#include <stdexcept>
#include <cmath>

namespace pixelforge {

double Metrics::computeMSE(const Image& original, const Image& reconstructed) {
    if (original.getWidth() != reconstructed.getWidth() ||
        original.getHeight() != reconstructed.getHeight()) {
        throw std::invalid_argument("Metrics::computeMSE: Image dimensions must match.");
    }

    int width = original.getWidth();
    int height = original.getHeight();
    if (width <= 0 || height <= 0) return 0.0;

    double sum_sq_diff = 0.0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Pixel& o = original.at(x, y);
            const Pixel& r = reconstructed.at(x, y);

            double dr = static_cast<double>(o.r) - static_cast<double>(r.r);
            double dg = static_cast<double>(o.g) - static_cast<double>(r.g);
            double db = static_cast<double>(o.b) - static_cast<double>(r.b);

            sum_sq_diff += (dr * dr) + (dg * dg) + (db * db);
        }
    }

    double total_channel_samples = 3.0 * static_cast<double>(width) * height;
    return sum_sq_diff / total_channel_samples;
}

double Metrics::computePSNR(double mse) noexcept {
    if (mse <= 1e-12) {
        return 999.99; // Standard visual representation of infinity (lossless)
    }
    // PSNR = 10 * log10( (255^2) / MSE )
    constexpr double MAX_VAL_SQ = 255.0 * 255.0; // 65025.0
    return 10.0 * std::log10(MAX_VAL_SQ / mse);
}

QualityReport Metrics::evaluate(const Image& original, const Image& reconstructed) {
    double mse = computeMSE(original, reconstructed);
    double psnr = computePSNR(mse);

    double max_err = 0.0;
    int width = original.getWidth();
    int height = original.getHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Pixel& o = original.at(x, y);
            const Pixel& r = reconstructed.at(x, y);
            max_err = std::max(max_err, static_cast<double>(std::abs(o.r - r.r)));
            max_err = std::max(max_err, static_cast<double>(std::abs(o.g - r.g)));
            max_err = std::max(max_err, static_cast<double>(std::abs(o.b - r.b)));
        }
    }

    QualityReport report;
    report.mse = mse;
    report.psnr = psnr;
    report.is_lossless = (mse == 0.0);
    report.max_error = max_err;
    return report;
}

} // namespace pixelforge
