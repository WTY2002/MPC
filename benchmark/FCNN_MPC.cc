#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <zmq.hpp>

#include "PCNode.h"
#include "ProcessA2BProtocol.h"
#include "Timer.h"
#include "Tools.h"
#include "Type.h"

constexpr int FIXED_SHIFT = 13;
constexpr int FC1_WEIGHTS_NUM = 128 * 784;
constexpr int FC1_BIAS_NUM = FC1_WEIGHTS_NUM + 128;
constexpr int FC2_WEIGHTS_NUM = FC1_BIAS_NUM + 128 * 128;
constexpr int FC2_BIAS_NUM = FC2_WEIGHTS_NUM + 128;
constexpr int FC3_WEIGHTS_NUM = FC2_BIAS_NUM + 10 * 128;
constexpr int FC3_BIAS_NUM = FC3_WEIGHTS_NUM + 10;

struct LayerWeights {
    std::vector<uint64_t> weights;
    std::vector<uint64_t> bias;
};

struct FCNNWeights {
    LayerWeights fc1;
    LayerWeights fc2;
    LayerWeights fc3;
};

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

struct LayerConfig {
    uint32_t input_size;
    uint32_t output_size;
    uint32_t input_start_idx;
    uint32_t output_start_idx;
    uint32_t weight_start_idx;
};

constexpr std::array<LayerConfig, 3> layerConfigs = {{{784, 128, 1000, 1784, 3000},
                                                      {128, 128, 1784, 1912, 3000 + FC1_BIAS_NUM},
                                                      {128, 10, 1912, 2040, 3000 + FC2_BIAS_NUM}}};

void fc_mpc_inference(std::vector<Node>& nodes, CentralNode& cnode, int layer_id, Timer& timer) {
    std::vector<std::thread> threads;
    threads.reserve(nodes.size());

    const LayerConfig& config = layerConfigs[layer_id - 1];
    uint32_t input_size = config.input_size;
    uint32_t output_size = config.output_size;
    uint32_t input_start_idx = config.input_start_idx;
    uint32_t output_start_idx = config.output_start_idx;
    uint32_t weight_start_idx = config.weight_start_idx;

    std::vector<uint8_t> dot_msg{ProtocolType::DOT_PRODUCT_OFF};
    dot_msg.insert(dot_msg.end(), 16, 0);
    writeUint32(dot_msg, 1, input_size);
    writeUint32(dot_msg, 5, input_start_idx);

    std::vector<uint8_t> add_msg{ProtocolType::ADD};
    add_msg.insert(add_msg.end(), 12, 0);
    writeUint32(add_msg, 1, 2);

    for (int output = 0; output < output_size; output++) {
        // Dot Product: input * weight
        dot_msg[0] = ProtocolType::DOT_PRODUCT_OFF;
        writeUint32(dot_msg, 9, weight_start_idx + input_size * output);
        writeUint32(dot_msg, 13, 1);
        // offline phase
        for (auto& node : nodes) {
            threads.emplace_back([&node]() { node.Receive(); });
        }
        for (int i = 1; i <= 5; ++i) {
            cnode.Publish(i, dot_msg);
        }
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();

        timer.resume();
        // online phase
        dot_msg[0] = ProtocolType::DOT_PRODUCT_ON;
        for (auto& node : nodes) {
            threads.emplace_back([&node]() { node.Receive(); });
        }
        for (int i = 1; i <= 5; ++i) {
            cnode.Publish(i, dot_msg);
        }
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();
        timer.stop();

        // Truncation
        GenerateTruncationParams(nodes, cnode, 100, 1);
        timer.resume();
        ProcessTruncation(nodes, cnode, 1, 1, 2);
        timer.stop();

        // Add bias
        writeUint32(add_msg, 5, weight_start_idx + input_size * output_size + output);
        writeUint32(add_msg, 9, output_start_idx + output);
        timer.resume();
        for (auto& node : nodes) {
            threads.emplace_back([&node]() { node.Receive(); });
        }
        for (int i = 1; i <= 5; ++i) {
            cnode.Publish(i, add_msg);
        }
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();
        timer.stop();

        // A2B protocol to get MSB
        ProcessA2BOff(nodes, cnode, output_start_idx + output, 2, 1);
        timer.resume();
        ProcessA2BOn(nodes, cnode, output_start_idx + output, 2, 1);
        timer.stop();

        // for (int node_id = 1; node_id <= 5; ++node_id) {
        //}
    }
}

int main() {
    FCNNWeights model = load_model_weights("../benchmark/model_weights.bin");

    std::vector<std::vector<uint64_t>> test_images =
        load_test_pixels("../benchmark/test_pixels.bin");
    std::vector<uint64_t> test_labels = load_test_labels("../benchmark/test_labels.bin");

    zmq::context_t context(12);
    std::vector<Node> nodes;
    std::vector<std::thread> threads;
    CentralNode cnode(context);

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::vector<uint8_t> key(16);
    *reinterpret_cast<uint64_t*>(key.data()) = dis(gen);
    *reinterpret_cast<uint64_t*>(key.data() + 8) = dis(gen);

    uint32_t run_time_space = 2999;
    for (uint8_t id = 1; id <= 5; ++id) {
        nodes.emplace_back(context, id);
        nodes.back().SetKey(key);
        nodes.back().InitializeMaps(run_time_space);
    }

    // Set and share model weights
    uint32_t weight_space = run_time_space + 1;
    // fc1: weights [128 x 784]
    for (auto& weight : model.fc1.weights) {
        nodes[0].SetValues(weight_space++, weight);
    }
    // fc1 bias: 128
    for (auto& bias : model.fc1.bias) {
        nodes[0].SetValues(weight_space++, bias);
    }
    // fc2: weights [128 x 128]
    for (auto& weight : model.fc2.weights) {
        nodes[0].SetValues(weight_space++, weight);
    }
    // fc2 bias: 128
    for (auto& bias : model.fc2.bias) {
        nodes[0].SetValues(weight_space++, bias);
    }
    // fc3: weights [10 x 128]
    for (auto& weight : model.fc3.weights) {
        nodes[0].SetValues(weight_space++, weight);
    }
    // fc3 bias: 10
    for (auto& bias : model.fc3.bias) {
        nodes[0].SetValues(weight_space++, bias);
    }

    std::vector<uint8_t> share_msg = {ProtocolType::SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    for (uint32_t index = run_time_space + 1; index < weight_space; index++) {
        threads.reserve(nodes.size());
        for (auto& node : nodes) {
            threads.emplace_back([&node]() { node.Receive(); });
        }
        writeUint32(share_msg, 6, index);
        for (int i = 1; i <= 5; ++i) {
            cnode.Publish(i, share_msg);
        }
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();
    }

    share_msg[0] = ProtocolType::SHARE_BETA;
    for (uint32_t index = run_time_space + 1; index < weight_space; index++) {
        for (auto& node : nodes) {
            threads.emplace_back([&node]() { node.Receive(); });
        }
        writeUint32(share_msg, 6, index);
        for (int i = 1; i <= 5; ++i) {
            cnode.Publish(i, share_msg);
        }
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();
    }

    Timer timer;
    size_t total = test_images.size();
    for (size_t image = 0; image < 10; image++) {
        uint32_t input_start_idx = 1000;
        // Set and share inputs
        uint32_t input_space = input_start_idx;
        for (auto& input : test_images[image]) {
            nodes[0].SetValues(input_space++, input);
        }

        share_msg[0] = ProtocolType::SHARE_BETA_OFF;
        for (uint32_t index = input_space; index < input_space; index++) {
            for (auto& node : nodes) {
                threads.emplace_back([&node]() { node.Receive(); });
            }
            writeUint32(share_msg, 6, index);
            for (int i = 1; i <= 5; ++i) {
                cnode.Publish(i, share_msg);
            }
            for (auto& t : threads) {
                t.join();
            }
            threads.clear();
        }

        share_msg[0] = ProtocolType::SHARE_BETA;
        for (uint32_t index = input_start_idx; index < input_space; index++) {
            for (auto& node : nodes) {
                threads.emplace_back([&node]() { node.Receive(); });
            }
            writeUint32(share_msg, 6, index);
            for (int i = 1; i <= 5; ++i) {
                cnode.Publish(i, share_msg);
            }
            for (auto& t : threads) {
                t.join();
            }
            threads.clear();
        }

        for (int layer_id = 1; layer_id <= 3; layer_id++) {
            fc_mpc_inference(nodes, cnode, layer_id, timer);
        }
        timer.stop();
        std::cout << "Node" << image << " elapsed time: " << timer.elapsedMicroseconds() << " us"
                  << std::endl;
    }

    std::cout << "Total elapsed time: " << timer.elapsedMicroseconds() << " us" << std::endl;

    return 0;
}
