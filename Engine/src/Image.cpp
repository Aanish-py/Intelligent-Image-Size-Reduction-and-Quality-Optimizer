#include "Image.h"
#include <algorithm>

namespace pixelforge {

Image::Image() : width_(0), height_(0) {}

Image::Image(int width, int height) : width_(width), height_(height) {
    if (width > 0 && height > 0) {
        pixels_.resize(static_cast<size_t>(width) * height);
    }
}

Image::Image(int width, int height, const Pixel& fill_color)
    : width_(width), height_(height) {
    if (width > 0 && height > 0) {
        pixels_.resize(static_cast<size_t>(width) * height, fill_color);
    }
}

void Image::resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image::resize: Dimensions must be positive non-zero integers.");
    }
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<size_t>(width) * height, Pixel{});
}

void Image::clear() noexcept {
    width_ = 0;
    height_ = 0;
    pixels_.clear();
}

void Image::fill(const Pixel& color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

Pixel Image::getPixel(int x, int y) const {
    validateBounds(x, y);
    return pixels_[static_cast<size_t>(y) * width_ + x];
}

void Image::setPixel(int x, int y, const Pixel& pixel) {
    validateBounds(x, y);
    pixels_[static_cast<size_t>(y) * width_ + x] = pixel;
}

std::vector<uint8_t> Image::getChannelPlane(int channel_index) const {
    if (channel_index < 0 || channel_index > 2) {
        throw std::invalid_argument("Image::getChannelPlane: Channel index must be 0 (R), 1 (G), or 2 (B).");
    }

    std::vector<uint8_t> plane(pixels_.size());
    for (size_t i = 0; i < pixels_.size(); ++i) {
        if (channel_index == 0) plane[i] = pixels_[i].r;
        else if (channel_index == 1) plane[i] = pixels_[i].g;
        else plane[i] = pixels_[i].b;
    }
    return plane;
}

void Image::setChannelPlane(int channel_index, const std::vector<uint8_t>& plane_data) {
    if (channel_index < 0 || channel_index > 2) {
        throw std::invalid_argument("Image::setChannelPlane: Channel index must be 0 (R), 1 (G), or 2 (B).");
    }
    if (plane_data.size() < pixels_.size()) {
        throw std::invalid_argument("Image::setChannelPlane: Provided plane data buffer is smaller than image pixel count.");
    }

    for (size_t i = 0; i < pixels_.size(); ++i) {
        if (channel_index == 0) pixels_[i].r = plane_data[i];
        else if (channel_index == 1) pixels_[i].g = plane_data[i];
        else pixels_[i].b = plane_data[i];
    }
}

Image Image::crop(int start_x, int start_y, int crop_width, int crop_height) const {
    if (crop_width <= 0 || crop_height <= 0) {
        throw std::invalid_argument("Image::crop: Crop dimensions must be strictly positive.");
    }
    if (start_x < 0 || start_y < 0 || 
        start_x + crop_width > width_ || 
        start_y + crop_height > height_) {
        throw std::out_of_range("Image::crop: Crop rectangle exceeds image boundaries.");
    }

    Image cropped(crop_width, crop_height);
    for (int y = 0; y < crop_height; ++y) {
        for (int x = 0; x < crop_width; ++x) {
            cropped.at(x, y) = this->at(start_x + x, start_y + y);
        }
    }
    return cropped;
}

void Image::flipVertical() {
    for (int y = 0; y < height_ / 2; ++y) {
        int opp_y = height_ - 1 - y;
        for (int x = 0; x < width_; ++x) {
            std::swap(this->at(x, y), this->at(x, opp_y));
        }
    }
}

void Image::flipHorizontal() {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_ / 2; ++x) {
            int opp_x = width_ - 1 - x;
            std::swap(this->at(x, y), this->at(opp_x, y));
        }
    }
}

std::vector<uint8_t> Image::toRawRGB() const {
    std::vector<uint8_t> buffer(pixels_.size() * 3);
    for (size_t i = 0; i < pixels_.size(); ++i) {
        buffer[i * 3 + 0] = pixels_[i].r;
        buffer[i * 3 + 1] = pixels_[i].g;
        buffer[i * 3 + 2] = pixels_[i].b;
    }
    return buffer;
}

std::vector<uint8_t> Image::toRawBGR() const {
    std::vector<uint8_t> buffer(pixels_.size() * 3);
    for (size_t i = 0; i < pixels_.size(); ++i) {
        buffer[i * 3 + 0] = pixels_[i].b;
        buffer[i * 3 + 1] = pixels_[i].g;
        buffer[i * 3 + 2] = pixels_[i].r;
    }
    return buffer;
}

Image Image::fromRawRGB(int width, int height, const std::vector<uint8_t>& raw) {
    if (raw.size() < static_cast<size_t>(width) * height * 3) {
        throw std::invalid_argument("Image::fromRawRGB: Raw buffer too small for specified image dimensions.");
    }
    Image img(width, height);
    for (size_t i = 0; i < img.pixels_.size(); ++i) {
        img.pixels_[i].r = raw[i * 3 + 0];
        img.pixels_[i].g = raw[i * 3 + 1];
        img.pixels_[i].b = raw[i * 3 + 2];
    }
    return img;
}

Image Image::fromRawBGR(int width, int height, const std::vector<uint8_t>& raw) {
    if (raw.size() < static_cast<size_t>(width) * height * 3) {
        throw std::invalid_argument("Image::fromRawBGR: Raw buffer too small for specified image dimensions.");
    }
    Image img(width, height);
    for (size_t i = 0; i < img.pixels_.size(); ++i) {
        img.pixels_[i].b = raw[i * 3 + 0];
        img.pixels_[i].g = raw[i * 3 + 1];
        img.pixels_[i].r = raw[i * 3 + 2];
    }
    return img;
}

} // namespace pixelforge
