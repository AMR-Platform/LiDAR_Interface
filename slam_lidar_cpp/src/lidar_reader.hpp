#pragma once
#include <vector>
#include <string>

struct ScanPoint {
  double angle;     // radians
  double range;     // meters
  double intensity; // RSSI units
};

class LiDARReader {
public:
  LiDARReader(const std::string& host_ip,
              int port,
              int angle_offset = 0,
              bool inverted    = false);
  ~LiDARReader();

  /// Blocks until it has read a full 360° scan (12×16 points).
  std::vector<ScanPoint> readScan();

private:
  int  sockfd_;
  int  angle_offset_;
  bool inverted_;

  void setupSocket(const std::string& host_ip, int port);
  void recvPacket(MSOP_Data_t& buf);
};
