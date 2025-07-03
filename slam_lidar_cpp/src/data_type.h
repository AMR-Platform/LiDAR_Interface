#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <cstdint>

#pragma pack(push,1)
// One pair of distance+intensity readings
typedef struct __attribute__((packed)) {
    uint16_t Dist_1;
    uint8_t  RSSI_1;
    uint16_t Dist_2;
    uint8_t  RSSI_2;
} MeasuringResult;

// One block of 16 points with azimuth and a data-flag
typedef struct __attribute__((packed)) {
    uint16_t DataFlag;
    uint16_t Azimuth;
    MeasuringResult Result[16];
} Data_block;

// Full MSOP packet: 12 blocks + timestamp + factory code
typedef struct __attribute__((packed)) {
    Data_block BlockID[12];
    uint32_t   Timestamp;
    uint16_t   Factory;
} MSOP_Data_t;
#pragma pack(pop)

#endif // DATA_TYPE_H
