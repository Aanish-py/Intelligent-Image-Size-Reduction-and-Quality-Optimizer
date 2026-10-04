#include "CompressionResult.h"
#include "Image.h"

#include <iomanip>

// Default constructor: everything zero/empty.
CompressionResult::CompressionResult()
    : originalSize(0), compressedSize(0),
      compressionRatio(0.0), spaceSavedPercentage(0.0),
      outputPath(""), methodName(""), qualityParam(0) {}

CompressionResult::CompressionResult(long long original,
                                     long long compressed,
                                     const std::string& outPath,
                                     const std::string& method,
                                     int qualParam)
    : originalSize(original), compressedSize(compressed),
      compressionRatio(0.0), spaceSavedPercentage(0.0),
      outputPath(outPath), methodName(method), qualityParam(qualParam) {
    calculateCompressionRatio();
    calculateSpaceSaved();
}

// Compression ratio = originalSize / compressedSize
void CompressionResult::calculateCompressionRatio() {
    compressionRatio = (compressedSize > 0)
        ? static_cast<double>(originalSize) / static_cast<double>(compressedSize)
        : 0.0;
}

// Space saved = (original - compressed) / original * 100
// Negative if the compressed file ended up larger.
void CompressionResult::calculateSpaceSaved() {
    spaceSavedPercentage = (originalSize > 0)
        ? (static_cast<double>(originalSize - compressedSize) /
           static_cast<double>(originalSize)) * 100.0
        : 0.0;
}

void CompressionResult::setMetrics(const QualityMetrics& m) {
    metrics = m;
}

void CompressionResult::displayResult(std::ostream& out) const {
    out << "\n========== COMPRESSION RESULT ==========\n"
        << std::left << std::fixed << std::setprecision(2);

    out << std::setw(20) << "Original Size"    << ": " << Image::formatSize(originalSize)    << "\n"
        << std::setw(20) << "Compressed Size"  << ": " << Image::formatSize(compressedSize)  << "\n"
        << std::setw(20) << "Compression Ratio"<< ": " << compressionRatio << " : 1\n"
        << std::setw(20) << "Space Saved"      << ": " << spaceSavedPercentage << "%\n"
        << "\n"
        << std::setw(20) << "Method"           << ": " << methodName << "\n";

    // Label the quality parameter correctly for each method.
    if (methodName.find("Lossy") != std::string::npos)
        out << std::setw(20) << "JPEG Quality" << ": " << qualityParam << " / 100\n";
    else if (methodName.find("LIMG") != std::string::npos)
        out << std::setw(20) << "Algorithm" << ": Paeth + RLE + Huffman\n";
    else
        out << std::setw(20) << "PNG Level"    << ": " << qualityParam << " / 9\n";

    out << "\n"
        << std::setw(20) << "PSNR"           << ": " << metrics.getPSNRString() << "\n"
        << std::setw(20) << "SSIM"           << ": Not available (planned)\n"
        << "\n"
        << std::setw(20) << "Quality Status" << ": " << metrics.getQualityStatus() << "\n"
        << "\n"
        << std::setw(20) << "Output File"    << ": " << outputPath << "\n";

    if (compressedSize > originalSize) {
        out << "\nNote: Compressed file is LARGER than the original.\n"
            << "      This can happen when the source is already compressed\n"
            << "      (e.g. JPEG -> PNG) or when JPEG quality is very high.\n";
    }
    out << "========================================\n";
}

long long   CompressionResult::getOriginalSize()         const { return originalSize; }
long long   CompressionResult::getCompressedSize()       const { return compressedSize; }
double      CompressionResult::getCompressionRatio()     const { return compressionRatio; }
double      CompressionResult::getSpaceSavedPercentage() const { return spaceSavedPercentage; }
std::string CompressionResult::getOutputPath()           const { return outputPath; }
std::string CompressionResult::getMethodName()           const { return methodName; }
int         CompressionResult::getQualityParam()         const { return qualityParam; }
const QualityMetrics& CompressionResult::getMetrics()   const { return metrics; }
