#pragma once
#include <stdint.h>
#include <stddef.h>

namespace FirmwareImage {
constexpr uint32_t bootSize = 0x4000;
constexpr uint32_t maxSize = 0x3e000;
inline bool validSize(uint32_t size) { return size >= bootSize + 8 && size <= maxSize; }
inline uint32_t word(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline bool validVectors(const uint8_t* p, uint32_t size) {
    uint32_t sp = word(p), reset = word(p + 4);
    return sp > 0x20000000 && sp <= 0x20008000 && !(sp & 7) &&
           (reset & 1) && reset >= 0x6001 && reset < 0x2000 + size;
}
inline uint32_t crc32(uint32_t crc, const uint8_t* data, size_t size) {
    while (size--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}
}
