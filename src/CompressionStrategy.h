#ifndef COMPRESSION_STRATEGY_H
#define COMPRESSION_STRATEGY_H

#include <string>
#include "Image.h"

// ---------------------------------------------------------------
// Class: CompressionStrategy
// OOP concepts: ABSTRACTION + POLYMORPHISM
// This is an ABSTRACT class because it has pure virtual functions
// (= 0). You cannot create an object of it. It only defines WHAT
// every compressor must do; the child classes decide HOW.
// ---------------------------------------------------------------
class CompressionStrategy {
public:
    // Pure virtual function: every compressor must implement it.
    virtual bool compress(const Image& image,
                          const std::string& outputPath,
                          int quality) = 0;

    // Pure virtual function: name of the compression method.
    virtual std::string getMethodName() const = 0;

    // Virtual destructor: makes sure the child's destructor runs when
    // an object is deleted through a CompressionStrategy pointer.
    virtual ~CompressionStrategy() {}
};

#endif
