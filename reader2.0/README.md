# Lidar MSOP Reader

This project provides a C++ implementation for reading and parsing MSOP (Main Stream Output Protocol) packets from LakiBeam1(L) lidar devices on Ubuntu 18.04.

## Overview

The MSOP protocol is used for lidar data transmission over UDP on port 2368. Each packet contains:
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
- **MSOP packet parsing** with proper big-endian byte order handling
- **Azimuth interpolation** for accurate angular positioning
- **Dual return processing** (strongest and last returns)
- **Invalid data detection** for last packets in rotation
- **Point cloud data extraction** with validation

## Building

### Prerequisites
- Ubuntu 18.04 or compatible Linux distribution
- GCC with C++11 support
- CMake 3.10 or higher

### Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# Run the program
sudo ./lidar_reader
```

Note: Root privileges may be required to bind to UDP port 2368.

## Usage

1. **Connect your lidar** to the Jetson Nano via Ethernet
2. **Configure network** to receive UDP packets on port 2368
3. **Run the program**: `sudo ./lidar_reader`
4. **View real-time data** as packets are received and parsed

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
Timestamp: 245899399 μs, Factory: 0x3740, Points: 192
  Point 0: Azimuth=96.00°, Distance=2.300m, RSSI=49 (strongest)
  Point 1: Azimuth=96.25°, Distance=2.350m, RSSI=52 (strongest)
  ...
```

## Code Structure

- **`msop_parser.h/cpp`**: Core MSOP packet parsing logic
- **`main.cpp`**: UDP receiver and main application
- **`CMakeLists.txt`**: Build configuration

## Technical Details

### Byte Order
All multi-byte values in MSOP packets use big-endian byte order and are converted to host byte order during parsing.

### Azimuth Calculation
The azimuth for each measurement point is interpolated within each data block:
```cpp
azimuth_n = block_azimuth + (angle_increment / 16) × n
```

### Data Validation
- Checks for valid flag bytes (0xFFEE for normal blocks, 0xFFFF for invalid)
- Validates distance values (non-zero, not 0xFFFF)
- Handles last packet detection with invalid data blocks

## Troubleshooting

- **Permission denied**: Run with `sudo` for port access
- **No packets received**: Check network configuration and lidar connection
- **Unexpected packet size**: Verify lidar configuration and network settings

## License

This project is provided as-is for educational and development purposes.
