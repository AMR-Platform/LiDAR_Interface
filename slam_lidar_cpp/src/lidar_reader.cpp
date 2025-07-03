#include "lidar_reader.hpp"
#include "data_type.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

static constexpr int   BLOCKS_PER_SCAN   = 12;
static constexpr int   POINTS_PER_BLOCK  = 16;
static constexpr double INF_DIST         = std::numeric_limits<double>::infinity();

LiDARReader::LiDARReader(const std::string& host_ip,
                         int port,
                         int angle_offset,
                         bool inverted)
 : angle_offset_(angle_offset),
   inverted_(inverted)
{
  setupSocket(host_ip, port);
}

LiDARReader::~LiDARReader() {
  if (sockfd_ >= 0) close(sockfd_);
}

void LiDARReader::setupSocket(const std::string& host_ip, int port) {
  sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd_ < 0)
    throw std::runtime_error("socket() failed");

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = inet_addr(host_ip.c_str());
  addr.sin_port        = htons(port);

  if (bind(sockfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    throw std::runtime_error("bind() failed");
}

void LiDARReader::recvPacket(MSOP_Data_t& buf) {
  sockaddr_in client{};
  socklen_t   len = sizeof(client);
  ssize_t     n   = recvfrom(sockfd_, &buf, sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&client), &len);
  if (n < 0 || static_cast<size_t>(n) != sizeof(buf))
    throw std::runtime_error("recvfrom error or incomplete packet");
}

std::vector<ScanPoint> LiDARReader::readScan() {
  // pre-allocate result
  std::vector<ScanPoint> scan(BLOCKS_PER_SCAN * POINTS_PER_BLOCK);

  MSOP_Data_t packet;
  // 1) sync on block 0 with Azimuth==0
  do {
    recvPacket(packet);
  } while (packet.BlockID[0].Azimuth != 0);

  // compute angular step per point
  double resolution = (packet.BlockID[1].Azimuth - packet.BlockID[0].Azimuth)
                      / static_cast<double>(POINTS_PER_BLOCK);

  // 2) read all 12 blocks (we already have block 0)
  for (int b = 0; b < BLOCKS_PER_SCAN; ++b) {
    if (b > 0) recvPacket(packet);

    for (int i = 0; i < POINTS_PER_BLOCK; ++i) {
      auto &blk = packet.BlockID[b];
      if (blk.DataFlag != 0xEEFF) continue;  // invalid point

      double raw_ang = blk.Azimuth + resolution * i + angle_offset_;
      double ang_rad = raw_ang * M_PI / 180.0;
      double dist_m  = blk.Result[i].Dist_1 / 1000.0;
      double inten   = blk.Result[i].RSSI_1;

      if (dist_m <= 0) {
        dist_m = INF_DIST;
        inten  = 0;
      }

      int idx = b*POINTS_PER_BLOCK + i;
      if (inverted_)
        idx = BLOCKS_PER_SCAN*POINTS_PER_BLOCK - 1 - idx;

      scan[idx] = { ang_rad, dist_m, inten };
    }
  }

  return scan;
}
