#include <cstring>

#include "SharedMemory.h"

size_t SharedMemoryHelper::calculateMapSize(
    const std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>& map) {
    size_t total_size = sizeof(size_t);

    for (const auto& [_, inner_map] : map) {
        total_size += sizeof(uint8_t);
        total_size += sizeof(size_t);
        total_size += inner_map.size() * (sizeof(uint32_t) + sizeof(CipherData));
    }
    return total_size;
}

void SharedMemoryHelper::serializeMap(
    void* shm_ptr,
    const std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>& map) {
    char* buffer = static_cast<char*>(shm_ptr);

    size_t outer_size = map.size();
    memcpy(buffer, &outer_size, sizeof(size_t));
    buffer += sizeof(size_t);

    for (const auto& [outer_key, inner_map] : map) {
        memcpy(buffer, &outer_key, sizeof(uint8_t));
        buffer += sizeof(uint8_t);

        size_t inner_size = inner_map.size();
        memcpy(buffer, &inner_size, sizeof(size_t));
        buffer += sizeof(size_t);

        for (const auto& [inner_key, cipher_data] : inner_map) {
            memcpy(buffer, &inner_key, sizeof(uint32_t));
            buffer += sizeof(uint32_t);

            memcpy(buffer, &cipher_data, sizeof(CipherData));
            buffer += sizeof(CipherData);
        }
    }
}

void SharedMemoryHelper::deserializeMap(
    const void* shm_ptr,
    std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>& map) {
    const char* buffer = static_cast<const char*>(shm_ptr);

    size_t outer_size;
    memcpy(&outer_size, buffer, sizeof(size_t));
    buffer += sizeof(size_t);

    for (size_t i = 0; i < outer_size; i++) {
        uint8_t outer_key;
        memcpy(&outer_key, buffer, sizeof(uint8_t));
        buffer += sizeof(uint8_t);

        size_t inner_size;
        memcpy(&inner_size, buffer, sizeof(size_t));
        buffer += sizeof(size_t);

        auto& inner_map = map[outer_key];
        for (size_t j = 0; j < inner_size; j++) {
            uint32_t inner_key;
            memcpy(&inner_key, buffer, sizeof(uint32_t));
            buffer += sizeof(uint32_t);

            CipherData cipher_data;
            memcpy(&cipher_data, buffer, sizeof(CipherData));
            buffer += sizeof(CipherData);

            inner_map[inner_key] = cipher_data;
        }
    }
}

size_t SharedMemoryHelper::calculateVectorSize(const std::vector<std::vector<uint64_t>>& vec) {
    size_t total_size = sizeof(size_t);

    for (const auto& inner_vec : vec) {
        total_size += sizeof(size_t);
        total_size += inner_vec.size() * sizeof(uint64_t);
    }
    return total_size;
}

void SharedMemoryHelper::serializeVector(void* shm_ptr,
                                         const std::vector<std::vector<uint64_t>>& vec) {
    char* buffer = static_cast<char*>(shm_ptr);

    size_t outer_size = vec.size();
    memcpy(buffer, &outer_size, sizeof(size_t));
    buffer += sizeof(size_t);

    for (const auto& inner_vec : vec) {
        size_t inner_size = inner_vec.size();
        memcpy(buffer, &inner_size, sizeof(size_t));
        buffer += sizeof(size_t);

        memcpy(buffer, inner_vec.data(), inner_size * sizeof(uint64_t));
        buffer += inner_size * sizeof(uint64_t);
    }
}

void SharedMemoryHelper::deserializeVector(const void* shm_ptr,
                                           std::vector<std::vector<uint64_t>>& vec) {
    const char* buffer = static_cast<const char*>(shm_ptr);

    size_t outer_size;
    memcpy(&outer_size, buffer, sizeof(size_t));
    buffer += sizeof(size_t);

    vec.resize(outer_size);
    for (size_t i = 0; i < outer_size; i++) {
        size_t inner_size;
        memcpy(&inner_size, buffer, sizeof(size_t));
        buffer += sizeof(size_t);

        vec[i].resize(inner_size);
        memcpy(vec[i].data(), buffer, inner_size * sizeof(uint64_t));
        buffer += inner_size * sizeof(uint64_t);
    }
}
