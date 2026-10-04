#ifndef IMAGE_H
#define IMAGE_H

#include <iostream>
#include <string>
#include <opencv2/core.hpp>

// ---------------------------------------------------------------
// Class: Image
// OOP concept: ENCAPSULATION
// All data members are private. They can only be read through
// public getter functions and changed through loadImage().
// ---------------------------------------------------------------
class Image {
private:
    std::string fileName;
    std::string filePath;
    int width;
    int height;
    int channels;
    std::string format;
    long long fileSize;     // size on disk in bytes
    cv::Mat pixelData;      // the actual pixels (OpenCV matrix)
    bool loaded;

public:
    // Constructors
    Image();                                   // default constructor
    explicit Image(const std::string& path);   // parameterized constructor

    // Loads the image from disk and fills all attributes.
    bool loadImage(const std::string& path);

    // Prints the image properties.
    void displayInfo(std::ostream& out = std::cout) const;

    // Getters
    long long getFileSize() const;
    int getWidth() const;
    int getHeight() const;
    int getChannels() const;
    std::string getFormat() const;
    std::string getFileName() const;
    std::string getFilePath() const;
    const cv::Mat& getPixelData() const;
    bool isLoaded() const;

    // Helper functions (static: they belong to the class, not an object)
    static bool isSupportedFormat(const std::string& extension);
    static double bytesToKB(long long bytes);
    static double bytesToMB(long long bytes);
    static std::string formatSize(long long bytes);   // e.g. "4.82 MB"
};

#endif
