#include "msop_parser.h"
#include <cstring>
#include <arpa/inet.h>  // For ntohl, ntohs

MSOPParser::MSOPParser() : last_timestamp_(0), last_factory_info_(0) {
}

bool MSOPParser::parsePacket(const uint8_t* data, size_t size, std::vector<LidarPoint>& points) {
    // Clear previous points
    points.clear();
    
    // Validate packet size (should be 1206 bytes without UDP header)
    if (size != sizeof(MSOPPacket)) {
        return false;
    }
    
    // Cast data to MSOP packet structure
    const MSOPPacket* packet = reinterpret_cast<const MSOPPacket*>(data);
    
    // Extract timestamp and factory info
    last_timestamp_ = be32ToHost(packet->tail.timestamp);
    last_factory_info_ = be16ToHost(packet->tail.factory_info);
    
    // Parse each data block
    for (int block_idx = 0; block_idx < 12; ++block_idx) {
        const DataBlock* current_block = &packet->data_blocks[block_idx];
        
        // Check if this is a valid data block
        if (!isValidDataBlock(current_block)) {
            // This might be the last packet with invalid blocks
            continue;
        }
        
        uint16_t current_azimuth = be16ToHost(current_block->azimuth);
        
        // Calculate next block azimuth for interpolation
        uint16_t next_azimuth = current_azimuth;
        if (block_idx < 11) {
            const DataBlock* next_block = &packet->data_blocks[block_idx + 1];
            if (isValidDataBlock(next_block)) {
                next_azimuth = be16ToHost(next_block->azimuth);
            }
        }
        
        // Parse each measurement in the block
        for (int meas_idx = 0; meas_idx < 16; ++meas_idx) {
            const MeasuringResult* measurement = &current_block->measurements[meas_idx];
            
            // Parse strongest return
            uint16_t distance_strongest = be16ToHost(measurement->distance_strongest);
            if (distance_strongest != 0 && distance_strongest != 0xFFFF) {
                LidarPoint point;
                point.azimuth = calculateAzimuth(current_azimuth, next_azimuth, meas_idx);
                point.distance = distance_strongest / 1000.0f;  // Convert mm to meters
                
                // Validate both azimuth and distance are within realistic ranges
                if (isValidAzimuth(point.azimuth) && isValidDistance(point.distance)) {
                    point.rssi = measurement->rssi_strongest;
                    point.is_valid = true;
                    point.is_strongest = true;
                    points.push_back(point);
                }
            }
            
            // Parse last return (if different from strongest)
            uint16_t distance_last = be16ToHost(measurement->distance_last);
            if (distance_last != 0 && distance_last != 0xFFFF && 
                distance_last != distance_strongest) {
                LidarPoint point;
                point.azimuth = calculateAzimuth(current_azimuth, next_azimuth, meas_idx);
                point.distance = distance_last / 1000.0f;  // Convert mm to meters
                
                // Validate both azimuth and distance are within realistic ranges
                if (isValidAzimuth(point.azimuth) && isValidDistance(point.distance)) {
                    point.rssi = measurement->rssi_last;
                    point.is_valid = true;
                    point.is_strongest = false;
                    points.push_back(point);
                }
            }
        }
    }
    
    return true;
}

bool MSOPParser::isLastPacket(const MSOPPacket* packet) const {
    // Check if blocks 6-11 have invalid flags (0xFFFF)
    for (int i = 6; i < 12; ++i) {
        uint16_t flag = be16ToHost(packet->data_blocks[i].flag);
        if (flag != 0xFFFF) {
            return false;
        }
    }
    return true;
}

uint16_t MSOPParser::be16ToHost(uint16_t value) const {
    return ntohs(value);
}

uint32_t MSOPParser::be32ToHost(uint32_t value) const {
    return ntohl(value);
}

float MSOPParser::calculateAzimuth(uint16_t block_azimuth, uint16_t next_block_azimuth, int measurement_index) const {
    // Direct implementation matching ROS2 driver exactly
    // resolution = (next_azimuth - current_azimuth) / 16
    int resolution = 0;
    if ((next_block_azimuth - block_azimuth) > 0) {
        resolution = (next_block_azimuth - block_azimuth) / 16;
    } else {
        // Use a default resolution when can't calculate (like ROS2 driver does)
        resolution = 25;  // Default from ROS2 driver
    }
    
    // Simple addition: block_azimuth + (resolution * i)
    uint16_t calculated_azimuth = block_azimuth + (resolution * measurement_index);
    
    // Convert to degrees and return
    float azimuth_degrees = calculated_azimuth / 100.0f;
    
    // Normalize angle to 0-360 range
    while (azimuth_degrees < 0.0f) azimuth_degrees += 360.0f;
    while (azimuth_degrees >= 360.0f) azimuth_degrees -= 360.0f;
    
    return azimuth_degrees;
}

bool MSOPParser::isValidDataBlock(const DataBlock* block) const {
    uint16_t flag = be16ToHost(block->flag);
    uint16_t azimuth = be16ToHost(block->azimuth);
    
    // Valid blocks should have flag 0xFFEE and azimuth != 0xFFFF
    // ROS2 driver doesn't check azimuth range here, so we'll be less strict
    return (flag == 0xFFEE) && (azimuth != 0xFFFF);
}

bool MSOPParser::isValidAzimuth(float azimuth) const {
    // Allow wider range than strict 45°-315° like ROS2 driver
    // Filter out clearly invalid angles but be more permissive
    return (azimuth >= 0.0f && azimuth <= 360.0f);
}

bool MSOPParser::isValidDistance(float distance) const {
    // LakiBeam1(L) has maximum range of 15 meters
    // Minimum range is typically 0.1m to avoid noise
    return (distance >= 0.1f && distance <= 15.0f);
}
