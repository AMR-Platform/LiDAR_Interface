#include "lidar_reader.hpp"
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <host_ip> <udp_port>"
              << " [angle_offset] [inverted]\n";
    return 1;
  }

  std::string host_ip   = argv[1];
  int         port      = std::atoi(argv[2]);
  int         offset    = (argc>=4 ? std::atoi(argv[3]) : 0);
  bool        inverted  = (argc>=5 && std::atoi(argv[4])!=0);

  LiDARReader reader(host_ip, port, offset, inverted);

  while (true) {
    auto scan = reader.readScan();
    std::cout << "Scan (" << scan.size() << " points):\n";
    for (int i = 0; i < 5 && i < (int)scan.size(); ++i) {
      auto &p = scan[i];
      std::printf(" %2d | ang=%.3f rad | r=%.3f m | inten=%.0f\n",
                  i, p.angle, p.range, p.intensity);
    }
    std::cout << "----------------------\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}
