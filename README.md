# Smart Image Compressor

**C++ OOP Based Image Compression with Objective Quality Measurement**  
*C++17 • OpenCV • CMake*

---

## 1. Problem Statement

Digital images consume substantial disk space and bandwidth. Compressing image data reduces storage requirements and speeds up network transmission. However, aggressive compression introduces visual artifacts and destroys image clarity. The fundamental engineering trade-off lies in maximizing storage savings while preserving visual fidelity—and objectively quantifying image quality after compression.

---

## 2. Project Objective

The objective of this project is to develop a robust, modular console application in C++ that:
- Inspects and validates real image files from disk.
- Compresses images using both **Lossy (JPEG)** and **Lossless (PNG)** techniques.
- Computes genuine compression ratios, file sizes, and storage savings directly from disk.
- Evaluates visual degradation objectively by calculating **PSNR (Peak Signal-to-Noise Ratio)** using pixel-level comparisons via OpenCV.
- Demonstrates core Object-Oriented Programming (OOP) paradigms (Encapsulation, Inheritance, Polymorphism, Abstraction, and Composition) in an accessible, student-friendly architecture.

---

## 3. Implemented Features (Current Milestone)

| Feature | Description | Status |
|---|---|:---:|
| **Image Loading & Validation** | Validates existence, format (`.jpg`, `.jpeg`, `.png`, `.bmp`, `.tiff`), and read permissions | ✅ Implemented |
| **Metadata Inspection** | Displays dimensions (width × height), channels, and exact file size in Bytes, KB, and MB | ✅ Implemented |
| **Lossy Compression (JPEG)** | Configurable quality factor (10–100) using OpenCV encoder parameters | ✅ Implemented |
| **Lossless Compression (PNG)** | Configurable compression level (0–9) preserving exact pixel data | ✅ Implemented |
| **Disk-Based Metrics** | Computes compression ratio (`X:1`) and space saved (`%`) from real disk file sizes | ✅ Implemented |
| **Safety Warnings** | Alerts user if compressed size exceeds original, or if alpha channel is dropped during JPEG conversion | ✅ Implemented |
| **Before / After Comparison** | Displays structured comparison of original vs. compressed image attributes | ✅ Implemented |
| **PSNR Quality Calculation** | Computes Mean Squared Error (MSE) and Peak Signal-to-Noise Ratio across all channels | ✅ Implemented |
| **Quality Classification** | Categorizes lossy compression quality based on standard PSNR thresholds | ✅ Implemented |
| **Report Generation** | Exports structured plain-text summary report to `output/` directory | ✅ Implemented |

---

## 4. Planned Features (Future Scope)

| Feature | Description | Status |
|---|---|:---:|
| **SSIM Metric** | Structural Similarity Index for perceptual quality evaluation | 🔲 Planned / Not Implemented |
| **WebP Compression** | Support modern WebP lossy and lossless encoding as a new strategy subclass | 🔲 Planned / Not Implemented |
| **Batch Processing** | Compress multiple images or entire directories in a single operation | 🔲 Planned / Not Implemented |
| **Image Preview Window** | Graphical display window (`cv::imshow`) to visually inspect before/after results | 🔲 Planned / Not Implemented |
| **Resolution Scaling** | Optional spatial resizing before encoding to achieve target file sizes | 🔲 Planned / Not Implemented |
| **Target Size Optimizer** | Iterative search for quality factor meeting user-specified target size | 🔲 Planned / Not Implemented |

---

## 5. Technologies Used

- **Language:** C++17 (utilizing `std::filesystem` for accurate cross-platform file size measurement)
- **Image Processing Library:** OpenCV 4.x (`core`, `imgcodecs`, `imgproc`)
- **Build System:** CMake (version 3.10 or higher)

---

## 6. Object-Oriented Programming (OOP) Architecture

The application design emphasizes modular, extensible OOP principles:

| Concept | Implementation in Project |
|---|---|
| **Classes & Objects** | Concrete classes: `Image`, `CompressionResult`, `QualityMetrics`, `ImageCompressor`, `LossyCompressor`, `LosslessCompressor` |
| **Encapsulation** | Member variables are private. Controlled access is provided through public getters and validation routines |
| **Abstraction** | Abstract base class `CompressionStrategy` defines the contract (`compress()` and `getMethodName()`), hiding encoding mechanics |
| **Inheritance** | `LossyCompressor` and `LosslessCompressor` derive from `CompressionStrategy` |
| **Polymorphism** | `ImageCompressor` interacts with `CompressionStrategy*` uniformly at runtime via dynamic dispatch |
| **Virtual Functions** | Pure virtual functions in `CompressionStrategy` overridden with `override` keyword in derived strategies |
| **Constructors** | Default and parameterized constructors, with member initializer lists across all classes |
| **Composition** | `ImageCompressor` HAS-A `Image`, HAS-A `CompressionResult`, and `CompressionResult` HAS-A `QualityMetrics` |

```
                       ┌─────────────────────────┐
                       │   CompressionStrategy   │  (Abstract Base)
                       │ ─────────────────────── │
                       │ + compress()*           │
                       │ + getMethodName()*      │
                       └────────────┬────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
       ┌─────────────────────────┐     ┌─────────────────────────┐
       │     LossyCompressor     │     │   LosslessCompressor    │
       │ ─────────────────────── │     │ ─────────────────────── │
       │ (JPEG quality 10-100)   │     │ (PNG level 0-9)         │
       └────────────┬────────────┘     └────────────┬────────────┘
                    │                               │
                    └───────────────┬───────────────┘
                                    ▼
                       ┌─────────────────────────┐
                       │     ImageCompressor     │  (Context / Coordinator)
                       │ ─────────────────────── │
                       │ - strategy              │
                       │ - image                 │
                       │ - result                │
                       └────────────┬────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
       ┌─────────────────────────┐     ┌─────────────────────────┐
       │          Image          │     │    CompressionResult    │
       │ ─────────────────────── │     │ ─────────────────────── │
       │ (load, properties, raw) │     │ (ratio, space saved)    │
       └─────────────────────────┘     └────────────┬────────────┘
                                                    │
                                                    ▼
                                       ┌─────────────────────────┐
                                       │     QualityMetrics      │
                                       │ ─────────────────────── │
                                       │ (PSNR, quality status)  │
                                       └─────────────────────────┘
```

---

## 7. Repository Structure

```
. (Repository Root)
│
├── .gitignore                  ← Git tracking exclusions
├── CMakeLists.txt              ← CMake build configuration
├── README.md                   ← Project documentation
│
├── app/
│   └── main.cpp                ← Console user interface and application entry point
│
├── include/                    ← Header definitions
│   ├── Image.h                 ← Image representation and metadata
│   ├── CompressionResult.h     ← Compression statistics and reporting
│   ├── CompressionStrategy.h   ← Abstract base strategy interface
│   ├── LossyCompressor.h       ← JPEG lossy compressor declaration
│   ├── LosslessCompressor.h    ← PNG lossless compressor declaration
│   ├── ImageCompressor.h       ← Coordinator / context class
│   └── QualityMetrics.h        ← PSNR calculation and quality evaluation
│
├── src/                        ← Implementation files
│   ├── Image.cpp               ← Image loading and validation logic
│   ├── CompressionResult.cpp   ← Metric calculations and output formatting
│   ├── LossyCompressor.cpp     ← JPEG compression algorithm
│   ├── LosslessCompressor.cpp  ← PNG compression algorithm
│   ├── ImageCompressor.cpp     ← Pipeline coordination and report export
│   └── QualityMetrics.cpp      ← PSNR calculation via OpenCV
│
├── images/
│   ├── sample.bmp              ← Sample image for testing and demonstration
│   └── README.md               ← Notes on supported formats and sample image
│
└── output/
    ├── .gitkeep                ← Placeholder for directory tracking
    └── README.md               ← Output naming conventions and descriptions
```

---

## 8. Quality Evaluation Metrics

### Peak Signal-to-Noise Ratio (PSNR)
PSNR is a standard engineering metric used to evaluate image compression quality by comparing the original uncompressed image against the decompressed output image:

$$\text{MSE} = \frac{1}{M \cdot N \cdot C} \sum_{i,j,k} (I_1(i,j,k) - I_2(i,j,k))^2$$

$$\text{PSNR} = 10 \cdot \log_{10}\left(\frac{255^2}{\text{MSE}}\right) \text{ dB}$$

The application categorizes lossy quality into human-readable ratings:
- **$\ge$ 40 dB:** Excellent quality (compression artifacts virtually imperceptible)
- **30 to 40 dB:** Good quality (minor artifacts, visually acceptable for standard viewing)
- **20 to 30 dB:** Acceptable quality (visible degradation)
- **$<$ 20 dB:** Low quality (severe degradation, unacceptable distortion)

*Note on Lossless Compression:* For bit-for-bit identical images (e.g. lossless PNG), $\text{MSE} = 0$ resulting in infinite PSNR. The system detects this condition and reports `"Not available (Lossless / Identical)"` rather than displaying confusing mathematical infinities.

### Structural Similarity Index (SSIM)
- **Current Status:** Not implemented (Planned for future milestone).
- SSIM models human visual perception by measuring luminance, contrast, and structure.

---

## 9. Example Usage & Interface Format

The following excerpt demonstrates the interactive console menu workflow:

```text
=========================================
          SMART IMAGE COMPRESSOR
=========================================
1. Load Image
2. Display Image Information
3. Lossy Compression (JPEG)
4. Lossless Compression (PNG)
5. View Compression Result
6. Save Report to File
7. Exit

Enter your choice: 1
Enter image file path: images/sample.bmp
Image loaded successfully: images/sample.bmp

Enter your choice: 3
Enter JPEG quality (10-100): 80

========== COMPRESSION RESULT ==========
Original Size       : <calculated size> MB
Compressed Size     : <calculated size> KB
Compression Ratio   : <ratio> : 1
Space Saved         : <percentage>%

Method              : Lossy (JPEG)
JPEG Quality        : 80 / 100

PSNR                : <calculated dB> dB
SSIM                : Not available (planned for future milestone)

Quality Status      : Good (PSNR 30-40 dB)

Output File         : output/sample_compressed.jpg
========================================
```

> **Important Note:** All values in actual execution are computed dynamically from the files processed on disk. No metrics, sizes, or quality scores are hardcoded or fabricated.

---

## 10. Build Instructions

### Prerequisites
- A modern C++17 compatible compiler (MSVC 2019+, GCC 8+, Clang 7+)
- CMake 3.10 or higher
- OpenCV 4.x development libraries

### Windows (Visual Studio / MSVC — Recommended)
```bash
cmake -S . -B build -DOpenCV_DIR=C:/opencv/build
cmake --build build --config Release
```

### Windows (MinGW)
> *Note: MinGW requires version 8.0 or later for `std::filesystem` support.*
```bash
cmake -S . -B build -G "MinGW Makefiles" -DOpenCV_DIR=C:/opencv/build
cmake --build build
```

### Linux (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install -y build-essential cmake libopencv-dev
cmake -S . -B build
cmake --build build
```

### macOS
```bash
brew install cmake opencv
cmake -S . -B build
cmake --build build
```

---

## 11. Running the Application

Execute the binary from the **project root** directory so that relative paths (`images/sample.bmp`, `output/`) resolve correctly:

**Windows (MSVC):**
```powershell
.\build\Release\SmartImageCompressor.exe
```

**Windows (MinGW):**
```powershell
.\build\SmartImageCompressor.exe
```

**Linux / macOS:**
```bash
./build/SmartImageCompressor
```

---

## 12. Current Limitations

- **Console Only:** No graphical preview window or GUI.
- **Sequential Processing:** Operates on one image at a time (no batch directory pipeline).
- **Supported Encoders:** Supports JPEG (lossy) and PNG (lossless). Other modern formats (e.g. WebP, AVIF) are not yet integrated.
- **SSIM Unimplemented:** Perceptual quality analysis is currently limited to objective PSNR.
- **Lossless Size Growth:** Re-encoding already compressed images into PNG may result in equal or larger file sizes.
