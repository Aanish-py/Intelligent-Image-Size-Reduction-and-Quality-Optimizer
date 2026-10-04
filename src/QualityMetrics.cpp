#include "QualityMetrics.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <opencv2/imgproc.hpp>

QualityMetrics::QualityMetrics()
    : psnr(-1.0), psnrAvailable(false) {}

// Calculate PSNR by comparing original and compressed images pixel-by-pixel.
// cv::PSNR() handles the actual formula; we normalise channels/depth first.
bool QualityMetrics::calculatePSNR(const cv::Mat& original,
                                   const cv::Mat& compressed) {
    psnrAvailable = false;
    psnr = -1.0;

    if (original.empty() || compressed.empty()) {
        std::cerr << "QualityMetrics: one or both images are empty.\n";
        return false;
    }
    if (original.size() != compressed.size()) {
        std::cerr << "QualityMetrics: images have different dimensions "
                  << "(" << original.cols << "x" << original.rows
                  << " vs " << compressed.cols << "x" << compressed.rows << ").\n";
        return false;
    }

    // Normalise both images to 8-bit BGR so cv::PSNR() can compare them.
    // JPEG strips alpha; PNG keeps it — this makes both comparable.
    auto toBGR8 = [](const cv::Mat& src) -> cv::Mat {
        cv::Mat out;
        if (src.depth() != CV_8U)
            src.convertTo(out, CV_8U, 1.0 / 257.0);
        else
            out = src;

        if (out.channels() == 4)
            cv::cvtColor(out, out, cv::COLOR_BGRA2BGR);
        else if (out.channels() == 1)
            cv::cvtColor(out, out, cv::COLOR_GRAY2BGR);
        return out;
    };

    cv::Mat orig8 = toBGR8(original);
    cv::Mat comp8 = toBGR8(compressed);

    if (orig8.type() != comp8.type()) {
        std::cerr << "QualityMetrics: cannot normalise images to the same type.\n";
        return false;
    }

    try {
        psnr = cv::PSNR(orig8, comp8);
        psnrAvailable = true;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "QualityMetrics: PSNR error: " << e.what() << "\n";
        return false;
    }
}

double QualityMetrics::getPSNR()          const { return psnr; }
bool   QualityMetrics::isPSNRAvailable()  const { return psnrAvailable; }

std::string QualityMetrics::getPSNRString() const {
    if (!psnrAvailable) return "Not available";
    // Lossless compression gives bit-identical images; cv::PSNR returns +inf.
    if (std::isinf(psnr)) return "Perfect (lossless — images are identical)";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << psnr << " dB";
    return oss.str();
}

// Classify quality using documented thresholds (see QualityMetrics.h).
std::string QualityMetrics::getQualityStatus() const {
    if (!psnrAvailable) return "Not available";
    if (std::isinf(psnr))              return "Excellent (lossless — bit-identical)";
    if (psnr >= THRESHOLD_EXCELLENT)   return "Excellent  (>= 40 dB)";
    if (psnr >= THRESHOLD_GOOD)        return "Good       (30-40 dB)";
    if (psnr >= THRESHOLD_ACCEPTABLE)  return "Acceptable (20-30 dB)";
    return "Low (< 20 dB)";
}
