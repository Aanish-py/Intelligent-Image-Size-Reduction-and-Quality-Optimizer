#include "HuffmanTree.h"
#include <stdexcept>

namespace pixelforge {

void HuffmanTree::buildFromFrequencies(const std::array<uint64_t, 256>& frequencies) {
    std::priority_queue<std::shared_ptr<HuffmanNode>, 
                        std::vector<std::shared_ptr<HuffmanNode>>, 
                        HuffmanNodeComparator> pq;

    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            pq.push(std::make_shared<HuffmanNode>(static_cast<uint8_t>(i), frequencies[i]));
        }
    }

    if (pq.empty()) {
        root_ = nullptr;
        return;
    }

    // Special Case: Exactly one unique symbol
    if (pq.size() == 1) {
        auto single_leaf = pq.top();
        pq.pop();
        // Create a root with the single leaf on left and an empty leaf on right
        root_ = std::make_shared<HuffmanNode>(single_leaf, std::make_shared<HuffmanNode>(0, 0));
        return;
    }

    // Standard Huffman Greedy Merge
    while (pq.size() > 1) {
        auto left = pq.top();
        pq.pop();
        auto right = pq.top();
        pq.pop();

        auto parent = std::make_shared<HuffmanNode>(left, right);
        pq.push(parent);
    }

    root_ = pq.top();
}

std::array<HuffmanCode, 256> HuffmanTree::generateCodeTable() const {
    std::array<HuffmanCode, 256> table{};
    if (root_) {
        generateCodesRecursive(root_, 0, 0, table);
    }
    return table;
}

void HuffmanTree::generateCodesRecursive(const std::shared_ptr<HuffmanNode>& node,
                                         uint32_t current_bits,
                                         int current_length,
                                         std::array<HuffmanCode, 256>& table) const {
    if (!node) return;

    if (node->is_leaf) {
        // Handle single root leaf special case
        if (current_length == 0) {
            table[node->symbol] = HuffmanCode{0, 1};
        } else {
            table[node->symbol] = HuffmanCode{current_bits, current_length};
        }
        return;
    }

    generateCodesRecursive(node->left, (current_bits << 1) | 0, current_length + 1, table);
    generateCodesRecursive(node->right, (current_bits << 1) | 1, current_length + 1, table);
}

void HuffmanTree::serializeTree(BitWriter& writer) const {
    if (!root_) {
        writer.writeBit(0);
        return;
    }
    writer.writeBit(1); // Has tree flag
    serializeTreeRecursive(root_, writer);
}

void HuffmanTree::serializeTreeRecursive(const std::shared_ptr<HuffmanNode>& node, BitWriter& writer) const {
    if (node->is_leaf) {
        writer.writeBit(1); // 1 indicates Leaf Node
        writer.writeBits(node->symbol, 8);
    } else {
        writer.writeBit(0); // 0 indicates Internal Node
        serializeTreeRecursive(node->left, writer);
        serializeTreeRecursive(node->right, writer);
    }
}

std::shared_ptr<HuffmanNode> HuffmanTree::deserializeTree(BitReader& reader) {
    int has_tree = reader.readBit();
    if (!has_tree) {
        return nullptr;
    }
    return deserializeTreeRecursive(reader);
}

std::shared_ptr<HuffmanNode> HuffmanTree::deserializeTreeRecursive(BitReader& reader) {
    int is_leaf = reader.readBit();
    if (is_leaf) {
        uint8_t symbol = static_cast<uint8_t>(reader.readBits(8));
        return std::make_shared<HuffmanNode>(symbol, 0);
    } else {
        auto left = deserializeTreeRecursive(reader);
        auto right = deserializeTreeRecursive(reader);
        return std::make_shared<HuffmanNode>(left, right);
    }
}

void HuffmanTree::encodeData(const uint8_t* data, size_t size, BitWriter& writer) const {
    if (!data || size == 0) {
        writer.writeBits(0, 32); // 0 symbols
        return;
    }

    // 1. Serialize the tree
    serializeTree(writer);

    // 2. Write total symbol count (32-bit unsigned)
    writer.writeBits(static_cast<uint32_t>(size), 32);

    // 3. Write prefix codes for each input symbol
    std::array<HuffmanCode, 256> table = generateCodeTable();
    for (size_t i = 0; i < size; ++i) {
        const HuffmanCode& code = table[data[i]];
        if (code.length == 0) {
            throw std::runtime_error("HuffmanTree: Attempted to encode symbol with no prefix code.");
        }
        writer.writeBits(code.bits, code.length);
    }
}

std::vector<uint8_t> HuffmanTree::decodeData(BitReader& reader) {
    std::shared_ptr<HuffmanNode> root = deserializeTree(reader);
    uint32_t total_symbols = reader.readBits(32);

    std::vector<uint8_t> output;
    if (total_symbols == 0 || !root) {
        return output;
    }

    output.reserve(total_symbols);

    for (uint32_t i = 0; i < total_symbols; ++i) {
        auto current = root;
        while (!current->is_leaf) {
            int bit = reader.readBit();
            current = (bit == 0) ? current->left : current->right;
            if (!current) {
                throw std::runtime_error("HuffmanTree::decodeData: Corrupted tree path during decoding.");
            }
        }
        output.push_back(current->symbol);
    }

    return output;
}

} // namespace pixelforge
