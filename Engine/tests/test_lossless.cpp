#include "Image.h"
#include "BMPReader.h"
#include "LosslessEngine.h"
#include "RLEEngine.h"
#include "HuffmanTree.h"
#include <iostream>
#include <cassert>
#include <random>

using namespace pixelforge;

void testRLEDirect() {
    std::cout << "[TEST] Running direct RLE tests... ";
    RLEEngine rle;

    // Test 1: Empty input
    assert(rle.encode({}).empty());
    assert(rle.decode({}).empty());

    // Test 2: Long run of 500 identical bytes (exceeds single chunk limit of 130)
    std::vector<uint8_t> long_run(500, 0x42);
    auto compressed_run = rle.encode(long_run);
    assert(compressed_run.size() < long_run.size()); // Major compression
    auto decompressed_run = rle.decode(compressed_run);
    assert(decompressed_run == long_run);

    // Test 3: Alternating non-repeating literals
    std::vector<uint8_t> literals;
    for (int i = 0; i < 200; ++i) literals.push_back(static_cast<uint8_t>(i % 256));
    auto compressed_lit = rle.encode(literals);
    auto decompressed_lit = rle.decode(compressed_lit);
    assert(decompressed_lit == literals);

    std::cout << "PASSED!\n";
}

void testHuffmanDirect() {
    std::cout << "[TEST] Running direct Huffman tests... ";
    // Single symbol edge case
    std::vector<uint8_t> single_symbol(100, 0xAA);
    std::array<uint64_t, 256> freq{};
    for (uint8_t b : single_symbol) freq[b]++;

    HuffmanTree tree;
    tree.buildFromFrequencies(freq);
    BitWriter writer;
    tree.encodeData(single_symbol.data(), single_symbol.size(), writer);

    BitReader reader(writer.getBuffer());
    auto decoded = HuffmanTree::decodeData(reader);
    assert(decoded == single_symbol);

    std::cout << "PASSED!\n";
}

void testLosslessRoundTrip(const Image& original, const std::string& name) {
    std::cout << "[TEST] Lossless round-trip: " << name 
              << " (" << original.getWidth() << "x" << original.getHeight() << ")... ";

    LosslessEngine engine;
    LosslessStats stats;

    auto pfl_data = engine.compress(original, PredictorType::Paeth, true, &stats);
    Image reconstructed = engine.decompress(pfl_data, &stats);

    assert(reconstructed.getWidth() == original.getWidth());
    assert(reconstructed.getHeight() == original.getHeight());
    assert(stats.verified_exact == true);

    for (int y = 0; y < original.getHeight(); ++y) {
        for (int x = 0; x < original.getWidth(); ++x) {
            Pixel orig = original.at(x, y);
            Pixel recon = reconstructed.at(x, y);
            if (orig != recon) {
                std::cerr << "\n[FAIL] Pixel mismatch at (" << x << ", " << y << ") in " << name << "\n";
                std::exit(1);
            }
        }
    }

    std::cout << "PASSED! (Original: " << stats.original_bytes 
              << " B -> PFL: " << stats.compressed_bytes 
              << " B, Ratio: " << stats.compression_ratio << "x, Reduction: " 
              << stats.percentage_reduction << "%)\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "PixelForge — Lossless Engine Test Suite\n";
    std::cout << "========================================\n";

    testRLEDirect();
    testHuffmanDirect();

    // 1. Single Pixel
    {
        Image img(1, 1, Pixel(120, 200, 50));
        testLosslessRoundTrip(img, "1x1 Single Pixel");
    }

    // 2. Solid Color Image (Single Symbol Edge Case)
    {
        Image img(64, 64, Pixel(255, 255, 255));
        testLosslessRoundTrip(img, "64x64 Solid White");
    }

    // 3. Odd Dimensions
    {
        Image img(7, 5);
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 7; ++x)
                img.at(x, y) = Pixel(x * 30, y * 40, (x + y) * 20);
        testLosslessRoundTrip(img, "7x5 Odd Dimensions");
    }

    // 4. Test on generated sample files from disk
    BMPReader reader;
    try {
        Image blocks = reader.readFromFile("samples/input/blocks_256x256.bmp");
        testLosslessRoundTrip(blocks, "Blocks Sample (Flat Regions)");

        Image gradient = reader.readFromFile("samples/input/gradient_256x256.bmp");
        testLosslessRoundTrip(gradient, "Gradient Sample (Smooth Variations)");

        Image checker = reader.readFromFile("samples/input/checker_64x64.bmp");
        testLosslessRoundTrip(checker, "Checkerboard Sample (High Frequency)");
    } catch (const std::exception& e) {
        std::cerr << "Sample file test error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "ALL LOSSLESS ENGINE TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}
