#include "lidar_reader.hpp"
#include "data_type.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <cmath>

#ifndef M_PI
static constexpr double M_PI = 3.14159265358979323846;
#endif

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

void LiDARReader::setupSocket(const std::string& /*host_ip*/, int port) {
  sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd_ < 0)
    throw std::runtime_error("socket() failed");

  // Listen on all interfaces
  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
  std::vector<ScanPoint> scan(BLOCKS_PER_SCAN * POINTS_PER_BLOCK);
  MSOP_Data_t packet;

  // 1) Sync on block 0 (Azimuth == 0)
  do {
    recvPacket(packet);
  } while (packet.BlockID[0].Azimuth != 0);

  // 2) Byte-swap block 0 and block 1 headers + distances
  // Block 0
  auto &b0 = packet.BlockID[0];
  b0.DataFlag = ntohs(b0.DataFlag);
  b0.Azimuth  = ntohs(b0.Azimuth);
  for (int i = 0; i < POINTS_PER_BLOCK; ++i) {
    b0.Result[i].Dist_1 = ntohs(b0.Result[i].Dist_1);
  }
  // Block 1 (for resolution)
  auto &b1 = packet.BlockID[1];
  b1.DataFlag = ntohs(b1.DataFlag);
  b1.Azimuth  = ntohs(b1.Azimuth);
  for (int i = 0; i < POINTS_PER_BLOCK; ++i) {
    b1.Result[i].Dist_1 = ntohs(b1.Result[i].Dist_1);
  }

  // 3) Compute angular resolution in degrees
  // Azimuth values are in hundredths of a degree
  double az0_deg = b0.Azimuth / 100.0;
  double az1_deg = b1.Azimuth / 100.0;
  double res_deg = (az1_deg - az0_deg) / POINTS_PER_BLOCK;

  // 4) Parse all 12 blocks
  for (int b = 0; b < BLOCKS_PER_SCAN; ++b) {
    if (b > 0) {
      recvPacket(packet);
      auto &blk = packet.BlockID[b];
      blk.DataFlag = ntohs(blk.DataFlag);
      blk.Azimuth  = ntohs(blk.Azimuth);
      for (int i = 0; i < POINTS_PER_BLOCK; ++i) {
        blk.Result[i].Dist_1 = ntohs(blk.Result[i].Dist_1);
      }
    }
    auto &blk = packet.BlockID[b];
    double base_deg = blk.Azimuth / 100.0;

    for (int i = 0; i < POINTS_PER_BLOCK; ++i) {
      // Optionally filter invalid data:
      // if (blk.DataFlag != 0xEEFF) continue;

      double raw_deg = base_deg + res_deg * i + angle_offset_;
      double ang_rad = raw_deg * M_PI / 180.0;

      double dist_m = blk.Result[i].Dist_1 / 1000.0;
      double inten  = blk.Result[i].RSSI_1;
      if (dist_m <= 0) {
        dist_m = INF_DIST;
        inten  = 0;
      }

      int idx = b * POINTS_PER_BLOCK + i;
      if (inverted_)
        idx = BLOCKS_PER_SCAN * POINTS_PER_BLOCK - 1 - idx;

      scan[idx] = { ang_rad, dist_m, inten };
    }
  }

  return scan;
}
