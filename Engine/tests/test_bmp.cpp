#include "Image.h"
#include "BMPReader.h"
#include "BMPWriter.h"
#include "BMPRLE8Writer.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace pixelforge;

void testRLE8BMPRoundTrip() {
    std::cout << "[TEST] Running BMP 8-bit RLE (BI_RLE8) compression & decode test... ";

    // Create a 64x64 test image with flat color strips
    Image original(64, 64);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (x < 16) original.at(x, y) = Pixel(255, 0, 0);       // Red
            else if (x < 32) original.at(x, y) = Pixel(0, 255, 0);  // Green
            else if (x < 48) original.at(x, y) = Pixel(0, 0, 255);  // Blue
            else original.at(x, y) = Pixel(255, 255, 255);          // White
        }
    }

    BMPWriter writer24;
    BMPRLE8Writer rle_writer;
    BMPReader reader;

    auto uncomp_data = writer24.writeToMemory(original);
    auto rle_data = rle_writer.writeToMemory(original);

    assert(!rle_data.empty());
    // RLE BMP should be significantly smaller than 24-bit BMP
    assert(rle_data.size() < uncomp_data.size());

    // Read back using BMPReader
    Image decoded = reader.readFromMemory(rle_data);
    assert(decoded.getWidth() == 64);
    assert(decoded.getHeight() == 64);

    std::cout << "PASSED (Uncompressed: " << uncomp_data.size() 
              << " bytes -> RLE BMP: " << rle_data.size() << " bytes)!\n";
}

void testBMPRoundTrip(int width, int height, const std::string& description) {
    std::cout << "[TEST] Running BMP round-trip test: " << description 
              << " (" << width << "x" << height << ", padding=" << computeBMPPadding(width) << " bytes)... ";

    Image original(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t r = static_cast<uint8_t>((x * 255) / (width == 1 ? 1 : width - 1));
            uint8_t g = static_cast<uint8_t>((y * 255) / (height == 1 ? 1 : height - 1));
            uint8_t b = static_cast<uint8_t>((x + y) % 256);
            original.at(x, y) = Pixel(r, g, b);
        }
    }

    BMPWriter writer;
    BMPReader reader;

    // 1. In-memory round-trip
    std::vector<uint8_t> bmp_data = writer.writeToMemory(original);
    assert(!bmp_data.empty());
    assert(bmp_data.size() == 54 + computeBMPRowStride(width) * height);

    Image decoded = reader.readFromMemory(bmp_data);
    assert(decoded.getWidth() == width);
    assert(decoded.getHeight() == height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Pixel orig_px = original.at(x, y);
            Pixel deco_px = decoded.at(x, y);
            if (orig_px != deco_px) {
                std::cerr << "\n[FAIL] Mismatch at (" << x << ", " << y << "): expected ("
                          << (int)orig_px.r << "," << (int)orig_px.g << "," << (int)orig_px.b
                          << ") but got ("
                          << (int)deco_px.r << "," << (int)deco_px.g << "," << (int)deco_px.b << ")\n";
                std::exit(1);
            }
        }
    }

    std::cout << "PASSED (Exact match!)\n";
}

void testErrorHandling() {
    std::cout << "[TEST] Running BMPReader error-handling tests... ";
    BMPReader reader;

    // Test 1: Empty buffer
    try {
        std::vector<uint8_t> empty;
        reader.readFromMemory(empty);
        assert(false && "Should have thrown on empty buffer!");
    } catch (const std::exception&) {
        // Expected
    }

    // Test 2: Invalid magic signature
    try {
        std::vector<uint8_t> fake(100, 0);
        fake[0] = 'X';
        fake[1] = 'Y';
        reader.readFromMemory(fake);
        assert(false && "Should have thrown on invalid signature!");
    } catch (const std::exception&) {
        // Expected
    }

    // Test 3: Truncated header
    try {
        std::vector<uint8_t> truncated(30, 0);
        truncated[0] = 'B';
        truncated[1] = 'M';
        reader.readFromMemory(truncated);
        assert(false && "Should have thrown on truncated header!");
    } catch (const std::exception&) {
        // Expected
    }

    std::cout << "PASSED (All invalid inputs safely rejected!)\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "PixelForge — BMP Engine Verification Suite\n";
    std::cout << "========================================\n";

    // Test all 4 possible row padding offsets:
    // (width * 3) % 4 can be 0, 1, 2, or 3
    testBMPRoundTrip(1, 1, "Single Pixel (3 bytes/row, 1 pad)");
    testBMPRoundTrip(2, 2, "2x2 Tiny Block (6 bytes/row, 2 pad)");
    testBMPRoundTrip(3, 3, "3x3 Tiny Block (9 bytes/row, 3 pad)");
    testBMPRoundTrip(4, 4, "4x4 Aligned Block (12 bytes/row, 0 pad)");
    testBMPRoundTrip(7, 5, "Odd Dimensions (7x5)");
    testBMPRoundTrip(64, 64, "Power of Two Block (64x64)");
    testBMPRoundTrip(137, 89, "Arbitrary Dimensions (137x89)");

    testErrorHandling();
    testRLE8BMPRoundTrip();

    std::cout << "========================================\n";
    std::cout << "ALL BMP ENGINE TESTS PASSED SUCCESSFULLY!\n";
    std::cout << "========================================\n";
    return 0;
}
