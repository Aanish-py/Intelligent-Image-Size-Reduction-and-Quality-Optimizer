#include "Image.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <opencv2/imgcodecs.hpp>

namespace fs = std::filesystem;

namespace {
// Converts text to lower case (".JPG" -> ".jpg")
std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
}

// ---------- Constructors ----------
Image::Image()
    : fileName(""), filePath(""), width(0), height(0), channels(0),
      format(""), fileSize(0), loaded(false) {}

Image::Image(const std::string& path) : Image() {
    loadImage(path);
}

// ---------- Loading ----------
bool Image::loadImage(const std::string& path) {
    loaded = false;
    fs::path imagePath(path);
    std::error_code ec;

    // 1. File must exist
    if (!fs::exists(imagePath, ec) || !fs::is_regular_file(imagePath, ec)) {
        std::cerr << "Error: File does not exist: " << path << "\n";
        return false;
    }

    // 2. Format must be supported
    std::string extension = toLower(imagePath.extension().string());
    if (!isSupportedFormat(extension)) {
        std::cerr << "Error: Unsupported format '" << extension
                  << "'. Supported: .jpg .jpeg .png .bmp .tiff .tif\n";
        return false;
    }

    // 3. Image must load successfully (IMREAD_UNCHANGED keeps real channels)
    cv::Mat data = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (data.empty()) {
        std::cerr << "Error: OpenCV could not read the image (file may be corrupt).\n";
        return false;
    }

    // 4. File size from std::filesystem
    std::uintmax_t size = fs::file_size(imagePath, ec);
    if (ec) {
        std::cerr << "Error: Could not read file size.\n";
        return false;
    }

    // Everything is fine: store the values
    pixelData = data;
    width = data.cols;
    height = data.rows;
    channels = data.channels();
    fileName = imagePath.filename().string();
    filePath = fs::absolute(imagePath).string();
    format = toLower(extension.substr(1));      // "jpg", "png", ...
    std::transform(format.begin(), format.end(), format.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    fileSize = static_cast<long long>(size);
    loaded = true;
    return true;
}

// ---------- Display ----------
void Image::displayInfo(std::ostream& out) const {
    if (!loaded) {
        out << "No image loaded.\n";
        return;
    }
    out << "\n========== IMAGE INFORMATION ==========\n";
    out << std::left;
    out << std::setw(12) << "File Name"  << ": " << fileName << "\n";
    out << std::setw(12) << "File Path"  << ": " << filePath << "\n";
    out << std::setw(12) << "Format"     << ": " << format << "\n";
    out << std::setw(12) << "Width"      << ": " << width << " px\n";
    out << std::setw(12) << "Height"     << ": " << height << " px\n";
    out << std::setw(12) << "Channels"   << ": " << channels << "\n";
    out << std::fixed << std::setprecision(2);
    out << std::setw(12) << "Size"       << ": " << fileSize << " Bytes\n";
    out << std::setw(12) << ""           << "  " << bytesToKB(fileSize) << " KB\n";
    out << std::setw(12) << ""           << "  " << bytesToMB(fileSize) << " MB\n";
    out << "=======================================\n";
}

// ---------- Getters ----------
long long Image::getFileSize() const { return fileSize; }
int Image::getWidth() const { return width; }
int Image::getHeight() const { return height; }
int Image::getChannels() const { return channels; }
std::string Image::getFormat() const { return format; }
std::string Image::getFileName() const { return fileName; }
std::string Image::getFilePath() const { return filePath; }
const cv::Mat& Image::getPixelData() const { return pixelData; }
bool Image::isLoaded() const { return loaded; }

// ---------- Static helpers ----------
bool Image::isSupportedFormat(const std::string& extension) {
    std::string ext = toLower(extension);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
           ext == ".bmp" || ext == ".tiff" || ext == ".tif";
}

double Image::bytesToKB(long long bytes) {
    return static_cast<double>(bytes) / 1024.0;
}

double Image::bytesToMB(long long bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

std::string Image::formatSize(long long bytes) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(2);
    if (bytes >= 1024LL * 1024LL) {
        text << bytesToMB(bytes) << " MB";
    } else if (bytes >= 1024LL) {
        text << bytesToKB(bytes) << " KB";
    } else {
        text << bytes << " Bytes";
    }
    return text.str();
}
