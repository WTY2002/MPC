#include <AddProtocol.h>
#include <ctpl_stl.h>
#include <spdlog/spdlog.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <future>
#include <iostream>
#include <memory>

#include "A2BProtocol.h"
#include "B2AProtocol.h"
#include "DotProductProtocol.h"
#include "MulProtocol.h"
#include "NetworkNode.h"
#include "PCNode.h"
#include "RecProtocol.h"
#include "SharedMemory.h"
#include "SharingProtocol.h"
#include "Timer.h"
#include "TruncationProtocol.h"
#include "Type.h"
#include "Util.h"

std::pair<uint32_t, CipherData> SinglePointInference(
    int task_id, int operation_id, NetworkNode& network_node, int output,
    const std::unordered_map<uint32_t, CipherData>& model_beta_shares_map,
    const std::unordered_map<uint32_t, CipherData>& input_data, const FcnnLayerConfig& config) {
    TaskContext ctx = {task_id, operation_id};
    Node node(network_node.ID(), 999);
    // node.SkipOfflinePhase();

    uint32_t work_space = 100;
    uint32_t input_size = config.input_size;
    uint32_t output_size = config.output_size;
    uint32_t input_start_idx = config.input_start_idx;
    uint32_t output_start_idx = config.output_start_idx;
    uint32_t weight_start_idx = config.weight_start_idx;

    for (const auto& entry : model_beta_shares_map) {
        node.SetBetaShares(entry.first, entry.second);
    }
    for (const auto& entry : input_data) {
        node.SetBetaShares(entry.first, entry.second);
    }

    // dot product
    std::vector<uint8_t> dot_msg{ProtocolType::DOT_PRODUCT_OFF};
    dot_msg.insert(dot_msg.end(), 16, 0);
    writeUint32(dot_msg, 1, input_size);
    writeUint32(dot_msg, 5, input_start_idx);
    writeUint32(dot_msg, 9, weight_start_idx + input_size * output);
    writeUint32(dot_msg, 13, 1);

    auto dot_off_proto = new DotProductOffProtocol();
    dot_off_proto->Handle(dot_msg, node, network_node, ctx);
    ctx.operation_id += 20;

    dot_msg[0] = ProtocolType::DOT_PRODUCT_ON;
    auto dot_on_proto = new DotProductOnProtocol();
    dot_on_proto->Handle(dot_msg, node, network_node, ctx);
    ctx.operation_id += 5;

    // truncation
    std::vector<uint8_t> trun_off_msg = {ProtocolType::TRUN_OFF, 1, 0, 0, 0, 0};
    writeUint32(trun_off_msg, 2, work_space);
    auto trun_off_proto = new TrunOffProtocol();
    trun_off_proto->Handle(trun_off_msg, node, network_node, ctx);

    std::vector<uint8_t> trun_on_msg = {ProtocolType::TRUN_ON, 1, 1, 2};
    auto trun_on_proto = new TrunOnProtocol();
    trun_on_proto->Handle(trun_on_msg, node, network_node, ctx);

    // add
    std::vector<uint8_t> add_msg{ProtocolType::ADD};
    add_msg.insert(add_msg.end(), 12, 0);
    writeUint32(add_msg, 1, 2);
    writeUint32(add_msg, 5, weight_start_idx + input_size * output_size + output);
    writeUint32(add_msg, 9, 1);
    auto add_proto = new AddProtocol();
    add_proto->Handle(add_msg, node);

    // ReLu(x), to be updated
    // a2b
    std::vector<uint8_t> a2b_msg = {ProtocolType::A2B_OFF, 0, 0, work_space, 1};
    a2b_msg[1] = static_cast<uint8_t>(1 >> 8);
    a2b_msg[2] = static_cast<uint8_t>(1 & 0xFF);

    auto a2b_off_proto = new A2BOffProtocol();
    a2b_off_proto->Handle(a2b_msg, node, network_node, ctx);

    a2b_msg[0] = ProtocolType::A2B_ON;
    auto a2b_on_proto = new A2BOnProtocol();
    a2b_on_proto->Handle(a2b_msg, node, network_node, ctx);

    // bit transform
    uint32_t sign_idx = 163;
    std::vector<uint8_t> transform_msg = {ProtocolType::BIT_TRANSFORM, 0, 0, 1};
    auto bit_transform_proto = new BitTransformSharingProtocol();
    transform_msg[1] = static_cast<uint8_t>(sign_idx >> 8);
    transform_msg[2] = static_cast<uint8_t>(sign_idx & 0xFF);
    bit_transform_proto->Handle(transform_msg, node);

    // b2a
    std::vector<uint8_t> b2a_msg = {ProtocolType::B2A_OFF};
    b2a_msg.insert(b2a_msg.end(), 6, 0);
    b2a_msg[1] = static_cast<uint8_t>(sign_idx >> 8);
    b2a_msg[2] = static_cast<uint8_t>(sign_idx & 0xFF);
    b2a_msg[3] = static_cast<uint8_t>(work_space >> 8);
    b2a_msg[4] = static_cast<uint8_t>(work_space & 0xFF);
    b2a_msg[5] = static_cast<uint8_t>(2 >> 8);
    b2a_msg[6] = static_cast<uint8_t>(2 & 0xFF);

    auto b2a_off_proto = new B2AOffProtocol();
    b2a_off_proto->Handle(b2a_msg, node, network_node, ctx);

    b2a_msg[0] = ProtocolType::B2A_ON;
    auto b2a_on_proto = new B2AOnProtocol();
    b2a_on_proto->Handle(b2a_msg, node);

    // multiplication
    std::vector<uint8_t> mul_msg{ProtocolType::MUL_OFF};
    mul_msg.insert(mul_msg.end(), 12, 0);
    writeUint32(mul_msg, 1, 1);
    writeUint32(mul_msg, 5, 2);
    writeUint32(mul_msg, 9, 3);

    auto mul_off_proto = new MulOffProtocol();
    mul_off_proto->Handle(mul_msg, node, network_node, ctx);
    ctx.operation_id += 20;

    mul_msg[0] = ProtocolType::MUL_ON;
    auto mul_on_proto = new MulOnProtocol();
    mul_on_proto->Handle(mul_msg, node, network_node, ctx);
    ctx.operation_id += 5;

    return {output_start_idx + output, node.BetaShares(3)};
}

uint64_t FcnnInferenceTask(
    int task_id, int operation_id, NetworkNode& network_node,
    const std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>&
        model_beta_shares_map,
    const std::vector<uint64_t>& input_data, ctpl::thread_pool& pool) {
    TaskContext ctx = {task_id, operation_id};
    Node node(network_node.ID(), 0);

    uint32_t input_space = 1000;
    if (node.ID() == 1) {
        for (auto& input : input_data) {
            node.SetValues(input_space++, input);
        }
    }

    auto share_off_proto = new SharingBetaOfflineProtocol();
    auto share_on_proto = new SharingBetaProtocol();
    std::vector<uint8_t> share_msg = {ProtocolType::SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};

    for (uint32_t index = 1000; index < 1784; index++) {
        writeUint32(share_msg, 6, index);
        share_msg[0] = ProtocolType::SHARE_BETA_OFF;
        share_off_proto->Handle(share_msg, node);
        share_msg[0] = ProtocolType::SHARE_BETA;
        share_on_proto->Handle(share_msg, node, network_node, ctx);
        ctx.operation_id++;
    }

    for (int layer = 1; layer <= 3; layer++) {
        const FcnnLayerConfig& config = FcnnLayerConfigs[layer - 1];
        uint32_t output_size = config.output_size;
        uint32_t output_start_idx = config.output_start_idx;
        auto input_beta_shares_map = node.GetBetaSharesMapCopy();
        node.ResetBetaShares();

        auto current_layer_weight_iter = model_beta_shares_map.find(layer);
        if (current_layer_weight_iter == model_beta_shares_map.end()) {
            throw std::runtime_error("Layer weight not found in model_beta_shares_map");
        }
        const auto& current_layer_weight = current_layer_weight_iter->second;

        std::vector<std::future<std::pair<uint32_t, CipherData>>> tasks;
        for (int output_idx = 0; output_idx < output_size; output_idx++) {
            tasks.push_back(pool.push(
                [&](int /*thread_id*/, int inference_id, int output_pos) {
                    return SinglePointInference(inference_id, 1, network_node, output_pos,
                                                current_layer_weight, input_beta_shares_map,
                                                config);
                },
                task_id + output_idx + 1, output_idx));
        }

        for (auto& task : tasks) {
            auto result = task.get();
            if (layer != 3) {
                node.SetBetaShares(result.first, result.second);
            } else {
                node.SetBetaShares(result.first - output_start_idx, result.second);
            }
        }
    }

    std::vector<uint64_t> res{};
    auto rec_proto = new ReconstructionProtocol();
    std::vector<uint8_t> rec_msg = {ProtocolType::REC, 2, 3, 4, 5, 0};
    for (uint8_t output = 0; output < 10; output++) {
        rec_msg[5] = output;
        rec_proto->Handle(rec_msg, node, network_node, ctx);
        ctx.operation_id++;
        if (node.ID() == 5) {
            res.push_back(node.Values(rec_msg[5]));
        }
    }

    return std::distance(res.begin(), std::max_element(res.begin(), res.end()));
}

std::shared_ptr<std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>> InitModel(
    int task_id, int operation_id, NetworkNode& network_node) {
    TaskContext ctx = {task_id, operation_id};
    FCNNWeights model = load_model_weights("./benchmark/model_weights.bin");
    Node node(network_node.ID(), 0);

    auto result_map =
        std::make_shared<std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>>>();
    auto share_off_proto = new SharingBetaOfflineProtocol();
    auto share_on_proto = new SharingBetaProtocol();
    std::vector<uint8_t> share_msg = {ProtocolType::SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    uint32_t layer1_weight_start = FcnnLayerConfigs[0].weight_start_idx;
    uint32_t layer2_weight_start = FcnnLayerConfigs[1].weight_start_idx;
    uint32_t layer3_weight_start = FcnnLayerConfigs[2].weight_start_idx;

    // Layer 1
    uint32_t space_start = layer1_weight_start;
    if (node.ID() == 1) {
        for (auto& weight : model.fc1.weights) {
            node.SetValues(space_start++, weight);
        }
        for (auto& bias : model.fc1.bias) {
            node.SetValues(space_start++, bias);
        }
    }

    for (uint32_t index = layer1_weight_start; index < layer2_weight_start; index++) {
        writeUint32(share_msg, 6, index);
        share_msg[0] = ProtocolType::SHARE_BETA_OFF;
        share_off_proto->Handle(share_msg, node);
        share_msg[0] = ProtocolType::SHARE_BETA;
        share_on_proto->Handle(share_msg, node, network_node, ctx);
        ctx.operation_id++;
    }

    (*result_map)[1] = node.GetBetaSharesMapCopy();
    node.ResetBetaShares();

    // Layer 2
    space_start = layer2_weight_start;
    if (node.ID() == 1) {
        for (auto& weight : model.fc2.weights) {
            node.SetValues(space_start++, weight);
        }
        for (auto& bias : model.fc2.bias) {
            node.SetValues(space_start++, bias);
        }
    }

    for (uint32_t index = layer2_weight_start; index < layer3_weight_start; index++) {
        writeUint32(share_msg, 6, index);
        share_msg[0] = ProtocolType::SHARE_BETA_OFF;
        share_off_proto->Handle(share_msg, node);
        share_msg[0] = ProtocolType::SHARE_BETA;
        share_on_proto->Handle(share_msg, node, network_node, ctx);
        ctx.operation_id++;
    }

    (*result_map)[2] = node.GetBetaSharesMapCopy();
    node.ResetBetaShares();

    // Layer 3
    space_start = layer3_weight_start;
    if (node.ID() == 1) {
        for (auto& weight : model.fc3.weights) {
            node.SetValues(space_start++, weight);
        }
        for (auto& bias : model.fc3.bias) {
            node.SetValues(space_start++, bias);
        }
    }

    for (uint32_t index = layer3_weight_start; index < layer3_weight_start + 1290; index++) {
        writeUint32(share_msg, 6, index);
        share_msg[0] = ProtocolType::SHARE_BETA_OFF;
        share_off_proto->Handle(share_msg, node);
        share_msg[0] = ProtocolType::SHARE_BETA;
        share_on_proto->Handle(share_msg, node, network_node, ctx);
        ctx.operation_id++;
    }

    (*result_map)[3] = node.GetBetaSharesMapCopy();
    node.ResetBetaShares();

    return result_map;
}

void RunChildProcess(int node_id, int process_id, int shm_id_model, int shm_id_test) {
    ctpl::thread_pool pool(std::thread::hardware_concurrency());

    void* model_shm_ptr = shmat(shm_id_model, nullptr, 0);
    void* test_shm_ptr = shmat(shm_id_test, nullptr, 0);
    if (model_shm_ptr == reinterpret_cast<void*>(-1) ||
        test_shm_ptr == reinterpret_cast<void*>(-1)) {
        std::cerr << "Shared memory attach failed in child process " << process_id << "\n";
        exit(1);
    }

    std::unordered_map<uint8_t, std::unordered_map<uint32_t, CipherData>> model_beta_shares_map;
    std::vector<std::vector<uint64_t>> test_images;

    SharedMemoryHelper::deserializeMap(model_shm_ptr, model_beta_shares_map);
    SharedMemoryHelper::deserializeVector(test_shm_ptr, test_images);

    const int base_port = 5556 + 5 * process_id;
    std::unordered_map<int, std::string> node_addresses;
    for (int i = 0; i < 5; i++) {
        node_addresses[i + 1] = "tcp://127.0.0.1:" + std::to_string(base_port + i);
    }

    int io_threads = 1;
    NetworkNode network_node(node_id, io_threads, node_addresses);

    std::thread receiver(&NetworkNode::ReceiveMessages, &network_node);
    std::thread sender(&NetworkNode::SendMessages, &network_node);

    uint64_t result = FcnnInferenceTask(process_id, 1, network_node, model_beta_shares_map,
                                        test_images[process_id], pool);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    network_node.Stop();
    receiver.join();
    sender.join();

    shmdt(model_shm_ptr);
    shmdt(test_shm_ptr);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./FcnnNode <node_id> <num_processes>\n";
        return 1;
    }

    int node_id = std::stoi(argv[1]);
    if (node_id < 1 || node_id > 5) {
        std::cerr << "Invalid node_id. Choose between 1-5.\n";
        return 1;
    }

    int num_processes = std::stoi(argv[2]);
    if (num_processes < 1) {
        std::cerr << "Invalid num_processes. Must be > 0.\n";
        return 1;
    }

    int io_threads = 1;
    NetworkNode network_node(node_id, io_threads);

    std::thread receiver(&NetworkNode::ReceiveMessages, &network_node);
    std::thread sender(&NetworkNode::SendMessages, &network_node);

    auto model_ptr = InitModel(0, 1, network_node);
    const auto& model_beta_shares_map = *model_ptr;

    std::vector<std::vector<uint64_t>> test_images =
        load_test_pixels("./benchmark/test_pixels.bin");

    size_t model_size = SharedMemoryHelper::calculateMapSize(model_beta_shares_map);
    size_t test_size = SharedMemoryHelper::calculateVectorSize(test_images);

    int shm_id_model = shmget(IPC_PRIVATE, model_size, IPC_CREAT | 0666);
    int shm_id_test_data = shmget(IPC_PRIVATE, test_size, IPC_CREAT | 0666);

    if (shm_id_model == -1 || shm_id_test_data == -1) {
        std::cerr << "Shared memory allocation failed.\n";
        return 1;
    }

    void* model_shm_ptr = shmat(shm_id_model, nullptr, 0);
    void* test_data_shm_ptr = shmat(shm_id_test_data, nullptr, 0);

    if (model_shm_ptr == reinterpret_cast<void*>(-1) ||
        test_data_shm_ptr == reinterpret_cast<void*>(-1)) {
        std::cerr << "Shared memory attach failed.\n";
        return 1;
    }

    SharedMemoryHelper::serializeMap(model_shm_ptr, model_beta_shares_map);
    SharedMemoryHelper::serializeVector(test_data_shm_ptr, test_images);

    shmdt(model_shm_ptr);
    shmdt(test_data_shm_ptr);

    std::vector<pid_t> child_pids;
    for (int i = 0; i < num_processes; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            RunChildProcess(node_id, i, shm_id_model, shm_id_test_data);
        } else if (pid > 0) {
            child_pids.push_back(pid);
        } else {
            std::cerr << "Fork failed!\n";
            return 1;
        }
    }

    Timer timer;
    timer.start();
    for (pid_t pid : child_pids) {
        waitpid(pid, nullptr, 0);
    }
    timer.stop();
    std::cout << "Total time: " << timer.elapsedMicroseconds() << " us" << std::endl;

    shmctl(shm_id_model, IPC_RMID, nullptr);
    shmctl(shm_id_test_data, IPC_RMID, nullptr);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    network_node.Stop();

    receiver.join();
    sender.join();
    std::cout << "[Node " << network_node.ID() << "] Stopped.\n";

    return 0;
}

/*
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./FcnnNode <node_id> \n";
        return 1;
    }

    int node_id = std::stoi(argv[1]);
    if (node_id < 1 || node_id > 5) {
        std::cerr << "Invalid node_id. Choose between 1-5.\n";
        return 1;
    }

    int io_threads = 12;
    NetworkNode network_node(node_id, io_threads);

    std::thread receiver(&NetworkNode::ReceiveMessages, &network_node);
    std::thread sender(&NetworkNode::SendMessages, &network_node);

    auto model_ptr = InitModel(0, 1, network_node);
    const auto& model_beta_shares_map = *model_ptr;

    std::vector<std::vector<uint64_t>> test_images =
        load_test_pixels("./benchmark/test_pixels.bin");

    int interval = 270;
    std::vector<std::future<uint64_t>> tasks;
    size_t total = test_images.size();
    for (int thread = 0; thread < 1; thread++) {
        tasks.push_back(std::async(std::launch::async, FcnnInferenceTask, thread * interval, 1,
                                   std::ref(network_node), model_beta_shares_map,
                                   std::ref(test_images[thread])));
    }

    Timer timer;
    timer.start();
    for (auto& t : tasks) {
        auto predict = t.get();
    }
    timer.stop();
    std::cout << "Total time: " << timer.elapsedMicroseconds() << " us" << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));
    network_node.Stop();

    receiver.join();
    sender.join();
    std::cout << "[Node " << network_node.ID() << "] Stopped.\n";

    return 0;
}
*/