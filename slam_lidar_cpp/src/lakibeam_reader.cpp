#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>

#define PORT 2368
#define BUFLEN 2048
#define HEADER_FLAG 0xFFEE
#define MAX_VALID_DIST_MM 16000
#define MIN_VALID_RSSI 5

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

    std::cout << " Listening on port " << PORT << "...\n";

    while (true) {
        ssize_t len = recv(sockfd, buffer, BUFLEN, 0);
        if (len < 100) {
            std::cout << "❌ Short packet (" << len << " bytes), skipping...\n";
            continue;
        }

        uint16_t flag = read_be16(buffer);
        if (flag != HEADER_FLAG) continue;

        uint16_t az_raw = read_be16(buffer + 2);
        if (az_raw == 0xFFFF) continue;

        float az_deg = az_raw / 100.0f;
        std::cout << std::dec << " Azimuth: " << az_deg << "°\n";

        for (int i = 0; i < 16; ++i) {
            const uint8_t* p = buffer + 4 + i * 6;

            uint16_t dist1 = read_be16(p);
            uint8_t rssi1  = p[2];
            uint16_t dist2 = read_be16(p + 3);
            uint8_t rssi2  = p[5];

            uint16_t final_dist = (rssi1 >= rssi2) ? dist1 : dist2;
            uint8_t final_rssi  = (rssi1 >= rssi2) ? rssi1 : rssi2;

            // Filter invalid or ghost data
            if (final_dist == 0 || final_dist == 0xFFFF ||
                final_dist > MAX_VALID_DIST_MM || final_rssi < MIN_VALID_RSSI)
                continue;

            // Optional debug: log suspiciously high distances
            if (final_dist > 15000) {
                std::cout << std::hex << "  Raw dist bytes: "
                          << std::setw(2) << std::setfill('0') << (int)p[0] << " "
                          << (int)p[1] << " | "
                          << (int)p[3] << " " << (int)p[4] << std::dec << "\n";
            }

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
