# Lidar MSOP Reader

This project provides a C++ implementation for reading and parsing MSOP (Main Stream Output Protocol) packets from LakiBeam1(L) lidar devices on Ubuntu 18.04.

## Overview

The MSOP protocol is used for lidar data transmission over UDP on port 2368. The LakiBeam1(L) has a **270° field of view**, scanning from **45° to 315°**. Each packet contains:
- 12 data blocks (100 bytes each)
- Each data block contains 16 measurement results
- Each measurement has both strongest and last return data
- Timestamp and factory information in the packet tail

## Packet Structure

```
MSOP Packet (1206 bytes without UDP header):
├── Data Blocks 0-11 (1200 bytes total)
│   ├── Flag (2 bytes): 0xFFEE
│   ├── Azimuth (2 bytes): Horizontal angle × 100
│   └── 16 × Measuring Results (6 bytes each)
│       ├── Strongest Return (3 bytes): Distance (2) + RSSI (1)
│       └── Last Return (3 bytes): Distance (2) + RSSI (1)
└── Tail (6 bytes)
    ├── Timestamp (4 bytes): Microseconds
    └── Factory Info (2 bytes)
```

## Features

- **Real-time UDP packet reception** on port 2368
- **Hardware-aware filtering** (calibrated for LV3 filter level)
- **MSOP packet parsing** with proper big-endian byte order handling
- **Accurate azimuth calculation** matching ROS2 driver implementation
- **270° field of view support** (45° to 315° scanning range)
- **Dual return processing** (strongest and last returns)
- **Distance validation** (0.1m to 15m range filtering)
- **Point cloud data extraction** with comprehensive validation
- **Data visualization tools** for analysis and debugging

## Building

### Prerequisites
- Ubuntu 18.04 or compatible Linux distribution
- GCC with C++11 support
- CMake 3.10 or higher

### Build Instructions

```bash
# Use the provided build script (recommended)
chmod +x build.sh
./build.sh

# Or build manually:
mkdir build
cd build
cmake ..
make

# Run the programs
sudo ./lidar_reader              # Real-time viewer
sudo ./lidar_visualizer          # Data collector
./test_angle_calculation         # Test angle computation
```

Note: Root privileges may be required to bind to UDP port 2368.

### Quick Test
To verify the angle calculation is working correctly:
```bash
./build/test_angle_calculation
```
This will show you how angles are calculated and verify the fix for the "multiples of 82°" issue.

## Usage

### Real-time Data Viewing
1. **Connect your lidar** to the Jetson Nano via Ethernet
2. **Configure network** to receive UDP packets on port 2368
3. **Run the program**: `sudo ./lidar_reader`
4. **View real-time data** as packets are received and parsed

### Data Collection and Visualization
1. **Collect data**: `sudo ./lidar_visualizer`
2. **Install Python dependencies**: `pip3 install matplotlib pandas numpy`
3. **Visualize data**: `python3 visualize_lidar.py`

## Output

The program displays:
- Packet count and size information
- Timestamp and factory information
- Sample point data (azimuth, distance, RSSI)
- Detection of last packets in rotation cycles

Example output:
```
--- Packet 1 ---
Received 1206 bytes
MSOP packet detected (1206 bytes - UDP header stripped)
LakiBeam1(L) - 270° Field of View (45° to 315°)
Timestamp: 245899399 μs, Factory: 0x3740, Points: 192 (270° FOV: 45°-315°)
  Point 0: Azimuth=96.00°, Distance=2.300m, RSSI=49 (strongest)
  Point 1: Azimuth=96.25°, Distance=2.350m, RSSI=52 (strongest)
  ...
```

## Code Structure

- **`msop_parser.h/cpp`**: Core MSOP packet parsing logic (exactly matches ROS2 driver)
- **`main.cpp`**: Real-time UDP receiver and data display
- **`lidar_visualizer.cpp`**: Data collector and visualization generator
- **`test_angle_calculation.cpp`**: Test program to verify angle calculation
- **`CMakeLists.txt`**: Build configuration for all programs
- **`build.sh`**: Convenient build script for Linux

## Technical Details

### Byte Order
All multi-byte values in MSOP packets use big-endian byte order and are converted to host byte order during parsing.

### Azimuth Calculation
The azimuth calculation now **exactly matches** the official ROS2 driver implementation:
```cpp
// Calculate resolution dynamically like ROS2 driver (integer math)
if ((next_block_azimuth - current_block_azimuth) > 0) {
    resolution = (next_block_azimuth - current_block_azimuth) / 16;
} else {
    resolution = 25;  // Default resolution when can't calculate
}

// Simple addition: block_azimuth + (resolution × measurement_index)
calculated_azimuth = block_azimuth + (resolution × measurement_index);
azimuth_degrees = calculated_azimuth / 100.0f;
```

This fixes the issue where angles appeared only in multiples of 82°. The resolution is now calculated dynamically for each packet pair, providing smooth, continuous angle coverage.

### Data Validation
The parser implements multi-level filtering to ensure data quality:

**1. Basic Validation:**
- Checks for valid flag bytes (0xFFEE for normal blocks, 0xFFFF for invalid)
- Validates distance values (0.1m to 15m range)
- Filters out zero or invalid distances

**2. Signal Strength Filtering:**
- RSSI threshold > 15 for initial collection
- RSSI threshold > 20-25 for final acceptance (accounting for LV3 hardware filtering)
- RSSI threshold > 25 for single-sample acceptance
- These thresholds are calibrated for LV3 hardware filter level on the lidar
- Higher thresholds help filter out weak reflections from hands, glass, or other poor reflectors

**3. Multi-Sample Validation (Visualizer):**
- Collects multiple samples per 0.5° angle bin over 150 packets (3 rounds of 50)
- Uses median-based filtering to handle outliers robustly:
  - **3+ samples**: Calculates median distance/RSSI, filters outliers beyond 50% deviation
  - **2 samples**: Accepts if distance difference < 30% and min RSSI > 15
  - **1 sample**: Accepts if RSSI > 20
- Selects the most representative point (closest to median with good RSSI)
- This approach is more robust than strict consistency checks and handles dynamic environments
- **Improved azimuth processing** matching ROS2 driver behavior
- Handles scanning direction changes properly

## Troubleshooting

### Common Issues

**"Angles only show multiples of 82°"**
- This was a bug in the angle calculation that has been **fixed**
- Run `./test_angle_calculation` to verify the fix
- The angles should now be continuous and smooth

**"Getting readings at 15m when blocking with hand"**
- This is normal behavior - weak reflections from your hand
- The improved filtering now rejects these with RSSI thresholds calibrated for LV3 hardware filtering
- Use `lidar_visualizer` for best filtering with multi-sample validation
- If still seeing issues, consider increasing lidar's hardware filter level to LV4 or LV5

**Permission denied**
- Run with `sudo` for UDP port access: `sudo ./lidar_reader`

**No packets received**
- Check network configuration and lidar connection
- Verify lidar is sending to port 2368
- Check firewall settings

**Unexpected packet size**
- Verify lidar configuration and network settings
- Some network stacks may add/remove UDP headers

**Build errors**
- Ensure CMake 3.10+ and GCC with C++11 support
- Try the build script: `./build.sh`

### Debugging Tips

1. **Test angle calculation**: `./test_angle_calculation`
2. **Check packet reception**: Look for "Received X bytes" messages
3. **Verify data validity**: Check the azimuth and distance ranges in output
4. **Use visualization**: `sudo ./lidar_visualizer` then `python3 visualize_lidar.py`

## License

This project is provided as-is for educational and development purposes.
