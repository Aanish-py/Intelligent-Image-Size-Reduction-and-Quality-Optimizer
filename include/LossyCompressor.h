#ifndef LOSSY_COMPRESSOR_H
#define LOSSY_COMPRESSOR_H

#include <string>
#include <opencv2/core.hpp>
#include "CompressionStrategy.h"

// ---------------------------------------------------------------
// Class: LossyCompressor
// OOP concepts: INHERITANCE + FUNCTION OVERRIDING
// Inherits from CompressionStrategy and overrides its two pure
// virtual functions. Uses JPEG (lossy) encoding from OpenCV.
// ---------------------------------------------------------------
class LossyCompressor : public CompressionStrategy {
private:
    static const int MIN_QUALITY = 10;
    static const int MAX_QUALITY = 100;

    // Converts the image into a form JPEG can store (8-bit, no alpha)
    cv::Mat prepareForJpeg(const cv::Mat& source) const;

public:
    LossyCompressor();

    bool compress(const Image& image,
                  const std::string& outputPath,
                  int quality) override;

    std::string getMethodName() const override;
};

#endif
