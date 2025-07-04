#include <cstdint>
#include <vector>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

// MSOP Packet Structure - 1206 bytes (without UDP header)
#pragma pack(push, 1)

struct ReturnData {
    uint16_t distance;  // Distance in mm (big-endian)
    uint8_t rssi;       // Signal strength
};

struct MeasuringResult {
    ReturnData strongest_return;  // First 3 bytes
    ReturnData last_return;       // Last 3 bytes  
};

struct DataBlock {
    uint16_t flag;              // 0xFFEE for valid, 0xFFFF for invalid
    uint16_t azimuth;           // Azimuth in 0.01 degrees (big-endian)
    MeasuringResult results[16]; // 16 measuring results
};

struct MSOPPacket {
    DataBlock blocks[12];    // 12 data blocks (1200 bytes)
    uint32_t timestamp;      // Timestamp in microseconds (big-endian)
    uint16_t factory;        // Factory information (big-endian)
};

#pragma pack(pop)

// Parsed point structure
struct ParsedPoint {
    double azimuth_degrees;
    double distance_meters;
    uint8_t rssi;
    bool is_valid;
    bool use_strongest_return;  // true for strongest, false for last return
};

class MSOPParser {
private:
    static constexpr uint16_t VALID_FLAG = 0xEEFF;    // Note: appears as 0xFFEE in documentation but stored as little-endian
    static constexpr uint16_t INVALID_FLAG = 0xFFFF;
    static constexpr int BLOCKS_PER_PACKET = 12;
    static constexpr int POINTS_PER_BLOCK = 16;

    // Convert big-endian to host byte order
    uint16_t ntoh16(uint16_t value) {
        return ntohs(value);
    }
    
    uint32_t ntoh32(uint32_t value) {
        return ntohl(value);
    }

    // Check if a data block is valid
    bool isValidBlock(const DataBlock& block) {
        return block.flag == VALID_FLAG;
    }

    // Get azimuth in degrees from data block
    double getAzimuthDegrees(const DataBlock& block) {
        uint16_t azimuth_raw = ntoh16(block.azimuth);
        return azimuth_raw / 100.0;  // Convert from 0.01 degree units
    }

    // Get distance in meters from return data
    double getDistanceMeters(const ReturnData& return_data) {
        uint16_t distance_raw = ntoh16(return_data.distance);
        if (distance_raw == 0 || distance_raw == 0xFFFF) {
            return 0.0;  // Invalid distance
        }
        return distance_raw / 1000.0;  // Convert from mm to meters
    }

    // Calculate interpolated azimuth for point N in a block
    double calculatePointAzimuth(double current_azimuth, double next_azimuth, int point_index) {
        if (point_index < 0 || point_index >= POINTS_PER_BLOCK) {
            return current_azimuth;
        }
        
        // Handle azimuth wraparound (360 degrees)
        double angle_diff = next_azimuth - current_azimuth;
        if (angle_diff < -180.0) {
            angle_diff += 360.0;
        } else if (angle_diff > 180.0) {
            angle_diff -= 360.0;
        }
        
        double angle_increment = angle_diff / POINTS_PER_BLOCK;
        double point_azimuth = current_azimuth + (angle_increment * point_index);
        
        // Normalize to 0-360 range
        while (point_azimuth < 0.0) point_azimuth += 360.0;
        while (point_azimuth >= 360.0) point_azimuth -= 360.0;
        
        return point_azimuth;
    }

public:
    // Parse a complete MSOP packet
    std::vector<ParsedPoint> parsePacket(const uint8_t* raw_data, size_t data_size) {
        std::vector<ParsedPoint> points;
        
        if (data_size != sizeof(MSOPPacket)) {
            std::cerr << "Invalid packet size: " << data_size << " expected: " << sizeof(MSOPPacket) << std::endl;
            return points;
        }
        
        const MSOPPacket* packet = reinterpret_cast<const MSOPPacket*>(raw_data);
        
        // Reserve space for all possible points
        points.reserve(BLOCKS_PER_PACKET * POINTS_PER_BLOCK * 2); // *2 for both returns
        
        // Process each data block
        for (int block_idx = 0; block_idx < BLOCKS_PER_PACKET; block_idx++) {
            const DataBlock& current_block = packet->blocks[block_idx];
            
            // Skip invalid blocks
            if (!isValidBlock(current_block)) {
                continue;
            }
            
            double current_azimuth = getAzimuthDegrees(current_block);
            double next_azimuth = current_azimuth;
            
            // Get next block azimuth for interpolation
            if (block_idx < BLOCKS_PER_PACKET - 1) {
                const DataBlock& next_block = packet->blocks[block_idx + 1];
                if (isValidBlock(next_block)) {
                    next_azimuth = getAzimuthDegrees(next_block);
                }
            } else {
                // For last block, estimate next azimuth
                if (block_idx > 0) {
                    const DataBlock& prev_block = packet->blocks[block_idx - 1];
                    if (isValidBlock(prev_block)) {
                        double prev_azimuth = getAzimuthDegrees(prev_block);
                        double increment = current_azimuth - prev_azimuth;
                        if (increment < -180.0) increment += 360.0;
                        else if (increment > 180.0) increment -= 360.0;
                        next_azimuth = current_azimuth + increment;
                    }
                }
            }
            
            // Process each point in the block
            for (int point_idx = 0; point_idx < POINTS_PER_BLOCK; point_idx++) {
                const MeasuringResult& result = current_block.results[point_idx];
                
                double point_azimuth = calculatePointAzimuth(current_azimuth, next_azimuth, point_idx);
                
                // Process strongest return
                double strongest_distance = getDistanceMeters(result.strongest_return);
                if (strongest_distance > 0.0) {
                    ParsedPoint point;
                    point.azimuth_degrees = point_azimuth;
                    point.distance_meters = strongest_distance;
                    point.rssi = result.strongest_return.rssi;
                    point.is_valid = true;
                    point.use_strongest_return = true;
                    points.push_back(point);
                }
                
                // Process last return (if different from strongest)
                double last_distance = getDistanceMeters(result.last_return);
                if (last_distance > 0.0 && last_distance != strongest_distance) {
                    ParsedPoint point;
                    point.azimuth_degrees = point_azimuth;
                    point.distance_meters = last_distance;
                    point.rssi = result.last_return.rssi;
                    point.is_valid = true;
                    point.use_strongest_return = false;
                    points.push_back(point);
                }
            }
        }
        
        return points;
    }
    
    // Get timestamp from packet
    uint32_t getTimestamp(const uint8_t* raw_data, size_t data_size) {
        if (data_size != sizeof(MSOPPacket)) {
            return 0;
        }
        
        const MSOPPacket* packet = reinterpret_cast<const MSOPPacket*>(raw_data);
        return ntoh32(packet->timestamp);
    }
    
    // Get factory information from packet
    uint16_t getFactoryInfo(const uint8_t* raw_data, size_t data_size) {
        if (data_size != sizeof(MSOPPacket)) {
            return 0;
        }
        
        const MSOPPacket* packet = reinterpret_cast<const MSOPPacket*>(raw_data);
        return ntoh16(packet->factory);
    }
    
    // Check if this is the last packet in a rotation (has invalid blocks at the end)
    bool isLastPacket(const uint8_t* raw_data, size_t data_size) {
        if (data_size != sizeof(MSOPPacket)) {
            return false;
        }
        
        const MSOPPacket* packet = reinterpret_cast<const MSOPPacket*>(raw_data);
        
        // Check if blocks 6-11 are invalid (as mentioned in documentation)
        int invalid_count = 0;
        for (int i = 6; i < BLOCKS_PER_PACKET; i++) {
            if (!isValidBlock(packet->blocks[i])) {
                invalid_count++;
            }
        }
        
        return invalid_count >= 3; // If more than half of the last blocks are invalid
    }
    
    // Validate packet structure
    bool validatePacket(const uint8_t* raw_data, size_t data_size) {
        if (data_size != sizeof(MSOPPacket)) {
            return false;
        }
        
        const MSOPPacket* packet = reinterpret_cast<const MSOPPacket*>(raw_data);
        
        // Check if at least one block is valid
        for (int i = 0; i < BLOCKS_PER_PACKET; i++) {
            if (isValidBlock(packet->blocks[i])) {
                return true;
            }
        }
        
        return false;
    }
    
    // Print packet information for debugging
    void printPacketInfo(const uint8_t* raw_data, size_t data_size) {
        if (data_size != sizeof(MSOPPacket)) {
            std::cout << "Invalid packet size: " << data_size << std::endl;
            return;
        }
        
        const MSOPPacket* packet = reinterpret_cast<const MSOPPacket*>(raw_data);
        
        std::cout << "MSOP Packet Info:" << std::endl;
        std::cout << "Timestamp: " << getTimestamp(raw_data, data_size) << " us" << std::endl;
        std::cout << "Factory: 0x" << std::hex << getFactoryInfo(raw_data, data_size) << std::dec << std::endl;
        std::cout << "Is Last Packet: " << (isLastPacket(raw_data, data_size) ? "Yes" : "No") << std::endl;
        
        std::cout << "Valid blocks: ";
        for (int i = 0; i < BLOCKS_PER_PACKET; i++) {
            if (isValidBlock(packet->blocks[i])) {
                double azimuth = getAzimuthDegrees(packet->blocks[i]);
                std::cout << i << "(" << azimuth << "°) ";
            }
        }
        std::cout << std::endl;
        
        auto points = parsePacket(raw_data, data_size);
        std::cout << "Total parsed points: " << points.size() << std::endl;
    }
};

// Example usage function
void example_usage() {
    MSOPParser parser;
    
    // Example: assuming you have raw UDP packet data
    uint8_t raw_packet[1206]; // Your packet data goes here
    
    // Validate the packet
    if (!parser.validatePacket(raw_packet, sizeof(raw_packet))) {
        std::cerr << "Invalid MSOP packet" << std::endl;
        return;
    }
    
    // Parse the packet
    auto points = parser.parsePacket(raw_packet, sizeof(raw_packet));
    
    // Get additional info
    uint32_t timestamp = parser.getTimestamp(raw_packet, sizeof(raw_packet));
    uint16_t factory = parser.getFactoryInfo(raw_packet, sizeof(raw_packet));
    bool is_last = parser.isLastPacket(raw_packet, sizeof(raw_packet));
    
    std::cout << "Parsed " << points.size() << " points" << std::endl;
    std::cout << "Timestamp: " << timestamp << " us" << std::endl;
    std::cout << "Factory: 0x" << std::hex << factory << std::dec << std::endl;
    std::cout << "Last packet: " << (is_last ? "Yes" : "No") << std::endl;
    
    // Print first few points
    for (size_t i = 0; i < std::min(size_t(5), points.size()); i++) {
        const auto& point = points[i];
        std::cout << "Point " << i << ": "
                  << "Azimuth=" << point.azimuth_degrees << "° "
                  << "Distance=" << point.distance_meters << "m "
                  << "RSSI=" << (int)point.rssi << " "
                  << "Return=" << (point.use_strongest_return ? "Strongest" : "Last")
                  << std::endl;
    }
}
