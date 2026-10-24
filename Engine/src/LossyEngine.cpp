#include "LossyEngine.h"
#include <chrono>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <iostream>

namespace pixelforge {

#pragma pack(push, 1)
struct PFZHeader {
    char     magic[4]{'P', 'F', 'Z', '1'}; // "PFZ1"
    uint32_t width{0};
    uint32_t height{0};
    uint8_t  quality{75};
    uint32_t payload_bytes{0}; // Size of Huffman-encoded bitstream
};
#pragma pack(pop)

static_assert(sizeof(PFZHeader) == 17, "PFZHeader must be exactly 17 bytes");

std::vector<uint8_t> LossyEngine::encodeCoefficients(const std::vector<ZigzagArray>& blocks) const {
    std::vector<uint8_t> stream;
    stream.reserve(blocks.size() * 16);

    int16_t prev_dc = 0;

    for (const auto& zz : blocks) {
        // 1. Differential DC encoding (DPCM)
        int16_t curr_dc = zz[0];
        int16_t diff_dc = curr_dc - prev_dc;
        prev_dc = curr_dc;

        stream.push_back(static_cast<uint8_t>((diff_dc >> 8) & 0xFF));
        stream.push_back(static_cast<uint8_t>(diff_dc & 0xFF));

        // 2. AC coefficient zero-run encoding
        int k = 1;
        while (k < 64) {
            // Check if all remaining AC coefficients are zero (End-of-Block)
            bool all_zeros = true;
            for (int j = k; j < 64; ++j) {
                if (zz[j] != 0) {
                    all_zeros = false;
                    break;
                }
            }

            if (all_zeros) {
                stream.push_back(0xFF); // EOB Marker
                break;
            }

            // Count leading zeros
            uint8_t zeros = 0;
            while (k < 64 && zz[k] == 0) {
                zeros++;
                k++;
            }

            if (k < 64) {
                int16_t val = zz[k];
                stream.push_back(zeros);
                stream.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
                stream.push_back(static_cast<uint8_t>(val & 0xFF));
                k++;
            }
        }
    }

    return stream;
}

std::vector<ZigzagArray> LossyEngine::decodeCoefficients(const std::vector<uint8_t>& stream, size_t num_blocks) const {
    std::vector<ZigzagArray> blocks(num_blocks);
    size_t i = 0;
    size_t stream_size = stream.size();
    size_t blocks_per_channel = num_blocks / 3;
    int16_t prev_dc = 0;

    for (size_t b = 0; b < num_blocks; ++b) {
        // Reset differential DC predictor at the boundary of each color channel
        if (blocks_per_channel > 0 && b % blocks_per_channel == 0) {
            prev_dc = 0;
        }

        if (i + 2 > stream_size) {
            throw std::runtime_error("LossyEngine::decodeCoefficients: Truncated DC stream.");
        }

        // 1. Decode DC term
        int16_t diff_dc = static_cast<int16_t>((static_cast<uint16_t>(stream[i]) << 8) | static_cast<uint16_t>(stream[i + 1]));
        i += 2;
        int16_t curr_dc = prev_dc + diff_dc;
        prev_dc = curr_dc;

        ZigzagArray& zz = blocks[b];
        zz.fill(0);
        zz[0] = curr_dc;

        // 2. Decode AC terms
        int k = 1;
        while (k < 64 && i < stream_size) {
            uint8_t marker = stream[i++];
            if (marker == 0xFF) {
                // EOB: Remaining coefficients stay zero
                break;
            }

            uint8_t zeros = marker;
            k += zeros;
            if (k >= 64 || i + 2 > stream_size) {
                break;
            }

            int16_t val = static_cast<int16_t>((static_cast<uint16_t>(stream[i]) << 8) | static_cast<uint16_t>(stream[i + 1]));
            i += 2;
            zz[k++] = val;
        }
    }

    return blocks;
}

std::vector<uint8_t> LossyEngine::compress(const Image& image, 
                                          int quality,
                                          LossyStats* out_stats) const {
    if (!image.isValid()) {
        throw std::invalid_argument("LossyEngine::compress: Cannot compress an empty image.");
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    quality = std::clamp(quality, 10, 100);

    int orig_w = image.getWidth();
    int orig_h = image.getHeight();

    // 1. Pad dimensions to multiple of 8
    int pad_w = ((orig_w + 7) / 8) * 8;
    int pad_h = ((orig_h + 7) / 8) * 8;
    int blocks_x = pad_w / 8;
    int blocks_y = pad_h / 8;
    size_t total_blocks = static_cast<size_t>(blocks_x) * blocks_y;

    // 2. Convert RGB Image to YCbCr planar buffers with edge clamping
    std::vector<double> y_plane(pad_w * pad_h);
    std::vector<double> cb_plane(pad_w * pad_h);
    std::vector<double> cr_plane(pad_w * pad_h);

    for (int y = 0; y < pad_h; ++y) {
        int src_y = std::min(y, orig_h - 1);
        for (int x = 0; x < pad_w; ++x) {
            int src_x = std::min(x, orig_w - 1);
            YCbCr ycb = ColorSpace::rgbToYCbCr(image.at(src_x, src_y));
            size_t idx = static_cast<size_t>(y) * pad_w + x;
            y_plane[idx] = ycb.y;
            cb_plane[idx] = ycb.cb;
            cr_plane[idx] = ycb.cr;
        }
    }

    // 3. Process 8x8 blocks for each channel: Level Shift (-128) -> DCT -> Quantize -> Zigzag
    auto processPlane = [&](const std::vector<double>& plane, bool is_chroma) {
        std::vector<ZigzagArray> zz_blocks(total_blocks);
        for (int by = 0; by < blocks_y; ++by) {
            for (int bx = 0; bx < blocks_x; ++bx) {
                Block8x8 block{};
                for (int r = 0; r < 8; ++r) {
                    for (int c = 0; c < 8; ++c) {
                        size_t px_idx = static_cast<size_t>(by * 8 + r) * pad_w + (bx * 8 + c);
                        block[r][c] = plane[px_idx] - 128.0; // Level shift
                    }
                }

                Block8x8 dct_coeff = dct_.forwardDCT(block);
                QuantBlock8x8 q_coeff = dct_.quantize(dct_coeff, quality, is_chroma);
                size_t b_idx = static_cast<size_t>(by) * blocks_x + bx;
                zz_blocks[b_idx] = DCT::toZigzag(q_coeff);
            }
        }
        return zz_blocks;
    };

    auto y_zz  = processPlane(y_plane, false);
    auto cb_zz = processPlane(cb_plane, true);
    auto cr_zz = processPlane(cr_plane, true);

    // 4. Zero-Run Encode coefficients for all channels
    std::vector<uint8_t> coeff_stream;
    auto y_encoded = encodeCoefficients(y_zz);
    auto cb_encoded = encodeCoefficients(cb_zz);
    auto cr_encoded = encodeCoefficients(cr_zz);

    coeff_stream.insert(coeff_stream.end(), y_encoded.begin(), y_encoded.end());
    coeff_stream.insert(coeff_stream.end(), cb_encoded.begin(), cb_encoded.end());
    coeff_stream.insert(coeff_stream.end(), cr_encoded.begin(), cr_encoded.end());

    // 5. Huffman Entropy Encoding
    std::array<uint64_t, 256> frequencies{};
    for (uint8_t byte : coeff_stream) {
        frequencies[byte]++;
    }

    HuffmanTree tree;
    tree.buildFromFrequencies(frequencies);

    BitWriter bit_writer;
    tree.encodeData(coeff_stream.data(), coeff_stream.size(), bit_writer);
    const auto& bitstream = bit_writer.getBuffer();

    // 6. Assemble PFZ Container
    PFZHeader header;
    header.width = static_cast<uint32_t>(orig_w);
    header.height = static_cast<uint32_t>(orig_h);
    header.quality = static_cast<uint8_t>(quality);
    header.payload_bytes = static_cast<uint32_t>(bitstream.size());

    std::vector<uint8_t> pfz_file(sizeof(PFZHeader) + bitstream.size());
    std::memcpy(pfz_file.data(), &header, sizeof(PFZHeader));
    std::memcpy(pfz_file.data() + sizeof(PFZHeader), bitstream.data(), bitstream.size());

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    size_t orig_bytes = static_cast<size_t>(orig_w) * orig_h * 3;

    if (out_stats) {
        out_stats->original_bytes = orig_bytes;
        out_stats->compressed_bytes = pfz_file.size();
        out_stats->compression_ratio = static_cast<double>(orig_bytes) / pfz_file.size();
        out_stats->percentage_reduction = (1.0 - static_cast<double>(pfz_file.size()) / orig_bytes) * 100.0;
        out_stats->compression_time_ms = elapsed_ms;
        out_stats->quality = quality;
    }

    return pfz_file;
}

Image LossyEngine::decompress(const std::vector<uint8_t>& pfz_data, LossyStats* out_stats) const {
    if (pfz_data.size() < sizeof(PFZHeader)) {
        throw std::runtime_error("LossyEngine::decompress: Data is smaller than PFZHeader.");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    PFZHeader header;
    std::memcpy(&header, pfz_data.data(), sizeof(PFZHeader));

    if (std::memcmp(header.magic, "PFZ1", 4) != 0) {
        throw std::runtime_error("LossyEngine::decompress: Invalid magic header (expected 'PFZ1').");
    }

    if (pfz_data.size() < sizeof(PFZHeader) + header.payload_bytes) {
        throw std::runtime_error("LossyEngine::decompress: Truncated .pfz payload.");
    }

    int width = static_cast<int>(header.width);
    int height = static_cast<int>(header.height);
    int quality = header.quality;

    int pad_w = ((width + 7) / 8) * 8;
    int pad_h = ((height + 7) / 8) * 8;
    int blocks_x = pad_w / 8;
    int blocks_y = pad_h / 8;
    size_t total_blocks = static_cast<size_t>(blocks_x) * blocks_y;

    // 1. BitReader on the Huffman stream
    const uint8_t* payload_ptr = pfz_data.data() + sizeof(PFZHeader);
    BitReader reader(payload_ptr, header.payload_bytes);

    // 2. Decode Huffman Entropy Stream
    std::vector<uint8_t> coeff_stream = HuffmanTree::decodeData(reader);

    // 3. Decode Coefficients into Zigzag blocks for Y, Cb, Cr
    // All 3 channels share total_blocks each
    auto all_blocks = decodeCoefficients(coeff_stream, total_blocks * 3);
    if (all_blocks.size() < total_blocks * 3) {
        throw std::runtime_error("LossyEngine::decompress: Insufficient blocks decoded from stream.");
    }

    std::vector<double> y_plane(pad_w * pad_h);
    std::vector<double> cb_plane(pad_w * pad_h);
    std::vector<double> cr_plane(pad_w * pad_h);

    auto reconstructPlane = [&](const std::vector<ZigzagArray>& blocks, size_t block_offset, 
                                 std::vector<double>& plane, bool is_chroma) {
        for (int by = 0; by < blocks_y; ++by) {
            for (int bx = 0; bx < blocks_x; ++bx) {
                size_t b_idx = block_offset + static_cast<size_t>(by) * blocks_x + bx;
                QuantBlock8x8 q_coeff = DCT::fromZigzag(blocks[b_idx]);
                Block8x8 dct_coeff = dct_.dequantize(q_coeff, quality, is_chroma);
                Block8x8 spatial = dct_.inverseDCT(dct_coeff);

                for (int r = 0; r < 8; ++r) {
                    for (int c = 0; c < 8; ++c) {
                        size_t px_idx = static_cast<size_t>(by * 8 + r) * pad_w + (bx * 8 + c);
                        plane[px_idx] = spatial[r][c] + 128.0; // Invert level shift
                    }
                }
            }
        }
    };

    reconstructPlane(all_blocks, 0, y_plane, false);
    reconstructPlane(all_blocks, total_blocks, cb_plane, true);
    reconstructPlane(all_blocks, total_blocks * 2, cr_plane, true);

    // 4. Convert YCbCr back to RGB and crop to original image dimensions
    Image reconstructed(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * pad_w + x;
            YCbCr ycb(y_plane[idx], cb_plane[idx], cr_plane[idx]);
            reconstructed.at(x, y) = ColorSpace::ycbcrToRGB(ycb);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (out_stats) {
        out_stats->original_bytes = static_cast<size_t>(width) * height * 3;
        out_stats->compressed_bytes = pfz_data.size();
        out_stats->decompression_time_ms = elapsed_ms;
        out_stats->quality = quality;
    }

    return reconstructed;
}

bool LossyEngine::saveToFile(const Image& image, const std::string& filepath, int quality) const {
    auto data = compress(image, quality);
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

Image LossyEngine::loadFromFile(const std::string& filepath) const {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("LossyEngine: Failed to open file: " + filepath);
    }
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return decompress(buffer);
}

} // namespace pixelforge
