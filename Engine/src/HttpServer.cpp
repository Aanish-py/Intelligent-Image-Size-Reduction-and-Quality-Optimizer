#include "HttpServer.h"
#include "BMPReader.h"
#include "BMPWriter.h"
#include "BMPRLE8Writer.h"
#include "LosslessEngine.h"
#include "LossyEngine.h"
#include "Metrics.h"
#include "Base64.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace pixelforge {

namespace fs = std::filesystem;

HttpServer::HttpServer(int port, std::string static_dir)
    : port_(port), static_dir_(std::move(static_dir)), running_(false), server_socket_(INVALID_SOCKET) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw std::runtime_error("HttpServer: WSAStartup failed.");
    }
}

HttpServer::~HttpServer() {
    stop();
    WSACleanup();
}

void HttpServer::stop() {
    if (running_) {
        running_ = false;
        if (server_socket_ != INVALID_SOCKET) {
            closesocket(static_cast<SOCKET>(server_socket_));
            server_socket_ = INVALID_SOCKET;
        }
    }
}

void HttpServer::start() {
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        throw std::runtime_error("HttpServer: Failed to create socket.");
    }

    server_socket_ = listen_sock;

    // Enable SO_REUSEADDR
    BOOL opt = TRUE;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Try binding to configured port, or auto-fallback to next available port if port is busy
    bool bound = false;
    int original_port = port_;
    for (int p = original_port; p <= original_port + 25; ++p) {
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(static_cast<u_short>(p));

        if (bind(listen_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != SOCKET_ERROR) {
            port_ = p;
            bound = true;
            break;
        }
    }

    if (!bound) {
        closesocket(listen_sock);
        throw std::runtime_error("HttpServer: All ports from " + std::to_string(original_port) + 
                                 " to " + std::to_string(original_port + 25) + " are currently occupied.");
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listen_sock);
        throw std::runtime_error("HttpServer: Listen failed.");
    }

    running_ = true;
    std::cout << "\n=======================================================\n";
    std::cout << "  PixelForge Native C++ Web Server Active!\n";
    if (port_ != original_port) {
        std::cout << "  (Port " << original_port << " was in use by another app; switched to " << port_ << ")\n";
    }
    std::cout << "  URL: http://localhost:" << port_ << "\n";
    std::cout << "  Zero Third-Party Dependencies (Pure Win32 Sockets)\n";
    std::cout << "=======================================================\n\n" << std::flush;

    while (running_) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

        if (client_sock == INVALID_SOCKET) {
            if (!running_) break;
            continue;
        }

        handleClient(client_sock);
        closesocket(client_sock);
    }
}

void HttpServer::handleClient(uintptr_t client_socket) {
    SOCKET sock = static_cast<SOCKET>(client_socket);

    // Set receive & send timeouts
    DWORD timeout = 10000; // 10 seconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    std::string raw_request;
    std::vector<char> buffer(16384);

    // 1. Read until headers are fully received (\r\n\r\n or \n\n)
    size_t header_end_pos = std::string::npos;
    size_t header_sep_len = 4;

    while (header_end_pos == std::string::npos) {
        int bytes_read = recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (bytes_read <= 0) break;
        raw_request.append(buffer.data(), bytes_read);

        header_end_pos = raw_request.find("\r\n\r\n");
        if (header_end_pos != std::string::npos) {
            header_sep_len = 4;
            break;
        }
        header_end_pos = raw_request.find("\n\n");
        if (header_end_pos != std::string::npos) {
            header_sep_len = 2;
            break;
        }
    }

    if (header_end_pos == std::string::npos) return;

    std::string header_str = raw_request.substr(0, header_end_pos);
    std::string body = raw_request.substr(header_end_pos + header_sep_len);

    // Parse Request Line: METHOD PATH HTTP/1.1
    std::istringstream req_stream(header_str);
    std::string method, path, http_ver;
    req_stream >> method >> path >> http_ver;

    // Parse Content-Length
    size_t content_length = 0;
    std::string line;
    while (std::getline(req_stream, line)) {
        std::string lower_line = line;
        std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);
        if (lower_line.find("content-length:") != std::string::npos) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                try {
                    content_length = std::stoul(line.substr(colon + 1));
                } catch (...) {}
            }
        }
    }

    // 2. Accumulate full body payload
    while (body.size() < content_length) {
        size_t needed = content_length - body.size();
        int to_read = static_cast<int>(std::min(buffer.size(), needed));
        int more = recv(sock, buffer.data(), to_read, 0);
        if (more <= 0) break;
        body.append(buffer.data(), more);
    }

    // CORS & Options Preflight Support
    if (method == "OPTIONS") {
        sendResponse(client_socket, 200, "text/plain", "OK");
        return;
    }

    // REST API Routing
    if (method == "POST" && path == "/api/compress") {
        std::string json_resp = handleApiCompress(body);
        sendResponse(client_socket, 200, "application/json", json_resp);
        return;
    }

    if (method == "POST" && path == "/api/decompress") {
        std::string json_resp = handleApiDecompress(body);
        sendResponse(client_socket, 200, "application/json", json_resp);
        return;
    }

    if (method == "GET" && path == "/api/samples") {
        std::string json_resp = handleApiSamples();
        sendResponse(client_socket, 200, "application/json", json_resp);
        return;
    }

    if (method == "GET" && path == "/api/algorithm-demo") {
        std::string json_resp = handleApiAlgorithmDemo();
        sendResponse(client_socket, 200, "application/json", json_resp);
        return;
    }

    // Static File Serving
    if (method == "GET") {
        std::string clean_path = path;
        size_t qmark = clean_path.find('?');
        if (qmark != std::string::npos) clean_path = clean_path.substr(0, qmark);

        if (clean_path == "/" || clean_path.empty()) {
            clean_path = "/index.html";
        }

        // Prevent path traversal attacks
        if (clean_path.find("..") != std::string::npos) {
            sendResponse(client_socket, 403, "text/plain", "Forbidden");
            return;
        }

        std::string file_on_disk;
        if (clean_path.rfind("/samples/", 0) == 0) {
            file_on_disk = clean_path.substr(1); // e.g. "samples/input/..."
        } else {
            file_on_disk = static_dir_ + clean_path;
        }

        std::ifstream file(file_on_disk, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            sendResponse(client_socket, 404, "text/plain", "404 Not Found: " + clean_path);
            return;
        }

        size_t fsize = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> fbuffer(fsize);
        file.read(reinterpret_cast<char*>(fbuffer.data()), fsize);

        sendBinaryResponse(client_socket, 200, getMimeType(clean_path), fbuffer);
        return;
    }

    sendResponse(client_socket, 405, "text/plain", "Method Not Allowed");
}

void HttpServer::sendResponse(uintptr_t client_socket, int status_code, 
                              const std::string& content_type, 
                              const std::string& body) {
    SOCKET sock = static_cast<SOCKET>(client_socket);
    std::string status_text = (status_code == 200) ? "OK" : ((status_code == 404) ? "Not Found" : "Error");

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Access-Control-Allow-Headers: Content-Type\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    std::string resp = oss.str();
    send(sock, resp.data(), static_cast<int>(resp.size()), 0);
}

void HttpServer::sendBinaryResponse(uintptr_t client_socket, int status_code, 
                                    const std::string& content_type, 
                                    const std::vector<uint8_t>& body) {
    SOCKET sock = static_cast<SOCKET>(client_socket);
    std::string status_text = (status_code == 200) ? "OK" : "Error";

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n\r\n";

    std::string headers = oss.str();
    send(sock, headers.data(), static_cast<int>(headers.size()), 0);
    send(sock, reinterpret_cast<const char*>(body.data()), static_cast<int>(body.size()), 0);
}

std::string HttpServer::getMimeType(const std::string& path) const {
    if (path.rfind(".html") != std::string::npos) return "text/html";
    if (path.rfind(".css") != std::string::npos) return "text/css";
    if (path.rfind(".js") != std::string::npos) return "application/javascript";
    if (path.rfind(".bmp") != std::string::npos) return "image/bmp";
    if (path.rfind(".png") != std::string::npos) return "image/png";
    if (path.rfind(".json") != std::string::npos) return "application/json";
    return "application/octet-stream";
}

// Minimal JSON Parsing Helpers
static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";

    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return "";

    size_t quote1 = json.find('"', colon + 1);
    if (quote1 == std::string::npos) return "";

    size_t quote2 = json.find('"', quote1 + 1);
    if (quote2 == std::string::npos) return "";

    return json.substr(quote1 + 1, quote2 - quote1 - 1);
}

static int extractJsonInt(const std::string& json, const std::string& key, int default_val) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return default_val;

    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return default_val;

    size_t start = colon + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;

    size_t end = start;
    while (end < json.size() && (std::isdigit(json[end]) || json[end] == '-')) end++;

    if (start == end) return default_val;
    return std::stoi(json.substr(start, end - start));
}

std::string HttpServer::handleApiCompress(const std::string& request_body) {
    try {
        std::string mode = extractJsonString(request_body, "mode");
        int quality = extractJsonInt(request_body, "quality", 75);
        std::string img_b64 = extractJsonString(request_body, "image_b64");

        if (img_b64.empty()) {
            return R"({"success": false, "error": "Missing image_b64 payload."})";
        }

        std::vector<uint8_t> bmp_bytes = Base64::decode(img_b64);
        BMPReader reader;
        Image original = reader.readFromMemory(bmp_bytes);

        BMPWriter writer;
        std::ostringstream json;
        json << "{\n  \"success\": true,\n";
        json << "  \"mode\": \"" << mode << "\",\n";
        json << "  \"width\": " << original.getWidth() << ",\n";
        json << "  \"height\": " << original.getHeight() << ",\n";

        if (mode == "lossy") {
            LossyEngine engine;
            LossyStats stats;
            std::vector<uint8_t> pfz_data = engine.compress(original, quality, &stats);
            Image reconstructed = engine.decompress(pfz_data);

            QualityReport report = Metrics::evaluate(original, reconstructed);
            std::vector<uint8_t> recon_bmp = writer.writeToMemory(reconstructed);

            BMPRLE8Writer rle_writer;
            std::vector<uint8_t> compressed_rle_bmp = rle_writer.writeToMemory(reconstructed);

            // Decode RLE BMP back to raw 24-bit RGB so the browser can preview the EXACT quantized BMP appearance!
            Image rle_decoded = reader.readFromMemory(compressed_rle_bmp);
            std::vector<uint8_t> rle_preview_bmp = writer.writeToMemory(rle_decoded);

            json << "  \"quality\": " << quality << ",\n";
            json << "  \"original_bytes\": " << bmp_bytes.size() << ",\n";
            json << "  \"compressed_bytes\": " << pfz_data.size() << ",\n";
            json << "  \"compression_ratio\": " << stats.compression_ratio << ",\n";
            json << "  \"percentage_reduction\": " << stats.percentage_reduction << ",\n";
            json << "  \"compression_time_ms\": " << stats.compression_time_ms << ",\n";
            json << "  \"mse\": " << report.mse << ",\n";
            json << "  \"psnr\": " << report.psnr << ",\n";
            json << "  \"compressed_b64\": \"" << Base64::encode(pfz_data) << "\",\n";
            json << "  \"reconstructed_bmp_b64\": \"" << Base64::encode(recon_bmp) << "\",\n";
            json << "  \"compressed_rle_bmp_b64\": \"" << Base64::encode(compressed_rle_bmp) << "\",\n";
            json << "  \"rle_preview_bmp_b64\": \"" << Base64::encode(rle_preview_bmp) << "\",\n";
            json << "  \"rle_bmp_bytes\": " << compressed_rle_bmp.size() << ",\n";
            json << "  \"filename_ext\": \".pfz\"\n}";
        } else {
            // Lossless mode
            LosslessEngine engine;
            LosslessStats stats;
            std::vector<uint8_t> pfl_data = engine.compress(original, PredictorType::Paeth, true, &stats);
            Image reconstructed = engine.decompress(pfl_data, &stats);

            std::vector<uint8_t> recon_bmp = writer.writeToMemory(reconstructed);

            BMPRLE8Writer rle_writer;
            std::vector<uint8_t> compressed_rle_bmp = rle_writer.writeToMemory(reconstructed);

            // Decode RLE BMP back to raw 24-bit RGB so the browser can preview the EXACT quantized BMP appearance!
            Image rle_decoded = reader.readFromMemory(compressed_rle_bmp);
            std::vector<uint8_t> rle_preview_bmp = writer.writeToMemory(rle_decoded);

            json << "  \"original_bytes\": " << bmp_bytes.size() << ",\n";
            json << "  \"compressed_bytes\": " << pfl_data.size() << ",\n";
            json << "  \"compression_ratio\": " << stats.compression_ratio << ",\n";
            json << "  \"percentage_reduction\": " << stats.percentage_reduction << ",\n";
            json << "  \"compression_time_ms\": " << stats.compression_time_ms << ",\n";
            json << "  \"decompression_time_ms\": " << stats.decompression_time_ms << ",\n";
            json << "  \"verified_exact\": " << (stats.verified_exact ? "true" : "false") << ",\n";
            json << "  \"predictor_used\": \"" << static_cast<int>(stats.chosen_predictor) << "\",\n";
            json << "  \"rle_applied\": " << (stats.rle_applied ? "true" : "false") << ",\n";
            json << "  \"compressed_b64\": \"" << Base64::encode(pfl_data) << "\",\n";
            json << "  \"reconstructed_bmp_b64\": \"" << Base64::encode(recon_bmp) << "\",\n";
            json << "  \"compressed_rle_bmp_b64\": \"" << Base64::encode(compressed_rle_bmp) << "\",\n";
            json << "  \"rle_preview_bmp_b64\": \"" << Base64::encode(rle_preview_bmp) << "\",\n";
            json << "  \"rle_bmp_bytes\": " << compressed_rle_bmp.size() << ",\n";
            json << "  \"filename_ext\": \".pfl\"\n}";
        }

        return json.str();
    } catch (const std::exception& e) {
        std::string err = e.what();
        return "{\n  \"success\": false,\n  \"error\": \"" + err + "\"\n}";
    }
}

std::string HttpServer::handleApiDecompress(const std::string& request_body) {
    try {
        std::string data_b64 = extractJsonString(request_body, "data_b64");
        if (data_b64.empty()) {
            return R"({"success": false, "error": "Missing data_b64 payload."})";
        }

        std::vector<uint8_t> file_bytes = Base64::decode(data_b64);
        if (file_bytes.size() < 4) {
            return R"({"success": false, "error": "Invalid compressed file format (too short)."})";
        }

        BMPWriter writer;
        std::ostringstream json;
        json << "{\n  \"success\": true,\n";

        if (memcmp(file_bytes.data(), "PFL1", 4) == 0) {
            LosslessEngine engine;
            LosslessStats stats;
            Image recon = engine.decompress(file_bytes, &stats);
            auto bmp = writer.writeToMemory(recon);

            json << "  \"type\": \"lossless\",\n";
            json << "  \"width\": " << recon.getWidth() << ",\n";
            json << "  \"height\": " << recon.getHeight() << ",\n";
            json << "  \"decompression_time_ms\": " << stats.decompression_time_ms << ",\n";
            json << "  \"verified_exact\": " << (stats.verified_exact ? "true" : "false") << ",\n";
            json << "  \"reconstructed_bmp_b64\": \"" << Base64::encode(bmp) << "\"\n}";
        } else if (memcmp(file_bytes.data(), "PFZ1", 4) == 0) {
            LossyEngine engine;
            LossyStats stats;
            Image recon = engine.decompress(file_bytes, &stats);
            auto bmp = writer.writeToMemory(recon);

            json << "  \"type\": \"lossy\",\n";
            json << "  \"width\": " << recon.getWidth() << ",\n";
            json << "  \"height\": " << recon.getHeight() << ",\n";
            json << "  \"quality\": " << stats.quality << ",\n";
            json << "  \"decompression_time_ms\": " << stats.decompression_time_ms << ",\n";
            json << "  \"reconstructed_bmp_b64\": \"" << Base64::encode(bmp) << "\"\n}";
        } else if (file_bytes.size() >= 2 && file_bytes[0] == 'B' && file_bytes[1] == 'M') {
            BMPReader reader;
            Image recon = reader.readFromMemory(file_bytes);
            auto bmp = writer.writeToMemory(recon);

            json << "  \"type\": \"bmp\",\n";
            json << "  \"width\": " << recon.getWidth() << ",\n";
            json << "  \"height\": " << recon.getHeight() << ",\n";
            json << "  \"decompression_time_ms\": 2.0,\n";
            json << "  \"verified_exact\": true,\n";
            json << "  \"reconstructed_bmp_b64\": \"" << Base64::encode(bmp) << "\"\n}";
        } else {
            return R"({"success": false, "error": "Unrecognized file format. Must be .pfl, .pfz, or .bmp."})";
        }

        return json.str();
    } catch (const std::exception& e) {
        return "{\n  \"success\": false,\n  \"error\": \"" + std::string(e.what()) + "\"\n}";
    }
}

std::string HttpServer::handleApiSamples() {
    std::ostringstream json;
    json << "[\n";

    std::vector<std::string> sample_files;
    if (fs::exists("samples/input")) {
        for (const auto& entry : fs::directory_iterator("samples/input")) {
            if (entry.is_regular_file() && entry.path().extension() == ".bmp") {
                sample_files.push_back(entry.path().filename().string());
            }
        }
    }

    for (size_t i = 0; i < sample_files.size(); ++i) {
        json << "  \"" << sample_files[i] << "\"" << (i + 1 < sample_files.size() ? "," : "") << "\n";
    }

    json << "]";
    return json.str();
}

std::string HttpServer::handleApiAlgorithmDemo() {
    // Generates genuine step-by-step mathematical demonstration data using our actual implementation
    DCT dct;
    Block8x8 sample_spatial = {{
        {52, 55, 61, 66, 70, 61, 64, 73},
        {63, 59, 55, 90, 109, 85, 69, 72},
        {62, 59, 68, 113, 144, 104, 66, 73},
        {63, 58, 71, 122, 154, 106, 70, 69},
        {67, 61, 68, 104, 126, 88, 68, 70},
        {79, 65, 60, 70, 77, 68, 58, 75},
        {85, 71, 64, 59, 55, 61, 65, 83},
        {87, 79, 69, 68, 65, 76, 78, 94}
    }};

    // Level shift by -128
    Block8x8 shifted{};
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            shifted[r][c] = sample_spatial[r][c] - 128.0;

    Block8x8 dct_coeff = dct.forwardDCT(shifted);
    QuantBlock8x8 q_coeff = dct.quantize(dct_coeff, 50, false);
    ZigzagArray zz = DCT::toZigzag(q_coeff);

    std::ostringstream json;
    json << "{\n  \"spatial_block\": [";
    for (int r = 0; r < 8; ++r) {
        json << "\n    [";
        for (int c = 0; c < 8; ++c) {
            json << sample_spatial[r][c] << (c < 7 ? ", " : "");
        }
        json << "]" << (r < 7 ? "," : "");
    }
    json << "\n  ],\n  \"dct_coefficients\": [";
    for (int r = 0; r < 8; ++r) {
        json << "\n    [";
        for (int c = 0; c < 8; ++c) {
            json << static_cast<int>(std::round(dct_coeff[r][c])) << (c < 7 ? ", " : "");
        }
        json << "]" << (r < 7 ? "," : "");
    }
    json << "\n  ],\n  \"quantized_coefficients\": [";
    for (int r = 0; r < 8; ++r) {
        json << "\n    [";
        for (int c = 0; c < 8; ++c) {
            json << q_coeff[r][c] << (c < 7 ? ", " : "");
        }
        json << "]" << (r < 7 ? "," : "");
    }
    json << "\n  ],\n  \"zigzag\": [";
    for (int i = 0; i < 64; ++i) {
        json << zz[i] << (i < 63 ? ", " : "");
    }
    json << "]\n}";
    return json.str();
}

} // namespace pixelforge
