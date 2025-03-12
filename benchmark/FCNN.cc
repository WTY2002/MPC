#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

constexpr int FIXED_SHIFT = 13;
constexpr uint64_t FIXED_SCALE = 1ULL << FIXED_SHIFT;

int64_t fixed_mul(int64_t a, int64_t b, int shift = FIXED_SHIFT) {
    __int128 temp = static_cast<__int128>(a) * static_cast<__int128>(b);
    return static_cast<int64_t>(temp >> shift);
}

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
    // fc1: weights [128 x 784], bias [128]
    load_array(fin, model.fc1.weights, 128 * 784);
    load_array(fin, model.fc1.bias, 128);
    // fc2: weights [128 x 128], bias [128]
    load_array(fin, model.fc2.weights, 128 * 128);
    load_array(fin, model.fc2.bias, 128);
    // fc3: weights [10 x 128], bias [10]
    load_array(fin, model.fc3.weights, 10 * 128);
    load_array(fin, model.fc3.bias, 10);
    fin.close();
    return model;
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
