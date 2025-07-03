#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>

#define PORT 2368
#define BUFLEN 2048  // allow more space than 1206
#define HEADER_FLAG 0xFFEE

uint16_t read_be16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

int main() {
    int sockfd;
    struct sockaddr_in si_me{};
    uint8_t buffer[BUFLEN];

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Bind to any local IP and specified port
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me)) < 0) {
        perror("Bind failed");
        return 1;
    }

    std::cout << " Listening on UDP port " << PORT << "...\n";

    while (true) {
        ssize_t len = recv(sockfd, buffer, BUFLEN, 0);
        if (len < 100) {
            std::cout << " Short packet (" << len << " bytes), skipping...\n";
            continue;
        }

        // Check header
        uint16_t flag = read_be16(buffer);
        if (flag != HEADER_FLAG) continue;

        // Read azimuth
        uint16_t az_raw = read_be16(buffer + 2);
        if (az_raw == 0xFFFF) continue;

        float az_deg = az_raw / 100.0f;
        std::cout << std::dec << "Azimuth: " << az_deg << "°\n";

        // Parse all 16 points
        for (int i = 0; i < 16; ++i) {
            const uint8_t* p = buffer + 4 + i * 6;

            // Extract both returns
            uint16_t dist1 = read_be16(p);
            uint8_t rssi1  = p[2];
            uint16_t dist2 = read_be16(p + 3);
            uint8_t rssi2  = p[5];

            // Choose stronger return
            uint16_t final_dist = (rssi1 >= rssi2) ? dist1 : dist2;
            uint8_t final_rssi  = (rssi1 >= rssi2) ? rssi1 : rssi2;

            // Filter bad readings
            if (final_dist == 0 || final_dist == 0xFFFF || final_dist > 20000 || final_rssi == 0)
                continue;

            float distance_m = final_dist / 1000.0f;

            std::cout << "  🔹 Point " << std::setw(2) << i
                      << ": Distance = " << std::fixed << std::setprecision(2)
                      << distance_m << " m, RSSI = " << (int)final_rssi << "\n";
        }

        std::cout << "----------------------------------\n";
    }

    close(sockfd);
    return 0;
}
