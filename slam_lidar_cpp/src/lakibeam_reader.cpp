#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>

#define PORT 2368
#define BUFLEN 1206
#define HEADER_FLAG 0xFFEE

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
        if (len != BUFLEN) continue;

        uint16_t flag = read_be16(buffer);
        if (flag != HEADER_FLAG) continue;

        uint16_t az_raw = read_be16(buffer + 2);
        if (az_raw == 0xFFFF) continue;

        float az_deg = az_raw / 100.0f;
        if (az_deg < 45.0f || az_deg > 315.0f) continue;

        std::cout << " Azimuth: " << az_deg << "°\n";

        for (int i = 0; i < 16; ++i) {
            const uint8_t* p = buffer + 4 + i * 6;
            uint16_t dist = read_be16(p);
            uint8_t rssi = p[2];

            if (dist == 0xFFFF) continue;

            float meters = dist / 100.0f;
            std::cout << "  🔹 Point " << i << ": Distance = " << meters << " m, RSSI = " << (int)rssi << "\n";
        }

        std::cout << "----------------------------------\n";
    }

    close(sockfd);
    return 0;
}
