# PixelForge — Viva Voce Examination & Defense Guide
> **Comprehensive Questions, Mathematical Derivations & Algorithmic Rationale**

This guide is structured specifically for your college Object-Oriented Programming (OOP) and Data Structures / Algorithms project viva.

---

## 1. Project Concept & Architecture

### Q1: What is the core objective of PixelForge?
**Answer:** PixelForge is a dual-mode image compression platform implemented from first principles in modern C++ (C++17) with zero third-party compression libraries (no OpenCV, libjpeg, libpng, or stb_image). It demonstrates two fundamentally different compression paradigms:
1. **Lossless Engine (`.pfl`)**: Produces 100% bit-exact pixel reconstruction ($MSE = 0, PSNR = \infty$) by eliminating statistical and spatial redundancy using spatial prediction (Paeth/Average/Left/Up), reversible modulo-256 residual mapping, custom chunked Run-Length Encoding (RLE), and canonical Huffman entropy coding.
2. **Lossy Engine (`.pfz`)**: Achieves dramatic compression ratios (15x to 30x+) by eliminating psychoacoustic redundancy using ITU-R BT.601 $RGB \leftrightarrow YCbCr$ color conversion, $8 \times 8$ block partitioning, pure mathematical 2D Discrete Cosine Transform (2D-DCT-II), quality-scaled quantization matrices ($Q \in [10, 100]$), diagonal zigzag scanning, and zero-run Huffman coding.

### Q2: How is the web frontend connected to the C++ engine without third-party networking libraries?
**Answer:** We utilize the native **Windows Sockets 2 (Winsock2) API** (`winsock2.h` backed by `ws2_32.dll` in Windows 11). MinGW GCC links this via `-lws2_32`. 
We wrote an embedded C++ `HttpServer` class that:
- Creates a native TCP stream socket, binds to `127.0.0.1:8080`, and listens for connections.
- Parses HTTP/1.1 requests (`GET` to serve vanilla HTML5/CSS3/ES6 assets; `POST` to handle `/api/compress` and `/api/decompress` with Base64 payloads).
- Operates entirely in memory without requiring external runtimes (like Node.js or Python) or heavy C++ networking frameworks (like Boost.Beast or Crow).

---

## 2. Object-Oriented Programming (OOP) Design Principles

### Q3: How does PixelForge demonstrate core OOP principles?
**Answer:**
1. **Encapsulation & Data Hiding:**
   - `Pixel`: Encapsulates 24-bit RGB channel state, preventing invalid channel values with `Pixel::clamp()`.
   - `Image`: Manages dynamic contiguous memory allocation (`std::vector<Pixel>`) using RAII, protecting the raw buffer and providing bounds-checked `.getPixel(x,y)` and fast `.at(x,y)`.
   - `BitWriter` & `BitReader`: Encapsulate sub-byte bit manipulation, hiding bit accumulators, byte-flushing, and stream position tracking from the higher-level Huffman engine.
2. **Abstraction & Interfaces (Polymorphism / Strategy Pattern):**
   - The abstract base class `IPredictor` defines a pure virtual interface `predict(a, b, c)`.
   - Concrete derived classes `NonePredictor`, `LeftPredictor`, `UpPredictor`, `AveragePredictor`, and `PaethPredictor` override the prediction algorithm.
   - The `LosslessEngine` interacts solely with `IPredictor*` through dynamic polymorphism, allowing runtime selection of the best predictor without changing engine code.
3. **Single Responsibility Principle (SRP):**
   - File format parsing (`BMPReader`, `BMPWriter`) is completely decoupled from image representation (`Image`) and compression logic (`LosslessEngine`, `LossyEngine`).
4. **Composition over Inheritance:**
   - `LosslessEngine` is composed of `IPredictor`, `RLEEngine`, `HuffmanTree`, and `BitWriter`.
   - `LossyEngine` is composed of `DCT`, `ColorSpace`, and `HuffmanTree`.

---

## 3. Image Format & BMP Parsing

### Q4: Why must struct packing (`#pragma pack(push, 1)`) be used when reading BMP headers?
**Answer:** Modern C++ compilers align struct members on 4-byte or 8-byte boundaries to optimize CPU memory access. 
The standard Windows `BITMAPFILEHEADER` is 14 bytes:
- `uint16_t bfType` (2 bytes)
- `uint32_t bfSize` (4 bytes)
- `uint16_t bfReserved1` (2 bytes)
- `uint16_t bfReserved2` (2 bytes)
- `uint32_t bfOffBits` (4 bytes)
Without 1-byte packing, the compiler inserts 2 padding bytes after `bfType` to align `bfSize` on a 4-byte boundary, making `sizeof(BITMAPFILEHEADER)` 16 bytes instead of 14 bytes. This shifts all subsequent header reads by 2 bytes, corrupting image dimensions and pixel data. We enforce `#pragma pack(push, 1)` and verify it at compile-time with:
```cpp
static_assert(sizeof(BMPFileHeader) == 14, "BMPFileHeader must be exactly 14 bytes");
static_assert(sizeof(BMPInfoHeader) == 40, "BMPInfoHeader must be exactly 40 bytes");
```

### Q5: How do you handle BMP row padding and why does it exist?
**Answer:** The Windows DIB specification requires that each row of pixel data start on a 4-byte boundary. For a 24-bit image (3 bytes per pixel), the row byte count is $Width \times 3$. If $(Width \times 3)$ is not divisible by 4, zero-padding bytes are appended:
$$\text{RowStride} = \left\lfloor \frac{Width \times 3 + 3}{4} \right\rfloor \times 4$$
$$\text{Padding} = (4 - (Width \times 3) \pmod 4) \pmod 4$$
If an image is $3 \times 3$, each row has $3 \times 3 = 9$ color bytes + 3 padding bytes = 12 bytes. If the reader does not skip these 3 padding bytes, the subsequent scanlines will read padding bytes as color channels, resulting in diagonal image shearing.

---

## 4. Lossless Compression Pipeline

### Q6: Explain the Paeth Predictor and why modulo-256 residual mapping is reversible.
**Answer:** 
The Paeth predictor (used in PNG) estimates current pixel $X$ from Left ($A$), Up ($B$), and Up-Left ($C$):
1. Compute base estimate: $p = A + B - C$.
2. Compute absolute distances: $D_A = |p - A|$, $D_B = |p - B|$, $D_C = |p - C|$.
3. Choose the neighbor corresponding to the minimum distance.
To map residuals without negative values or extra sign bits, we use unsigned 8-bit arithmetic modulo 256:
$$\text{Residual} = (X - \hat{X}) \pmod{256}$$
$$\text{Reconstruction} = (\hat{X} + \text{Residual}) \pmod{256}$$
Because $X, \hat{X} \in [0, 255]$, unsigned 8-bit integer wrap-around (`uint8_t`) in C++ implements this modulo arithmetic natively in hardware with zero CPU branches.

### Q7: How does your custom RLE prevent escape-sequence collisions?
**Answer:** We implemented a chunked packet protocol where every packet starts with a control byte:
- **Run Packet:** If MSB is 1 (`ctrl & 0x80`), length is `(ctrl & 0x7F) + 3` (range 3..130), followed by 1 byte value.
- **Literal Packet:** If MSB is 0, length is `ctrl + 1` (range 1..128), followed by `Length` literal bytes.
Because the control byte explicitly specifies the packet type and length before the payload, there are no special escape bytes. Any arbitrary binary sequence (including `0x00`, `0xFF`, or random noise) is handled unambiguously without collisions.

### Q8: How does Huffman Tree construction work, and how is the tree serialized?
**Answer:**
1. **Construction:** A 256-entry frequency histogram is calculated. Non-zero frequencies are inserted into a min-priority queue (`std::priority_queue` with custom comparator). The two lowest-frequency nodes are repeatedly popped, merged under a new parent node whose frequency is their sum, and re-inserted until one node remains (the root).
2. **Tree Serialization:** To avoid storing large frequency tables, we serialize the tree using bit-level pre-order traversal:
   - For an internal node: write a single `0` bit, recurse left, recurse right.
   - For a leaf node: write a single `1` bit, followed by the 8-bit symbol byte.
   This serializes the entire 256-symbol tree in fewer than 320 bytes.

---

## 5. Lossy Compression Pipeline

### Q9: Why is RGB converted to YCbCr before compression?
**Answer:** The Human Visual System (HVS) has much higher sensitivity to brightness and spatial luminance variations ($Y$) than to color hue and saturation ($Cb, Cr$). In RGB, spatial details are redundant across all three channels. Converting to $YCbCr$ decorrelates brightness from chrominance, allowing aggressive quantization on $Cb$ and $Cr$ with minimal perceived quality degradation.

### Q10: What is the Discrete Cosine Transform (2D-DCT), and why is it preferred over FFT?
**Answer:** The 2D-DCT-II maps an $8 \times 8$ spatial block of pixel values $f(x, y)$ into frequency coefficients $F(u, v)$:
$$F(u, v) = \frac{1}{4} C(u) C(v) \sum_{x=0}^{7} \sum_{y=0}^{7} f(x, y) \cos\left[ \frac{(2x+1)u\pi}{16} \right] \cos\left[ \frac{(2y+1)v\pi}{16} \right]$$
**Advantages over FFT:**
1. **Real-Valued:** Uses purely real cosine coefficients; requires no complex imaginary numbers.
2. **Energy Compaction:** Natural images have high spatial correlation; DCT concentrates almost all signal energy into the top-left DC coefficient ($u=0, v=0$) and low AC frequencies.
3. **Boundary Continuity:** DCT implicitly mirrors the block at boundaries (even symmetry), preventing Gibbs phenomenon ringing artifacts that occur with FFT.

### Q11: Explain Quantization and how the Quality Factor ($Q$) controls compression.
**Answer:** Quantization is the only lossy step in the entire pipeline. It divides each frequency coefficient by a corresponding entry in a psychoacoustic quantization matrix:
$$F_Q(u, v) = \text{round}\left( \frac{F(u, v)}{Q_{\text{matrix}}(u, v)} \right)$$
High-frequency entries in $Q_{\text{matrix}}$ are large, rounding small high-frequency variations to exact zeros. 
The quality factor $Q \in [10, 100]$ scales the matrix using the standard formula:
$$S = \begin{cases} \lfloor 5000 / Q \rfloor, & \text{if } Q < 50 \\ 200 - 2Q, & \text{if } Q \ge 50 \end{cases}$$
$$Q_{\text{scaled}}(u, v) = \max\left(1, \min\left(255, \left\lfloor \frac{Q_{\text{base}}(u, v) \times S + 50}{100} \right\rfloor \right)\right)$$
- When $Q = 10$: $S = 500$, quantization is coarse, creating long zero runs and small file sizes with noticeable compression artifacts.
- When $Q = 90$: $S = 20$, quantization is fine, preserving subtle details and achieving high PSNR ($>45$ dB).

### Q12: Why is Zigzag Scanning performed?
**Answer:** An $8 \times 8$ matrix stores frequencies in a 2D grid where low frequencies sit at the top-left and high frequencies sit at the bottom-right. Quantization sets high frequencies to zero. Zigzag scanning traverses diagonals of increasing spatial frequency, grouping all non-zero coefficients at the beginning and consolidating all zeros at the end. An End-of-Block (EOB) marker terminates the block, avoiding storing dozens of trailing zeros.

---

## 6. Mathematical Quality Metrics

### Q13: Explain MSE and PSNR.
**Answer:**
1. **Mean Squared Error (MSE):** Measures the average squared difference between original and reconstructed pixel values:
   $$\text{MSE} = \frac{1}{3 \cdot W \cdot H} \sum_{c \in \{R,G,B\}} \sum_{y=0}^{H-1} \sum_{x=0}^{W-1} (I_{\text{orig}}(x,y,c) - I_{\text{recon}}(x,y,c))^2$$
2. **Peak Signal-to-Noise Ratio (PSNR):** Expresses logarithmic fidelity in decibels (dB) relative to maximum possible pixel power ($255^2$):
   $$\text{PSNR} = 10 \cdot \log_{10}\left( \frac{255^2}{\text{MSE}} \right)$$
   - In lossless mode: $\text{MSE} = 0 \implies \text{PSNR} = \infty$.
   - Typical high-quality lossy reconstruction yields $\text{PSNR} \ge 35\text{--}45\text{ dB}$.

---

### Q7: Why does saving an image back into 24-bit BMP produce the exact same file size on disk, and how does PixelForge achieve genuine BMP-to-BMP size reduction?

**Answer:**
1. **The Mathematical Nature of Raw 24-bit BMP:**
   The standard Windows 24-bit BMP specification (`BI_RGB`, `biCompression = 0`) defines a fixed uncompressed raster. Every scanline must store 3 raw bytes ($B, G, R$) per pixel, followed by padding bytes to align to a 4-byte boundary:
   $$\text{Row Stride} = \left\lfloor \frac{\text{Width} \times 3 + 3}{4} \right\rfloor \times 4$$
   $$\text{Total File Size} = 14\,(\text{FileHeader}) + 40\,(\text{InfoHeader}) + \text{Row Stride} \times \text{Height}$$
   Because raw 24-bit `BI_RGB` allocates 3 bytes per pixel regardless of pixel entropy or visual redundancy, re-saving any reconstructed image into 24-bit `BI_RGB` will mathematically always produce the identical byte size on disk.

2. **PixelForge's Dual Solution:**
   - **Custom Archive Containers (`.pfl` / `.pfz`):** For maximum compression ($70\%\text{--}98\%$ reduction), we designed project-specific container formats implementing Paeth prediction, chunked RLE, Canonical Huffman coding, and 2D-DCT quantization.
   - **Native Windows Compressed BMP Format (`BI_RLE8`):** To provide a genuine smaller `.bmp` file on disk that opens natively in Windows Photos, Paint, and browser engines, we built `BMPRLE8Writer` and integrated `BI_RLE8` decoding into `BMPReader`. It quantizes colors to a 256-color palette (1024 bytes) and encodes repeated pixels as run-length byte pairs `(count, index)`. For example, a $196\text{ KB}$ input BMP compresses down to $2.6\text{ KB}$ ($98.7\%$ reduction) while remaining a valid `.bmp` file!

---

### Q8: How does Windows 8-bit Run-Length Encoded BMP (`BI_RLE8`) work internally?

**Answer:**
`BI_RLE8` (`biCompression = 1`) is the official Microsoft standard for compressed bitmaps.
It consists of:
1. `BITMAPFILEHEADER` (14 bytes) + `BITMAPINFOHEADER` (40 bytes, with `biBitCount = 8` and `biCompression = 1`).
2. An indexed color palette of 256 `RGBQUAD` structures (1024 bytes: Blue, Green, Red, Reserved).
3. Encoded byte stream evaluated as 2-byte pairs:
   - **Encoded Mode (`count > 0`):** The first byte is the repetition count ($1\text{--}255$), and the second byte is the palette index. Repeats that color across the scanline.
   - **Absolute / Escape Mode (`count == 0`):**
     - Second byte $= 0$: End of scanline marker.
     - Second byte $= 1$: End of bitmap marker.
     - Second byte $= 2$: Delta marker (next 2 bytes contain $\Delta x, \Delta y$).
     - Second byte $\ge 3$: Absolute uncompressed run of $N$ palette indices (padded to 16-bit word alignment if $N$ is odd).
We implemented both the `BMPRLE8Writer` serializer and the `BMPReader` decoder from first principles without external libraries.
