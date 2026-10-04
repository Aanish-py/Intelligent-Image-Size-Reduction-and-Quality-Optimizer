#ifndef LOSSLESS_COMPRESSOR_H
#define LOSSLESS_COMPRESSOR_H

#include <string>
#include "CompressionStrategy.h"

// ---------------------------------------------------------------
// Class: LosslessCompressor
// OOP concepts: INHERITANCE + FUNCTION OVERRIDING
// Uses PNG (lossless) encoding from OpenCV.
// ---------------------------------------------------------------
class LosslessCompressor : public CompressionStrategy {
private:
    static const int MIN_LEVEL = 0;   // PNG compression level 0 = fastest
    static const int MAX_LEVEL = 9;   // 9 = smallest file, slowest

public:
    LosslessCompressor();

    // For PNG the 'quality' argument is the PNG compression LEVEL (0-9).
    // It never changes pixel values, only how hard the encoder works.
    bool compress(const Image& image,
                  const std::string& outputPath,
                  int quality) override;

    std::string getMethodName() const override;
};

#endif
