#ifndef COMPRESSION_RESULT_H
#define COMPRESSION_RESULT_H

#include <iostream>
#include <string>
#include "QualityMetrics.h"

// ---------------------------------------------------------------
// Class: CompressionResult
// OOP concept: ENCAPSULATION
//
// Stores the numbers produced by one compression run.
// The ratio and the saved percentage are calculated INSIDE the
// class, so outside code cannot accidentally put wrong values in.
//
// Quality metrics (PSNR) are stored here so the full result can
// be printed or saved in one place.
// ---------------------------------------------------------------
class CompressionResult {
private:
    long long originalSize;          // in bytes, from std::filesystem
    long long compressedSize;        // in bytes, from std::filesystem
    double    compressionRatio;      // originalSize / compressedSize
    double    spaceSavedPercentage;  // (original - compressed) / original * 100
    std::string outputPath;          // where the compressed file was saved
    std::string methodName;          // e.g. "Lossy (JPEG)"
    int         qualityParam;        // JPEG quality 10-100 or PNG level 0-9
    QualityMetrics metrics;          // PSNR (and future SSIM)

    // Private helpers — only the class itself should call these.
    void calculateCompressionRatio();   // original / compressed
    void calculateSpaceSaved();         // percentage saved (can be negative)

public:
    // Default constructor: everything zero / empty.
    CompressionResult();

    // Parameterized constructor: calculates ratio and space-saved immediately.
    CompressionResult(long long original,
                      long long compressed,
                      const std::string& outPath,
                      const std::string& method,
                      int qualParam = 0);

    // Attach quality metrics after the result is created.
    // (Called by ImageCompressor after loading both images.)
    void setMetrics(const QualityMetrics& m);

    // Prints the result in the documented layout.
    void displayResult(std::ostream& out = std::cout) const;

    // Getters
    long long   getOriginalSize()         const;
    long long   getCompressedSize()       const;
    double      getCompressionRatio()     const;
    double      getSpaceSavedPercentage() const;
    std::string getOutputPath()           const;
    std::string getMethodName()           const;
    int         getQualityParam()         const;
    const QualityMetrics& getMetrics()    const;
};

#endif
