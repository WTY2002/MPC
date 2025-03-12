#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <vector>

std::pair<uint8_t, uint8_t> uint16ToBytes(uint16_t value);

uint16_t bytesToUint16(uint8_t high, uint8_t low);

void writeUint32(std::vector<uint8_t> &msg, std::size_t offset, uint32_t value_idx);

uint32_t readUint32(const std::vector<uint8_t> &msg, std::size_t offset);

#endif