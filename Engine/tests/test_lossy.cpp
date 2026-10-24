#include "Image.h"
#include "BMPReader.h"
#include "LossyEngine.h"
#include "DCT.h"
#include "ColorSpace.h"
#include "Metrics.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace pixelforge;

void testDCTDirect() {
    std::cout << "[TEST] Running mathematical 2D-DCT forward/inverse identity test... ";
    DCT dct;
    Block8x8 original{};

    // Fill with synthetic sample values
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            original[r][c] = (r * 15.0) + (c * 10.0) - 100.0;
        }
    }

    Block8x8 freq = dct.forwardDCT(original);
    Block8x8 reconstructed = dct.inverseDCT(freq);

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double diff = std::abs(original[r][c] - reconstructed[r][c]);
            if (diff > 1e-4) {
                std::cerr << "\n[FAIL] DCT mismatch at (" << r << "," << c << "): diff=" << diff << "\n";
                std::exit(1);
            }
        }
    }
    std::cout << "PASSED (Floating-point error < 1e-4)!\n";
}

void testColorSpaceDirect() {
    std::cout << "[TEST] Running RGB <-> YCbCr round-trip test... ";
    for (int r = 0; r < 256; r += 32) {
        for (int g = 0; g < 256; g += 32) {
            for (int b = 0; b < 256; b += 32) {
                Pixel orig(r, g, b);
                YCbCr ycb = ColorSpace::rgbToYCbCr(orig);
                Pixel recon = ColorSpace::ycbcrToRGB(ycb);

                // Small rounding differences (at most 1) due to integer conversion are standard in BT.601
                assert(std::abs(orig.r - recon.r) <= 1);
                assert(std::abs(orig.g - recon.g) <= 1);
                assert(std::abs(orig.b - recon.b) <= 1);
            }
        }
    }
    std::cout << "PASSED!\n";
}

void testLossyAtQuality(const Image& img, int quality, const std::string& name) {
    (void)name;
    LossyEngine engine;
    LossyStats stats;

    auto pfz = engine.compress(img, quality, &stats);
    Image reconstructed = engine.decompress(pfz, &stats);

    QualityReport report = Metrics::evaluate(img, reconstructed);

    std::cout << "  * Q=" << quality << " | Compressed: " << pfz.size() 
              << " B (" << stats.compression_ratio << "x, " << stats.percentage_reduction 
              << "% red.) | MSE: " << report.mse << " | PSNR: " << report.psnr << " dB" << std::endl;

    assert(reconstructed.getWidth() == img.getWidth());
    assert(reconstructed.getHeight() == img.getHeight());
    assert(report.psnr > 10.0);
}

int main() {
    std::cout << "========================================\n";
    std::cout << "PixelForge — Lossy Engine Test Suite\n";
    std::cout << "========================================\n";

    testDCTDirect();
    testColorSpaceDirect();

    BMPReader reader;
    try {
        Image gradient = reader.readFromFile("samples/input/gradient_256x256.bmp");
        std::cout << "[TEST] Testing Lossy Compression on Smooth Gradient (256x256):\n";
        testLossyAtQuality(gradient, 25, "Gradient");
        testLossyAtQuality(gradient, 50, "Gradient");
        testLossyAtQuality(gradient, 75, "Gradient");
        testLossyAtQuality(gradient, 90, "Gradient");

        Image blocks = reader.readFromFile("samples/input/blocks_256x256.bmp");
        std::cout << "[TEST] Testing Lossy Compression on Blocks Sample (256x256):\n";
        testLossyAtQuality(blocks, 50, "Blocks");
        testLossyAtQuality(blocks, 75, "Blocks");

        // Non-multiples of 8 padding test
        Image odd(137, 89);
        for (int y = 0; y < 89; ++y)
            for (int x = 0; x < 137; ++x)
                odd.at(x, y) = Pixel((x * 2) % 256, (y * 3) % 256, (x + y) % 256);
        std::cout << "[TEST] Testing Lossy Compression on Arbitrary Non-Multiple of 8 (137x89):\n";
        testLossyAtQuality(odd, 75, "Arbitrary Dimensions");

    } catch (const std::exception& e) {
        std::cerr << "Lossy test failure: " << e.what() << "\n";
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "ALL LOSSY ENGINE TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}
