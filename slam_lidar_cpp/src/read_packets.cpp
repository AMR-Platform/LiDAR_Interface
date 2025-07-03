#include <iostream>
#include <iomanip>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "data_type.h"

int main(int argc, char** argv){
  if(argc < 3){
    std::cerr << "Usage: " << argv[0] << " <local_udp_port>\n";
    return 1;
  }
  int port = std::atoi(argv[1]);

  // 1) open socket
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0){
    perror("socket");
    return 1;
  }

  // allow reuse
  int yes=1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port        = htons(port);

  if(bind(sockfd, (sockaddr*)&addr, sizeof(addr))<0){
    perror("bind");
    return 1;
  }

  std::cout << "Listening on UDP port " << port << "...\n";

  // 2) receive one packet
  MSOP_Data_t packet;
  socklen_t len = sizeof(addr);
  ssize_t n = recvfrom(sockfd, &packet, sizeof(packet), 0,
                       (sockaddr*)&addr, &len);
  if(n < 0){
    perror("recvfrom");
    return 1;
  }
  std::cout << "Received " << n << " bytes\n";

  // 3) print some raw fields (no byte-swapping)
  std::cout << std::hex << std::showbase;
  std::cout << "Block0.DataFlag = " << packet.BlockID[0].DataFlag << "\n";
  std::cout << "Block0.Azimuth  = " << packet.BlockID[0].Azimuth  << "\n";
  std::cout << std::dec << std::noshowbase;

  // print first 4 distances and RSSIs raw
  for(int i=0;i<4;i++){
    auto &res = packet.BlockID[0].Result[i];
    std::cout << "  Result["<<i<<"].Dist_1= " << res.Dist_1
              << "  RSSI_1= " << int(res.RSSI_1) << "\n";
  }

  close(sockfd);
  return 0;
}
