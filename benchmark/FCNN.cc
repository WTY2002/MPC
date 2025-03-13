#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include "Util.h"

constexpr int FIXED_SHIFT = 13;
constexpr uint64_t FIXED_SCALE = 1ULL << FIXED_SHIFT;

int64_t fixed_mul(int64_t a, int64_t b, int shift = FIXED_SHIFT) {
    __int128 temp = static_cast<__int128>(a) * static_cast<__int128>(b);
    return static_cast<int64_t>(temp >> shift);
}

std::vector<uint64_t> fc_layer(const std::vector<uint64_t>& input,
                               const std::vector<uint64_t>& weights,
                               const std::vector<uint64_t>& bias, int input_size, int output_size,
                               bool relu = true) {
    std::vector<uint64_t> output(output_size, 0);
    for (int j = 0; j < output_size; j++) {
        int64_t sum = 0;
        for (int i = 0; i < input_size; i++) {
            auto a = static_cast<int64_t>(input[i]);
            auto w = static_cast<int64_t>(weights[j * input_size + i]);
            sum += fixed_mul(a, w);
        }
        sum += static_cast<int64_t>(bias[j]);
        if (relu && sum < 0)
            sum = 0;
        output[j] = static_cast<uint64_t>(sum);
    }
    return output;
}

int fcnn_inference(const std::vector<uint64_t>& image, const FCNNWeights& weights) {
    std::vector<uint64_t> out_fc1 =
        fc_layer(image, weights.fc1.weights, weights.fc1.bias, 784, 128, true);
    std::vector<uint64_t> out_fc2 =
        fc_layer(out_fc1, weights.fc2.weights, weights.fc2.bias, 128, 128, true);
    std::vector<uint64_t> out_fc3 =
        fc_layer(out_fc2, weights.fc3.weights, weights.fc3.bias, 128, 10, false);

    int best = 0;
    auto best_val = static_cast<int64_t>(out_fc3[0]);
    for (int i = 1; i < 10; i++) {
        auto cur = static_cast<int64_t>(out_fc3[i]);
        if (cur > best_val) {
            best_val = cur;
            best = i;
        }
    }
    return best;
}

int main() {
    FCNNWeights model = load_model_weights("../benchmark/model_weights.bin");

    std::vector<std::vector<uint64_t>> test_images =
        load_test_pixels("../benchmark/test_pixels.bin");
    std::vector<uint64_t> test_labels = load_test_labels("../benchmark/test_labels.bin");

    size_t total = test_images.size();
    size_t correct = 0;

    for (size_t i = 0; i < total; i++) {
        int pred = fcnn_inference(test_images[i], model);
        int label = static_cast<int>(test_labels[i]);
        if (pred == label) {
            correct++;
        }
    }

    std::cout << "Total samples: " << total << std::endl;
    std::cout << "Correct predictions: " << correct << std::endl;

    return 0;
}
