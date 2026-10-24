#pragma once

#include "BitWriter.h"
#include "BitReader.h"
#include <vector>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <queue>
#include <unordered_map>

namespace pixelforge {

/**
 * @brief Node in a binary Huffman Coding Tree.
 */
struct HuffmanNode {
    uint8_t symbol{0};
    uint64_t frequency{0};
    bool is_leaf{false};
    std::shared_ptr<HuffmanNode> left{nullptr};
    std::shared_ptr<HuffmanNode> right{nullptr};

    HuffmanNode() = default;
    HuffmanNode(uint8_t sym, uint64_t freq)
        : symbol(sym), frequency(freq), is_leaf(true) {}
    HuffmanNode(std::shared_ptr<HuffmanNode> l, std::shared_ptr<HuffmanNode> r)
        : symbol(0), frequency(l->frequency + r->frequency), is_leaf(false), left(l), right(r) {}
};

/**
 * @brief Comparator for min-priority queue to select lowest-frequency nodes first.
 */
struct HuffmanNodeComparator {
    bool operator()(const std::shared_ptr<HuffmanNode>& a, const std::shared_ptr<HuffmanNode>& b) const noexcept {
        return a->frequency > b->frequency;
    }
};

/**
 * @brief Represents a variable-length prefix code for a single symbol.
 */
struct HuffmanCode {
    uint32_t bits{0};
    int length{0};
};

/**
 * @brief Custom Canonical/Prefix Huffman Tree implementation for entropy encoding.
 * Implements greedy tree construction, pre-order bit-level serialization, and decoding.
 */
class HuffmanTree {
public:
    HuffmanTree() = default;

    /**
     * @brief Builds optimal prefix tree from 256-symbol frequency table.
     * @param frequencies Array of 256 symbol counts.
     */
    void buildFromFrequencies(const std::array<uint64_t, 256>& frequencies);

    /**
     * @brief Generates prefix codes for all symbols present in the tree.
     */
    std::array<HuffmanCode, 256> generateCodeTable() const;

    /**
     * @brief Encodes raw data using this Huffman tree and writes bitstream to BitWriter.
     * @param data Input byte buffer.
     * @param size Number of bytes.
     * @param writer BitWriter to append serialized tree and encoded payload to.
     */
    void encodeData(const uint8_t* data, size_t size, BitWriter& writer) const;

    /**
     * @brief Decodes a Huffman-encoded stream from a BitReader back to raw bytes.
     * @param reader BitReader positioned at the start of the serialized Huffman block.
     * @return Decoded byte vector.
     */
    static std::vector<uint8_t> decodeData(BitReader& reader);

    /**
     * @brief Serializes tree structure into BitWriter using pre-order bit traversal.
     */
    void serializeTree(BitWriter& writer) const;

    /**
     * @brief Deserializes tree structure from BitReader.
     */
    static std::shared_ptr<HuffmanNode> deserializeTree(BitReader& reader);

private:
    std::shared_ptr<HuffmanNode> root_{nullptr};

    void generateCodesRecursive(const std::shared_ptr<HuffmanNode>& node,
                                uint32_t current_bits,
                                int current_length,
                                std::array<HuffmanCode, 256>& table) const;

    void serializeTreeRecursive(const std::shared_ptr<HuffmanNode>& node, BitWriter& writer) const;
    static std::shared_ptr<HuffmanNode> deserializeTreeRecursive(BitReader& reader);
};

} // namespace pixelforge
