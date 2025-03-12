#include "Util.h"

std::pair<uint8_t, uint8_t> uint16ToBytes(uint16_t value) {
    uint8_t high = static_cast<uint8_t>(value >> 8);
    uint8_t low = static_cast<uint8_t>(value & 0xFF);
    return {high, low};
}

uint16_t bytesToUint16(const uint8_t high, const uint8_t low) {
    return (static_cast<uint16_t>(high) << 8) | low;
}

void writeUint32(std::vector<uint8_t> &msg, std::size_t offset, uint32_t value_idx) {
    if (msg.size() < offset + 4) {
        msg.resize(offset + 4, 0);
    }
    msg[offset] = static_cast<uint8_t>(value_idx & 0xFF);
    msg[offset + 1] = static_cast<uint8_t>((value_idx >> 8) & 0xFF);
    msg[offset + 2] = static_cast<uint8_t>((value_idx >> 16) & 0xFF);
    msg[offset + 3] = static_cast<uint8_t>((value_idx >> 24) & 0xFF);
}

uint32_t readUint32(const std::vector<uint8_t> &msg, std::size_t offset) {
    uint32_t value = 0;
    value |= static_cast<uint32_t>(msg[offset]);
    value |= static_cast<uint32_t>(msg[offset + 1]) << 8;
    value |= static_cast<uint32_t>(msg[offset + 2]) << 16;
    value |= static_cast<uint32_t>(msg[offset + 3]) << 24;
    return value;
}
