// Smart Image Compressor — console UI
// OOP with C++ course project

#include <filesystem>
#include <iostream>
#include <string>

#include "ImageCompressor.h"
#include "LossyCompressor.h"
#include "LosslessCompressor.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------

void showMenu() {
    std::cout << "\n=========================================\n"
              << "        SMART IMAGE COMPRESSOR\n"
              << "=========================================\n"
              << "1. Load Image\n"
              << "2. Display Image Information\n"
              << "3. Lossy Compression  (JPEG)\n"
              << "4. Lossless Compression (Custom LIMG)\n"
              << "5. Decompress LIMG File\n"
              << "6. View Last Compression Result\n"
              << "7. Save Report to File\n"
              << "8. Run Lossless Compression Tests\n"
              << "9. Exit\n"
              << "\nEnter your choice: ";
}

// Strips leading/trailing spaces and quotes from a path.
// Useful because Windows Explorer copies paths with surrounding quotes.
std::string cleanPath(const std::string& text) {
    const std::string junk = " \t\r\n\"'";
    std::size_t start = text.find_first_not_of(junk);
    if (start == std::string::npos) return "";
    std::size_t end = text.find_last_not_of(junk);
    return text.substr(start, end - start + 1);
}

// Reads a whole line safely (no leftover-newline issues).
std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// Keeps asking until the user types a valid integer in [minValue, maxValue].
// Returns false if stdin is closed.
bool readInt(const std::string& prompt, int minValue, int maxValue, int& value) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        try {
            std::size_t used = 0;
            int number = std::stoi(line, &used);
            // Accept only if the entire trimmed string was consumed as a number
            if (used == cleanPath(line).size() && number >= minValue && number <= maxValue) {
                value = number;
                return true;
            }
        } catch (const std::exception&) {}
        std::cout << "Invalid input. Enter a number from "
                  << minValue << " to " << maxValue << ".\n";
    }
}

// Builds the output file path: output/<stem>_compressed.<ext>
std::string buildOutputPath(const Image& image, const std::string& extension) {
    fs::path stem(image.getFileName());
    return (fs::path("output") / (stem.stem().string() + "_compressed" + extension)).string();
}

// ---------------------------------------------------------------
// Menu handlers — each one does one thing
// ---------------------------------------------------------------

void handleLoad(ImageCompressor& compressor) {
    std::string path = cleanPath(readLine("Enter image path (e.g. images/sample.bmp): "));
    if (path.empty()) {
        std::cout << "No path entered.\n";
        return;
    }
    if (compressor.loadImage(path)) {
        std::cout << "\nImage loaded successfully!\n";
        compressor.getImage().displayInfo();
    }
}

void handleLossy(ImageCompressor& compressor, LossyCompressor& lossy) {
    if (!compressor.hasImage()) {
        std::cout << "Please load an image first (option 1).\n";
        return;
    }
    std::cout << "\n100 = highest quality (largest file).\n"
              << "Lower values = smaller file, more visible artifacts.\n"
              << "Tip: 75-90 is usually a good balance.\n";

    int quality = 0;
    if (!readInt("Enter JPEG quality (10-100): ", 10, 100, quality)) return;

    // Polymorphism: setStrategy() takes a CompressionStrategy*
    // but we pass a LossyCompressor* (a subclass).
    compressor.setStrategy(&lossy);
    std::string outputPath = buildOutputPath(compressor.getImage(), ".jpg");

    if (compressor.compressImage(outputPath, quality)) {
        std::cout << "\nLossy compression completed.\n";
        compressor.getResult().displayResult();
    }
}

void handleLossless(ImageCompressor& compressor, LosslessCompressor& lossless) {
    if (!compressor.hasImage()) {
        std::cout << "Please load an image first (option 1).\n";
        return;
    }
    std::cout << "\nCustom lossless compression preserves every pixel exactly.\n"
              << "Uses Paeth prediction + RLE + Huffman coding.\n"
              << "Output format: .limg (custom binary format)\n\n";

    compressor.setStrategy(&lossless);
    std::string outputPath = buildOutputPath(compressor.getImage(), ".limg");

    if (compressor.compressImage(outputPath, 0)) {
        std::cout << "\nLossless compression completed.\n";
        compressor.getResult().displayResult();

        // Automatic round-trip verification
        std::cout << "\n--- Automatic Integrity Verification ---\n";
        LosslessCompressor::verifyRoundTrip(compressor.getImage(), outputPath);
    }
}

void handleDecompress() {
    std::string path = cleanPath(readLine("Enter .limg file path: "));
    if (path.empty()) {
        std::cout << "No path entered.\n";
        return;
    }

    cv::Mat decompressed;
    if (LosslessCompressor::decompress(path, decompressed)) {
        std::cout << "\nDecompression successful!\n"
                  << "  Dimensions: " << decompressed.cols << "x"
                  << decompressed.rows << " x " << decompressed.channels()
                  << " channels\n";

        // Ask if user wants to save as BMP/PNG
        std::string savePath = cleanPath(
            readLine("Save decompressed image as (e.g. output/restored.bmp, or press Enter to skip): "));
        if (!savePath.empty()) {
            if (cv::imwrite(savePath, decompressed)) {
                std::cout << "Saved to: " << savePath << "\n";
            } else {
                std::cerr << "Failed to save image.\n";
            }
        }
    } else {
        std::cerr << "Decompression failed.\n";
    }
}

void handleViewResult(const ImageCompressor& compressor) {
    if (!compressor.hasResult()) {
        std::cout << "No compression result yet. Use option 3 or 4 first.\n";
        return;
    }
    compressor.generateReport();
}

void handleSaveReport(const ImageCompressor& compressor) {
    if (!compressor.hasResult()) {
        std::cout << "No compression result yet. Use option 3 or 4 first.\n";
        return;
    }
    fs::path stem(compressor.getImage().getFileName());
    std::string reportPath =
        (fs::path("output") / (stem.stem().string() + "_report.txt")).string();

    if (compressor.saveResult(reportPath)) {
        std::cout << "Report saved to " << reportPath << "\n";
    }
}

void handleRunTests(const ImageCompressor& compressor) {
    std::string imagePath;
    if (compressor.hasImage()) {
        imagePath = compressor.getImage().getFilePath();
        std::cout << "\nWill include loaded image (" << compressor.getImage().getFileName()
                  << ") in the test suite.\n";
    } else {
        std::cout << "\nNo image loaded — running synthetic tests only.\n"
                  << "Load an image first (option 1) to also benchmark against OpenCV PNG.\n";
    }
    LosslessCompressor::runTests(imagePath);
}

// ---------------------------------------------------------------
// main — creates objects and runs the menu loop
// ---------------------------------------------------------------
int main() {
    // These three objects cover the whole application:
    ImageCompressor    compressor;  // coordinates Image + strategy + result
    LossyCompressor    lossy;       // strategy 1: JPEG (child of CompressionStrategy)
    LosslessCompressor lossless;    // strategy 2: Custom LIMG (child of CompressionStrategy)

    int choice = 0;
    while (true) {
        showMenu();
        if (!readInt("", 1, 9, choice)) break;

        switch (choice) {
            case 1: handleLoad(compressor);               break;
            case 2:
                if (compressor.hasImage())
                    compressor.getImage().displayInfo();
                else
                    std::cout << "Please load an image first (option 1).\n";
                break;
            case 3: handleLossy(compressor, lossy);       break;
            case 4: handleLossless(compressor, lossless);  break;
            case 5: handleDecompress();                    break;
            case 6: handleViewResult(compressor);          break;
            case 7: handleSaveReport(compressor);          break;
            case 8: handleRunTests(compressor);            break;
            case 9:
                std::cout << "Thank you for using Smart Image Compressor. Goodbye!\n";
                return 0;
        }
    }
    return 0;
}
