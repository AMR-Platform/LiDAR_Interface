#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>

#define PORT 2368
#define BUFLEN 1248
#define HEADER_FLAG 0xFFEE

uint16_t read_uint16(const uint8_t* data) {
    return (data[0] << 8) | data[1];  // Big endian
}

int main() {
    int sockfd;
    struct sockaddr_in si_me{};
    uint8_t buffer[BUFLEN];

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Socket creation failed");
        return 1;
    }

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me)) == -1) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    std::cout << "Listening on UDP port " << PORT << "...\n";

    while (true) {
        ssize_t len = recv(sockfd, buffer, BUFLEN, 0);
        if (len < 0) {
            perror("recv");
            continue;
        }

        // Each MSOP packet has 12 data blocks (100 bytes each)
        for (int i = 0; i < 12; ++i) {
            const uint8_t* block = buffer + 42 + i * 100;
            uint16_t flag = read_uint16(block);
            if (flag != HEADER_FLAG) continue;

            uint16_t azimuth = read_uint16(block + 2);
            float base_angle = azimuth / 100.0f;

            // Next block azimuth (for angle interpolation)
            uint16_t next_azimuth = 0;
            if (i < 11) {
                const uint8_t* next_block = buffer + 42 + (i + 1) * 100;
                next_azimuth = read_uint16(next_block + 2);
            } else {
                next_azimuth = read_uint16(buffer + 42 + i * 100 + 2);  // fallback
            }

            float angle_diff = ((next_azimuth - azimuth + 36000) % 36000) / 100.0f;
            float angle_step = angle_diff / 16.0f;

            for (int j = 0; j < 16; ++j) {
                float angle = base_angle + angle_step * j;
                if (angle >= 360.0f) angle -= 360.0f;

                if (angle < 45.0f || angle > 315.0f) continue;

                const uint8_t* point = block + 4 + j * 6;
                uint16_t dist = read_uint16(point);
                uint8_t rssi = point[2];

                if (dist != 0xFFFF) {
                    float distance_m = dist / 100.0f;
                    std::cout << "Angle: " << angle << "°, Distance: "
                              << distance_m << " m, RSSI: " << (int)rssi << "\n";
                }
            }
        }

        std::cout << "----------------------------------\n";
    }

    close(sockfd);
    return 0;
}
