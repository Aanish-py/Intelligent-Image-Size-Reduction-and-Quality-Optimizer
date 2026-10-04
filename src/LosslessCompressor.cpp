#include <algorithm>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace LosslessCompressor {

    struct CompressResult {
        size_t originalSize;
        size_t compressedSize;
        long long compressTimeMs;
        std::string predictorName;
        bool success;
        std::string errorMessage;
    };

    struct DecompressResult {
        long long decompressTimeMs;
        std::string predictorName;
        int width;
        int height;
        int channels;
        bool success;
        std::string errorMessage;
    };

    bool verify(const std::string& originalPath, const std::string& reconstructedPath, int& diffPixels);
    CompressResult compress(const std::string& inputPath, const std::string& outputPath);
    DecompressResult decompress(const std::string& inputPath, const std::string& outputPath);
}

namespace { // Anonymous namespace for all internal implementation details

// ====================================================================
// CRC-32 (ISO 3309)
// ====================================================================
uint32_t crc32(const uint8_t* data, size_t length) {
    static uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
            }
            table[i] = crc;
        }
        initialized = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// ====================================================================
// Bit-level I/O
// ====================================================================
class BitWriter {
public:
    void writeBit(uint8_t bit) {
        currentByte_ = (currentByte_ | ((bit & 1) << (7 - bitPos_)));
        ++bitPos_;
        ++totalBits_;
        if (bitPos_ == 8) {
            buffer_.push_back(currentByte_);
            currentByte_ = 0;
            bitPos_ = 0;
        }
    }
    void writeBits(uint32_t value, int numBits) {
        for (int i = numBits - 1; i >= 0; --i) writeBit((value >> i) & 1);
    }
    void flush() {
        if (bitPos_ > 0) {
            buffer_.push_back(currentByte_);
            currentByte_ = 0;
            bitPos_ = 0;
        }
    }
    const std::vector<uint8_t>& getData() const { return buffer_; }
    size_t getBitCount() const { return totalBits_; }
private:
    std::vector<uint8_t> buffer_;
    uint8_t currentByte_ = 0;
    int bitPos_ = 0;
    size_t totalBits_ = 0;
};

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    uint8_t readBit() {
        if (bytePos_ >= size_) return 0;
        uint8_t bit = (data_[bytePos_] >> bitPos_) & 1;
        --bitPos_;
        if (bitPos_ < 0) { bitPos_ = 7; ++bytePos_; }
        return bit;
    }
private:
    const uint8_t* data_;
    size_t size_;
    size_t bytePos_ = 0;
    int bitPos_ = 7;
};

// ====================================================================
// Huffman Coding
// ====================================================================
struct HuffmanNode {
    uint16_t symbol;
    uint64_t freq;
    std::shared_ptr<HuffmanNode> left, right;
    HuffmanNode(uint16_t s, uint64_t f) : symbol(s), freq(f) {}
    HuffmanNode(uint64_t f, std::shared_ptr<HuffmanNode> l, std::shared_ptr<HuffmanNode> r) 
        : symbol(0), freq(f), left(l), right(r) {}
    bool isLeaf() const { return !left && !right; }
};

struct HuffCode { uint32_t code; int bitLen; };

void buildCodes(const std::shared_ptr<HuffmanNode>& node, uint32_t code, int depth, std::unordered_map<uint16_t, HuffCode>& table) {
    if (!node) return;
    if (node->isLeaf()) {
        table[node->symbol] = { code, std::max(depth, 1) };
        return;
    }
    buildCodes(node->left, (code << 1) | 0, depth + 1, table);
    buildCodes(node->right, (code << 1) | 1, depth + 1, table);
}

std::vector<uint8_t> serializeHuffmanTable(const std::unordered_map<uint16_t, HuffCode>& table) {
    std::vector<uint8_t> out;
    uint16_t numEntries = (uint16_t)table.size();
    out.push_back(numEntries & 0xFF); out.push_back((numEntries >> 8) & 0xFF);
    for (auto& [sym, hc] : table) {
        out.push_back(sym & 0xFF); out.push_back((sym >> 8) & 0xFF);
        out.push_back(hc.bitLen);
        int numBytes = (hc.bitLen + 7) / 8;
        for (int b = numBytes - 1; b >= 0; --b) out.push_back((hc.code >> (b * 8)) & 0xFF);
    }
    return out;
}

std::shared_ptr<HuffmanNode> deserializeHuffmanTable(const uint8_t* data, size_t size, size_t& bytesRead) {
    auto root = std::make_shared<HuffmanNode>(0, 0);
    size_t pos = 0;
    if (size < 2) return root;
    uint16_t numEntries = data[pos] | (data[pos + 1] << 8); pos += 2;
    for (uint16_t i = 0; i < numEntries && pos < size; ++i) {
        uint16_t sym = data[pos] | (data[pos + 1] << 8); pos += 2;
        uint8_t bitLen = data[pos++];
        int numBytes = (bitLen + 7) / 8;
        uint32_t code = 0;
        for (int b = numBytes - 1; b >= 0; --b) code |= (static_cast<uint32_t>(data[pos++]) << (b * 8));
        
        auto node = root;
        for (int j = bitLen - 1; j >= 0; --j) {
            uint8_t bit = (code >> j) & 1;
            if (bit == 0) {
                if (!node->left) node->left = std::make_shared<HuffmanNode>(0, 0);
                node = node->left;
            } else {
                if (!node->right) node->right = std::make_shared<HuffmanNode>(0, 0);
                node = node->right;
            }
        }
        node->symbol = sym;
    }
    bytesRead = pos;
    return root;
}

// ====================================================================
// RLE Encoding
// ====================================================================
std::vector<uint16_t> rleEncode(const std::vector<uint8_t>& data) {
    std::vector<uint16_t> out;
    size_t i = 0;
    while (i < data.size()) {
        size_t start = i;
        uint8_t val = data[i];
        while (i < data.size() && data[i] == val && (i - start) < 65535) ++i;
        size_t len = i - start;
        if (len >= 4) {
            out.push_back(256); // escape
            out.push_back(val);
            out.push_back(len & 0xFFFF);
        } else {
            for (size_t j = 0; j < len; ++j) out.push_back(val);
        }
    }
    return out;
}

std::vector<uint8_t> rleDecode(const std::vector<uint16_t>& symbols) {
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < symbols.size()) {
        if (symbols[i] == 256) {
            if (i + 2 >= symbols.size()) break;
            uint8_t val = (uint8_t)symbols[i + 1];
            size_t len = symbols[i + 2];
            out.insert(out.end(), len, val);
            i += 3;
        } else {
            out.push_back((uint8_t)symbols[i]);
            ++i;
        }
    }
    return out;
}

// ====================================================================
// Predictors
// ====================================================================
enum PredictorType { NONE = 0, LEFT = 1, UP = 2, AVERAGE = 3, PAETH = 4, GRADIENT = 5 };

const char* getPredictorName(int p) {
    switch(p) {
        case NONE: return "NONE"; case LEFT: return "LEFT"; case UP: return "UP";
        case AVERAGE: return "AVERAGE"; case PAETH: return "PAETH"; case GRADIENT: return "GRADIENT";
        default: return "UNKNOWN";
    }
}

uint8_t predict(PredictorType type, uint8_t a, uint8_t b, uint8_t c) {
    switch (type) {
        case NONE: return 0;
        case LEFT: return a;
        case UP: return b;
        case AVERAGE: return (a + b) / 2;
        case PAETH: {
            int p = a + b - c;
            int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
            if (pa <= pb && pa <= pc) return a;
            if (pb <= pc) return b;
            return c;
        }
        case GRADIENT: {
            int p = a + b - c;
            return std::clamp(p, 0, 255);
        }
    }
    return 0;
}

std::vector<uint8_t> applyPredictor(const std::vector<uint8_t>& raw, int width, int height, PredictorType type) {
    std::vector<uint8_t> res(raw.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            uint8_t a = (x > 0) ? raw[idx - 1] : 0;
            uint8_t b = (y > 0) ? raw[idx - width] : 0;
            uint8_t c = (x > 0 && y > 0) ? raw[idx - width - 1] : 0;
            res[idx] = raw[idx] - predict(type, a, b, c);
        }
    }
    return res;
}

std::vector<uint8_t> reversePredictor(const std::vector<uint8_t>& res, int width, int height, PredictorType type) {
    std::vector<uint8_t> raw(res.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            uint8_t a = (x > 0) ? raw[idx - 1] : 0;
            uint8_t b = (y > 0) ? raw[idx - width] : 0;
            uint8_t c = (x > 0 && y > 0) ? raw[idx - width - 1] : 0;
            raw[idx] = res[idx] + predict(type, a, b, c);
        }
    }
    return raw;
}

PredictorType selectBestPredictor(const std::vector<std::vector<uint8_t>>& channels, int width, int height) {
    // Evaluate sum of absolute residuals on a sample (first channel)
    const auto& raw = channels[0];
    uint64_t bestScore = UINT64_MAX;
    PredictorType bestType = PAETH; // default
    PredictorType types[] = {NONE, LEFT, UP, AVERAGE, PAETH, GRADIENT};
    
    for (PredictorType type : types) {
        uint64_t score = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = y * width + x;
                uint8_t a = (x > 0) ? raw[idx - 1] : 0;
                uint8_t b = (y > 0) ? raw[idx - width] : 0;
                uint8_t c = (x > 0 && y > 0) ? raw[idx - width - 1] : 0;
                uint8_t res = raw[idx] - predict(type, a, b, c);
                // Map residual to a magnitude-like penalty (e.g. 255 -> 1, 1 -> 1)
                int penalty = (res > 127) ? (256 - res) : res;
                score += penalty;
            }
        }
        if (score < bestScore) {
            bestScore = score;
            bestType = type;
        }
    }
    return bestType;
}

// ====================================================================
// Header Format
// ====================================================================
#pragma pack(push, 1)
struct LimgHeader {
    char magic[4]; 
    uint8_t version;
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t predictor;
    uint32_t originalCRC;
    uint32_t huffTableSize;
    uint64_t numRLESymbols;
    uint64_t compressedBits;
};
#pragma pack(pop)

} // namespace

namespace LosslessCompressor {

CompressResult compress(const std::string& inputPath, const std::string& outputPath) {
    CompressResult result;
    result.success = false;

    auto t0 = std::chrono::high_resolution_clock::now();

    cv::Mat mat = cv::imread(inputPath, cv::IMREAD_UNCHANGED);
    if (mat.empty()) {
        result.errorMessage = "Failed to load image. Invalid or unsupported format.";
        return result;
    }
    if (mat.depth() != CV_8U) {
        result.errorMessage = "Only 8-bit images are currently supported.";
        return result;
    }

    int width = mat.cols;
    int height = mat.rows;
    int channels = mat.channels();
    
    cv::Mat continuous = mat.isContinuous() ? mat : mat.clone();
    size_t totalBytes = continuous.total() * continuous.elemSize();
    std::vector<uint8_t> rawBytes(continuous.data, continuous.data + totalBytes);
    
    uint32_t originalCRC = crc32(rawBytes.data(), rawBytes.size());

    // Separate channels
    std::vector<std::vector<uint8_t>> planes(channels, std::vector<uint8_t>(width * height));
    for (int p = 0; p < width * height; ++p) {
        for (int c = 0; c < channels; ++c) planes[c][p] = continuous.data[p * channels + c];
    }

    // Select Predictor
    PredictorType bestPredictor = selectBestPredictor(planes, width, height);
    result.predictorName = getPredictorName(bestPredictor);

    // Apply Prediction
    std::vector<uint8_t> allResiduals;
    allResiduals.reserve(totalBytes);
    for (int c = 0; c < channels; ++c) {
        auto res = applyPredictor(planes[c], width, height, bestPredictor);
        allResiduals.insert(allResiduals.end(), res.begin(), res.end());
    }

    // RLE
    std::vector<uint16_t> rleSymbols = rleEncode(allResiduals);

    // Huffman
    std::unordered_map<uint16_t, uint64_t> freq;
    for (uint16_t s : rleSymbols) freq[s]++;
    
    auto cmp = [](const std::shared_ptr<HuffmanNode>& a, const std::shared_ptr<HuffmanNode>& b) { return a->freq > b->freq; };
    std::priority_queue<std::shared_ptr<HuffmanNode>, std::vector<std::shared_ptr<HuffmanNode>>, decltype(cmp)> pq(cmp);
    for (auto& [sym, f] : freq) pq.push(std::make_shared<HuffmanNode>(sym, f));
    
    if (pq.empty()) pq.push(std::make_shared<HuffmanNode>(0, 1)); // edge case
    while (pq.size() > 1) {
        auto l = pq.top(); pq.pop();
        auto r = pq.top(); pq.pop();
        pq.push(std::make_shared<HuffmanNode>(l->freq + r->freq, l, r));
    }
    
    std::unordered_map<uint16_t, HuffCode> huffTable;
    buildCodes(pq.top(), 0, 0, huffTable);
    
    std::vector<uint8_t> serializedTable = serializeHuffmanTable(huffTable);

    // Bit Packing
    BitWriter bw;
    for (uint16_t s : rleSymbols) {
        const auto& hc = huffTable[s];
        bw.writeBits(hc.code, hc.bitLen);
    }
    bw.flush();

    // Write file
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        result.errorMessage = "Failed to open output file for writing.";
        return result;
    }

    LimgHeader header;
    std::memcpy(header.magic, "LIMG", 4);
    header.version = 1;
    header.width = width;
    header.height = height;
    header.channels = channels;
    header.predictor = bestPredictor;
    header.originalCRC = originalCRC;
    header.huffTableSize = serializedTable.size();
    header.numRLESymbols = rleSymbols.size();
    header.compressedBits = bw.getBitCount();

    out.write((const char*)&header, sizeof(header));
    out.write((const char*)serializedTable.data(), serializedTable.size());
    out.write((const char*)bw.getData().data(), bw.getData().size());
    
    uint32_t payloadCRC = crc32(bw.getData().data(), bw.getData().size());
    out.write((const char*)&payloadCRC, sizeof(payloadCRC));

    auto t1 = std::chrono::high_resolution_clock::now();
    
    result.originalSize = std::filesystem::file_size(inputPath);
    result.compressedSize = std::filesystem::file_size(outputPath);
    result.compressTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    result.success = true;
    return result;
}

DecompressResult decompress(const std::string& inputPath, const std::string& outputPath) {
    DecompressResult result;
    result.success = false;
    
    auto t0 = std::chrono::high_resolution_clock::now();

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        result.errorMessage = "Failed to open .limg file.";
        return result;
    }

    LimgHeader header;
    in.read((char*)&header, sizeof(header));
    if (std::memcmp(header.magic, "LIMG", 4) != 0 || header.version != 1) {
        result.errorMessage = "Not a valid or supported .limg file.";
        return result;
    }

    std::vector<uint8_t> huffData(header.huffTableSize);
    in.read((char*)huffData.data(), huffData.size());

    size_t payloadBytes = (header.compressedBits + 7) / 8;
    std::vector<uint8_t> compData(payloadBytes);
    in.read((char*)compData.data(), compData.size());

    uint32_t storedCRC;
    in.read((char*)&storedCRC, sizeof(storedCRC));
    
    if (crc32(compData.data(), compData.size()) != storedCRC) {
        result.errorMessage = "Compressed payload integrity check failed.";
        return result;
    }

    // Decode Huffman
    size_t bytesRead = 0;
    auto root = deserializeHuffmanTable(huffData.data(), huffData.size(), bytesRead);
    
    BitReader br(compData.data(), compData.size());
    std::vector<uint16_t> rleSymbols;
    rleSymbols.reserve(header.numRLESymbols);
    bool singleSym = root && root->isLeaf();
    
    for (size_t i = 0; i < header.numRLESymbols; ++i) {
        auto node = root;
        if (singleSym) { br.readBit(); rleSymbols.push_back(node->symbol); continue; }
        while (node && !node->isLeaf()) {
            node = (br.readBit() == 0) ? node->left : node->right;
        }
        if (node) rleSymbols.push_back(node->symbol);
    }

    // Decode RLE
    auto residuals = rleDecode(rleSymbols);

    // Reverse Prediction
    size_t planeSize = header.width * header.height;
    std::vector<std::vector<uint8_t>> planes(header.channels);
    for (int c = 0; c < header.channels; ++c) {
        std::vector<uint8_t> resPlane(residuals.begin() + c * planeSize, residuals.begin() + (c + 1) * planeSize);
        planes[c] = reversePredictor(resPlane, header.width, header.height, (PredictorType)header.predictor);
    }

    // Interleave channels
    cv::Mat outMat(header.height, header.width, CV_8UC(header.channels));
    for (size_t p = 0; p < planeSize; ++p) {
        for (int c = 0; c < header.channels; ++c) outMat.data[p * header.channels + c] = planes[c][p];
    }

    // Verify CRC
    std::vector<uint8_t> rawBytes(outMat.data, outMat.data + outMat.total() * outMat.elemSize());
    if (crc32(rawBytes.data(), rawBytes.size()) != header.originalCRC) {
        result.errorMessage = "Pixel data verification failed. File may be corrupted.";
        return result;
    }

    cv::imwrite(outputPath, outMat);

    auto t1 = std::chrono::high_resolution_clock::now();
    result.decompressTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    result.predictorName = getPredictorName(header.predictor);
    result.width = header.width;
    result.height = header.height;
    result.channels = header.channels;
    result.success = true;
    return result;
}

bool verify(const std::string& originalPath, const std::string& reconstructedPath, int& diffPixels) {
    cv::Mat orig = cv::imread(originalPath, cv::IMREAD_UNCHANGED);
    cv::Mat recon = cv::imread(reconstructedPath, cv::IMREAD_UNCHANGED);
    
    diffPixels = 0;
    if (orig.empty() || recon.empty()) { diffPixels = -1; return false; }
    if (orig.size() != recon.size() || orig.channels() != recon.channels()) { diffPixels = -1; return false; }

    cv::Mat diff;
    cv::compare(orig, recon, diff, cv::CMP_NE);
    
    // diff is 255 where pixels differ. Sum over all channels.
    std::vector<cv::Mat> channels;
    cv::split(diff, channels);
    for (const auto& ch : channels) {
        diffPixels += cv::countNonZero(ch);
    }
    
    return diffPixels == 0;
}

} // namespace LosslessCompressor
