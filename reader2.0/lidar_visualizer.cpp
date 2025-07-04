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
        
        py_file << R"(#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import LinearSegmentedColormap

# Read the CSV file
df = pd.read_csv(')" << csv_filename << R"(')

# Create figure with subplots
fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 12))

# 1. Polar plot (like traditional lidar view)
ax1.set_title('Lidar Data - Polar View')
scatter1 = ax1.scatter(df['azimuth'], df['distance'], 
                      c=df['rssi'], cmap='viridis', s=1, alpha=0.7)
ax1.set_xlabel('Azimuth (degrees)')
ax1.set_ylabel('Distance (meters)')
ax1.set_xlim(0, 360)
ax1.set_ylim(0, 15)
ax1.grid(True, alpha=0.3)
plt.colorbar(scatter1, ax=ax1, label='RSSI')

# 2. Cartesian plot (top-down view)
ax2.set_title('Lidar Data - Top-Down View')
scatter2 = ax2.scatter(df['x'], df['y'], 
                      c=df['rssi'], cmap='plasma', s=1, alpha=0.7)
ax2.set_xlabel('X (meters)')
ax2.set_ylabel('Y (meters)')
ax2.set_aspect('equal')
ax2.grid(True, alpha=0.3)
plt.colorbar(scatter2, ax=ax2, label='RSSI')

# 3. Distance histogram
ax3.set_title('Distance Distribution')
ax3.hist(df['distance'], bins=50, alpha=0.7, color='skyblue', edgecolor='black')
ax3.set_xlabel('Distance (meters)')
ax3.set_ylabel('Count')
ax3.grid(True, alpha=0.3)

# 4. Azimuth vs Distance heatmap
ax4.set_title('Azimuth vs Distance Density')
# Create 2D histogram
azimuth_bins = np.linspace(0, 360, 73)  # 5-degree bins
distance_bins = np.linspace(0, 15, 31)  # 0.5-meter bins
H, xedges, yedges = np.histogram2d(df['azimuth'], df['distance'], 
                                  bins=[azimuth_bins, distance_bins])
X, Y = np.meshgrid(xedges, yedges)
im = ax4.pcolormesh(X, Y, H.T, cmap='hot')
ax4.set_xlabel('Azimuth (degrees)')
ax4.set_ylabel('Distance (meters)')
plt.colorbar(im, ax=ax4, label='Point Density')

plt.tight_layout()

# Print statistics
print(f"Total points: {len(df)}")
print(f"Distance range: {df['distance'].min():.2f} - {df['distance'].max():.2f} m")
print(f"Azimuth range: {df['azimuth'].min():.1f} - {df['azimuth'].max():.1f}°")
print(f"RSSI range: {df['rssi'].min()} - {df['rssi'].max()}")

# Show valid field of view
valid_270_points = df[(df['azimuth'] >= 45) & (df['azimuth'] <= 315)]
print(f"Points in 270° FOV (45°-315°): {len(valid_270_points)} ({len(valid_270_points)/len(df)*100:.1f}%)")

plt.show()

# Save the plot
plt.savefig('lidar_visualization.png', dpi=300, bbox_inches='tight')
print("Visualization saved as 'lidar_visualization.png'")
)";
        
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
    std::cout << "Will collect 100 packets, then save and create visualizer" << std::endl;
    std::cout << "Press Ctrl+C to stop early" << std::endl;
    
    uint8_t buffer[2048];
    std::vector<LidarPoint> all_points;
    int packet_count = 0;
    const int max_packets = 100;
    
    while (packet_count < max_packets) {
        size_t received_size;
        if (!collector.receivePacket(buffer, sizeof(buffer), received_size)) {
            continue;
        }
        
        packet_count++;
        std::cout << "\rPacket " << packet_count << "/" << max_packets << std::flush;
        
        if (received_size == 1206 || received_size == 1248) {
            std::vector<LidarPoint> points;
            const uint8_t* data = (received_size == 1248) ? buffer + 42 : buffer;
            size_t data_size = (received_size == 1248) ? received_size - 42 : received_size;
            
            if (parser.parsePacket(data, data_size, points)) {
                // Add points to collection
                for (const auto& point : points) {
                    if (point.is_valid && point.distance > 0.1f && point.distance < 15.0f) {
                        all_points.push_back(point);
                    }
                }
            }
        }
    }
    
    std::cout << "\nCollected " << all_points.size() << " valid points" << std::endl;
    
    if (!all_points.empty()) {
        std::string csv_filename = "lidar_data.csv";
        collector.savePointsToCSV(all_points, csv_filename);
        collector.generatePythonVisualizer(csv_filename);
        
        std::cout << "\nData collection complete!" << std::endl;
        std::cout << "Files created:" << std::endl;
        std::cout << "- " << csv_filename << " (raw data)" << std::endl;
        std::cout << "- visualize_lidar.py (visualization script)" << std::endl;
        std::cout << "\nTo visualize: python3 visualize_lidar.py" << std::endl;
    } else {
        std::cout << "No valid points collected!" << std::endl;
    }
    
    return 0;
}
