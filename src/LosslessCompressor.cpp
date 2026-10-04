// ====================================================================
// LosslessCompressor.cpp — From-scratch lossless image compression
//
// Pipeline:  Raw pixels
//               → Channel separation
//               → Paeth predictive filter (per channel, per scanline)
//               → Residual bytes
//               → RLE (collapse runs of identical residuals)
//               → Huffman coding (entropy coding)
//               → Custom .limg binary format with CRC32 checksum
//
// Decompression is the exact reverse. Pixel-perfect reconstruction
// is verified via CRC32 of the raw pixel buffer.
// ====================================================================

#include "LosslessCompressor.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <sstream>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// ====================================================================
// CRC-32 (ISO 3309 / ITU-T V.42)
// ====================================================================

namespace Checksum {

// Pre-computed CRC-32 lookup table (polynomial 0xEDB88320).
static uint32_t crcTable[256];
static bool     crcTableReady = false;

static void initCRCTable() {
    if (crcTableReady) return;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
        crcTable[i] = crc;
    }
    crcTableReady = true;
}

uint32_t crc32(const uint8_t* data, size_t length) {
    initCRCTable();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc = crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32(const std::vector<uint8_t>& data) {
    return crc32(data.data(), data.size());
}

} // namespace Checksum

// ====================================================================
// BitWriter
// ====================================================================

BitWriter::BitWriter() : currentByte_(0), bitPos_(0), totalBits_(0) {}

void BitWriter::writeBit(uint8_t bit) {
    currentByte_ = static_cast<uint8_t>(currentByte_ | ((bit & 1) << (7 - bitPos_)));
    ++bitPos_;
    ++totalBits_;
    if (bitPos_ == 8) {
        buffer_.push_back(currentByte_);
        currentByte_ = 0;
        bitPos_ = 0;
    }
}

void BitWriter::writeBits(uint32_t value, int numBits) {
    for (int i = numBits - 1; i >= 0; --i) {
        writeBit(static_cast<uint8_t>((value >> i) & 1));
    }
}

void BitWriter::flush() {
    if (bitPos_ > 0) {
        buffer_.push_back(currentByte_);
        currentByte_ = 0;
        bitPos_ = 0;
    }
}

const std::vector<uint8_t>& BitWriter::getData() const {
    return buffer_;
}

size_t BitWriter::getBitCount() const {
    return totalBits_;
}

// ====================================================================
// BitReader
// ====================================================================

BitReader::BitReader(const std::vector<uint8_t>& data)
    : data_(data.data()), size_(data.size()), bytePos_(0), bitPos_(7) {}

BitReader::BitReader(const uint8_t* data, size_t size)
    : data_(data), size_(size), bytePos_(0), bitPos_(7) {}

uint8_t BitReader::readBit() {
    if (bytePos_ >= size_) return 0;        // past end → zero-padded
    uint8_t bit = (data_[bytePos_] >> bitPos_) & 1;
    --bitPos_;
    if (bitPos_ < 0) {
        bitPos_ = 7;
        ++bytePos_;
    }
    return bit;
}

uint32_t BitReader::readBits(int numBits) {
    uint32_t value = 0;
    for (int i = 0; i < numBits; ++i) {
        value = (value << 1) | readBit();
    }
    return value;
}

bool BitReader::hasMore() const {
    return bytePos_ < size_;
}

// ====================================================================
// Huffman Node
// ====================================================================

HuffmanNode::HuffmanNode(uint16_t sym, uint64_t f)
    : symbol(sym), freq(f) {}

HuffmanNode::HuffmanNode(uint64_t f,
                         std::shared_ptr<HuffmanNode> l,
                         std::shared_ptr<HuffmanNode> r)
    : symbol(0), freq(f), left(std::move(l)), right(std::move(r)) {}

bool HuffmanNode::isLeaf() const {
    return !left && !right;
}

// ====================================================================
// Huffman Encoder
// ====================================================================

void HuffmanEncoder::buildCodes(const std::shared_ptr<HuffmanNode>& node,
                                uint32_t code, int depth) {
    if (!node) return;
    if (node->isLeaf()) {
        // Ensure at least 1-bit code even for a single-symbol alphabet
        codeTable_[node->symbol] = { code, std::max(depth, 1) };
        return;
    }
    buildCodes(node->left,  (code << 1) | 0, depth + 1);
    buildCodes(node->right, (code << 1) | 1, depth + 1);
}

void HuffmanEncoder::buildFromFrequencies(
        const std::unordered_map<uint16_t, uint64_t>& freq) {
    codeTable_.clear();

    if (freq.empty()) return;

    // Edge case: only one unique symbol
    if (freq.size() == 1) {
        auto it = freq.begin();
        codeTable_[it->first] = { 0, 1 };
        return;
    }

    // Min-heap of Huffman nodes ordered by frequency
    auto cmp = [](const std::shared_ptr<HuffmanNode>& a,
                  const std::shared_ptr<HuffmanNode>& b) {
        return a->freq > b->freq;
    };
    std::priority_queue<std::shared_ptr<HuffmanNode>,
                        std::vector<std::shared_ptr<HuffmanNode>>,
                        decltype(cmp)> pq(cmp);

    for (auto& [sym, f] : freq) {
        pq.push(std::make_shared<HuffmanNode>(sym, f));
    }

    // Build the tree bottom-up
    while (pq.size() > 1) {
        auto left  = pq.top(); pq.pop();
        auto right = pq.top(); pq.pop();
        pq.push(std::make_shared<HuffmanNode>(
            left->freq + right->freq, left, right));
    }

    buildCodes(pq.top(), 0, 0);
}

void HuffmanEncoder::encode(const std::vector<uint16_t>& symbols,
                            BitWriter& writer) const {
    for (uint16_t sym : symbols) {
        auto it = codeTable_.find(sym);
        if (it == codeTable_.end()) {
            std::cerr << "HuffmanEncoder: unknown symbol " << sym << "\n";
            continue;
        }
        writer.writeBits(it->second.code, it->second.bitLen);
    }
}

std::vector<uint8_t> HuffmanEncoder::serializeTable() const {
    // Format: [numEntries:uint16]
    //   For each entry: [symbol:uint16] [codeBitLen:uint8] [code bits: ceil(bitLen/8) bytes]
    std::vector<uint8_t> out;
    uint16_t numEntries = static_cast<uint16_t>(codeTable_.size());
    out.push_back(static_cast<uint8_t>(numEntries & 0xFF));
    out.push_back(static_cast<uint8_t>((numEntries >> 8) & 0xFF));

    for (auto& [sym, hc] : codeTable_) {
        out.push_back(static_cast<uint8_t>(sym & 0xFF));
        out.push_back(static_cast<uint8_t>((sym >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(hc.bitLen));
        // Store code in ceil(bitLen/8) bytes, big-endian
        int numBytes = (hc.bitLen + 7) / 8;
        for (int b = numBytes - 1; b >= 0; --b) {
            out.push_back(static_cast<uint8_t>((hc.code >> (b * 8)) & 0xFF));
        }
    }
    return out;
}

int HuffmanEncoder::getCodeLength(uint16_t symbol) const {
    auto it = codeTable_.find(symbol);
    return (it != codeTable_.end()) ? it->second.bitLen : -1;
}

// ====================================================================
// Huffman Decoder
// ====================================================================

void HuffmanDecoder::insertCode(uint16_t symbol, uint32_t code, int bitLen) {
    if (!root_) root_ = std::make_shared<HuffmanNode>(0, 0);

    auto node = root_;
    for (int i = bitLen - 1; i >= 0; --i) {
        uint8_t bit = (code >> i) & 1;
        if (bit == 0) {
            if (!node->left) node->left = std::make_shared<HuffmanNode>(0, 0);
            node = node->left;
        } else {
            if (!node->right) node->right = std::make_shared<HuffmanNode>(0, 0);
            node = node->right;
        }
    }
    node->symbol = symbol;
    // Mark as leaf by ensuring left/right stay null (they should be)
}

void HuffmanDecoder::deserializeTable(const uint8_t* data, size_t size) {
    root_.reset();
    if (size < 2) return;

    size_t pos = 0;
    uint16_t numEntries = static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
    pos += 2;

    for (uint16_t i = 0; i < numEntries && pos < size; ++i) {
        if (pos + 3 > size) break;
        uint16_t sym = static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        uint8_t bitLen = data[pos++];
        int numBytes = (bitLen + 7) / 8;
        if (pos + numBytes > size) break;

        uint32_t code = 0;
        for (int b = numBytes - 1; b >= 0; --b) {
            code |= (static_cast<uint32_t>(data[pos++]) << (b * 8));
        }
        insertCode(sym, code, bitLen);
    }
}

std::vector<uint16_t> HuffmanDecoder::decode(BitReader& reader,
                                             size_t numSymbols) const {
    std::vector<uint16_t> result;
    result.reserve(numSymbols);

    // Handle degenerate single-symbol case
    bool singleSymbol = root_ && root_->isLeaf();

    for (size_t i = 0; i < numSymbols; ++i) {
        auto node = root_;
        if (singleSymbol) {
            reader.readBit();  // consume the 1-bit code
            result.push_back(node->symbol);
            continue;
        }
        while (node && !node->isLeaf()) {
            uint8_t bit = reader.readBit();
            node = (bit == 0) ? node->left : node->right;
        }
        if (node) {
            result.push_back(node->symbol);
        }
    }
    return result;
}

// ====================================================================
// Predictor — Paeth filter (same as PNG filter type 4)
// ====================================================================

namespace Predictor {

// Paeth predictor: picks the neighbour whose value is closest to
// the linear predictor p = a + b - c.
static inline uint8_t paethPredictor(uint8_t a, uint8_t b, uint8_t c) {
    int p  = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
    int pa = std::abs(p - static_cast<int>(a));
    int pb = std::abs(p - static_cast<int>(b));
    int pc = std::abs(p - static_cast<int>(c));
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc)             return b;
    return c;
}

std::vector<uint8_t> applyPaeth(const std::vector<uint8_t>& raw,
                                int width, int height) {
    std::vector<uint8_t> residuals(raw.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * width + x;
            uint8_t a = (x > 0)                ? raw[idx - 1]         : 0;
            uint8_t b = (y > 0)                ? raw[idx - width]     : 0;
            uint8_t c = (x > 0 && y > 0)       ? raw[idx - width - 1] : 0;
            uint8_t pred = paethPredictor(a, b, c);
            // Residual stored mod 256 — this is perfectly invertible
            residuals[idx] = static_cast<uint8_t>(raw[idx] - pred);
        }
    }
    return residuals;
}

std::vector<uint8_t> reversePaeth(const std::vector<uint8_t>& residuals,
                                   int width, int height) {
    std::vector<uint8_t> raw(residuals.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * width + x;
            uint8_t a = (x > 0)                ? raw[idx - 1]         : 0;
            uint8_t b = (y > 0)                ? raw[idx - width]     : 0;
            uint8_t c = (x > 0 && y > 0)       ? raw[idx - width - 1] : 0;
            uint8_t pred = paethPredictor(a, b, c);
            raw[idx] = static_cast<uint8_t>(residuals[idx] + pred);
        }
    }
    return raw;
}

} // namespace Predictor

// ====================================================================
// RLE — Run-Length Encoding
// ====================================================================
// Uses symbol 256 as the escape marker (not a valid byte value).
// Format for a run of length N >= 4 of byte B:
//   [256] [B] [N_low] [N_high]    (N stored as 16-bit little-endian)
// Runs shorter than 4 are emitted verbatim (RLE overhead not worth it).

namespace RLE {

std::vector<uint16_t> encode(const std::vector<uint8_t>& data) {
    std::vector<uint16_t> out;
    out.reserve(data.size());

    size_t i = 0;
    while (i < data.size()) {
        // Count run length
        size_t runStart = i;
        uint8_t val = data[i];
        while (i < data.size() && data[i] == val && (i - runStart) < 65535) {
            ++i;
        }
        size_t runLen = i - runStart;

        if (runLen >= 4) {
            // Emit RLE marker
            out.push_back(256);                                       // escape
            out.push_back(static_cast<uint16_t>(val));                // byte value
            out.push_back(static_cast<uint16_t>(runLen & 0xFFFF));    // length low
            out.push_back(static_cast<uint16_t>((runLen >> 16) & 0xFFFF)); // length high (always 0 for <=65535)
        } else {
            // Emit verbatim
            for (size_t j = 0; j < runLen; ++j) {
                out.push_back(static_cast<uint16_t>(val));
            }
        }
    }
    return out;
}

std::vector<uint8_t> decode(const std::vector<uint16_t>& symbols) {
    std::vector<uint8_t> out;
    out.reserve(symbols.size());          // rough estimate

    size_t i = 0;
    while (i < symbols.size()) {
        if (symbols[i] == 256) {
            // RLE marker: next 3 symbols are [byte] [lenLow] [lenHigh]
            if (i + 3 >= symbols.size()) break;   // malformed
            uint8_t val = static_cast<uint8_t>(symbols[i + 1]);
            size_t len = static_cast<size_t>(symbols[i + 2])
                       | (static_cast<size_t>(symbols[i + 3]) << 16);
            out.insert(out.end(), len, val);
            i += 4;
        } else {
            out.push_back(static_cast<uint8_t>(symbols[i]));
            ++i;
        }
    }
    return out;
}

} // namespace RLE

// ====================================================================
// Helper: flatten a cv::Mat into a vector of raw bytes
// ====================================================================
static std::vector<uint8_t> matToBytes(const cv::Mat& mat) {
    // Ensure the Mat is contiguous
    cv::Mat continuous;
    if (mat.isContinuous()) {
        continuous = mat;
    } else {
        continuous = mat.clone();
    }
    size_t totalBytes = static_cast<size_t>(continuous.total()) * continuous.elemSize();
    std::vector<uint8_t> bytes(totalBytes);
    std::memcpy(bytes.data(), continuous.data, totalBytes);
    return bytes;
}

// ====================================================================
// Helper: separate a multi-channel image into per-channel planes
// ====================================================================
static std::vector<std::vector<uint8_t>> separateChannels(
        const cv::Mat& mat, int width, int height, int channels) {
    std::vector<std::vector<uint8_t>> planes(channels);
    size_t pixelCount = static_cast<size_t>(width) * height;
    for (int c = 0; c < channels; ++c) {
        planes[c].resize(pixelCount);
    }

    // OpenCV stores pixels interleaved: B0 G0 R0 B1 G1 R1 ...
    const uint8_t* data = mat.data;
    for (size_t p = 0; p < pixelCount; ++p) {
        for (int c = 0; c < channels; ++c) {
            planes[c][p] = data[p * channels + c];
        }
    }
    return planes;
}

// ====================================================================
// Helper: interleave per-channel planes back into a cv::Mat
// ====================================================================
static cv::Mat interleaveChannels(
        const std::vector<std::vector<uint8_t>>& planes,
        int width, int height, int channels, int cvType) {
    cv::Mat mat(height, width, cvType);
    size_t pixelCount = static_cast<size_t>(width) * height;
    uint8_t* data = mat.data;
    for (size_t p = 0; p < pixelCount; ++p) {
        for (int c = 0; c < channels; ++c) {
            data[p * channels + c] = planes[c][p];
        }
    }
    return mat;
}

// ====================================================================
// LosslessCompressor — construction
// ====================================================================

LosslessCompressor::LosslessCompressor() {}

std::string LosslessCompressor::getMethodName() const {
    return "Lossless (Custom LIMG)";
}

// ====================================================================
// LosslessCompressor::compress — full compression pipeline
// ====================================================================

bool LosslessCompressor::compress(const Image& image,
                                  const std::string& outputPath,
                                  int /*quality*/) {
    if (!image.isLoaded()) {
        std::cerr << "Error: No image loaded.\n";
        return false;
    }

    const cv::Mat& mat = image.getPixelData();
    int width    = mat.cols;
    int height   = mat.rows;
    int channels = mat.channels();

    // Currently only support 8-bit images
    if (mat.depth() != CV_8U) {
        std::cerr << "Error: Only 8-bit images are supported for custom lossless compression.\n";
        return false;
    }

    // --- Step 1: Compute CRC32 of the raw pixel buffer ---
    std::vector<uint8_t> rawBytes = matToBytes(mat);
    uint32_t originalCRC = Checksum::crc32(rawBytes);

    // --- Step 2: Separate into channels ---
    std::vector<std::vector<uint8_t>> channelPlanes =
        separateChannels(mat, width, height, channels);

    // --- Step 3: Apply Paeth prediction to each channel ---
    std::vector<uint8_t> allResiduals;
    allResiduals.reserve(rawBytes.size());
    for (int c = 0; c < channels; ++c) {
        std::vector<uint8_t> residuals =
            Predictor::applyPaeth(channelPlanes[c], width, height);
        allResiduals.insert(allResiduals.end(),
                            residuals.begin(), residuals.end());
    }

    // --- Step 4: RLE encode the residual stream ---
    std::vector<uint16_t> rleSymbols = RLE::encode(allResiduals);

    // --- Step 5: Build frequency table and Huffman tree ---
    std::unordered_map<uint16_t, uint64_t> freq;
    for (uint16_t sym : rleSymbols) {
        ++freq[sym];
    }

    HuffmanEncoder encoder;
    encoder.buildFromFrequencies(freq);

    // --- Step 6: Serialize the Huffman table ---
    std::vector<uint8_t> huffTable = encoder.serializeTable();

    // --- Step 7: Huffman-encode the RLE symbols ---
    BitWriter writer;
    encoder.encode(rleSymbols, writer);
    writer.flush();

    const std::vector<uint8_t>& compressedBits = writer.getData();

    // --- Step 8: Write the .limg file ---
    LimgHeader header;
    std::memcpy(header.magic, "LIMG", 4);
    header.version        = 1;
    header.width          = static_cast<uint32_t>(width);
    header.height         = static_cast<uint32_t>(height);
    header.channels       = static_cast<uint8_t>(channels);
    header.bitsPerChannel = 8;
    header.cvType         = mat.type();
    header.originalCRC    = originalCRC;
    header.huffTableSize  = static_cast<uint32_t>(huffTable.size());
    header.numRLESymbols  = static_cast<uint64_t>(rleSymbols.size());
    header.compressedBits = static_cast<uint64_t>(writer.getBitCount());

    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "Error: Cannot create output file: " << outputPath << "\n";
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    ofs.write(reinterpret_cast<const char*>(huffTable.data()),
              static_cast<std::streamsize>(huffTable.size()));
    ofs.write(reinterpret_cast<const char*>(compressedBits.data()),
              static_cast<std::streamsize>(compressedBits.size()));

    // Write trailing CRC of the compressed payload for integrity
    uint32_t payloadCRC = Checksum::crc32(compressedBits);
    ofs.write(reinterpret_cast<const char*>(&payloadCRC), sizeof(payloadCRC));

    ofs.close();
    if (!ofs) {
        std::cerr << "Error: Failed to write output file.\n";
        return false;
    }

    return true;
}

// ====================================================================
// LosslessCompressor::decompress — full decompression pipeline
// ====================================================================

bool LosslessCompressor::decompress(const std::string& limgPath,
                                    cv::Mat& output) {
    std::ifstream ifs(limgPath, std::ios::binary);
    if (!ifs) {
        std::cerr << "Error: Cannot open file: " << limgPath << "\n";
        return false;
    }

    // --- Step 1: Read header ---
    LimgHeader header;
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!ifs) {
        std::cerr << "Error: Failed to read LIMG header.\n";
        return false;
    }

    // Validate magic number
    if (std::memcmp(header.magic, "LIMG", 4) != 0) {
        std::cerr << "Error: Not a valid LIMG file (bad magic number).\n";
        return false;
    }
    if (header.version != 1) {
        std::cerr << "Error: Unsupported LIMG version " << (int)header.version << ".\n";
        return false;
    }

    int width    = static_cast<int>(header.width);
    int height   = static_cast<int>(header.height);
    int channels = static_cast<int>(header.channels);

    // --- Step 2: Read Huffman table ---
    std::vector<uint8_t> huffTable(header.huffTableSize);
    ifs.read(reinterpret_cast<char*>(huffTable.data()),
             static_cast<std::streamsize>(header.huffTableSize));
    if (!ifs) {
        std::cerr << "Error: Failed to read Huffman table.\n";
        return false;
    }

    // --- Step 3: Read compressed payload ---
    size_t compressedByteCount = (static_cast<size_t>(header.compressedBits) + 7) / 8;
    std::vector<uint8_t> compressedData(compressedByteCount);
    ifs.read(reinterpret_cast<char*>(compressedData.data()),
             static_cast<std::streamsize>(compressedByteCount));
    if (!ifs) {
        std::cerr << "Error: Failed to read compressed data.\n";
        return false;
    }

    // --- Step 4: Read and verify payload CRC ---
    uint32_t storedPayloadCRC = 0;
    ifs.read(reinterpret_cast<char*>(&storedPayloadCRC), sizeof(storedPayloadCRC));
    uint32_t computedPayloadCRC = Checksum::crc32(compressedData);
    if (storedPayloadCRC != computedPayloadCRC) {
        std::cerr << "Error: Compressed payload CRC mismatch! File may be corrupted.\n";
        return false;
    }

    // --- Step 5: Huffman decode ---
    HuffmanDecoder decoder;
    decoder.deserializeTable(huffTable.data(), huffTable.size());
    BitReader reader(compressedData);
    std::vector<uint16_t> rleSymbols =
        decoder.decode(reader, static_cast<size_t>(header.numRLESymbols));

    // --- Step 6: RLE decode ---
    std::vector<uint8_t> allResiduals = RLE::decode(rleSymbols);

    // --- Step 7: Reverse Paeth prediction per channel ---
    size_t pixelCount = static_cast<size_t>(width) * height;
    std::vector<std::vector<uint8_t>> channelPlanes(channels);
    for (int c = 0; c < channels; ++c) {
        std::vector<uint8_t> residuals(
            allResiduals.begin() + c * static_cast<ptrdiff_t>(pixelCount),
            allResiduals.begin() + (c + 1) * static_cast<ptrdiff_t>(pixelCount));
        channelPlanes[c] = Predictor::reversePaeth(residuals, width, height);
    }

    // --- Step 8: Interleave channels back into a cv::Mat ---
    output = interleaveChannels(channelPlanes, width, height, channels,
                                header.cvType);

    // --- Step 9: Verify original CRC ---
    std::vector<uint8_t> reconstructedBytes = matToBytes(output);
    uint32_t reconstructedCRC = Checksum::crc32(reconstructedBytes);
    if (reconstructedCRC != header.originalCRC) {
        std::cerr << "Error: Pixel data CRC mismatch! Reconstruction failed.\n"
                  << "  Expected CRC: 0x" << std::hex << header.originalCRC
                  << "  Got CRC:      0x" << reconstructedCRC << std::dec << "\n";
        return false;
    }

    return true;
}

// ====================================================================
// LosslessCompressor::verifyRoundTrip
// ====================================================================

bool LosslessCompressor::verifyRoundTrip(const Image& image,
                                         const std::string& tempPath) {
    if (!image.isLoaded()) {
        std::cerr << "Error: No image loaded for verification.\n";
        return false;
    }

    LosslessCompressor compressor;

    // Compress
    auto t0 = std::chrono::high_resolution_clock::now();
    bool ok = compressor.compress(image, tempPath, 0);
    auto t1 = std::chrono::high_resolution_clock::now();

    if (!ok) {
        std::cerr << "Verification FAILED: compression error.\n";
        return false;
    }

    double compressMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Decompress
    cv::Mat decompressed;
    auto t2 = std::chrono::high_resolution_clock::now();
    ok = decompress(tempPath, decompressed);
    auto t3 = std::chrono::high_resolution_clock::now();

    if (!ok) {
        std::cerr << "Verification FAILED: decompression error.\n";
        return false;
    }

    double decompressMs = std::chrono::duration<double, std::milli>(t3 - t2).count();

    // Pixel-perfect comparison
    const cv::Mat& original = image.getPixelData();
    bool identical = true;

    if (original.size() != decompressed.size() ||
        original.type() != decompressed.type()) {
        identical = false;
    } else {
        // Compare every byte
        std::vector<uint8_t> origBytes = matToBytes(original);
        std::vector<uint8_t> decBytes  = matToBytes(decompressed);
        if (origBytes.size() != decBytes.size()) {
            identical = false;
        } else {
            identical = std::memcmp(origBytes.data(), decBytes.data(),
                                    origBytes.size()) == 0;
        }
    }

    // File sizes
    std::error_code ec;
    auto compressedSize = std::filesystem::file_size(tempPath, ec);
    size_t originalSize = static_cast<size_t>(original.total()) * original.elemSize();

    double ratio = (compressedSize > 0)
        ? static_cast<double>(originalSize) / static_cast<double>(compressedSize)
        : 0.0;
    double savedPct = (originalSize > 0)
        ? (1.0 - static_cast<double>(compressedSize) / static_cast<double>(originalSize)) * 100.0
        : 0.0;

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Original pixel data : " << originalSize << " bytes ("
              << Image::formatSize(static_cast<long long>(originalSize)) << ")\n";
    std::cout << "  Compressed file     : " << compressedSize << " bytes ("
              << Image::formatSize(static_cast<long long>(compressedSize)) << ")\n";
    std::cout << "  Compression ratio   : " << ratio << " : 1\n";
    std::cout << "  Space saved         : " << savedPct << "%\n";
    std::cout << "  Compress time       : " << compressMs << " ms\n";
    std::cout << "  Decompress time     : " << decompressMs << " ms\n";
    std::cout << "  Integrity check     : "
              << (identical ? "PASS (pixel-perfect)" : "FAIL") << "\n";

    return identical;
}

// ====================================================================
// LosslessCompressor::runTests — comprehensive test suite
// ====================================================================

void LosslessCompressor::runTests(const std::string& realImagePath) {
    namespace fs = std::filesystem;

    std::cout << "\n"
              << "================================================================\n"
              << "    LOSSLESS COMPRESSOR — COMPREHENSIVE TEST SUITE\n"
              << "================================================================\n\n";

    // Ensure output directory exists
    fs::create_directories("output");

    // Helper lambda: create a synthetic Image, run the round-trip test
    auto testSynthetic = [](const std::string& name,
                            const cv::Mat& mat,
                            const std::string& outDir) -> bool {
        std::cout << "--- Test: " << name << " ---\n";
        std::cout << "  Dimensions: " << mat.cols << "x" << mat.rows
                  << " x " << mat.channels() << " channels\n";

        // Save a temporary BMP so Image can load it (Image requires a file)
        std::string bmpPath = outDir + "/" + name + "_temp.bmp";
        cv::imwrite(bmpPath, mat);

        Image img;
        if (!img.loadImage(bmpPath)) {
            std::cerr << "  FAILED to load synthetic image.\n\n";
            return false;
        }

        std::string limgPath = outDir + "/" + name + ".limg";
        bool ok = LosslessCompressor::verifyRoundTrip(img, limgPath);
        std::cout << "\n";

        // Clean up temp BMP
        std::error_code ec;
        fs::remove(bmpPath, ec);

        return ok;
    };

    int passed = 0;
    int total  = 0;

    // ---- Test 1: Solid black (best case — all zeros) ----
    {
        cv::Mat solid = cv::Mat::zeros(256, 256, CV_8UC3);
        ++total;
        if (testSynthetic("solid_black", solid, "output")) ++passed;
    }

    // ---- Test 2: Solid white ----
    {
        cv::Mat solid(256, 256, CV_8UC3, cv::Scalar(255, 255, 255));
        ++total;
        if (testSynthetic("solid_white", solid, "output")) ++passed;
    }

    // ---- Test 3: Solid color (arbitrary) ----
    {
        cv::Mat solid(256, 256, CV_8UC3, cv::Scalar(42, 128, 200));
        ++total;
        if (testSynthetic("solid_color", solid, "output")) ++passed;
    }

    // ---- Test 4: Horizontal gradient ----
    {
        cv::Mat grad(256, 256, CV_8UC3);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
                grad.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uint8_t>(x),
                    static_cast<uint8_t>(x),
                    static_cast<uint8_t>(x));
        ++total;
        if (testSynthetic("horizontal_gradient", grad, "output")) ++passed;
    }

    // ---- Test 5: Vertical gradient ----
    {
        cv::Mat grad(256, 256, CV_8UC3);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
                grad.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uint8_t>(y),
                    static_cast<uint8_t>(y),
                    static_cast<uint8_t>(y));
        ++total;
        if (testSynthetic("vertical_gradient", grad, "output")) ++passed;
    }

    // ---- Test 6: Diagonal gradient (RGB channels differ) ----
    {
        cv::Mat grad(256, 256, CV_8UC3);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
                grad.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uint8_t>(x),
                    static_cast<uint8_t>(y),
                    static_cast<uint8_t>((x + y) / 2));
        ++total;
        if (testSynthetic("diagonal_gradient", grad, "output")) ++passed;
    }

    // ---- Test 7: Random noise (worst case for compression) ----
    {
        cv::Mat noise(256, 256, CV_8UC3);
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, 255);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
                noise.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uint8_t>(dist(rng)),
                    static_cast<uint8_t>(dist(rng)),
                    static_cast<uint8_t>(dist(rng)));
        ++total;
        if (testSynthetic("random_noise", noise, "output")) ++passed;
    }

    // ---- Test 8: Small image (1x1) ----
    {
        cv::Mat tiny(1, 1, CV_8UC3, cv::Scalar(100, 150, 200));
        ++total;
        if (testSynthetic("tiny_1x1", tiny, "output")) ++passed;
    }

    // ---- Test 9: Small image (2x2) ----
    {
        cv::Mat small(2, 2, CV_8UC3);
        small.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 0, 0);
        small.at<cv::Vec3b>(0, 1) = cv::Vec3b(255, 0, 0);
        small.at<cv::Vec3b>(1, 0) = cv::Vec3b(0, 255, 0);
        small.at<cv::Vec3b>(1, 1) = cv::Vec3b(0, 0, 255);
        ++total;
        if (testSynthetic("tiny_2x2", small, "output")) ++passed;
    }

    // ---- Test 10: Checkerboard pattern ----
    {
        cv::Mat checker(256, 256, CV_8UC3);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) {
                uint8_t v = ((x / 8 + y / 8) % 2 == 0) ? 0 : 255;
                checker.at<cv::Vec3b>(y, x) = cv::Vec3b(v, v, v);
            }
        ++total;
        if (testSynthetic("checkerboard", checker, "output")) ++passed;
    }

    // ---- Test 11: Repeated horizontal stripes ----
    {
        cv::Mat stripes(256, 256, CV_8UC3);
        for (int y = 0; y < 256; ++y) {
            uint8_t v = (y % 8 < 4) ? 0 : 255;
            for (int x = 0; x < 256; ++x)
                stripes.at<cv::Vec3b>(y, x) = cv::Vec3b(v, v, v);
        }
        ++total;
        if (testSynthetic("horizontal_stripes", stripes, "output")) ++passed;
    }

    // ---- Test 12: Large image (1024x768) ----
    {
        cv::Mat large(768, 1024, CV_8UC3);
        std::mt19937 rng(123);
        std::uniform_int_distribution<int> dist(0, 255);
        // Fill with smooth blocks
        for (int y = 0; y < 768; y += 16)
            for (int x = 0; x < 1024; x += 16) {
                cv::Vec3b color(static_cast<uint8_t>(dist(rng)),
                                static_cast<uint8_t>(dist(rng)),
                                static_cast<uint8_t>(dist(rng)));
                for (int dy = 0; dy < 16 && (y + dy) < 768; ++dy)
                    for (int dx = 0; dx < 16 && (x + dx) < 1024; ++dx)
                        large.at<cv::Vec3b>(y + dy, x + dx) = color;
            }
        ++total;
        if (testSynthetic("large_1024x768_blocks", large, "output")) ++passed;
    }

    // ---- Test 13: Grayscale (single channel) ----
    {
        cv::Mat gray(256, 256, CV_8UC1);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
                gray.at<uint8_t>(y, x) = static_cast<uint8_t>((x + y) % 256);
        ++total;
        if (testSynthetic("grayscale_gradient", gray, "output")) ++passed;
    }

    // ---- Test 14: 4-channel BGRA (with alpha) ----
    {
        cv::Mat bgra(128, 128, CV_8UC4);
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
                bgra.at<cv::Vec4b>(y, x) = cv::Vec4b(
                    static_cast<uint8_t>(x * 2),
                    static_cast<uint8_t>(y * 2),
                    static_cast<uint8_t>((x + y)),
                    static_cast<uint8_t>(255 - x));
        ++total;
        if (testSynthetic("bgra_4channel", bgra, "output")) ++passed;
    }

    // ---- Test with real image if provided ----
    if (!realImagePath.empty() && fs::exists(realImagePath)) {
        std::cout << "--- Test: Real Image ---\n";
        std::cout << "  File: " << realImagePath << "\n";

        Image realImg;
        if (realImg.loadImage(realImagePath)) {
            ++total;
            std::string limgPath = "output/real_image_test.limg";
            if (verifyRoundTrip(realImg, limgPath)) ++passed;

            // Benchmark against OpenCV PNG
            std::cout << "\n--- Benchmark: Custom LIMG vs OpenCV PNG ---\n";

            // Custom LIMG (already timed above, redo for fair comparison)
            auto t0 = std::chrono::high_resolution_clock::now();
            LosslessCompressor lc;
            lc.compress(realImg, "output/benchmark.limg", 0);
            auto t1 = std::chrono::high_resolution_clock::now();
            double limgCompMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

            cv::Mat limgDecomp;
            auto t2 = std::chrono::high_resolution_clock::now();
            decompress("output/benchmark.limg", limgDecomp);
            auto t3 = std::chrono::high_resolution_clock::now();
            double limgDecompMs = std::chrono::duration<double, std::milli>(t3 - t2).count();

            auto limgSize = fs::file_size("output/benchmark.limg");

            // OpenCV PNG (level 6 = default)
            auto t4 = std::chrono::high_resolution_clock::now();
            cv::imwrite("output/benchmark_opencv.png", realImg.getPixelData(),
                        {cv::IMWRITE_PNG_COMPRESSION, 6});
            auto t5 = std::chrono::high_resolution_clock::now();
            double pngCompMs = std::chrono::duration<double, std::milli>(t5 - t4).count();

            auto t6 = std::chrono::high_resolution_clock::now();
            cv::imread("output/benchmark_opencv.png", cv::IMREAD_UNCHANGED);
            auto t7 = std::chrono::high_resolution_clock::now();
            double pngDecompMs = std::chrono::duration<double, std::milli>(t7 - t6).count();

            auto pngSize = fs::file_size("output/benchmark_opencv.png");

            size_t rawSize = static_cast<size_t>(realImg.getPixelData().total())
                           * realImg.getPixelData().elemSize();

            std::cout << std::fixed << std::setprecision(2);
            std::cout << "\n  Raw pixel data size : " << rawSize << " bytes\n\n";
            std::cout << std::left
                      << std::setw(22) << "  Metric"
                      << std::setw(20) << "Custom LIMG"
                      << std::setw(20) << "OpenCV PNG" << "\n";
            std::cout << "  " << std::string(58, '-') << "\n";
            std::cout << std::setw(22) << "  Compressed size"
                      << std::setw(20) << (std::to_string(limgSize) + " B")
                      << std::setw(20) << (std::to_string(pngSize) + " B") << "\n";
            std::cout << std::setw(22) << "  Ratio"
                      << std::setw(20) << (std::to_string(
                            static_cast<double>(rawSize) / limgSize).substr(0, 5) + ":1")
                      << std::setw(20) << (std::to_string(
                            static_cast<double>(rawSize) / pngSize).substr(0, 5) + ":1") << "\n";
            std::cout << std::setw(22) << "  Compress time"
                      << std::setw(20) << (std::to_string(limgCompMs).substr(0, 7) + " ms")
                      << std::setw(20) << (std::to_string(pngCompMs).substr(0, 7) + " ms") << "\n";
            std::cout << std::setw(22) << "  Decompress time"
                      << std::setw(20) << (std::to_string(limgDecompMs).substr(0, 7) + " ms")
                      << std::setw(20) << (std::to_string(pngDecompMs).substr(0, 7) + " ms") << "\n";

            std::cout << "\n  Analysis:\n";
            if (limgSize < pngSize) {
                std::cout << "  * Custom LIMG achieves BETTER compression than OpenCV PNG.\n";
            } else if (limgSize > pngSize) {
                std::cout << "  * OpenCV PNG achieves better compression (PNG uses DEFLATE\n"
                          << "    which combines LZ77 + Huffman, a more aggressive algorithm).\n"
                          << "    Our pipeline uses Paeth + RLE + Huffman — effective but simpler.\n";
            } else {
                std::cout << "  * Both produce identical file sizes.\n";
            }
            if (limgCompMs < pngCompMs) {
                std::cout << "  * Custom LIMG is FASTER at compression.\n";
            } else {
                std::cout << "  * OpenCV PNG is faster at compression (highly optimized C library).\n";
            }

            // Clean up benchmark files
            std::error_code ec;
            fs::remove("output/benchmark.limg", ec);
            fs::remove("output/benchmark_opencv.png", ec);
        }
        std::cout << "\n";
    }

    // ---- Summary ----
    std::cout << "================================================================\n"
              << "  TEST RESULTS: " << passed << " / " << total << " passed\n"
              << "================================================================\n\n";

    if (passed == total) {
        std::cout << "  ALL TESTS PASSED — pixel-perfect lossless compression verified.\n\n";
    } else {
        std::cout << "  WARNING: " << (total - passed) << " test(s) FAILED.\n\n";
    }
}
