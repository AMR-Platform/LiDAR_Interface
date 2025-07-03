#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>

#define PORT 2368
#define BUFLEN 2048

uint16_t read_be16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

int main() {
    int sockfd;
    struct sockaddr_in si_me{};
    uint8_t buffer[BUFLEN];

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me));

    std::cout << "Listening on port " << PORT << "...\n";

    while (true) {
        ssize_t len = recv(sockfd, buffer, BUFLEN, 0);
        if (len < 100) continue;

        // Check header
        if (read_be16(buffer) != 0xFFEE) continue;

        // Read azimuth
        uint16_t az_raw = read_be16(buffer + 2);
        float azimuth = az_raw / 100.0f;

        // Filter: Show only packets within 45°–315°
        if (azimuth < 45.0f || azimuth > 315.0f) continue;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Azimuth: " << azimuth << "°\n";

        // Read 16 points
        for (int i = 0; i < 16; ++i) {
            const uint8_t* p = buffer + 4 + i * 6;
            uint16_t dist_raw = read_be16(p);
            uint8_t rssi = p[2];

            if (dist_raw == 0xFFFF || dist_raw == 0) continue;

            float dist_m = dist_raw / 1000.0f;  // convert mm to meters
            std::cout << "  🔹 Point " << std::setw(2) << i
                      << ": Distance = " << dist_m << " m"
                      << ", RSSI = " << static_cast<int>(rssi) << "\n";
        }

        std::cout << "----------------------------------\n";
    }

    close(sockfd);
    return 0;
}
