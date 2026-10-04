#ifndef LOSSLESS_COMPRESSOR_H
#define LOSSLESS_COMPRESSOR_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "CompressionStrategy.h"

// ---------------------------------------------------------------
// Forward declarations for internal helper classes.
// All are defined in LosslessCompressor.cpp.
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Bit-level I/O
// ---------------------------------------------------------------

/// Writes individual bits into a byte buffer (MSB-first packing).
class BitWriter {
public:
    BitWriter();
    void writeBit(uint8_t bit);
    void writeBits(uint32_t value, int numBits);
    void flush();                         // pad remaining bits in current byte
    const std::vector<uint8_t>& getData() const;
    size_t getBitCount() const;

private:
    std::vector<uint8_t> buffer_;
    uint8_t currentByte_;
    int bitPos_;            // bits written into currentByte_ (0-7)
    size_t totalBits_;
};

/// Reads individual bits from a byte buffer (MSB-first unpacking).
class BitReader {
public:
    explicit BitReader(const std::vector<uint8_t>& data);
    explicit BitReader(const uint8_t* data, size_t size);
    uint8_t readBit();
    uint32_t readBits(int numBits);
    bool hasMore() const;

private:
    const uint8_t* data_;
    size_t size_;
    size_t bytePos_;
    int bitPos_;            // next bit to read within current byte (7..0)
};

// ---------------------------------------------------------------
// Huffman coding
// ---------------------------------------------------------------

/// A node in the Huffman tree (used during construction).
struct HuffmanNode {
    uint16_t symbol;        // 0-255 for byte values, 256+ for RLE markers
    uint64_t freq;
    std::shared_ptr<HuffmanNode> left;
    std::shared_ptr<HuffmanNode> right;

    HuffmanNode(uint16_t sym, uint64_t f);
    HuffmanNode(uint64_t f,
                std::shared_ptr<HuffmanNode> l,
                std::shared_ptr<HuffmanNode> r);
    bool isLeaf() const;
};

/// Builds a Huffman tree from symbol frequencies and encodes data.
class HuffmanEncoder {
public:
    /// Build the code table from a frequency map.
    void buildFromFrequencies(const std::unordered_map<uint16_t, uint64_t>& freq);

    /// Encode a symbol sequence using the built code table.
    void encode(const std::vector<uint16_t>& symbols, BitWriter& writer) const;

    /// Serialize the code table so the decoder can rebuild it.
    /// Format: [numEntries:uint16][symbol:uint16 codeBitLen:uint8 code:variable]...
    std::vector<uint8_t> serializeTable() const;

    /// Get code length for a symbol (for diagnostics).
    int getCodeLength(uint16_t symbol) const;

private:
    struct HuffCode {
        uint32_t code;
        int bitLen;
    };
    std::unordered_map<uint16_t, HuffCode> codeTable_;
    void buildCodes(const std::shared_ptr<HuffmanNode>& node,
                    uint32_t code, int depth);
};

/// Rebuilds a Huffman tree from the serialized table and decodes data.
class HuffmanDecoder {
public:
    /// Deserialize the code table produced by HuffmanEncoder::serializeTable().
    void deserializeTable(const uint8_t* data, size_t size);

    /// Decode symbols from a bit stream.
    /// @param reader     Bit reader positioned at the start of encoded data.
    /// @param numSymbols Number of symbols to decode.
    std::vector<uint16_t> decode(BitReader& reader, size_t numSymbols) const;

private:
    std::shared_ptr<HuffmanNode> root_;
    void insertCode(uint16_t symbol, uint32_t code, int bitLen);
};

// ---------------------------------------------------------------
// Pixel prediction (Paeth filter — same as PNG filter type 4)
// ---------------------------------------------------------------
namespace Predictor {
    /// Apply Paeth prediction to raw channel data, producing residuals.
    /// @param raw     Single-channel scanline data in row-major order.
    /// @param width   Image width in pixels.
    /// @param height  Image height in pixels.
    /// @return Residuals (actual − predicted), stored as uint8_t (mod 256).
    std::vector<uint8_t> applyPaeth(const std::vector<uint8_t>& raw,
                                    int width, int height);

    /// Reverse the Paeth prediction: reconstruct original bytes from residuals.
    std::vector<uint8_t> reversePaeth(const std::vector<uint8_t>& residuals,
                                      int width, int height);
}

// ---------------------------------------------------------------
// Run-Length Encoding (RLE) on a byte stream
// ---------------------------------------------------------------
namespace RLE {
    /// Encode a byte stream with RLE.
    /// Uses a special escape symbol (256) followed by the repeated byte
    /// and a run-length count. Symbols are uint16_t to accommodate
    /// the escape marker.
    ///
    /// Format: for runs >= 4 of byte B:
    ///   [256] [B] [runLength-4 as uint16]
    /// Shorter runs are emitted verbatim.
    std::vector<uint16_t> encode(const std::vector<uint8_t>& data);

    /// Decode an RLE-encoded symbol stream back to bytes.
    std::vector<uint8_t> decode(const std::vector<uint16_t>& symbols);
}

// ---------------------------------------------------------------
// CRC-32 checksum (ISO 3309 / ITU-T V.42)
// ---------------------------------------------------------------
namespace Checksum {
    uint32_t crc32(const uint8_t* data, size_t length);
    uint32_t crc32(const std::vector<uint8_t>& data);
}

// ---------------------------------------------------------------
// Custom lossless file format header (.limg)
// ---------------------------------------------------------------
#pragma pack(push, 1)
struct LimgHeader {
    char     magic[4];      // "LIMG"
    uint8_t  version;       // 1
    uint32_t width;
    uint32_t height;
    uint8_t  channels;
    uint8_t  bitsPerChannel;
    int32_t  cvType;        // cv::Mat::type()
    uint32_t originalCRC;   // CRC32 of the raw pixel buffer
    uint32_t huffTableSize; // size of serialized Huffman table in bytes
    uint64_t numRLESymbols; // number of RLE-encoded symbols
    uint64_t compressedBits;// number of bits in Huffman-encoded payload
};
#pragma pack(pop)

// ---------------------------------------------------------------
// Class: LosslessCompressor
// OOP concepts: INHERITANCE + FUNCTION OVERRIDING
//
// Implements a genuinely lossless image compression system
// FROM SCRATCH using:
//   Paeth predictive filtering → RLE → Huffman coding
//
// Produces a custom .limg file format.
// ---------------------------------------------------------------
class LosslessCompressor : public CompressionStrategy {
public:
    LosslessCompressor();

    /// Compress the image to the .limg format.
    /// The 'quality' parameter is accepted for interface compatibility
    /// but has no effect on lossless compression (always perfect).
    bool compress(const Image& image,
                  const std::string& outputPath,
                  int quality) override;

    std::string getMethodName() const override;

    // ---------------------------------------------------------------
    // Additional public methods (not part of the Strategy interface)
    // ---------------------------------------------------------------

    /// Decompress a .limg file back into a cv::Mat.
    /// Returns true on success and stores the result in `output`.
    static bool decompress(const std::string& limgPath, cv::Mat& output);

    /// Compress and immediately decompress, then verify pixel-perfect
    /// reconstruction. Prints detailed results to stdout.
    /// Returns true if the integrity check passes.
    static bool verifyRoundTrip(const Image& image,
                                const std::string& tempPath);

    /// Run a comprehensive test suite with synthetic images and print
    /// benchmark results. Designed to be called from main().
    static void runTests(const std::string& realImagePath = "");
};

#endif
