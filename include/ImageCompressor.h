#ifndef IMAGE_COMPRESSOR_H
#define IMAGE_COMPRESSOR_H

#include <string>
#include "Image.h"
#include "CompressionResult.h"
#include "CompressionStrategy.h"

// ---------------------------------------------------------------
// Class: ImageCompressor
// OOP concepts:
//  * COMPOSITION: it HAS-A Image and HAS-A CompressionResult.
//  * RUNTIME POLYMORPHISM: it holds a CompressionStrategy*
//    pointer, which can point to LossyCompressor OR
//    LosslessCompressor.  The correct compress() is chosen at
//    runtime — ImageCompressor does not know which one it is.
// ---------------------------------------------------------------
class ImageCompressor {
private:
    Image               image;      // composition: owns an Image object
    CompressionStrategy* strategy;  // not owned; set externally via setStrategy()
    CompressionResult   result;     // composition: owns the last result
    bool                compressed; // true after a successful compression

    // Creates the output folder if it does not exist yet.
    bool prepareOutputFolder(const std::string& outputPath) const;

    // Builds the full printable/saveable report string.
    std::string buildReport() const;

public:
    ImageCompressor();

    // Loads an image; resets any previous result.
    bool loadImage(const std::string& path);

    // Assigns which compression algorithm to use next.
    void setStrategy(CompressionStrategy* newStrategy);

    // Compresses the loaded image using the current strategy.
    // quality = JPEG quality (10-100) or PNG level (0-9).
    // Measures the output file size and calculates PSNR.
    bool compressImage(const std::string& outputPath, int quality);

    // Prints the full report to stdout.
    void generateReport() const;

    // Saves the report to a .txt file.
    bool saveResult(const std::string& reportPath) const;

    // Accessors
    const Image&             getImage()  const;
    const CompressionResult& getResult() const;
    bool hasImage()  const;
    bool hasResult() const;
};

#endif
