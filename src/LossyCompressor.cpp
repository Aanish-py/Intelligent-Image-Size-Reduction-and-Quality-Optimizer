#include "LossyCompressor.h"

#include <algorithm>
#include <iostream>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

LossyCompressor::LossyCompressor() {}

// JPEG only accepts 8-bit, 1 or 3 channel images.
// This converts 16-bit depth to 8-bit and strips the alpha channel.
cv::Mat LossyCompressor::prepareForJpeg(const cv::Mat& source) const {
    cv::Mat prepared = source;

    if (prepared.depth() != CV_8U) {
        cv::Mat converted;
        prepared.convertTo(converted, CV_8U, 1.0 / 257.0);
        prepared = converted;
    }
    if (prepared.channels() == 4) {
        cv::Mat noAlpha;
        cv::cvtColor(prepared, noAlpha, cv::COLOR_BGRA2BGR);
        prepared = noAlpha;
    }
    return prepared;
}

// LOSSY compression: permanently discards some data for a smaller file.
// Higher quality = better image, larger file.
bool LossyCompressor::compress(const Image& image,
                               const std::string& outputPath,
                               int quality) {
    if (!image.isLoaded()) {
        std::cerr << "Error: No image loaded.\n";
        return false;
    }

    // Check the output extension (case-insensitive)
    std::string ext = outputPath.substr(outputPath.rfind('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".jpg" && ext != ".jpeg") {
        std::cerr << "Error: Lossy output file must end with .jpg\n";
        return false;
    }

    int safeQuality = std::clamp(quality, MIN_QUALITY, MAX_QUALITY);
    cv::Mat prepared = prepareForJpeg(image.getPixelData());
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, safeQuality };

    try {
        return cv::imwrite(outputPath, prepared, params);
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error while writing JPEG: " << e.what() << "\n";
        return false;
    }
}

std::string LossyCompressor::getMethodName() const {
    return "Lossy (JPEG)";
}
