#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <cstdint>

#pragma pack(push,1)

// Single return data: 2 bytes distance + 1 byte RSSI
typedef struct __attribute__((packed)) {
    uint16_t distance;  // Distance in mm (big-endian)
    uint8_t  rssi;      // Signal strength
} ReturnData;

// Measuring result: strongest return + last return (6 bytes total)
typedef struct __attribute__((packed)) {
    ReturnData strongest_return;  // First 3 bytes
    ReturnData last_return;       // Last 3 bytes
} MeasuringResult;

// Data block: 100 bytes total
// 2 bytes flag + 2 bytes azimuth + 16 * 6 bytes measuring results = 100 bytes
typedef struct __attribute__((packed)) {
    uint16_t flag;              // 0xFFEE for valid data, 0xFFFF for invalid
    uint16_t azimuth;           // Azimuth in 0.01 degrees (big-endian)
    MeasuringResult results[16]; // 16 measuring results
} DataBlock;

// MSOP packet structure: 1206 bytes (without UDP header)
// 12 data blocks (1200 bytes) + 4 bytes timestamp + 2 bytes factory = 1206 bytes
typedef struct __attribute__((packed)) {
    DataBlock blocks[12];    // 12 data blocks (1200 bytes)
    uint32_t  timestamp;     // Timestamp in microseconds (big-endian)
    uint16_t  factory;       // Factory information (big-endian)
} MSOPPacket;

// Legacy typedef for compatibility
typedef MSOPPacket MSOP_Data_t;
typedef DataBlock Data_block;

#pragma pack(pop)

// Constants
static constexpr uint16_t VALID_FLAG = 0xFFEE;
static constexpr uint16_t INVALID_FLAG = 0xFFFF;
static constexpr int BLOCKS_PER_PACKET = 12;
static constexpr int POINTS_PER_BLOCK = 16;
static constexpr int PACKET_SIZE = 1206; // Without UDP header

#endif // DATA_TYPE_H
