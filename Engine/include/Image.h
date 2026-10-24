#pragma once

#include "Pixel.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace pixelforge {

/**
 * @brief Core Image class encapsulating an uncompressed 24-bit 2D image matrix.
 * Implements RAII memory management, bounds-checked pixel access, channel extraction,
 * and essential image processing transformations.
 */
class Image {
public:
    // Constructors & Destructor
    Image();
    Image(int width, int height);
    Image(int width, int height, const Pixel& fill_color);
    ~Image() = default;

    // Copy and Move Semantics
    Image(const Image&) = default;
    Image& operator=(const Image&) = default;
    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    // Dimension & Capacity Properties
    int getWidth() const noexcept { return width_; }
    int getHeight() const noexcept { return height_; }
    bool isValid() const noexcept { return width_ > 0 && height_ > 0 && !pixels_.empty(); }
    size_t getPixelCount() const noexcept { return pixels_.size(); }
    size_t getByteSize() const noexcept { return pixels_.size() * sizeof(Pixel); }

    // Memory Allocation & Reinitialization
    void resize(int width, int height);
    void clear() noexcept;
    void fill(const Pixel& color);

    // Bounds-checked pixel access (throws std::out_of_range on invalid coordinates)
    Pixel getPixel(int x, int y) const;
    void setPixel(int x, int y, const Pixel& pixel);

    // High-performance unchecked access for performance-critical inner loops (e.g. DCT, prediction)
    inline Pixel& at(int x, int y) noexcept {
        return pixels_[static_cast<size_t>(y) * width_ + x];
    }

    inline const Pixel& at(int x, int y) const noexcept {
        return pixels_[static_cast<size_t>(y) * width_ + x];
    }

    // Direct access to underlying contiguous pixel buffer
    const std::vector<Pixel>& getPixels() const noexcept { return pixels_; }
    std::vector<Pixel>& getPixels() noexcept { return pixels_; }

    // Color Channel Operations (0 = Red, 1 = Green, 2 = Blue)
    std::vector<uint8_t> getChannelPlane(int channel_index) const;
    void setChannelPlane(int channel_index, const std::vector<uint8_t>& plane_data);

    // Image Transformations
    Image crop(int start_x, int start_y, int crop_width, int crop_height) const;
    void flipVertical();
    void flipHorizontal();

    // Raw Buffer Conversions for Serialization & I/O
    std::vector<uint8_t> toRawRGB() const;
    std::vector<uint8_t> toRawBGR() const;
    static Image fromRawRGB(int width, int height, const std::vector<uint8_t>& raw);
    static Image fromRawBGR(int width, int height, const std::vector<uint8_t>& raw);

private:
    int width_{0};
    int height_{0};
    std::vector<Pixel> pixels_;

    inline void validateBounds(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            throw std::out_of_range("Image: Pixel coordinates (" + std::to_string(x) + ", " + 
                                    std::to_string(y) + ") out of bounds (" + 
                                    std::to_string(width_) + "x" + std::to_string(height_) + ").");
        }
    }
};

} // namespace pixelforge
