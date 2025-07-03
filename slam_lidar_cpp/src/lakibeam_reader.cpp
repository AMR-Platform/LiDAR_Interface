#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>

#define PORT 2368
#define BUFLEN 2048
#define HEADER_FLAG 0xFFEE
#define MEASUREMENTS_PER_BLOCK 16

uint16_t read_be16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

int main() {
    int sockfd;
    struct sockaddr_in si_me{};
    uint8_t buffer[BUFLEN];
    
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }
    
    std::cout << "🟢 Listening on port " << PORT << "...\n";
    
    while (true) {
        ssize_t len = recv(sockfd, buffer, BUFLEN, 0);
        
        std::cout << "📦 Received packet: " << len << " bytes\n";
        
        if (len < 100) {
            std::cout << "❌ Packet too small for a data block\n";
            continue;
        }
        
        // Parse the first (and likely only) data block
        const uint8_t* block_ptr = buffer;
        
        // Read block header
        uint16_t flag = read_be16(block_ptr);
        uint16_t azimuth_raw = read_be16(block_ptr + 2);
        
        // Skip invalid blocks
        if (flag == 0xFFFF || azimuth_raw == 0xFFFF) {
            std::cout << "⚠️  Block is invalid (flag: 0x" 
                      << std::hex << flag << ", azimuth: 0x" << azimuth_raw << std::dec << ")\n";
            continue;
        }
        
        if (flag != HEADER_FLAG) {
            std::cout << "❌ Invalid block flag: 0x" << std::hex << flag << std::dec << "\n";
            continue;
        }
        
        float azimuth_deg = azimuth_raw / 100.0f;
        std::cout << "🔄 Azimuth: " << azimuth_deg << "°\n";
        
        // Parse 16 measurement points in this block
        for (int point = 0; point < MEASUREMENTS_PER_BLOCK; point++) {
            const uint8_t* measurement_ptr = block_ptr + 4 + point * 6;
            
            // Check if we have enough data for this measurement
            if (measurement_ptr + 6 > buffer + len) {
                std::cout << "❌ Not enough data for measurement " << point << "\n";
                break;
            }
            
            // First return (strongest)
            uint16_t dist1 = read_be16(measurement_ptr);
            uint8_t rssi1 = measurement_ptr[2];
            
            // Second return (last)
            uint16_t dist2 = read_be16(measurement_ptr + 3);
            uint8_t rssi2 = measurement_ptr[5];
            
            // Debug: show raw bytes for first few measurements
            if (point < 3) {
                std::cout << "  Raw measurement " << point << ": ";
                for (int i = 0; i < 6; i++) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << (int)measurement_ptr[i] << " ";
                }
                std::cout << std::dec << "\n";
            }
            
            // Use strongest return if valid, otherwise last return
            uint16_t final_dist = 0;
            uint8_t final_rssi = 0;
            
            if (dist1 > 0 && dist1 != 0xFFFF) {
                final_dist = dist1;
                final_rssi = rssi1;
            } else if (dist2 > 0 && dist2 != 0xFFFF) {
                final_dist = dist2;
                final_rssi = rssi2;
            }
            
            if (final_dist > 0 && final_rssi > 0) {
                // Calculate angle for this specific measurement point
                // For now, assume equal angular spacing across the block
                float angle_increment = 0.25f; // This is typical, but should be calculated properly
                float point_azimuth = azimuth_deg + (angle_increment * point);
                
                float distance_m = final_dist / 1000.0f;
                std::cout << "  🔹 Point " << std::setw(2) << point
                          << ": Azimuth = " << std::fixed << std::setprecision(2) << point_azimuth << "°"
                          << ", Distance = " << distance_m << " m"
                          << ", RSSI = " << (int)final_rssi << "\n";
            }
        }
        
        std::cout << "================================\n";
    }
    
    close(sockfd);
    return 0;
}
