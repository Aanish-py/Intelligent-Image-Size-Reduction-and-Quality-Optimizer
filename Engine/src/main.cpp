#include "Image.h"
#include "BMPReader.h"
#include "BMPWriter.h"
#include "BMPRLE8Writer.h"
#include "LosslessEngine.h"
#include "LossyEngine.h"
#include "Metrics.h"
#include "HttpServer.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace pixelforge;

void printUsage() {
    std::cout << "\n=======================================================\n";
    std::cout << "PixelForge — Dual-Mode Image Compression Platform\n";
    std::cout << "Built from scratch with zero third-party compression libraries.\n";
    std::cout << "=======================================================\n";
    std::cout << "Usage:\n";
    std::cout << "  pixelforge --serve [port]                     Start embedded C++ Web Server (auto-fallback)\n";
    std::cout << "  pixelforge --compress-bmp <in.bmp> <out.bmp>  Compress BMP to standard 8-bit RLE BMP (smaller .bmp)\n";
    std::cout << "  pixelforge --compress-lossy-bmp <in.bmp> <out.bmp> [quality 10-100]\n";
    std::cout << "                                                Lossy DCT + RLE8 compression to smaller .bmp\n";
    std::cout << "  pixelforge --compress-lossless-bmp <in.bmp> <out.bmp>\n";
    std::cout << "                                                Direct BMP output via RLE8 compression\n";
    std::cout << "  pixelforge --compress-lossless <in.bmp> <out.pfl>\n";
    std::cout << "                                                Custom Lossless archive (Paeth + RLE + Huffman)\n";
    std::cout << "  pixelforge --decompress-lossless <in.pfl> <out.bmp>\n";
    std::cout << "                                                Decompress .pfl to 24-bit BMP\n";
    std::cout << "  pixelforge --compress-lossy <in.bmp> <out.pfz> [quality 10-100]\n";
    std::cout << "                                                Custom Lossy archive (2D-DCT + Quant + Huffman)\n";
    std::cout << "  pixelforge --decompress-lossy <in.pfz> <out.bmp>\n";
    std::cout << "                                                Decompress .pfz to 24-bit BMP\n";
    std::cout << "  pixelforge --decompress-bmp <in.bmp> <out.bmp>\n";
    std::cout << "                                                Decompress any BMP (including RLE8) to raw 24-bit BMP\n";
    std::cout << "  pixelforge --benchmark <in.bmp>               Run comprehensive comparison benchmark\n";
    std::cout << "=======================================================\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        // Default action: Launch web server!
        std::cout << "No CLI arguments provided. Launching PixelForge Web Server on port 8080...\n";
        HttpServer server(8080, "frontend");
        server.start();
        return 0;
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }

    if (command == "--serve") {
        int port = (argc >= 3) ? std::stoi(argv[2]) : 8080;
        HttpServer server(port, "frontend");
        server.start();
        return 0;
    }

    try {
        BMPReader reader;
        BMPWriter writer;
        BMPRLE8Writer rle_writer;

        // =====================================================================
        // Direct BMP-to-BMP Compression (Windows BI_RLE8 standard format)
        // =====================================================================
        if ((command == "--compress-bmp" || command == "--compress-lossless-bmp") && argc >= 4) {
            std::string in_path = argv[2];
            std::string out_path = argv[3];

            Image img = reader.readFromFile(in_path);
            std::vector<uint8_t> compressed_bmp = rle_writer.writeToMemory(img);

            std::ofstream out(out_path, std::ios::binary);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open output file: " + out_path);
            }
            out.write(reinterpret_cast<const char*>(compressed_bmp.data()), compressed_bmp.size());

            std::ifstream in_file(in_path, std::ios::binary | std::ios::ate);
            size_t orig_size = in_file.tellg();
            size_t comp_size = compressed_bmp.size();
            double ratio = (comp_size > 0) ? static_cast<double>(orig_size) / comp_size : 1.0;
            double reduction = (orig_size > 0) ? (1.0 - static_cast<double>(comp_size) / orig_size) * 100.0 : 0.0;

            std::cout << "[SUCCESS] BMP-to-BMP Compression Complete!\n";
            std::cout << "  Format:          Windows Standard BI_RLE8 (8-bit Run-Length Encoded BMP)\n";
            std::cout << "  Input File:      " << in_path << " (" << orig_size << " bytes)\n";
            std::cout << "  Compressed File: " << out_path << " (" << comp_size << " bytes)\n";
            std::cout << "  Reduction:       " << std::fixed << std::setprecision(1) << reduction << "%\n";
            std::cout << "  Ratio:           " << std::fixed << std::setprecision(2) << ratio << "x\n";
            std::cout << "  Status:          Native .bmp format viewable in Windows Photos, Paint, etc.\n";
            return 0;
        }

        // =====================================================================
        // Lossy DCT + BMP-to-BMP Compression
        // =====================================================================
        if (command == "--compress-lossy-bmp" && argc >= 4) {
            std::string in_path = argv[2];
            std::string out_path = argv[3];
            int quality = (argc >= 5) ? std::stoi(argv[4]) : 75;

            Image img = reader.readFromFile(in_path);
            LossyEngine engine;
            LossyStats stats;
            auto pfz = engine.compress(img, quality, &stats);
            Image reconstructed = engine.decompress(pfz);

            QualityReport report = Metrics::evaluate(img, reconstructed);
            std::vector<uint8_t> compressed_bmp = rle_writer.writeToMemory(reconstructed);

            std::ofstream out(out_path, std::ios::binary);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open output file: " + out_path);
            }
            out.write(reinterpret_cast<const char*>(compressed_bmp.data()), compressed_bmp.size());

            std::ifstream in_file(in_path, std::ios::binary | std::ios::ate);
            size_t orig_size = in_file.tellg();
            size_t comp_size = compressed_bmp.size();
            double ratio = (comp_size > 0) ? static_cast<double>(orig_size) / comp_size : 1.0;
            double reduction = (orig_size > 0) ? (1.0 - static_cast<double>(comp_size) / orig_size) * 100.0 : 0.0;

            std::cout << "[SUCCESS] Lossy DCT + BMP-to-BMP Compression Complete (Quality=" << quality << ")!\n";
            std::cout << "  Format:          Windows Standard BI_RLE8 (8-bit Run-Length Encoded BMP)\n";
            std::cout << "  Input File:      " << in_path << " (" << orig_size << " bytes)\n";
            std::cout << "  Compressed File: " << out_path << " (" << comp_size << " bytes)\n";
            std::cout << "  Reduction:       " << std::fixed << std::setprecision(1) << reduction << "%\n";
            std::cout << "  Ratio:           " << std::fixed << std::setprecision(2) << ratio << "x\n";
            std::cout << "  PSNR:            " << report.psnr << " dB (MSE: " << report.mse << ")\n";
            return 0;
        }

        // =====================================================================
        // Decompress any BMP (including RLE8) back to 24-bit raw uncompressed BMP
        // =====================================================================
        if (command == "--decompress-bmp" && argc >= 4) {
            std::string in_path = argv[2];
            std::string out_path = argv[3];

            Image img = reader.readFromFile(in_path);
            writer.writeToFile(img, out_path);

            std::cout << "[SUCCESS] BMP Decompression complete!\n";
            std::cout << "  Dimensions: " << img.getWidth() << "x" << img.getHeight() << "\n";
            std::cout << "  Output BMP: " << out_path << " (24-bit uncompressed RGB)\n";
            return 0;
        }

        // =====================================================================
        // Lossless Pipeline (.pfl)
        // =====================================================================
        if (command == "--compress-lossless" && argc >= 4) {
            std::string in_path = argv[2];
            std::string out_path = argv[3];

            Image img = reader.readFromFile(in_path);
            LosslessEngine engine;
            LosslessStats stats;
            auto pfl = engine.compress(img, PredictorType::Paeth, true, &stats);

            std::ofstream out(out_path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(pfl.data()), pfl.size());

            std::cout << "[SUCCESS] Lossless compression complete!\n";
            std::cout << "  Original:   " << stats.original_bytes << " bytes\n";
            std::cout << "  Compressed: " << stats.compressed_bytes << " bytes\n";
            std::cout << "  Ratio:      " << std::fixed << std::setprecision(2) << stats.compression_ratio << "x\n";
            std::cout << "  Reduction:  " << stats.percentage_reduction << "%\n";
            std::cout << "  Time:       " << stats.compression_time_ms << " ms\n";
            return 0;
        }

        if (command == "--decompress-lossless" && argc >= 4) {
            std::string in_path = argv[2];
            std::string out_path = argv[3];

            LosslessEngine engine;
            LosslessStats stats;
            Image img = engine.loadFromFile(in_path);
            writer.writeToFile(img, out_path);

            std::cout << "[SUCCESS] Lossless decompression complete!\n";
            std::cout << "  Dimensions: " << img.getWidth() << "x" << img.getHeight() << "\n";
            std::cout << "  Output BMP: " << out_path << "\n";
            return 0;
        }

        // =====================================================================
        // Lossy Pipeline (.pfz)
        // =====================================================================
        if (command == "--compress-lossy" && argc >= 4) {
            std::string in_path = argv[2];
            std::string out_path = argv[3];
            int quality = (argc >= 5) ? std::stoi(argv[4]) : 75;

            Image img = reader.readFromFile(in_path);
            LossyEngine engine;
            LossyStats stats;
            auto pfz = engine.compress(img, quality, &stats);

            std::ofstream out(out_path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(pfz.data()), pfz.size());

            // Decompress in memory to compute genuine MSE / PSNR
            Image recon = engine.decompress(pfz);
            QualityReport report = Metrics::evaluate(img, recon);

            std::cout << "[SUCCESS] Lossy compression complete (Quality=" << quality << ")!\n";
            std::cout << "  Original:   " << stats.original_bytes << " bytes\n";
            std::cout << "  Compressed: " << stats.compressed_bytes << " bytes\n";
            std::cout << "  Ratio:      " << std::fixed << std::setprecision(2) << stats.compression_ratio << "x\n";
            std::cout << "  Reduction:  " << stats.percentage_reduction << "%\n";
            std::cout << "  MSE:        " << report.mse << "\n";
            std::cout << "  PSNR:       " << report.psnr << " dB\n";
            std::cout << "  Time:       " << stats.compression_time_ms << " ms\n";
            return 0;
        }

        if (command == "--decompress-lossy" && argc >= 4) {
            std::string in_path = argv[2];
            std::string out_path = argv[3];

            LossyEngine engine;
            Image img = engine.loadFromFile(in_path);
            writer.writeToFile(img, out_path);

            std::cout << "[SUCCESS] Lossy decompression complete!\n";
            std::cout << "  Dimensions: " << img.getWidth() << "x" << img.getHeight() << "\n";
            std::cout << "  Output BMP: " << out_path << "\n";
            return 0;
        }

        // =====================================================================
        // Comprehensive Multi-Mode Benchmark
        // =====================================================================
        if (command == "--benchmark" && argc >= 3) {
            std::string in_path = argv[2];
            Image original = reader.readFromFile(in_path);

            std::ifstream in_file(in_path, std::ios::binary | std::ios::ate);
            size_t disk_orig_size = in_file.tellg();

            std::cout << "\n========================================================================\n";
            std::cout << "PixelForge Comprehensive Benchmark: " << in_path << " (" 
                      << original.getWidth() << "x" << original.getHeight() << ")\n";
            std::cout << "========================================================================\n";

            // 1. Lossless Container (.pfl)
            LosslessEngine lossless;
            LosslessStats lstats;
            auto pfl = lossless.compress(original, PredictorType::Paeth, true, &lstats);
            Image recon_lossless = lossless.decompress(pfl, &lstats);
            QualityReport lreport = Metrics::evaluate(original, recon_lossless);

            std::cout << "\n--- LOSSLESS CONTAINER (.pfl) ---\n";
            std::cout << "  Original Size:    " << lstats.original_bytes << " bytes\n";
            std::cout << "  Compressed Size:  " << lstats.compressed_bytes << " bytes\n";
            std::cout << "  Ratio:            " << lstats.compression_ratio << "x\n";
            std::cout << "  Reduction:        " << lstats.percentage_reduction << "%\n";
            std::cout << "  Compression Time: " << lstats.compression_time_ms << " ms\n";
            std::cout << "  Decomp. Time:     " << lstats.decompression_time_ms << " ms\n";
            std::cout << "  Bit-Exact Match:  " << (lstats.verified_exact ? "YES (Exact 100%)" : "NO") << "\n";
            std::cout << "  PSNR:             " << lreport.psnr << " dB\n";

            // 2. Windows 8-bit RLE BMP Format (.bmp)
            auto rle_bmp = rle_writer.writeToMemory(original);
            double rle_ratio = (rle_bmp.size() > 0) ? static_cast<double>(disk_orig_size) / rle_bmp.size() : 1.0;
            double rle_reduc = (disk_orig_size > 0) ? (1.0 - static_cast<double>(rle_bmp.size()) / disk_orig_size) * 100.0 : 0.0;

            std::cout << "\n--- WINDOWS COMPRESSED BMP (.bmp RLE8) ---\n";
            std::cout << "  Original BMP:     " << disk_orig_size << " bytes\n";
            std::cout << "  Compressed BMP:   " << rle_bmp.size() << " bytes\n";
            std::cout << "  Ratio:            " << std::fixed << std::setprecision(2) << rle_ratio << "x\n";
            std::cout << "  Reduction:        " << std::fixed << std::setprecision(1) << rle_reduc << "%\n";
            std::cout << "  Format:           Standard BI_RLE8 Windows Bitmap (Native image viewer compatible)\n";

            // 3. Lossy Container (.pfz) across quality presets
            std::cout << "\n--- LOSSY CONTAINER (.pfz) ACROSS QUALITY PRESETS ---\n";
            std::cout << std::left << std::setw(10) << "Quality" 
                      << std::setw(16) << "Size (Bytes)" 
                      << std::setw(12) << "Ratio" 
                      << std::setw(14) << "Reduction" 
                      << std::setw(12) << "MSE" 
                      << std::setw(12) << "PSNR (dB)" 
                      << "Time (ms)\n";
            std::cout << std::string(80, '-') << "\n";

            LossyEngine lossy;
            const int qualities[] = {25, 50, 75, 90};
            for (int q : qualities) {
                LossyStats qstats;
                auto pfz = lossy.compress(original, q, &qstats);
                Image recon_lossy = lossy.decompress(pfz, &qstats);
                QualityReport qreport = Metrics::evaluate(original, recon_lossy);

                std::cout << std::left << std::setw(10) << ("Q=" + std::to_string(q))
                          << std::setw(16) << qstats.compressed_bytes
                          << std::setw(12) << (std::to_string(qstats.compression_ratio).substr(0, 5) + "x")
                          << std::setw(14) << (std::to_string(qstats.percentage_reduction).substr(0, 5) + "%")
                          << std::setw(12) << std::to_string(qreport.mse).substr(0, 7)
                          << std::setw(12) << std::to_string(qreport.psnr).substr(0, 6)
                          << qstats.compression_time_ms << " ms\n";
            }
            std::cout << "========================================================================\n\n";
            return 0;
        }

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    printUsage();
    return 1;
}
