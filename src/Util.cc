#include "Util.h"

std::pair<uint8_t, uint8_t> uint16ToBytes(uint16_t value) {
    uint8_t high = static_cast<uint8_t>(value >> 8);
    uint8_t low = static_cast<uint8_t>(value & 0xFF);
    return {high, low};
}

uint16_t bytesToUint16(const uint8_t high, const uint8_t low) {
    return (static_cast<uint16_t>(high) << 8) | low;
}

void writeUint32(std::vector<uint8_t>& msg, std::size_t offset, uint32_t value_idx) {
    if (msg.size() < offset + 4) {
        msg.resize(offset + 4, 0);
    }
    msg[offset] = static_cast<uint8_t>(value_idx & 0xFF);
    msg[offset + 1] = static_cast<uint8_t>((value_idx >> 8) & 0xFF);
    msg[offset + 2] = static_cast<uint8_t>((value_idx >> 16) & 0xFF);
    msg[offset + 3] = static_cast<uint8_t>((value_idx >> 24) & 0xFF);
}

uint32_t readUint32(const std::vector<uint8_t>& msg, std::size_t offset) {
    uint32_t value = 0;
    value |= static_cast<uint32_t>(msg[offset]);
    value |= static_cast<uint32_t>(msg[offset + 1]) << 8;
    value |= static_cast<uint32_t>(msg[offset + 2]) << 16;
    value |= static_cast<uint32_t>(msg[offset + 3]) << 24;
    return value;
}

void load_array(std::ifstream& fin, std::vector<uint64_t>& arr, size_t num_elements) {
    arr.resize(num_elements);
    fin.read(reinterpret_cast<char*>(arr.data()), num_elements * sizeof(uint64_t));
    if (!fin) {
        std::cerr << "Error reading data from file." << std::endl;
        std::exit(1);
    }
}

FCNNWeights load_model_weights(const std::string& filename) {
    FCNNWeights model;
    std::ifstream fin(filename, std::ios::binary);
    if (!fin) {
        std::cerr << "Cannot open model weights file: " << filename << std::endl;
        std::exit(1);
    }

    load_array(fin, model.fc1.weights, 128 * 784);
    load_array(fin, model.fc1.bias, 128);
    load_array(fin, model.fc2.weights, 128 * 128);
    load_array(fin, model.fc2.bias, 128);
    load_array(fin, model.fc3.weights, 10 * 128);
    load_array(fin, model.fc3.bias, 10);

    fin.close();
    return model;
}

std::vector<std::vector<uint64_t>> load_test_pixels(const std::string& filename) {
    std::ifstream fin(filename, std::ios::binary);
    if (!fin) {
        std::cerr << "Cannot open test pixels file: " << filename << std::endl;
        std::exit(1);
    }
    fin.seekg(0, std::ios::end);
    size_t filesize = fin.tellg();
    fin.seekg(0, std::ios::beg);
    size_t num_values = filesize / sizeof(uint64_t);
    size_t num_images = num_values / 784;
    std::vector<uint64_t> all_pixels(num_values);
    fin.read(reinterpret_cast<char*>(all_pixels.data()), num_values * sizeof(uint64_t));
    fin.close();

    std::vector<std::vector<uint64_t>> images(num_images, std::vector<uint64_t>(784));
    for (size_t i = 0; i < num_images; i++) {
        for (size_t j = 0; j < 784; j++) {
            images[i][j] = all_pixels[i * 784 + j];
        }
    }
    return images;
}

std::vector<uint64_t> load_test_labels(const std::string& filename) {
    std::ifstream fin(filename, std::ios::binary);
    if (!fin) {
        std::cerr << "Cannot open test labels file: " << filename << std::endl;
        std::exit(1);
    }
    fin.seekg(0, std::ios::end);
    size_t filesize = fin.tellg();
    fin.seekg(0, std::ios::beg);
    size_t num_labels = filesize / sizeof(uint64_t);
    std::vector<uint64_t> labels(num_labels);
    fin.read(reinterpret_cast<char*>(labels.data()), num_labels * sizeof(uint64_t));
    fin.close();
    return labels;
}