#pragma once

#include "Pixel.h"
#include <algorithm>
#include <cmath>

namespace pixelforge {

struct YCbCr {
    double y{0.0};
    double cb{0.0};
    double cr{0.0};

    YCbCr() = default;
    YCbCr(double y_val, double cb_val, double cr_val)
        : y(y_val), cb(cb_val), cr(cr_val) {}
};

/**
 * @brief Color space converter implementing ITU-R BT.601 standard equations.
 * Converts RGB to YCbCr (Luminance, Chrominance Blue, Chrominance Red) and back.
 */
class ColorSpace {
public:
    /**
     * @brief Converts a 24-bit RGB Pixel to YCbCr.
     * Equations:
     * Y  =  0.299 * R + 0.587 * G + 0.114 * B
     * Cb = -0.168736 * R - 0.331264 * G + 0.5 * B + 128.0
     * Cr =  0.5 * R - 0.418688 * G - 0.081312 * B + 128.0
     */
    static inline YCbCr rgbToYCbCr(const Pixel& px) noexcept {
        double r = static_cast<double>(px.r);
        double g = static_cast<double>(px.g);
        double b = static_cast<double>(px.b);

        double y  =  0.299000 * r + 0.587000 * g + 0.114000 * b;
        double cb = -0.168736 * r - 0.331264 * g + 0.500000 * b + 128.0;
        double cr =  0.500000 * r - 0.418688 * g - 0.081312 * b + 128.0;

        return YCbCr(y, cb, cr);
    }

    /**
     * @brief Converts YCbCr back to a 24-bit RGB Pixel with strict [0, 255] clamping.
     * Equations:
     * R = Y + 1.402000 * (Cr - 128.0)
     * G = Y - 0.344136 * (Cb - 128.0) - 0.714136 * (Cr - 128.0)
     * B = Y + 1.772000 * (Cb - 128.0)
     */
    static inline Pixel ycbcrToRGB(const YCbCr& ycb) noexcept {
        double y_val = ycb.y;
        double cb_shift = ycb.cb - 128.0;
        double cr_shift = ycb.cr - 128.0;

        double r = y_val + 1.402000 * cr_shift;
        double g = y_val - 0.344136 * cb_shift - 0.714136 * cr_shift;
        double b = y_val + 1.772000 * cb_shift;

        return Pixel(Pixel::clamp(static_cast<int>(std::round(r))),
                     Pixel::clamp(static_cast<int>(std::round(g))),
                     Pixel::clamp(static_cast<int>(std::round(b))));
    }
};

} // namespace pixelforge
