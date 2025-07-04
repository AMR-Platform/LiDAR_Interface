#include "msop_parser.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iomanip>

class LidarUDPReceiver {
public:
    LidarUDPReceiver(int port = 2368) : port_(port), socket_fd_(-1) {
    }
    
    ~LidarUDPReceiver() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }
    
    bool initialize() {
        // Create UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Set socket options to reuse address
        int opt = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Error setting socket options: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Bind socket to port
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port_);
        
        if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Error binding socket to port " << port_ << ": " << strerror(errno) << std::endl;
            return false;
        }
        
        std::cout << "UDP receiver initialized on port " << port_ << std::endl;
        return true;
    }
    
    bool receivePacket(uint8_t* buffer, size_t buffer_size, size_t& received_size) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        ssize_t bytes_received = recvfrom(socket_fd_, buffer, buffer_size, 0,
                                         (struct sockaddr*)&client_addr, &client_len);
        
        if (bytes_received < 0) {
            std::cerr << "Error receiving packet: " << strerror(errno) << std::endl;
            return false;
        }
        
        received_size = bytes_received;
        return true;
    }
    
private:
    int port_;
    int socket_fd_;
};

void printPacketInfo(const std::vector<LidarPoint>& points, uint32_t timestamp, uint16_t factory_info) {
    std::cout << "Timestamp: " << timestamp << " μs, Factory: 0x" 
              << std::hex << factory_info << std::dec 
              << ", Points: " << points.size() << " (270° FOV: 45°-315°, Max: 15m)" << std::endl;
    
    // Print first few points as examples
    for (size_t i = 0; i < std::min(points.size(), size_t(5)); ++i) {
        const auto& point = points[i];
        std::cout << "  Point " << i << ": "
                  << "Azimuth=" << std::fixed << std::setprecision(2) << point.azimuth << "°, "
                  << "Distance=" << std::setprecision(3) << point.distance << "m, "
                  << "RSSI=" << (int)point.rssi
                  << " (" << (point.is_strongest ? "strongest" : "last") << ")"
                  << std::endl;
    }
    if (points.size() > 5) {
        std::cout << "  ... and " << (points.size() - 5) << " more points" << std::endl;
    }
}

int main() {
    LidarUDPReceiver receiver(2368);  // Default MSOP port
    MSOPParser parser;
    
    if (!receiver.initialize()) {
        return -1;
    }
    
    std::cout << "Listening for MSOP packets on port 2368..." << std::endl;
    std::cout << "LakiBeam1(L) - 270° Field of View (45° to 315°)" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    
    uint8_t buffer[2048];  // Buffer for received packets
    std::vector<LidarPoint> points;
    int packet_count = 0;
    
    while (true) {
        size_t received_size;
        if (!receiver.receivePacket(buffer, sizeof(buffer), received_size)) {
            continue;
        }
        
        std::cout << "\n--- Packet " << ++packet_count << " ---" << std::endl;
        std::cout << "Received " << received_size << " bytes" << std::endl;
        
        // Check if this is the expected MSOP packet size
        if (received_size == 1206) {
            std::cout << "MSOP packet detected (1206 bytes - UDP header stripped)" << std::endl;
            
            if (parser.parsePacket(buffer, received_size, points)) {
                printPacketInfo(points, parser.getLastTimestamp(), parser.getLastFactoryInfo());
                
                // Show range statistics for first few packets
                if (packet_count <= 5 && !points.empty()) {
                    float min_dist = 999.0f, max_dist = 0.0f;
                    float min_azim = 999.0f, max_azim = 0.0f;
                    
                    for (const auto& point : points) {
                        min_dist = std::min(min_dist, point.distance);
                        max_dist = std::max(max_dist, point.distance);
                        min_azim = std::min(min_azim, point.azimuth);
                        max_azim = std::max(max_azim, point.azimuth);
                    }
                    
                    std::cout << "  Range stats: Distance " << std::fixed << std::setprecision(2) 
                              << min_dist << "m to " << max_dist << "m, "
                              << "Azimuth " << min_azim << "° to " << max_azim << "°" << std::endl;
                }
            } else {
                std::cout << "Failed to parse MSOP packet" << std::endl;
            }
        } else if (received_size == 1248) {
            std::cout << "MSOP packet with UDP header detected (1248 bytes)" << std::endl;
            
            // Skip the first 42 bytes (UDP header) and parse the rest
            if (parser.parsePacket(buffer + 42, received_size - 42, points)) {
                printPacketInfo(points, parser.getLastTimestamp(), parser.getLastFactoryInfo());
            } else {
                std::cout << "Failed to parse MSOP packet" << std::endl;
            }
        } else {
            std::cout << "Unexpected packet size: " << received_size << " bytes" << std::endl;
            
            // Print first few bytes in hex for debugging
            std::cout << "First 16 bytes: ";
            for (size_t i = 0; i < std::min(received_size, size_t(16)); ++i) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)buffer[i] << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }
    
    return 0;
}
