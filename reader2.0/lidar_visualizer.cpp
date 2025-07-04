#include "msop_parser.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <map>
#include <algorithm>

class LidarDataCollector {
public:
    LidarDataCollector(int port = 2368) : port_(port), socket_fd_(-1) {
    }
    
    ~LidarDataCollector() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }
    
    bool initialize() {
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        int opt = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Error setting socket options: " << strerror(errno) << std::endl;
            return false;
        }
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port_);
        
        if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Error binding socket to port " << port_ << ": " << strerror(errno) << std::endl;
            return false;
        }
        
        std::cout << "Data collector initialized on port " << port_ << std::endl;
        return true;
    }
    
    bool receivePacket(uint8_t* buffer, size_t buffer_size, size_t& received_size) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        ssize_t bytes_received = recvfrom(socket_fd_, buffer, buffer_size, 0,
                                         (struct sockaddr*)&client_addr, &client_len);
        
        if (bytes_received < 0) {
            std::cerr << "Error receiving packet: " << strerror(errno) << std::endl;
            return false;
        }
        
        received_size = bytes_received;
        return true;
    }
    
    void savePointsToCSV(const std::vector<LidarPoint>& all_points, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }
        
        // Write CSV header
        file << "x,y,distance,azimuth,rssi,return_type\n";
        
        // Convert polar to cartesian and save
        for (const auto& point : all_points) {
            if (point.is_valid && point.distance > 0.1f && point.distance < 15.0f) {
                // Convert polar to cartesian coordinates
                float azimuth_rad = point.azimuth * M_PI / 180.0f;
                float x = point.distance * cos(azimuth_rad);
                float y = point.distance * sin(azimuth_rad);
                
                file << std::fixed << std::setprecision(3) 
                     << x << "," << y << "," 
                     << point.distance << "," << point.azimuth << ","
                     << (int)point.rssi << ","
                     << (point.is_strongest ? "strongest" : "last") << "\n";
            }
        }
        
        file.close();
        std::cout << "Saved " << all_points.size() << " points to " << filename << std::endl;
    }
    
    void generatePythonVisualizer(const std::string& csv_filename) {
        std::string py_filename = "visualize_lidar.py";
        std::ofstream py_file(py_filename);
        
        py_file << "#!/usr/bin/env python3\n";
        py_file << "import pandas as pd\n";
        py_file << "import matplotlib.pyplot as plt\n";
        py_file << "import numpy as np\n";
        py_file << "from matplotlib.colors import LinearSegmentedColormap\n\n";
        
        py_file << "# Read the CSV file\n";
        py_file << "df = pd.read_csv('" << csv_filename << "')\n\n";
        
        py_file << "# Create figure with subplots\n";
        py_file << "fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 12))\n\n";
        
        py_file << "# 1. Polar plot (like traditional lidar view)\n";
        py_file << "ax1.set_title('Lidar Data - Polar View')\n";
        py_file << "scatter1 = ax1.scatter(df['azimuth'], df['distance'], c=df['rssi'], cmap='viridis', s=1, alpha=0.7)\n";
        py_file << "ax1.set_xlabel('Azimuth (degrees)')\n";
        py_file << "ax1.set_ylabel('Distance (meters)')\n";
        py_file << "ax1.set_xlim(0, 360)\n";
        py_file << "ax1.set_ylim(0, 15)\n";
        py_file << "ax1.grid(True, alpha=0.3)\n";
        py_file << "plt.colorbar(scatter1, ax=ax1, label='RSSI')\n\n";
        
        py_file << "# 2. Cartesian plot (top-down view)\n";
        py_file << "ax2.set_title('Lidar Data - Top-Down View')\n";
        py_file << "scatter2 = ax2.scatter(df['x'], df['y'], c=df['rssi'], cmap='plasma', s=1, alpha=0.7)\n";
        py_file << "ax2.set_xlabel('X (meters)')\n";
        py_file << "ax2.set_ylabel('Y (meters)')\n";
        py_file << "ax2.set_aspect('equal')\n";
        py_file << "ax2.grid(True, alpha=0.3)\n";
        py_file << "plt.colorbar(scatter2, ax=ax2, label='RSSI')\n\n";
        
        py_file << "# 3. Distance histogram\n";
        py_file << "ax3.set_title('Distance Distribution')\n";
        py_file << "ax3.hist(df['distance'], bins=50, alpha=0.7, color='skyblue', edgecolor='black')\n";
        py_file << "ax3.set_xlabel('Distance (meters)')\n";
        py_file << "ax3.set_ylabel('Count')\n";
        py_file << "ax3.grid(True, alpha=0.3)\n\n";
        
        py_file << "# 4. Azimuth vs Distance heatmap\n";
        py_file << "ax4.set_title('Azimuth vs Distance Density')\n";
        py_file << "azimuth_bins = np.linspace(0, 360, 73)  # 5-degree bins\n";
        py_file << "distance_bins = np.linspace(0, 15, 31)  # 0.5-meter bins\n";
        py_file << "H, xedges, yedges = np.histogram2d(df['azimuth'], df['distance'], bins=[azimuth_bins, distance_bins])\n";
        py_file << "X, Y = np.meshgrid(xedges, yedges)\n";
        py_file << "im = ax4.pcolormesh(X, Y, H.T, cmap='hot')\n";
        py_file << "ax4.set_xlabel('Azimuth (degrees)')\n";
        py_file << "ax4.set_ylabel('Distance (meters)')\n";
        py_file << "plt.colorbar(im, ax=ax4, label='Point Density')\n\n";
        
        py_file << "plt.tight_layout()\n\n";
        
        py_file << "# Print statistics\n";
        py_file << "print(f\"Total points: {len(df)}\")\n";
        py_file << "print(f\"Distance range: {df['distance'].min():.2f} - {df['distance'].max():.2f} m\")\n";
        py_file << "print(f\"Azimuth range: {df['azimuth'].min():.1f} - {df['azimuth'].max():.1f} degrees\")\n";
        py_file << "print(f\"RSSI range: {df['rssi'].min()} - {df['rssi'].max()}\")\n\n";
        
        py_file << "# Show valid field of view\n";
        py_file << "valid_270_points = df[(df['azimuth'] >= 45) & (df['azimuth'] <= 315)]\n";
        py_file << "print(f\"Points in 270 degree FOV (45-315): {len(valid_270_points)} ({len(valid_270_points)/len(df)*100:.1f}%)\")\n\n";
        
        py_file << "plt.show()\n\n";
        py_file << "# Save the plot\n";
        py_file << "plt.savefig('lidar_visualization.png', dpi=300, bbox_inches='tight')\n";
        py_file << "print('Visualization saved as lidar_visualization.png')\n";
        
        py_file.close();
        std::cout << "Generated Python visualizer: " << py_filename << std::endl;
        std::cout << "Run: python3 " << py_filename << " (requires matplotlib and pandas)" << std::endl;
    }

private:
    int port_;
    int socket_fd_;
};

int main() {
    LidarDataCollector collector(2368);
    MSOPParser parser;
    
    if (!collector.initialize()) {
        return -1;
    }
    
    std::cout << "Collecting lidar data for visualization..." << std::endl;
    std::cout << "LakiBeam1(L): 10Hz frequency, 0.5° resolution" << std::endl;
    std::cout << "Will collect multiple samples per angle bin for reliability" << std::endl;
    std::cout << "Will collect 100 packets with RSSI filtering" << std::endl;
    std::cout << "Press Ctrl+C to stop early" << std::endl;
    
    uint8_t buffer[2048];
    
    // Structure to store multiple samples per angle
    struct AngleSamples {
        std::vector<LidarPoint> samples;
        LidarPoint best_point;
        bool has_valid_point = false;
    };
    
    // Use a map to store multiple samples per 0.5° angle bin
    std::map<int, AngleSamples> angle_map;  // Key: angle * 2 (for 0.5° bins)
    
    int packet_count = 0;
    const int max_packets = 100;  // More packets for better sampling
    
    while (packet_count < max_packets) {
        size_t received_size;
        if (!collector.receivePacket(buffer, sizeof(buffer), received_size)) {
            continue;
        }
        
        packet_count++;
        std::cout << "\rPacket " << packet_count << "/" << max_packets 
                  << " (Unique angles: " << angle_map.size() << ")" << std::flush;
        
        if (received_size == 1206 || received_size == 1248) {
            std::vector<LidarPoint> points;
            const uint8_t* data = (received_size == 1248) ? buffer + 42 : buffer;
            size_t data_size = (received_size == 1248) ? received_size - 42 : received_size;
            
            if (parser.parsePacket(data, data_size, points)) {
                // Process each point and collect multiple samples per angle bin
                for (const auto& point : points) {
                    // Enhanced filtering for reliable measurements
                    // Use stricter thresholds to filter out weak reflections from hands/objects
                    if (point.is_valid && 
                        point.distance > 0.1f && point.distance < 13.0f &&  // Reduced max range
                        point.rssi > 20) {  // Increased RSSI threshold to filter weak reflections
                        
                        // Create angle bin key (0.5° resolution)
                        int angle_bin = static_cast<int>(point.azimuth * 2.0f);
                        
                        // Store multiple samples for this angle
                        angle_map[angle_bin].samples.push_back(point);
                    }
                }
            }
        }
    }
    
    std::cout << "\nCollected " << angle_map.size() << " unique angle measurements" << std::endl;
    std::cout << "Processing samples and selecting best points..." << std::endl;
    
    if (!angle_map.empty()) {
        // Convert map to vector, selecting best point from samples
        std::vector<LidarPoint> unique_points;
        unique_points.reserve(angle_map.size());
        
        int filtered_bins = 0;
        for (const auto& pair : angle_map) {
            const auto& samples = pair.second.samples;
            
            // Require at least 2 consistent samples for reliability
            if (samples.size() >= 2) {
                // Calculate statistics for this angle bin
                float mean_distance = 0.0f;
                float mean_rssi = 0.0f;
                for (const auto& sample : samples) {
                    mean_distance += sample.distance;
                    mean_rssi += sample.rssi;
                }
                mean_distance /= samples.size();
                mean_rssi /= samples.size();
                
                // Check for consistency: all samples should be within 20% of mean distance
                bool consistent = true;
                for (const auto& sample : samples) {
                    float distance_deviation = std::abs(sample.distance - mean_distance) / mean_distance;
                    if (distance_deviation > 0.20f) {  // 20% tolerance
                        consistent = false;
                        break;
                    }
                }
                
                if (consistent && mean_rssi > 25) {  // Higher RSSI threshold for reliable points
                    // Find the sample with the strongest signal (highest RSSI)
                    auto best_sample = *std::max_element(samples.begin(), samples.end(),
                        [](const LidarPoint& a, const LidarPoint& b) {
                            return a.rssi < b.rssi;
                        });
                    
                    unique_points.push_back(best_sample);
                } else {
                    filtered_bins++;
                }
            } else {
                // Single sample - require higher RSSI for acceptance
                if (!samples.empty() && samples[0].rssi > 35) {
                    unique_points.push_back(samples[0]);
                } else {
                    filtered_bins++;
                }
            }
        }
        
        std::cout << "Filtered out " << filtered_bins << " unreliable angle bins" << std::endl;
        std::cout << "Keeping " << unique_points.size() << " reliable measurements" << std::endl;
        
        // Sort by azimuth for cleaner output
        std::sort(unique_points.begin(), unique_points.end(), 
                  [](const LidarPoint& a, const LidarPoint& b) {
                      return a.azimuth < b.azimuth;
                  });
        
        std::string csv_filename = "lidar_scan.csv";
        collector.savePointsToCSV(unique_points, csv_filename);
        collector.generatePythonVisualizer(csv_filename);
        
        // Print angle coverage statistics
        float min_angle = unique_points.front().azimuth;
        float max_angle = unique_points.back().azimuth;
        float angle_span = max_angle - min_angle;
        
        std::cout << "\nData collection complete!" << std::endl;
        std::cout << "Expected angles for 270° FOV at 0.5° resolution: " << (270.0f / 0.5f) << " points" << std::endl;
        std::cout << "Actual coverage: " << unique_points.size() << " points" << std::endl;
        std::cout << "Angle coverage: " << std::fixed << std::setprecision(1) 
                  << min_angle << "° to " << max_angle << "° (span: " << angle_span << "°)" << std::endl;
        std::cout << "Average angular resolution: " << std::setprecision(2) 
                  << (angle_span / (unique_points.size() - 1)) << "° per point" << std::endl;
        std::cout << "Files created:" << std::endl;
        std::cout << "- " << csv_filename << " (scan line data)" << std::endl;
        std::cout << "- visualize_lidar.py (visualization script)" << std::endl;
        std::cout << "\nTo visualize: python3 visualize_lidar.py" << std::endl;
    } else {
        std::cout << "No valid points collected!" << std::endl;
    }
    
    return 0;
}
