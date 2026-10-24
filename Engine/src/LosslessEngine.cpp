#include "LosslessEngine.h"
#include <chrono>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace pixelforge {

#pragma pack(push, 1)
struct PFLHeader {
    char     magic[4]{'P', 'F', 'L', '1'}; // "PFL1"
    uint32_t width{0};
    uint32_t height{0};
    uint8_t  predictor_type{0}; // 0=None, 1=Left, 2=Up, 3=Average, 4=Paeth
    uint8_t  rle_flag{0};       // 1 if RLE applied, 0 if bypassed
    uint32_t uncompressed_size{0};
    uint32_t payload_bytes{0};  // Size of the bitstream payload
    uint32_t checksum{0};       // Adler-32 checksum of original raw RGB bytes
};
#pragma pack(pop)

static_assert(sizeof(PFLHeader) == 26, "PFLHeader must be exactly 26 bytes");

std::vector<uint8_t> LosslessEngine::generateResiduals(const Image& image, const IPredictor& predictor) const {
    int width = image.getWidth();
    int height = image.getHeight();
    size_t plane_size = static_cast<size_t>(width) * height;

    std::vector<uint8_t> residuals(plane_size * 3);
    uint8_t* r_plane = residuals.data();
    uint8_t* g_plane = residuals.data() + plane_size;
    uint8_t* b_plane = residuals.data() + (plane_size * 2);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Pixel& current = image.at(x, y);
            size_t idx = static_cast<size_t>(y) * width + x;

            // Channel 0: Red
            uint8_t ra = (x > 0) ? image.at(x - 1, y).r : 0;
            uint8_t rb = (y > 0) ? image.at(x, y - 1).r : 0;
            uint8_t rc = (x > 0 && y > 0) ? image.at(x - 1, y - 1).r : 0;
            uint8_t r_pred = predictor.predict(ra, rb, rc);
            r_plane[idx] = IPredictor::computeResidual(current.r, r_pred);

            // Channel 1: Green
            uint8_t ga = (x > 0) ? image.at(x - 1, y).g : 0;
            uint8_t gb = (y > 0) ? image.at(x, y - 1).g : 0;
            uint8_t gc = (x > 0 && y > 0) ? image.at(x - 1, y - 1).g : 0;
            uint8_t g_pred = predictor.predict(ga, gb, gc);
            g_plane[idx] = IPredictor::computeResidual(current.g, g_pred);

            // Channel 2: Blue
            uint8_t ba = (x > 0) ? image.at(x - 1, y).b : 0;
            uint8_t bb = (y > 0) ? image.at(x, y - 1).b : 0;
            uint8_t bc = (x > 0 && y > 0) ? image.at(x - 1, y - 1).b : 0;
            uint8_t b_pred = predictor.predict(ba, bb, bc);
            b_plane[idx] = IPredictor::computeResidual(current.b, b_pred);
        }
    }

    return residuals;
}

Image LosslessEngine::reconstructFromResiduals(int width, int height, 
                                             const std::vector<uint8_t>& residuals, 
                                             const IPredictor& predictor) const {
    size_t plane_size = static_cast<size_t>(width) * height;
    if (residuals.size() < plane_size * 3) {
        throw std::runtime_error("LosslessEngine: Residual buffer too small for image dimensions.");
    }

    const uint8_t* r_plane = residuals.data();
    const uint8_t* g_plane = residuals.data() + plane_size;
    const uint8_t* b_plane = residuals.data() + (plane_size * 2);

    Image reconstructed(width, height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * width + x;

            // Channel 0: Red
            uint8_t ra = (x > 0) ? reconstructed.at(x - 1, y).r : 0;
            uint8_t rb = (y > 0) ? reconstructed.at(x, y - 1).r : 0;
            uint8_t rc = (x > 0 && y > 0) ? reconstructed.at(x - 1, y - 1).r : 0;
            uint8_t r_pred = predictor.predict(ra, rb, rc);
            uint8_t r_val = IPredictor::reconstruct(r_pred, r_plane[idx]);

            // Channel 1: Green
            uint8_t ga = (x > 0) ? reconstructed.at(x - 1, y).g : 0;
            uint8_t gb = (y > 0) ? reconstructed.at(x, y - 1).g : 0;
            uint8_t gc = (x > 0 && y > 0) ? reconstructed.at(x - 1, y - 1).g : 0;
            uint8_t g_pred = predictor.predict(ga, gb, gc);
            uint8_t g_val = IPredictor::reconstruct(g_pred, g_plane[idx]);

            // Channel 2: Blue
            uint8_t ba = (x > 0) ? reconstructed.at(x - 1, y).b : 0;
            uint8_t bb = (y > 0) ? reconstructed.at(x, y - 1).b : 0;
            uint8_t bc = (x > 0 && y > 0) ? reconstructed.at(x - 1, y - 1).b : 0;
            uint8_t b_pred = predictor.predict(ba, bb, bc);
            uint8_t b_val = IPredictor::reconstruct(b_pred, b_plane[idx]);

            reconstructed.at(x, y) = Pixel(r_val, g_val, b_val);
        }
    }

    return reconstructed;
}

std::vector<uint8_t> LosslessEngine::compress(const Image& image, 
                                             PredictorType predictor_type,
                                             bool adaptive_selection,
                                             LosslessStats* out_stats) const {
    if (!image.isValid()) {
        throw std::invalid_argument("LosslessEngine::compress: Cannot compress an empty image.");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    int width = image.getWidth();
    int height = image.getHeight();
    std::vector<uint8_t> raw_rgb = image.toRawRGB();
    uint32_t original_checksum = Checksum::adler32(raw_rgb.data(), raw_rgb.size());

    PredictorType chosen_predictor_type = predictor_type;
    std::unique_ptr<IPredictor> active_predictor;
    std::vector<uint8_t> best_residuals;
    bool best_use_rle = false;
    std::vector<uint8_t> best_intermediate;

    RLEEngine rle;

    if (adaptive_selection) {
        // Evaluate all 5 predictor candidates and pick the one producing the smallest payload
        const std::array<PredictorType, 5> candidates = {
            PredictorType::None,
            PredictorType::Left,
            PredictorType::Up,
            PredictorType::Average,
            PredictorType::Paeth
        };

        size_t min_size = std::numeric_limits<size_t>::max();

        for (auto candidate_type : candidates) {
            auto candidate_pred = PredictorFactory::create(candidate_type);
            auto residuals = generateResiduals(image, *candidate_pred);
            auto rle_candidate = rle.encode(residuals);

            bool candidate_rle = (rle_candidate.size() < residuals.size());
            size_t size = candidate_rle ? rle_candidate.size() : residuals.size();

            if (size < min_size) {
                min_size = size;
                chosen_predictor_type = candidate_type;
                best_residuals = std::move(residuals);
                best_use_rle = candidate_rle;
                best_intermediate = candidate_rle ? std::move(rle_candidate) : best_residuals;
            }
        }
        active_predictor = PredictorFactory::create(chosen_predictor_type);
    } else {
        active_predictor = PredictorFactory::create(chosen_predictor_type);
        best_residuals = generateResiduals(image, *active_predictor);
        auto rle_data = rle.encode(best_residuals);
        if (rle_data.size() < best_residuals.size()) {
            best_use_rle = true;
            best_intermediate = std::move(rle_data);
        } else {
            best_use_rle = false;
            best_intermediate = best_residuals;
        }
    }

    // Huffman Entropy Encoding of the chosen intermediate stream
    std::array<uint64_t, 256> frequencies{};
    for (uint8_t byte : best_intermediate) {
        frequencies[byte]++;
    }

    HuffmanTree tree;
    tree.buildFromFrequencies(frequencies);

    BitWriter bit_writer;
    tree.encodeData(best_intermediate.data(), best_intermediate.size(), bit_writer);
    const auto& bitstream = bit_writer.getBuffer();

    // Construct custom .pfl container file
    PFLHeader header;
    header.width = static_cast<uint32_t>(width);
    header.height = static_cast<uint32_t>(height);
    header.predictor_type = static_cast<uint8_t>(chosen_predictor_type);
    header.rle_flag = best_use_rle ? 1 : 0;
    header.uncompressed_size = static_cast<uint32_t>(raw_rgb.size());
    header.payload_bytes = static_cast<uint32_t>(bitstream.size());
    header.checksum = original_checksum;

    std::vector<uint8_t> pfl_file(sizeof(PFLHeader) + bitstream.size());
    std::memcpy(pfl_file.data(), &header, sizeof(PFLHeader));
    std::memcpy(pfl_file.data() + sizeof(PFLHeader), bitstream.data(), bitstream.size());

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (out_stats) {
        out_stats->original_bytes = raw_rgb.size();
        out_stats->compressed_bytes = pfl_file.size();
        out_stats->compression_ratio = static_cast<double>(raw_rgb.size()) / pfl_file.size();
        out_stats->percentage_reduction = (1.0 - static_cast<double>(pfl_file.size()) / raw_rgb.size()) * 100.0;
        out_stats->compression_time_ms = elapsed_ms;
        out_stats->chosen_predictor = chosen_predictor_type;
        out_stats->rle_applied = best_use_rle;
    }

    return pfl_file;
}

Image LosslessEngine::decompress(const std::vector<uint8_t>& pfl_data, 
                                LosslessStats* out_stats) const {
    if (pfl_data.size() < sizeof(PFLHeader)) {
        throw std::runtime_error("LosslessEngine::decompress: Data is smaller than PFLHeader.");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    PFLHeader header;
    std::memcpy(&header, pfl_data.data(), sizeof(PFLHeader));

    if (std::memcmp(header.magic, "PFL1", 4) != 0) {
        throw std::runtime_error("LosslessEngine::decompress: Invalid magic header (expected 'PFL1').");
    }

    if (pfl_data.size() < sizeof(PFLHeader) + header.payload_bytes) {
        throw std::runtime_error("LosslessEngine::decompress: Truncated .pfl payload.");
    }

    // 1. BitReader on the Huffman stream
    const uint8_t* payload_ptr = pfl_data.data() + sizeof(PFLHeader);
    BitReader reader(payload_ptr, header.payload_bytes);

    // 2. Decode Huffman Entropy Coding
    std::vector<uint8_t> intermediate = HuffmanTree::decodeData(reader);

    // 3. Reverse RLE if applied
    std::vector<uint8_t> residuals;
    if (header.rle_flag) {
        RLEEngine rle;
        residuals = rle.decode(intermediate);
    } else {
        residuals = std::move(intermediate);
    }

    // 4. Reconstruct pixels using predictor inverse formula
    auto predictor = PredictorFactory::create(static_cast<PredictorType>(header.predictor_type));
    Image reconstructed = reconstructFromResiduals(header.width, header.height, residuals, *predictor);

    // 5. Verify integrity via Adler-32 Checksum
    std::vector<uint8_t> recon_raw = reconstructed.toRawRGB();
    uint32_t recon_checksum = Checksum::adler32(recon_raw.data(), recon_raw.size());
    bool verified = (recon_checksum == header.checksum);

    if (!verified) {
        throw std::runtime_error("LosslessEngine::decompress: Checksum mismatch! Reconstructed pixels corrupted.");
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (out_stats) {
        out_stats->original_bytes = header.uncompressed_size;
        out_stats->compressed_bytes = pfl_data.size();
        out_stats->decompression_time_ms = elapsed_ms;
        out_stats->verified_exact = verified;
    }

    return reconstructed;
}

bool LosslessEngine::saveToFile(const Image& image, const std::string& filepath, bool adaptive) const {
    auto data = compress(image, PredictorType::Paeth, adaptive);
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

Image LosslessEngine::loadFromFile(const std::string& filepath) const {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("LosslessEngine: Failed to open file: " + filepath);
    }
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return decompress(buffer);
}

} // namespace pixelforge
