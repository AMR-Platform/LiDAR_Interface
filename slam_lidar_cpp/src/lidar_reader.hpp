// src/lidar_reader.hpp

#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "data_type.h"

struct ScanPoint {
  double angle;     // radians
  double range;     // meters
  double intensity; // RSSI units
};

class LiDARReader {
public:
  LiDARReader(const std::string& host_ip,  // unused, kept for API compatibility
              int port,
              int angle_offset = 0,
              bool inverted    = false);
  ~LiDARReader();

  /// Blocks until one full scan (12×16 points) is read.
  std::vector<ScanPoint> readScan();

private:
  int sockfd_;
  int angle_offset_;
  bool inverted_;

  /// Bind UDP socket on all local interfaces, port only.
  void setupSocket(int port);
  void recvPacket(MSOP_Data_t& buf);
};
