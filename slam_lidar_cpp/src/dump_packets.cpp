#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#define PORT 2368
#define BUFLEN 1248

int main() {
    int sockfd;
    struct sockaddr_in si_me{};
    uint8_t buffer[BUFLEN];

    // Step 1: Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Step 2: Bind to any IP, port 2368
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    std::cout << "Listening on port " << PORT << "...\n";

    // Step 3: Read and dump packets
    while (true) {
        ssize_t len = recv(sockfd, buffer, BUFLEN, 0);
        if (len <= 0) continue;

        std::cout << "\n Packet (" << len << " bytes):\n";
        for (int i = 0; i < std::min(64L, len); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i] << " ";
            if ((i + 1) % 16 == 0) std::cout << "\n";
        }
        std::cout << "\n-----------------------------------------\n";
    }

    close(sockfd);
    return 0;
}
