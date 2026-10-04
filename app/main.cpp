#include <iostream>
#include <string>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <chrono>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// Forward declarations to avoid custom header files (.h)
namespace LosslessCompressor {
    struct CompressResult {
        size_t originalSize;
        size_t compressedSize;
        long long compressTimeMs;
        std::string predictorName;
        bool success;
        std::string errorMessage;
    };

    struct DecompressResult {
        long long decompressTimeMs;
        std::string predictorName;
        int width;
        int height;
        int channels;
        bool success;
        std::string errorMessage;
    };

    bool verify(const std::string& originalPath, const std::string& reconstructedPath, int& diffPixels);
    CompressResult compress(const std::string& inputPath, const std::string& outputPath);
    DecompressResult decompress(const std::string& inputPath, const std::string& outputPath);
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

std::string formatSize(size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
    return out.str();
}

void printHeader() {
    clearScreen();
    std::cout << "=================================================\n";
    std::cout << "           LOSSLESS IMAGE COMPRESSOR             \n";
    std::cout << "         Compress . Decompress . Verify          \n";
    std::cout << "=================================================\n\n";
}

void handleCompress() {
    printHeader();
    std::cout << "--- COMPRESS IMAGE ---\n\n";
    std::cout << "Enter the path of the image to compress (e.g., test.bmp): ";
    std::string inputPath;
    std::getline(std::cin >> std::ws, inputPath);

    if (!std::filesystem::exists(inputPath)) {
        std::cout << "\nError: File does not exist.\n";
        return;
    }

    cv::Mat image = cv::imread(inputPath, cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        std::cout << "\nError: Invalid or unsupported image format.\n";
        return;
    }

    size_t origSize = std::filesystem::file_size(inputPath);

    std::cout << "\n[ Image Information ]\n";
    std::cout << "File Name      : " << std::filesystem::path(inputPath).filename().string() << "\n";
    std::cout << "Resolution     : " << image.cols << " x " << image.rows << "\n";
    std::cout << "Color Channels : " << image.channels() << "\n";
    std::cout << "Original Size  : " << formatSize(origSize) << "\n\n";

    std::string outputPath = inputPath + ".limg";
    std::cout << "Compression Method:\n[ Our Custom Lossless Algorithm ]\n\n";
    std::cout << "Predictor:\n[ Auto Select (Evaluate NONE, LEFT, UP, AVG, PAETH, GRADIENT) ]\n\n";
    std::cout << "Output Format:\n.limg\n\n";

    std::cout << "Press ENTER to [ COMPRESS IMAGE ]...";
    std::cin.get();

    std::cout << "\nAnalyzing image...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "Selecting predictor...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "Building frequency table...\n";
    std::cout << "Generating Huffman tree...\n";
    std::cout << "Compressing...\n\n";

    auto result = LosslessCompressor::compress(inputPath, outputPath);

    if (!result.success) {
        std::cout << "ERROR: " << result.errorMessage << "\n";
        return;
    }

    std::cout << "=================================================\n";
    std::cout << "              COMPRESSION COMPLETE               \n";
    std::cout << "=================================================\n\n";
    
    std::cout << std::left << std::setw(20) << "Predictor Used" << ": " << result.predictorName << "\n";
    std::cout << std::left << std::setw(20) << "Original Size" << ": " << formatSize(result.originalSize) << "\n";
    std::cout << std::left << std::setw(20) << "Compressed Size" << ": " << formatSize(result.compressedSize) << "\n";
    
    if (result.originalSize > result.compressedSize) {
        size_t saved = result.originalSize - result.compressedSize;
        double ratio = (double)result.compressedSize / result.originalSize * 100.0;
        std::cout << std::left << std::setw(20) << "Space Saved" << ": " << formatSize(saved) << "\n";
        std::cout << std::left << std::setw(20) << "Compression Ratio" << ": " << std::fixed << std::setprecision(2) << ratio << "%\n";
    } else {
        std::cout << std::left << std::setw(20) << "Space Saved" << ": None (Output is larger)\n";
        double ratio = (double)result.compressedSize / result.originalSize * 100.0;
        std::cout << std::left << std::setw(20) << "Compression Ratio" << ": " << std::fixed << std::setprecision(2) << ratio << "%\n";
        std::cout << "\nNote: Lossless compression cannot guarantee size reduction for all images, especially random noise.\n";
    }

    std::cout << std::left << std::setw(20) << "Compression Time" << ": " << result.compressTimeMs << " ms\n";
    std::cout << "Output File        : " << outputPath << "\n";
}

void handleDecompress() {
    printHeader();
    std::cout << "--- DECOMPRESS IMAGE ---\n\n";
    std::cout << "┌─────────────────────────────────┐\n";
    std::cout << "│                                 │\n";
    std::cout << "│      Drag & Drop Image Here     │\n";
    std::cout << "│              or                 │\n";
    std::cout << "│   Type path to .limg file       │\n";
    std::cout << "│                                 │\n";
    std::cout << "└─────────────────────────────────┘\n\n";

    std::cout << "Enter .limg file path: ";
    std::string inputPath;
    std::getline(std::cin >> std::ws, inputPath);

    // Remove quotes if dragged and dropped in Windows terminal
    if (inputPath.size() > 2 && inputPath.front() == '"' && inputPath.back() == '"') {
        inputPath = inputPath.substr(1, inputPath.size() - 2);
    }

    if (!std::filesystem::exists(inputPath)) {
        std::cout << "\nError: File does not exist.\n";
        return;
    }

    std::cout << "\nEnter output file path (e.g., restored.bmp): ";
    std::string outputPath;
    std::getline(std::cin >> std::ws, outputPath);

    std::cout << "\nPress ENTER to [ DECOMPRESS ]...";
    std::cin.get();
    
    std::cout << "\nDecompressing...\n\n";

    auto result = LosslessCompressor::decompress(inputPath, outputPath);

    if (!result.success) {
        std::cout << "ERROR: " << result.errorMessage << "\n";
        return;
    }

    std::cout << "=================================================\n";
    std::cout << "             DECOMPRESSION COMPLETE              \n";
    std::cout << "=================================================\n\n";
    
    std::cout << std::left << std::setw(20) << "Original Resolution" << ": " << result.width << " x " << result.height << "\n";
    std::cout << std::left << std::setw(20) << "Channels" << ": " << result.channels << "\n";
    std::cout << std::left << std::setw(20) << "Predictor Used" << ": " << result.predictorName << "\n";
    std::cout << std::left << std::setw(20) << "Compression Method" << ": Paeth/Gradient + RLE + Huffman\n";
    std::cout << std::left << std::setw(20) << "Decompression Time" << ": " << result.decompressTimeMs << " ms\n\n";

    std::cout << "--- VERIFICATION ---\n";
    std::cout << "Original image path to verify against (leave blank to skip): ";
    std::string origPath;
    std::getline(std::cin, origPath);

    if (!origPath.empty() && std::filesystem::exists(origPath)) {
        int diffPixels = 0;
        bool verified = LosslessCompressor::verify(origPath, outputPath, diffPixels);
        
        std::cout << "\n";
        if (verified) {
            std::cout << "✓ LOSSLESS VERIFICATION PASSED\n\n";
            std::cout << "Original Pixels      : " << (result.width * result.height) << "\n";
            std::cout << "Reconstructed Pixels : " << (result.width * result.height) << "\n";
            std::cout << "Different Pixels     : " << diffPixels << "\n\n";
            std::cout << "The reconstructed image is pixel-for-pixel\nidentical to the original.\n";
        } else {
            std::cout << "✗ VERIFICATION FAILED\n\n";
            if (diffPixels == -1) std::cout << "Dimensions or channels do not match, or file missing.\n";
            else std::cout << "Different Pixels: " << diffPixels << "\n";
        }
    }
}

void printAbout() {
    printHeader();
    std::cout << "--- How It Works ---\n\n";
    std::cout << "Image\n";
    std::cout << "  |\n";
    std::cout << "  v\n";
    std::cout << "Predictor (Analyzes neighborhood: None, Left, Up, Avg, Paeth, Gradient)\n";
    std::cout << "  |\n";
    std::cout << "  v\n";
    std::cout << "Residuals (Difference between actual and predicted pixel)\n";
    std::cout << "  |\n";
    std::cout << "  v\n";
    std::cout << "Entropy Coding (RLE followed by Canonical Huffman Tree)\n";
    std::cout << "  |\n";
    std::cout << "  v\n";
    std::cout << "Bit Packing (Bits squeezed without byte-boundary waste)\n";
    std::cout << "  |\n";
    std::cout << "  v\n";
    std::cout << ".limg (Custom format with Metadata + Checksum + Huffman Table + Payload)\n\n";
    
    std::cout << "Compression Algorithm : Custom built from scratch\n";
    std::cout << "Predictor Used        : Auto-evaluated per image\n";
    std::cout << "Entropy Coding        : Run-Length Encoding + Huffman\n";
    std::cout << "File Format           : .limg\n";
    std::cout << "Lossless Verification : Strict CRC32 check + Pixel Buffer Comparison\n\n";
    
    std::cout << "Press ENTER to return to menu...";
    std::cin.get();
}

int main() {
    while (true) {
        printHeader();
        std::cout << "1. Compress Image\n";
        std::cout << "2. Decompress .limg File\n";
        std::cout << "3. About / How it works\n";
        std::cout << "4. Exit\n\n";
        std::cout << "Select an option: ";
        
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "1") {
            handleCompress();
            std::cout << "\nPress ENTER to continue...";
            std::cin.get();
        } else if (choice == "2") {
            handleDecompress();
            std::cout << "\nPress ENTER to continue...";
            std::cin.get();
        } else if (choice == "3") {
            printAbout();
        } else if (choice == "4") {
            break;
        }
    }
    return 0;
}
