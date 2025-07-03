// lidar_reader.cpp

#include "lidar_reader.hpp"
#include "data_type.h"

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <vector>

#ifndef M_PI
static constexpr double M_PI = 3.14159265358979323846;
#endif

static constexpr int   BLOCKS_PER_SCAN   = 12;
static constexpr int   POINTS_PER_BLOCK  = 16;
static constexpr double INF_DIST         = std::numeric_limits<double>::infinity();

LiDARReader::LiDARReader(const std::string& /*host_ip*/,
                         int port,
                         int angle_offset,
                         bool inverted)
  : angle_offset_(angle_offset),
    inverted_(inverted)
{
  // Create UDP socket and bind to all interfaces on the given port
  sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd_ < 0) throw std::runtime_error("socket() failed");

  int yes = 1;
  setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port        = htons(port);

  if (bind(sockfd_,
           reinterpret_cast<sockaddr*>(&addr),
           sizeof(addr)) < 0)
    throw std::runtime_error("bind() failed");
}

LiDARReader::~LiDARReader() {
  if (sockfd_ >= 0) close(sockfd_);
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
  // 1) Read exactly one MSOP packet (12 Data Blocks)
  MSOP_Data_t packet;
  recvPacket(packet);

  // 2) Byte-swap and store all blocks
  Data_block blocks[BLOCKS_PER_SCAN];
  for (int b = 0; b < BLOCKS_PER_SCAN; ++b) {
    Data_block blk = packet.BlockID[b];
    blk.DataFlag = ntohs(blk.DataFlag);
    blk.Azimuth  = ntohs(blk.Azimuth);
    for (int i = 0; i < POINTS_PER_BLOCK; ++i) {
      blk.Result[i].Dist_1 = ntohs(blk.Result[i].Dist_1);
    }
    blocks[b] = blk;
  }

  // 3) Compute angular increment between points (degrees)
  //    Handle wrap-around mod 36000 hundredths-of-degree
  int raw0   = blocks[0].Azimuth;
  int raw1   = blocks[1].Azimuth;
  int diff100 = (raw1 - raw0 + 36000) % 36000;        // hundredths of degree
  double step_deg = (diff100 / 100.0) / POINTS_PER_BLOCK;

  // 4) Prepare output vector
  std::vector<ScanPoint> scan(BLOCKS_PER_SCAN * POINTS_PER_BLOCK);

  // 5) Fill each beam
  for (int b = 0; b < BLOCKS_PER_SCAN; ++b) {
    const auto &blk = blocks[b];
    double base_deg = (blk.Azimuth / 100.0) + angle_offset_;

    for (int i = 0; i < POINTS_PER_BLOCK; ++i) {
      // compute angle, wrap into [0,360)
      double raw_deg = base_deg + step_deg * i;
      raw_deg = std::fmod(raw_deg, 360.0);
      if (raw_deg < 0) raw_deg += 360.0;
      double ang_rad = raw_deg * M_PI / 180.0;

      // distance & intensity
      double dist_m   = blk.Result[i].Dist_1 / 1000.0;
      double intensity = blk.Result[i].RSSI_1;

      // filter invalid blocks or zero returns
      if (blk.DataFlag != 0xFFEE || dist_m <= 0) {
        dist_m    = INF_DIST;
        intensity = 0;
      }

      int idx = b * POINTS_PER_BLOCK + i;
      if (inverted_) {
        idx = BLOCKS_PER_SCAN * POINTS_PER_BLOCK - 1 - idx;
      }

      scan[idx] = { ang_rad, dist_m, intensity };
    }
  }

  return scan;
}
