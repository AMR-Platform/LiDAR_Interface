#include "data_type.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <cstring>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <udp_port>\n";
    return 1;
  }
  int port = std::atoi(argv[1]);

  // 1) Open & bind UDP socket
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) { perror("socket"); return 1; }
  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port        = htons(port);
  if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind"); return 1;
  }

  // 2) Receive one MSOP packet
  MSOP_Data_t pkt;
  ssize_t n = recvfrom(sock, &pkt, sizeof(pkt), 0, nullptr, nullptr);
  if (n < 0) { perror("recvfrom"); return 1; }
  std::cout << "Got " << n << " bytes:\n";

  // 3) For each of the 12 blocks, print flag, azimuth, first 4 distances+RSSI
  for (int b = 0; b < 12; ++b) {
    auto blk = pkt.BlockID[b];
    uint16_t flag = ntohs(blk.DataFlag);
    uint16_t az   = ntohs(blk.Azimuth);
    std::cout << "Block " << std::setw(2) << b
              << " | flag=0x" << std::hex << flag
              << " | az=" << std::dec << (az/100.0) << "°\n";
    for (int i = 0; i < 4; ++i) {
      uint16_t d = ntohs(blk.Result[i].Dist_1);
      uint8_t  r = blk.Result[i].RSSI_1;
      std::cout << "    ["<<i<<"] dist="<<d<<" mm, rssi="<<(int)r<<"\n";
    }
  }

  close(sock);
  return 0;
}
