#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

namespace pixelforge {

/**
 * @brief Zero-External-Dependency Embedded HTTP Server for Windows.
 * Uses native Windows Sockets 2 (ws2_32.dll) to serve static frontend files
 * and REST API endpoints for lossless and lossy image compression.
 */
class HttpServer {
public:
    HttpServer(int port = 8080, std::string static_dir = "frontend");
    ~HttpServer();

    /**
     * @brief Starts the HTTP server loop on the configured port. Blocks until stopped.
     */
    void start();

    /**
     * @brief Signals the server to stop listening and close sockets.
     */
    void stop();

private:
    int port_{8080};
    std::string static_dir_;
    std::atomic<bool> running_{false};
    uintptr_t server_socket_{0}; // SOCKET in Winsock

    void handleClient(uintptr_t client_socket);
    void sendResponse(uintptr_t client_socket, int status_code, 
                      const std::string& content_type, 
                      const std::string& body);
    void sendBinaryResponse(uintptr_t client_socket, int status_code, 
                            const std::string& content_type, 
                            const std::vector<uint8_t>& body);

    std::string getMimeType(const std::string& path) const;
    std::string handleApiCompress(const std::string& request_body);
    std::string handleApiDecompress(const std::string& request_body);
    std::string handleApiSamples();
    std::string handleApiAlgorithmDemo();
};

} // namespace pixelforge
