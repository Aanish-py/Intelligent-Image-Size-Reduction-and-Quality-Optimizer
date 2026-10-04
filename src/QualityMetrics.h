#ifndef QUALITY_METRICS_H
#define QUALITY_METRICS_H

#include <string>
#include <opencv2/core.hpp>

// ---------------------------------------------------------------
// Class: QualityMetrics
// OOP concepts: ENCAPSULATION
//
// Calculates objective image quality metrics by comparing the
// original and compressed images pixel-by-pixel.
//
// PSNR (Peak Signal-to-Noise Ratio):
//   Measures how different the compressed image is from the original.
//   Higher is better (decibels).
//   Thresholds used:  >= 40 dB = Excellent
//                     30-40 dB = Good
//                     20-30 dB = Acceptable
//                      < 20 dB = Low
//
// SSIM: planned for the final submission.
// ---------------------------------------------------------------
class QualityMetrics {
private:
    double psnr;           // PSNR in dB; -1.0 if not calculated
    bool   psnrAvailable;  // true only after a successful calculatePSNR()

    // Thresholds documented here so they are easy to explain in a viva.
    static constexpr double THRESHOLD_EXCELLENT  = 40.0;
    static constexpr double THRESHOLD_GOOD       = 30.0;
    static constexpr double THRESHOLD_ACCEPTABLE = 20.0;

public:
    QualityMetrics();

    // Compare original and compressed images and calculate PSNR.
    // Returns false if the images are incompatible or empty.
    bool calculatePSNR(const cv::Mat& original, const cv::Mat& compressed);

    double getPSNR()         const;
    bool   isPSNRAvailable() const;

    // Returns the PSNR value as a formatted string, or "Not available".
    std::string getPSNRString()    const;

    // Returns a quality label based on PSNR thresholds, or "Not available".
    std::string getQualityStatus() const;
};

#endif
