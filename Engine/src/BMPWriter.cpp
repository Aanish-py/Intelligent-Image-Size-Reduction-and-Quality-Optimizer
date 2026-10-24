#include "BMPWriter.h"
#include <fstream>
#include <cstring>

namespace pixelforge {

bool BMPWriter::writeToFile(const Image& image, const std::string& filepath) const {
    std::vector<uint8_t> buffer = writeToMemory(image);

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("BMPWriter: Failed to open file for writing: " + filepath);
    }

    if (!file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size())) {
        throw std::runtime_error("BMPWriter: Failed to write data to file: " + filepath);
    }

    return true;
}

std::vector<uint8_t> BMPWriter::writeToMemory(const Image& image) const {
    if (!image.isValid()) {
        throw std::runtime_error("BMPWriter: Cannot serialize an empty or invalid Image.");
    }

    int width = image.getWidth();
    int height = image.getHeight();

    size_t row_stride = computeBMPRowStride(width);
    size_t padding = computeBMPPadding(width);
    size_t pixel_data_size = row_stride * static_cast<size_t>(height);
    size_t total_file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + pixel_data_size;

    std::vector<uint8_t> buffer(total_file_size, 0);

    BMPFileHeader file_header;
    file_header.file_type = 0x4D42; // 'BM'
    file_header.file_size = static_cast<uint32_t>(total_file_size);
    file_header.reserved1 = 0;
    file_header.reserved2 = 0;
    file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

    BMPInfoHeader info_header;
    info_header.header_size = sizeof(BMPInfoHeader);
    info_header.width = width;
    info_header.height = height; // Positive = bottom-up (standard)
    info_header.planes = 1;
    info_header.bit_count = 24;
    info_header.compression = 0; // BI_RGB
    info_header.size_image = static_cast<uint32_t>(pixel_data_size);
    info_header.x_pixels_per_meter = 2835; // ~72 DPI
    info_header.y_pixels_per_meter = 2835;
    info_header.colors_used = 0;
    info_header.colors_important = 0;

    // Copy headers into buffer
    std::memcpy(buffer.data(), &file_header, sizeof(BMPFileHeader));
    std::memcpy(buffer.data() + sizeof(BMPFileHeader), &info_header, sizeof(BMPInfoHeader));

    // Pointer to pixel payload
    uint8_t* pixel_dest = buffer.data() + file_header.offset_data;

    // Write scanlines bottom-up
    for (int r = 0; r < height; ++r) {
        int src_y = height - 1 - r;
        uint8_t* row_dest = pixel_dest + (static_cast<size_t>(r) * row_stride);

        for (int x = 0; x < width; ++x) {
            const Pixel& px = image.at(x, src_y);
            row_dest[x * 3 + 0] = px.b; // Blue
            row_dest[x * 3 + 1] = px.g; // Green
            row_dest[x * 3 + 2] = px.r; // Red
        }

        // Explicitly zero out padding bytes
        for (size_t p = 0; p < padding; ++p) {
            row_dest[width * 3 + p] = 0;
        }
    }

    return buffer;
}

} // namespace pixelforge
