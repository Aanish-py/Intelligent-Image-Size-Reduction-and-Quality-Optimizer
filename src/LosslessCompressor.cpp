#include "LosslessCompressor.h"

#include <algorithm>
#include <iostream>
#include <vector>
#include <opencv2/imgcodecs.hpp>

LosslessCompressor::LosslessCompressor() {}

// LOSSLESS compression: every pixel is preserved exactly.
// The level only controls how hard the encoder works, not quality.
bool LosslessCompressor::compress(const Image& image,
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
    if (ext != ".png") {
        std::cerr << "Error: Lossless output file must end with .png\n";
        return false;
    }

    // PNG compression level: 0 = fastest/biggest, 9 = slowest/smallest
    int level = std::clamp(quality, MIN_LEVEL, MAX_LEVEL);
    std::vector<int> params = { cv::IMWRITE_PNG_COMPRESSION, level };

    try {
        return cv::imwrite(outputPath, image.getPixelData(), params);
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error while writing PNG: " << e.what() << "\n";
        return false;
    }
}

std::string LosslessCompressor::getMethodName() const {
    return "Lossless (PNG)";
}
