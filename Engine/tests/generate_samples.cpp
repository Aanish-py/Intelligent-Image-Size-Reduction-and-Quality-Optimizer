#include "Image.h"
#include "BMPWriter.h"
#include <iostream>
#include <cmath>

using namespace pixelforge;

int main() {
    BMPWriter writer;

    // Sample 1: Geometric Color Blocks (256x256) - Ideal for RLE and Lossless testing
    {
        Image img(256, 256);
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                if (x < 128 && y < 128) {
                    img.at(x, y) = Pixel(255, 0, 0); // Red quadrant
                } else if (x >= 128 && y < 128) {
                    img.at(x, y) = Pixel(0, 255, 0); // Green quadrant
                } else if (x < 128 && y >= 128) {
                    img.at(x, y) = Pixel(0, 0, 255); // Blue quadrant
                } else {
                    img.at(x, y) = Pixel(255, 255, 0); // Yellow quadrant
                }
            }
        }
        writer.writeToFile(img, "samples/input/blocks_256x256.bmp");
        std::cout << "[GENERATED] samples/input/blocks_256x256.bmp\n";
    }

    // Sample 2: Smooth Radial Color Gradient (256x256) - Ideal for DCT / Lossy testing
    {
        Image img(256, 256);
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                double dx = x - 128.0;
                double dy = y - 128.0;
                double dist = std::sqrt(dx * dx + dy * dy);
                uint8_t r = Pixel::clamp(static_cast<int>(std::sin(dist * 0.05) * 127 + 128));
                uint8_t g = Pixel::clamp(static_cast<int>(std::cos(dist * 0.05) * 127 + 128));
                uint8_t b = Pixel::clamp(static_cast<int>((x * 255) / 255));
                img.at(x, y) = Pixel(r, g, b);
            }
        }
        writer.writeToFile(img, "samples/input/gradient_256x256.bmp");
        std::cout << "[GENERATED] samples/input/gradient_256x256.bmp\n";
    }

    // Sample 3: Checkerboard pattern (64x64) - Tests high frequency details
    {
        Image img(64, 64);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                bool is_white = ((x / 8) + (y / 8)) % 2 == 0;
                img.at(x, y) = is_white ? Pixel(255, 255, 255) : Pixel(0, 0, 0);
            }
        }
        writer.writeToFile(img, "samples/input/checker_64x64.bmp");
        std::cout << "[GENERATED] samples/input/checker_64x64.bmp\n";
    }

    return 0;
}
