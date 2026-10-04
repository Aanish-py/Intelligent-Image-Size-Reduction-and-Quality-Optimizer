#include "ImageCompressor.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

ImageCompressor::ImageCompressor()
    : strategy(nullptr), compressed(false) {}

bool ImageCompressor::loadImage(const std::string& path) {
    compressed = false;
    result = CompressionResult();
    return image.loadImage(path);
}

// strategy can be any CompressionStrategy subclass (polymorphism).
void ImageCompressor::setStrategy(CompressionStrategy* newStrategy) {
    strategy = newStrategy;
}

// Creates the output folder if it does not already exist.
bool ImageCompressor::prepareOutputFolder(const std::string& outputPath) const {
    fs::path parent = fs::path(outputPath).parent_path();
    if (parent.empty()) return true;

    std::error_code ec;
    if (!fs::exists(parent, ec)) {
        fs::create_directories(parent, ec);
    }
    if (ec) {
        std::cerr << "Error: Cannot create output folder: " << parent.string() << "\n";
        return false;
    }
    return true;
}

bool ImageCompressor::compressImage(const std::string& outputPath, int quality) {
    compressed = false;

    if (!image.isLoaded()) {
        std::cerr << "Error: Please load an image first.\n";
        return false;
    }
    if (strategy == nullptr) {
        std::cerr << "Error: No compression method selected.\n";
        return false;
    }

    // JPEG cannot store transparency — warn the user before proceeding.
    if (image.getChannels() == 4 &&
        strategy->getMethodName().find("Lossy") != std::string::npos) {
        std::cout << "\nWarning: This image has an alpha (transparency) channel.\n"
                  << "         JPEG does not support transparency.\n"
                  << "         The alpha channel will be removed from the output.\n";
    }

    if (!prepareOutputFolder(outputPath)) return false;

    // Runtime polymorphism: compress() calls LossyCompressor or LosslessCompressor
    // depending on what strategy currently points to.
    if (!strategy->compress(image, outputPath, quality)) {
        std::cerr << "Error: Compression failed.\n";
        return false;
    }

    // Measure actual output file size from disk.
    std::error_code ec;
    std::uintmax_t newSize = fs::file_size(outputPath, ec);
    if (ec) {
        std::cerr << "Error: Cannot read compressed file: " << outputPath << "\n";
        return false;
    }

    result = CompressionResult(image.getFileSize(),
                               static_cast<long long>(newSize),
                               outputPath,
                               strategy->getMethodName(),
                               quality);

    // Calculate PSNR from actual pixel data.
    // Load the compressed image back and compare pixel-by-pixel.
    Image compressedImage;
    if (compressedImage.loadImage(outputPath)) {
        QualityMetrics metrics;
        if (metrics.calculatePSNR(image.getPixelData(), compressedImage.getPixelData())) {
            result.setMetrics(metrics);
        }
    }

    compressed = true;
    return true;
}

// Builds the full report string (result + comparison table + method note).
std::string ImageCompressor::buildReport() const {
    std::ostringstream report;
    result.displayResult(report);

    // Append an original-vs-compressed comparison table.
    Image compressedImage;
    if (compressedImage.loadImage(result.getOutputPath())) {
        report << "\n===== ORIGINAL vs COMPRESSED =====\n"
               << std::left
               << std::setw(12) << "Property"
               << std::setw(16) << "Original"
               << std::setw(16) << "Compressed" << "\n"
               << std::string(44, '-') << "\n"
               << std::setw(12) << "Format"
               << std::setw(16) << image.getFormat()
               << std::setw(16) << compressedImage.getFormat() << "\n"
               << std::setw(12) << "Width"
               << std::setw(16) << (std::to_string(image.getWidth()) + " px")
               << std::setw(16) << (std::to_string(compressedImage.getWidth()) + " px") << "\n"
               << std::setw(12) << "Height"
               << std::setw(16) << (std::to_string(image.getHeight()) + " px")
               << std::setw(16) << (std::to_string(compressedImage.getHeight()) + " px") << "\n"
               << std::setw(12) << "Channels"
               << std::setw(16) << image.getChannels()
               << std::setw(16) << compressedImage.getChannels() << "\n"
               << std::setw(12) << "Size"
               << std::setw(16) << Image::formatSize(image.getFileSize())
               << std::setw(16) << Image::formatSize(compressedImage.getFileSize()) << "\n"
               << "==================================\n";
    }

    if (result.getMethodName().find("Lossy") != std::string::npos) {
        report << "\nLossy: some pixel data is permanently discarded to reduce file size.\n"
               << "JPEG quality " << result.getQualityParam() << " was used.\n";
    } else {
        report << "\nLossless: every pixel value is preserved exactly.\n"
               << "PNG compression level " << result.getQualityParam() << " was used.\n";
    }
    return report.str();
}

void ImageCompressor::generateReport() const {
    if (!compressed) {
        std::cout << "No compression result yet. Compress an image first.\n";
        return;
    }
    std::cout << buildReport();
}

bool ImageCompressor::saveResult(const std::string& reportPath) const {
    if (!compressed) {
        std::cerr << "Error: Nothing to save yet.\n";
        return false;
    }
    // Create the output folder if needed (same logic as compressImage).
    if (!prepareOutputFolder(reportPath)) return false;

    std::ofstream file(reportPath);
    if (!file) {
        std::cerr << "Error: Cannot create report file: " << reportPath << "\n";
        return false;
    }
    file << "SMART IMAGE COMPRESSOR - REPORT\n"
         << "Input image : " << image.getFilePath() << "\n\n"
         << buildReport();
    return true;
}

const Image&             ImageCompressor::getImage()  const { return image; }
const CompressionResult& ImageCompressor::getResult() const { return result; }
bool ImageCompressor::hasImage()  const { return image.isLoaded(); }
bool ImageCompressor::hasResult() const { return compressed; }
