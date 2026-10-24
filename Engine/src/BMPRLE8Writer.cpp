#include "BMPRLE8Writer.h"
#include "BMPCommon.h"
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <queue>
#include <limits>

namespace pixelforge {

namespace {

struct ColorSample {
    uint8_t r, g, b;
};

struct ColorBox {
    int start_idx{0};
    int end_idx{0};
    uint8_t min_r{255}, max_r{0};
    uint8_t min_g{255}, max_g{0};
    uint8_t min_b{255}, max_b{0};

    int longestRange() const noexcept {
        int dr = max_r - min_r;
        int dg = max_g - min_g;
        int db = max_b - min_b;
        return std::max({dr, dg, db});
    }

    int longestAxis() const noexcept {
        int dr = max_r - min_r;
        int dg = max_g - min_g;
        int db = max_b - min_b;
        if (dr >= dg && dr >= db) return 0; // Red
        if (dg >= dr && dg >= db) return 1; // Green
        return 2;                           // Blue
    }
};

void updateBoxBounds(ColorBox& box, const std::vector<ColorSample>& samples) {
    box.min_r = box.min_g = box.min_b = 255;
    box.max_r = box.max_g = box.max_b = 0;
    for (int i = box.start_idx; i < box.end_idx; ++i) {
        const auto& s = samples[i];
        if (s.r < box.min_r) box.min_r = s.r;
        if (s.r > box.max_r) box.max_r = s.r;
        if (s.g < box.min_g) box.min_g = s.g;
        if (s.g > box.max_g) box.max_g = s.g;
        if (s.b < box.min_b) box.min_b = s.b;
        if (s.b > box.max_b) box.max_b = s.b;
    }
}

} // namespace

void BMPRLE8Writer::buildAdaptivePalette(const Image& image, RGBQuad palette[256], int& active_colors) const {
    int width = image.getWidth();
    int height = image.getHeight();
    int total_pixels = width * height;

    // Subsample up to 60,000 pixels uniformly to determine optimal color palette
    std::vector<ColorSample> samples;
    samples.reserve(60000);
    int step = std::max(1, total_pixels / 60000);
    for (int i = 0; i < total_pixels; i += step) {
        int x = i % width;
        int y = i / width;
        const Pixel& px = image.at(x, y);
        samples.push_back({px.r, px.g, px.b});
    }

    if (samples.empty()) {
        active_colors = 256;
        for (int i = 0; i < 256; ++i) palette[i] = {0, 0, 0, 0};
        return;
    }

    auto cmp = [](const ColorBox& a, const ColorBox& b) {
        return a.longestRange() < b.longestRange();
    };
    std::priority_queue<ColorBox, std::vector<ColorBox>, decltype(cmp)> pq(cmp);

    ColorBox root{0, static_cast<int>(samples.size()), 255, 0, 255, 0, 255, 0};
    updateBoxBounds(root, samples);
    pq.push(root);

    std::vector<ColorBox> final_boxes;
    final_boxes.reserve(256);

    while (!pq.empty() && (pq.size() + final_boxes.size()) < 256) {
        ColorBox box = pq.top();
        pq.pop();

        if (box.longestRange() == 0 || (box.end_idx - box.start_idx) <= 1) {
            final_boxes.push_back(box);
            continue;
        }

        int axis = box.longestAxis();
        if (axis == 0) {
            std::sort(samples.begin() + box.start_idx, samples.begin() + box.end_idx,
                      [](const ColorSample& a, const ColorSample& b) { return a.r < b.r; });
        } else if (axis == 1) {
            std::sort(samples.begin() + box.start_idx, samples.begin() + box.end_idx,
                      [](const ColorSample& a, const ColorSample& b) { return a.g < b.g; });
        } else {
            std::sort(samples.begin() + box.start_idx, samples.begin() + box.end_idx,
                      [](const ColorSample& a, const ColorSample& b) { return a.b < b.b; });
        }

        int mid = box.start_idx + (box.end_idx - box.start_idx) / 2;
        ColorBox box1{box.start_idx, mid, 255, 0, 255, 0, 255, 0};
        ColorBox box2{mid, box.end_idx, 255, 0, 255, 0, 255, 0};
        updateBoxBounds(box1, samples);
        updateBoxBounds(box2, samples);

        pq.push(box1);
        pq.push(box2);
    }

    while (!pq.empty()) {
        final_boxes.push_back(pq.top());
        pq.pop();
    }

    active_colors = std::min<int>(256, static_cast<int>(final_boxes.size()));
    for (int b = 0; b < 256; ++b) {
        if (b < active_colors && final_boxes[b].end_idx > final_boxes[b].start_idx) {
            const auto& box = final_boxes[b];
            uint64_t sum_r = 0, sum_g = 0, sum_b = 0;
            int count = box.end_idx - box.start_idx;
            for (int i = box.start_idx; i < box.end_idx; ++i) {
                sum_r += samples[i].r;
                sum_g += samples[i].g;
                sum_b += samples[i].b;
            }
            palette[b].r = static_cast<uint8_t>(sum_r / count);
            palette[b].g = static_cast<uint8_t>(sum_g / count);
            palette[b].b = static_cast<uint8_t>(sum_b / count);
            palette[b].reserved = 0;
        } else {
            palette[b] = {0, 0, 0, 0};
        }
    }
}

std::vector<uint8_t> BMPRLE8Writer::writeToMemory(const Image& image) const {
    if (!image.isValid()) {
        throw std::invalid_argument("BMPRLE8Writer: Cannot serialize an empty image.");
    }

    int width = image.getWidth();
    int height = image.getHeight();

    // 1. Build Adaptive Palette using Median Cut Quantization
    RGBQuad palette[256];
    int active_colors = 256;
    buildAdaptivePalette(image, palette, active_colors);

    // 2. Initialize 5:5:5 fast lookup cache (32 KB on stack)
    int cache[32][32][32];
    std::memset(cache, -1, sizeof(cache));

    auto matchIndex = [&](const Pixel& px) noexcept -> uint8_t {
        int cr = px.r >> 3;
        int cg = px.g >> 3;
        int cb = px.b >> 3;
        int cached = cache[cr][cg][cb];
        if (cached != -1) return static_cast<uint8_t>(cached);

        int best_idx = 0;
        int min_dist = std::numeric_limits<int>::max();
        for (int i = 0; i < active_colors; ++i) {
            int dr = static_cast<int>(px.r) - palette[i].r;
            int dg = static_cast<int>(px.g) - palette[i].g;
            int db = static_cast<int>(px.b) - palette[i].b;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < min_dist) {
                min_dist = dist;
                best_idx = i;
                if (dist == 0) break;
            }
        }
        cache[cr][cg][cb] = best_idx;
        return static_cast<uint8_t>(best_idx);
    };

    // 3. Encode scanlines bottom-up (standard Windows BMP order)
    std::vector<uint8_t> rle_payload;
    rle_payload.reserve(static_cast<size_t>(width) * height);

    for (int r = 0; r < height; ++r) {
        int src_y = height - 1 - r;
        int x = 0;

        while (x < width) {
            uint8_t current_idx = matchIndex(image.at(x, src_y));
            int run = 1;

            while (x + run < width && run < 255 && matchIndex(image.at(x + run, src_y)) == current_idx) {
                run++;
            }

            rle_payload.push_back(static_cast<uint8_t>(run));
            rle_payload.push_back(current_idx);
            x += run;
        }

        // End of scanline marker: 0x00, 0x00
        rle_payload.push_back(0);
        rle_payload.push_back(0);
    }

    // End of bitmap marker: 0x00, 0x01
    rle_payload.push_back(0);
    rle_payload.push_back(1);

    size_t header_plus_palette_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + (256 * sizeof(RGBQuad));
    size_t total_file_size = header_plus_palette_size + rle_payload.size();

    BMPFileHeader file_header;
    file_header.file_type = 0x4D42; // 'BM'
    file_header.file_size = static_cast<uint32_t>(total_file_size);
    file_header.reserved1 = 0;
    file_header.reserved2 = 0;
    file_header.offset_data = static_cast<uint32_t>(header_plus_palette_size);

    BMPInfoHeader info_header;
    info_header.header_size = sizeof(BMPInfoHeader);
    info_header.width = width;
    info_header.height = height; // Positive = bottom-up
    info_header.planes = 1;
    info_header.bit_count = 8;   // 8-bit palette
    info_header.compression = 1; // 1 = BI_RLE8 (Run-Length Encoded)
    info_header.size_image = static_cast<uint32_t>(rle_payload.size());
    info_header.x_pixels_per_meter = 2835;
    info_header.y_pixels_per_meter = 2835;
    info_header.colors_used = 256;
    info_header.colors_important = 256;

    std::vector<uint8_t> buffer(total_file_size);
    std::memcpy(buffer.data(), &file_header, sizeof(BMPFileHeader));
    std::memcpy(buffer.data() + sizeof(BMPFileHeader), &info_header, sizeof(BMPInfoHeader));
    std::memcpy(buffer.data() + sizeof(BMPFileHeader) + sizeof(BMPInfoHeader), palette, 256 * sizeof(RGBQuad));
    std::memcpy(buffer.data() + header_plus_palette_size, rle_payload.data(), rle_payload.size());

    return buffer;
}

bool BMPRLE8Writer::writeToFile(const Image& image, const std::string& filepath) const {
    auto buffer = writeToMemory(image);
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return true;
}

} // namespace pixelforge