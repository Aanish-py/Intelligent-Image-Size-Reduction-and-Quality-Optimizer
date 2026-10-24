# PixelForge
> **A Dual-Mode Image Compression Platform Using Custom-Built Algorithms in C++**

PixelForge is an academic Object-Oriented Programming (OOP) capstone project engineered in modern C++ (C++17) on Windows 11. It provides both **Lossless** and **Lossy** image compression from first principles without relying on third-party compression libraries (e.g., no OpenCV, libjpeg, libpng, stb_image, or external frameworks).

---

## 1. Project Architecture

The project is structured into three distinct layers:
1. **Core Engine (`Engine/`)**:
   - `Pixel`, `Image`: Pixel abstraction and memory management.
   - `BMPReader`, `BMPWriter`: Scratch parser/writer for standard 24-bit uncompressed Windows BMP images.
   - `LosslessEngine`: Spatial predictors (Left, Up, Average, Paeth), modulo-256 residual mapping, custom RLE, and Huffman entropy coding (`.pfl`).
   - `LossyEngine`: RGB $\leftrightarrow$ YCbCr conversion, $8 \times 8$ block partitioning, pure 2D-DCT & 2D-IDCT, quality-scaled quantization ($Q \in [10, 100]$), zigzag coefficient scan, and zero-run coding (`.pfz`).
   - `Metrics`: Mathematical MSE and PSNR calculation.
2. **Zero-Dependency Native Backend (`Engine/src/HttpServer.cpp`)**:
   - Built on native Windows Sockets 2 (`winsock2.h`, linking `ws2_32.lib` from the Windows OS SDK).
   - Zero third-party network libraries required.
   - Serves the frontend web dashboard and provides high-speed binary REST endpoints.
3. **Frontend Dashboard (`frontend/`)**:
   - Responsive, modern vanilla HTML5, CSS3, and modern JavaScript web application.
   - Pages: Landing Overview, Interactive Compression Studio, Real-Time Results Inspector, and Visual Algorithm Explorer.

---

## 2. Directory Structure

```
Pixelforge/
├── CMakeLists.txt              # Root build configuration (C++17, MinGW-w64)
├── README.md                   # Project overview and documentation
├── Engine/
│   ├── CMakeLists.txt          # Engine library & test targets
│   ├── include/                # Modular OOP header files
│   │   ├── Pixel.h             # 24-bit RGB pixel encapsulation
│   │   ├── Image.h             # 2D image matrix container
│   │   ├── BMPCommon.h         # Packed BMP headers and row stride math
│   │   ├── BMPReader.h         # 24-bit BMP parser
│   │   └── BMPWriter.h         # 24-bit BMP serializer
│   ├── src/                    # Implementation files
│   │   ├── Image.cpp
│   │   ├── BMPReader.cpp
│   │   └── BMPWriter.cpp
│   └── tests/                  # Verification test suites
│       ├── test_bmp.cpp        # BMP round-trip & padding edge cases
│       └── generate_samples.cpp# Synthetic test image generator
├── frontend/                   # Web interface (HTML5, CSS3, JavaScript)
├── docs/                       # Technical reports & Viva Voce defense notes
└── samples/                    # Sample test images
    ├── input/                  # Uncompressed BMP inputs
    └── output/                 # Compressed & reconstructed outputs
```

---

## 3. Building and Running

### Prerequisites
- Windows 11
- MinGW-w64 GCC 16.1.0+ (`g++`)
- CMake 3.20+
- Visual Studio Code

### PowerShell Build & Run Commands
```powershell
# 1. Navigate to the project directory
cd "E:\OOPS CP\Pixelforge"

# 2. Compile the engine, server, and test binaries
cmake --build build

# 3. Launch the Web Application (Auto-port fallback if port 8080 is busy)
./build/bin/pixelforge.exe --serve 8085

# Open browser at: http://localhost:8085

# 4. Or use the Direct CLI Tools:

# Compress directly to a smaller .bmp file on disk (Windows BI_RLE8 format):
./build/bin/pixelforge.exe --compress-bmp samples/input/blocks_256x256.bmp compressed_blocks.bmp

# Compress using Lossy DCT + BI_RLE8 directly to a smaller .bmp file:
./build/bin/pixelforge.exe --compress-lossy-bmp samples/input/gradient_256x256.bmp compressed_grad.bmp 75

# Compress to custom PixelForge Lossless container (.pfl):
./build/bin/pixelforge.exe --compress-lossless samples/input/blocks_256x256.bmp archive.pfl

# Decompress .pfl back to 24-bit BMP:
./build/bin/pixelforge.exe --decompress-lossless archive.pfl restored.bmp

# Compress to custom PixelForge Lossy container (.pfz):
./build/bin/pixelforge.exe --compress-lossy samples/input/gradient_256x256.bmp archive.pfz 75

# Decompress .pfz back to 24-bit BMP:
./build/bin/pixelforge.exe --decompress-lossy archive.pfz restored_lossy.bmp

# Run comprehensive comparative benchmark across all algorithms:
./build/bin/pixelforge.exe --benchmark samples/input/gradient_256x256.bmp

# Run automated test suites:
ctest --test-dir build --output-on-failure
```

---

## 4. Understanding BMP File Sizes & Compression Modes

During Viva / Evaluation, explain the distinction between raw BMPs, compressed BMPs, and custom containers:

| Format / Mode | File Extension | Size on Disk | Openable in Windows Photos / Paint? | Algorithm Used |
| :--- | :---: | :---: | :---: | :--- |
| **Original 24-bit BMP** | `.bmp` | $\text{Fixed: } 54 + \text{Stride} \times H$ | Yes | Uncompressed raw BGR raster (`BI_RGB`) |
| **Compressed RLE BMP** | `.bmp` | **$88\% \text{ to } 98\%$ smaller** | **Yes (Native Windows Standard)** | 256-color palette quantization + `BI_RLE8` run-length encoding |
| **Lossless Container** | `.pfl` | **$70\% \text{ to } 99\%$ smaller** | Decompress via PixelForge | Paeth 2D prediction + Chunked RLE + Canonical Huffman Tree |
| **Lossy Container** | `.pfz` | **$90\% \text{ to } 98\%$ smaller** | Decompress via PixelForge | YCbCr + $8 \times 8$ 2D-DCT + Quantization ($Q=10\text{--}100$) + Zigzag + Huffman |
| **Reconstructed 24-bit** | `.bmp` | $\text{Fixed: } 54 + \text{Stride} \times H$ | Yes | Reconstructed 24-bit raster from `.pfl`/`.pfz` |
