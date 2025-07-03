#include "lidar_reader.hpp"
#include <iostream>
#include <cmath>

int main(int argc, char** argv) {
  if(argc < 3){
    std::cerr << "Usage: " << argv[0] 
              << " <LiDAR_IP> <UDP_Port>\n";
    return 1;
  }
  std::string ip   = argv[1];
  int port         = std::atoi(argv[2]);

  // angle_offset=0, inverted=false
  LiDARReader reader(ip, port, 0, false);

  std::cout << "Reading one full scan (192 points)...\n";
  auto scan = reader.readScan();

  for(int i = 0; i < (int)scan.size(); ++i) {
    double deg = scan[i].angle * 180.0 / M_PI;
    std::printf("%3d | %7.2f° | %7.3f m\n",
                i, deg, scan[i].range);
  }
  return 0;
}
