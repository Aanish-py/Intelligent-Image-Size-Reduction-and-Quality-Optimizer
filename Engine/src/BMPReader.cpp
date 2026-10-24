#include "BMPReader.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <sstream>

namespace pixelforge {

Image BMPReader::readFromFile(const std::string& filepath) const {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("BMPReader: Failed to open file for reading: " + filepath);
    }

    std::streamsize file_size = file.tellg();
    if (file_size < static_cast<std::streamsize>(sizeof(BMPFileHeader) + sizeof(BMPInfoHeader))) {
        throw std::runtime_error("BMPReader: File is smaller than minimum BMP header size.");
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        throw std::runtime_error("BMPReader: Error reading file data: " + filepath);
    }

    return decodeBMPStream(buffer.data(), buffer.size());
}

Image BMPReader::readFromMemory(const std::vector<uint8_t>& buffer) const {
    return decodeBMPStream(buffer.data(), buffer.size());
}

Image BMPReader::readFromMemory(const uint8_t* data, size_t size) const {
    return decodeBMPStream(data, size);
}

Image BMPReader::decodeBMPStream(const uint8_t* data, size_t size) const {
    if (!data || size < (sizeof(BMPFileHeader) + sizeof(BMPInfoHeader))) {
        throw std::runtime_error("BMPReader: Input buffer is too small to contain valid BMP headers.");
    }

    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    std::memcpy(&file_header, data, sizeof(BMPFileHeader));
    std::memcpy(&info_header, data + sizeof(BMPFileHeader), sizeof(BMPInfoHeader));

    // Validate Signature: 'BM' (0x4D42)
    if (file_header.file_type != 0x4D42) {
        std::ostringstream oss;
        oss << "BMPReader: Invalid BMP signature (0x" << std::hex << file_header.file_type << "), expected 0x4D42 ('BM').";
        throw std::runtime_error(oss.str());
    }

    int width = info_header.width;
    int raw_height = info_header.height;
    bool is_bottom_up = (raw_height > 0);
    int height = std::abs(raw_height);

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("BMPReader: Invalid image dimensions (" + 
                                std::to_string(width) + "x" + std::to_string(height) + ").");
    }

    // Safe dimension cap to protect against memory exhaustion attacks
    constexpr int MAX_DIMENSION = 16384;
    if (width > MAX_DIMENSION || height > MAX_DIMENSION) {
        throw std::runtime_error("BMPReader: Image dimensions exceed maximum safe limit (" + 
                                std::to_string(MAX_DIMENSION) + "px).");
    }

    size_t data_offset = file_header.offset_data;
    if (data_offset >= size) {
        throw std::runtime_error("BMPReader: Pixel data offset is beyond file boundary.");
    }

    Image image(width, height);

    // =========================================================================
    // Case 1: Standard 24-bit uncompressed RGB (BI_RGB, compression = 0)
    // =========================================================================
    if (info_header.bit_count == 24 && info_header.compression == 0) {
        size_t row_stride = computeBMPRowStride(width);
        size_t expected_pixel_bytes = row_stride * static_cast<size_t>(height);

        if (data_offset + expected_pixel_bytes > size) {
            throw std::runtime_error("BMPReader: Truncated 24-bit BMP file. Required bytes: " + 
                                    std::to_string(data_offset + expected_pixel_bytes));
        }

        const uint8_t* pixel_data_start = data + data_offset;
        for (int r = 0; r < height; ++r) {
            int target_y = is_bottom_up ? (height - 1 - r) : r;
            const uint8_t* row_ptr = pixel_data_start + (static_cast<size_t>(r) * row_stride);

            for (int x = 0; x < width; ++x) {
                uint8_t b = row_ptr[x * 3 + 0];
                uint8_t g = row_ptr[x * 3 + 1];
                uint8_t r_val = row_ptr[x * 3 + 2];
                image.at(x, target_y) = Pixel(r_val, g, b);
            }
        }
        return image;
    }

    // =========================================================================
    // Case 2: 8-bit Run-Length Encoded BMP (BI_RLE8, compression = 1)
    // =========================================================================
    if (info_header.bit_count == 8 && info_header.compression == 1) {
        // Read color palette (256 RGBQUADs)
        size_t palette_offset = sizeof(BMPFileHeader) + info_header.header_size;
        int num_colors = (info_header.colors_used == 0) ? 256 : info_header.colors_used;
        if (num_colors > 256) num_colors = 256;

        if (palette_offset + (static_cast<size_t>(num_colors) * 4) > size) {
            throw std::runtime_error("BMPReader: Truncated palette in 8-bit RLE BMP.");
        }

        Pixel palette[256];
        for (int i = 0; i < num_colors; ++i) {
            size_t entry_offset = palette_offset + (static_cast<size_t>(i) * 4);
            uint8_t b = data[entry_offset + 0];
            uint8_t g = data[entry_offset + 1];
            uint8_t r_val = data[entry_offset + 2];
            palette[i] = Pixel(r_val, g, b);
        }

        // Decode RLE payload
        const uint8_t* p = data + data_offset;
        const uint8_t* p_end = data + size;

        int cur_x = 0;
        int cur_y = is_bottom_up ? (height - 1) : 0;

        while (p + 1 < p_end) {
            uint8_t count = *p++;
            uint8_t value = *p++;

            if (count > 0) {
                // Encoded run: repeat palette[value] 'count' times
                Pixel px = palette[value];
                for (int i = 0; i < count; ++i) {
                    if (cur_x < width && cur_y >= 0 && cur_y < height) {
                        image.at(cur_x, cur_y) = px;
                    }
                    cur_x++;
                }
            } else {
                // Escape code: count == 0
                if (value == 0) {
                    // End of scanline
                    cur_x = 0;
                    if (is_bottom_up) cur_y--; else cur_y++;
                } else if (value == 1) {
                    // End of bitmap
                    break;
                } else if (value == 2) {
                    // Delta offset
                    if (p + 2 > p_end) break;
                    uint8_t dx = *p++;
                    uint8_t dy = *p++;
                    cur_x += dx;
                    if (is_bottom_up) cur_y -= dy; else cur_y += dy;
                } else {
                    // Absolute mode: 'value' literal indices follow
                    int run_len = value;
                    for (int i = 0; i < run_len; ++i) {
                        if (p >= p_end) break;
                        uint8_t idx = *p++;
                        if (cur_x < width && cur_y >= 0 && cur_y < height) {
                            image.at(cur_x, cur_y) = palette[idx];
                        }
                        cur_x++;
                    }
                    // Word-alignment padding: if run_len is odd, skip 1 byte
                    if (run_len % 2 != 0 && p < p_end) {
                        p++;
                    }
                }
            }
        }
        return image;
    }

    // =========================================================================
    // Case 3: 8-bit uncompressed palette BMP (BI_RGB, compression = 0)
    // =========================================================================
    if (info_header.bit_count == 8 && info_header.compression == 0) {
        size_t palette_offset = sizeof(BMPFileHeader) + info_header.header_size;
        int num_colors = (info_header.colors_used == 0) ? 256 : info_header.colors_used;
        if (num_colors > 256) num_colors = 256;

        Pixel palette[256];
        for (int i = 0; i < num_colors; ++i) {
            size_t entry_offset = palette_offset + (static_cast<size_t>(i) * 4);
            uint8_t b = data[entry_offset + 0];
            uint8_t g = data[entry_offset + 1];
            uint8_t r_val = data[entry_offset + 2];
            palette[i] = Pixel(r_val, g, b);
        }

        size_t row_stride = (static_cast<size_t>(width) + 3) & ~3;
        const uint8_t* pixel_data_start = data + data_offset;

        for (int r = 0; r < height; ++r) {
            int target_y = is_bottom_up ? (height - 1 - r) : r;
            const uint8_t* row_ptr = pixel_data_start + (static_cast<size_t>(r) * row_stride);

            for (int x = 0; x < width; ++x) {
                uint8_t idx = row_ptr[x];
                image.at(x, target_y) = palette[idx];
            }
        }
        return image;
    }

    // Unsupported format
    throw std::runtime_error("BMPReader: Unsupported BMP format (bit_count=" + 
                            std::to_string(info_header.bit_count) + 
                            ", compression=" + std::to_string(info_header.compression) + 
                            "). Supported formats: 24-bit RGB, 8-bit RLE (BI_RLE8), and 8-bit palette.");
}

} // namespace pixelforge
