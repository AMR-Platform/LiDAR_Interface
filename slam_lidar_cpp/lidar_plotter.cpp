#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>

#define PORT 2368
#define BUFLEN 2048
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

    std::cout << "Listening and plotting...\n";

    const int img_size = 800;
    const int center = img_size / 2;
    const float scale = 100.0f; // 1 meter = 100 pixels

    cv::Mat canvas(img_size, img_size, CV_8UC3, cv::Scalar(0, 0, 0));

    while (true) {
        ssize_t len = recv(sockfd, buffer, BUFLEN, 0);
        if (len < 100) continue;

        uint16_t flag = read_be16(buffer);
        if (flag != HEADER_FLAG) continue;

        uint16_t az_raw = read_be16(buffer + 2);
        if (az_raw == 0xFFFF) continue;

        float az_deg = az_raw / 100.0f;
        float az_rad = az_deg * M_PI / 180.0f;

        for (int i = 0; i < 16; ++i) {
            const uint8_t* p = buffer + 4 + i * 6;
            uint16_t dist_mm = read_be16(p);
            uint8_t rssi = p[2];

            if (dist_mm == 0xFFFF || dist_mm == 0 || rssi == 0) continue;

            float r = dist_mm / 1000.0f; // convert mm to meters
            float x = r * cos(az_rad);
            float y = r * sin(az_rad);

            int px = center + static_cast<int>(x * scale);
            int py = center - static_cast<int>(y * scale);

            if (px >= 0 && px < img_size && py >= 0 && py < img_size)
                cv::circle(canvas, cv::Point(px, py), 1, cv::Scalar(255, 255, 255), -1);
        }

        // Show and clear
        cv::imshow("LiDAR Point Cloud", canvas);
        cv::waitKey(1);
        canvas.setTo(cv::Scalar(0, 0, 0));  // clear for next frame
    }

    close(sockfd);
    return 0;
}
