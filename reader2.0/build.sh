#!/bin/bash

# Build script for Lidar MSOP Reader
# For Ubuntu 18.04 on Jetson Nano

echo "Building Lidar MSOP Reader..."

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

# Build the project
echo "Building project..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"
echo "To run the program: sudo ./build/lidar_reader"
echo "Note: Root privileges are required to bind to UDP port 2368"
