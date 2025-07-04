#ifndef MSOP_PARSER_H
#define MSOP_PARSER_H

#include <cstdint>
#include <vector>
#include <string>

#pragma pack(push, 1)  // Ensure no padding in structs

// Structure for a single measuring result (6 bytes)
struct MeasuringResult {
    uint16_t distance_strongest;  // Distance in mm (big endian)
    uint8_t rssi_strongest;       // RSSI of strongest return
    uint16_t distance_last;       // Distance in mm (big endian) 
    uint8_t rssi_last;            // RSSI of last return
};

// Structure for a data block (100 bytes)
struct DataBlock {
    uint16_t flag;                          // Should be 0xFFEE (big endian)
    uint16_t azimuth;                       // Azimuth angle * 100 (big endian)
    MeasuringResult measurements[16];       // 16 measurement results
};

// Structure for MSOP packet tail (6 bytes)
struct MSOPTail {
    uint32_t timestamp;                     // Timestamp in microseconds (big endian)
    uint16_t factory_info;                  // Factory information (big endian)
};

// Complete MSOP packet structure (1206 bytes without UDP header)
struct MSOPPacket {
    DataBlock data_blocks[12];              // 12 data blocks (1200 bytes)
    MSOPTail tail;                          // Timestamp and factory info (6 bytes)
};

#pragma pack(pop)

// Parsed point data
struct LidarPoint {
    float azimuth;          // Horizontal angle in degrees
    float distance;         // Distance in meters
    uint8_t rssi;           // Signal strength
    bool is_valid;          // Whether this point contains valid data
    bool is_strongest;      // True for strongest return, false for last return
};

class MSOPParser {
public:
    MSOPParser();
    
    // Parse a single MSOP packet
    bool parsePacket(const uint8_t* data, size_t size, std::vector<LidarPoint>& points);
    
    // Get timestamp from the last parsed packet
    uint32_t getLastTimestamp() const { return last_timestamp_; }
    
    // Get factory info from the last parsed packet
    uint16_t getLastFactoryInfo() const { return last_factory_info_; }
    
    // Check if this is likely the last packet in a rotation
    bool isLastPacket(const MSOPPacket* packet) const;
    
private:
    // Convert big endian to host byte order
    uint16_t be16ToHost(uint16_t value) const;
    uint32_t be32ToHost(uint32_t value) const;
    
    // Calculate azimuth for a specific measurement within a block
    float calculateAzimuth(uint16_t block_azimuth, uint16_t next_block_azimuth, int measurement_index) const;
    
    // Validate data block
    bool isValidDataBlock(const DataBlock* block) const;
    
    // Check if azimuth is within valid 270° range (45° to 315°)
    bool isValidAzimuth(float azimuth) const;
    
    // Check if distance is within valid range (0.1m to 15m)
    bool isValidDistance(float distance) const;
    
    uint32_t last_timestamp_;
    uint16_t last_factory_info_;
};

#endif // MSOP_PARSER_H
