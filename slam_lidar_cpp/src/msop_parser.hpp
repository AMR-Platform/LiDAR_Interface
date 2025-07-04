#pragma once

#include "data_type.h"
#include <vector>
#include <cstdint>

struct ParsedPoint {
    double azimuth_degrees;  // Azimuth in degrees
    double distance_meters;  // Distance in meters
    uint8_t rssi;           // Signal strength
    bool is_valid;          // Whether this point has valid data
};

struct ParsedPacket {
    std::vector<ParsedPoint> points;  // All points from the packet
    uint32_t timestamp_us;            // Timestamp in microseconds
    uint16_t factory_info;            // Factory information
    bool is_last_packet;              // True if this is the last packet in rotation
};

class MSOPParser {
public:
    MSOPParser();
    
    // Parse a raw MSOP packet into structured data
    ParsedPacket parsePacket(const MSOPPacket& packet);
    
    // Check if packet is valid
    bool isValidPacket(const MSOPPacket& packet);
    
    // Get azimuth from data block (handles big-endian conversion)
    double getAzimuthDegrees(const DataBlock& block);
    
    // Get distance in meters from return data (handles big-endian conversion)
    double getDistanceMeters(const ReturnData& return_data);
    
    // Calculate interpolated azimuth for point N in a block
    double calculatePointAzimuth(double block_azimuth, double next_block_azimuth, int point_index);

private:
    // Utility functions for big-endian conversion
    uint16_t ntoh16(uint16_t value);
    uint32_t ntoh32(uint32_t value);
    
    // Check if a data block is valid
    bool isValidBlock(const DataBlock& block);
};
