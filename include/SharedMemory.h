#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "CipherData.h"

class SharedMemoryHelper {
  public:
    static size_t calculateMapSize(
        const std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>& map);
    static void serializeMap(
        void* shm_ptr,
        const std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>& map);
    static void deserializeMap(
        const void* shm_ptr, std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>& map);

    static size_t calculateVectorSize(const std::vector<std::vector<uint64_t>>& vec);
    static void serializeVector(void* shm_ptr, const std::vector<std::vector<uint64_t>>& vec);
    static void deserializeVector(const void* shm_ptr, std::vector<std::vector<uint64_t>>& vec);
};

#endif  // SHAREDMEMORY_H
