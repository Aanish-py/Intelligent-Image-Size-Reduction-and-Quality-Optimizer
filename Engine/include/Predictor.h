#pragma once

#include "Pixel.h"
#include <cstdint>
#include <string>
#include <memory>
#include <cmath>

namespace pixelforge {

enum class PredictorType : uint8_t {
    None = 0,
    Left = 1,
    Up = 2,
    Average = 3,
    Paeth = 4
};

/**
 * @brief Abstract Base Class for spatial pixel prediction (Strategy Pattern).
 * Defines the contract for predicting pixel channel values based on spatial neighbors.
 */
class IPredictor {
public:
    virtual ~IPredictor() = default;

    /**
     * @brief Computes predicted pixel channel value from neighbors.
     * @param a Left neighbor (x-1, y)
     * @param b Upper neighbor (x, y-1)
     * @param c Upper-Left neighbor (x-1, y-1)
     * @return Predicted value in [0, 255]
     */
    virtual uint8_t predict(uint8_t a, uint8_t b, uint8_t c) const noexcept = 0;

    virtual PredictorType getType() const noexcept = 0;
    virtual std::string getName() const = 0;

    /**
     * @brief Computes the forward residual: (Actual - Predicted) mod 256.
     */
    static inline uint8_t computeResidual(uint8_t actual, uint8_t predicted) noexcept {
        return static_cast<uint8_t>(actual - predicted);
    }

    /**
     * @brief Inverses the residual: (Predicted + Residual) mod 256.
     */
    static inline uint8_t reconstruct(uint8_t predicted, uint8_t residual) noexcept {
        return static_cast<uint8_t>(predicted + residual);
    }
};

class NonePredictor final : public IPredictor {
public:
    uint8_t predict(uint8_t, uint8_t, uint8_t) const noexcept override {
        return 0;
    }
    PredictorType getType() const noexcept override { return PredictorType::None; }
    std::string getName() const override { return "None"; }
};

class LeftPredictor final : public IPredictor {
public:
    uint8_t predict(uint8_t a, uint8_t, uint8_t) const noexcept override {
        return a;
    }
    PredictorType getType() const noexcept override { return PredictorType::Left; }
    std::string getName() const override { return "Left"; }
};

class UpPredictor final : public IPredictor {
public:
    uint8_t predict(uint8_t, uint8_t b, uint8_t) const noexcept override {
        return b;
    }
    PredictorType getType() const noexcept override { return PredictorType::Up; }
    std::string getName() const override { return "Up"; }
};

class AveragePredictor final : public IPredictor {
public:
    uint8_t predict(uint8_t a, uint8_t b, uint8_t) const noexcept override {
        return static_cast<uint8_t>((static_cast<int>(a) + static_cast<int>(b)) / 2);
    }
    PredictorType getType() const noexcept override { return PredictorType::Average; }
    std::string getName() const override { return "Average"; }
};

class PaethPredictor final : public IPredictor {
public:
    uint8_t predict(uint8_t a, uint8_t b, uint8_t c) const noexcept override {
        // Paeth algorithm: computes base p = a + b - c and chooses closest neighbor
        int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
        int pa = std::abs(p - static_cast<int>(a));
        int pb = std::abs(p - static_cast<int>(b));
        int pc = std::abs(p - static_cast<int>(c));

        if (pa <= pb && pa <= pc) return a;
        if (pb <= pc) return b;
        return c;
    }
    PredictorType getType() const noexcept override { return PredictorType::Paeth; }
    std::string getName() const override { return "Paeth"; }
};

class PredictorFactory {
public:
    static std::unique_ptr<IPredictor> create(PredictorType type) {
        switch (type) {
            case PredictorType::Left:    return std::make_unique<LeftPredictor>();
            case PredictorType::Up:      return std::make_unique<UpPredictor>();
            case PredictorType::Average: return std::make_unique<AveragePredictor>();
            case PredictorType::Paeth:   return std::make_unique<PaethPredictor>();
            case PredictorType::None:
            default:                     return std::make_unique<NonePredictor>();
        }
    }
};

} // namespace pixelforge
