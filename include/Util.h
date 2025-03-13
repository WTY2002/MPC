#ifndef UTIL_H
#define UTIL_H

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

struct ShareCondition {
    std::array<uint8_t, 5> node_idx;  // [sender1, sender2, sender3, receiver, zero_share]
};

struct LayerWeights {
    std::vector<uint64_t> weights;
    std::vector<uint64_t> bias;
};

struct FCNNWeights {
    LayerWeights fc1;
    LayerWeights fc2;
    LayerWeights fc3;
};

struct FcnnLayerConfig {
    uint32_t input_size;
    uint32_t output_size;
    uint32_t input_start_idx;
    uint32_t output_start_idx;
    uint32_t weight_start_idx;
};

constexpr std::array<FcnnLayerConfig, 3> FcnnLayerConfigs = {
    {{784, 128, 1000, 1784, 3000}, {128, 128, 1784, 1912, 103480}, {128, 10, 1912, 2040, 119992}}};

std::pair<uint8_t, uint8_t> uint16ToBytes(uint16_t value);

uint16_t bytesToUint16(uint8_t high, uint8_t low);

void writeUint32(std::vector<uint8_t>& msg, std::size_t offset, uint32_t value_idx);

uint32_t readUint32(const std::vector<uint8_t>& msg, std::size_t offset);

void load_array(std::ifstream& fin, std::vector<uint64_t>& arr, size_t num_elements);

FCNNWeights load_model_weights(const std::string& filename);

std::vector<std::vector<uint64_t>> load_test_pixels(const std::string& filename);

std::vector<uint64_t> load_test_labels(const std::string& filename);

#endif