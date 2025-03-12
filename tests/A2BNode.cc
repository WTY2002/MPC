#include <spdlog/spdlog.h>
#include <bitset>
#include <iostream>
#include <random>

#include "A2BProtocol.h"
#include "NetworkNode.h"
#include "PCNode.h"
#include "RecProtocol.h"
#include "SharingProtocol.h"
#include "Timer.h"
#include "Type.h"
#include "Util.h"

void A2BTask(int task_id, int operation_id, NetworkNode& network_node) {
    TaskContext ctx = {task_id, operation_id};
    uint32_t share_count = 360;
    Node node(network_node.ID(), share_count);

    uint8_t bit_key = 1;
    uint8_t result_start_id = 10;
    uint32_t input_id = 1;

    /*
    static std::mt19937_64 gen(std::random_device{}());
    std::uniform_int_distribution<> bit_dis(40, 60);
    uint64_t mask = (1ULL << bit_dis(gen)) - 1;
    const uint64_t random_value = gen() & mask;
    */
    const uint64_t random_value = 50893722547205813;

    if (node.ID() == 1) {
        node.SetValues(1, random_value);
    }

    auto share_off_proto = new SharingBetaOfflineProtocol();
    std::vector<uint8_t> share_msg = {ProtocolType::SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    writeUint32(share_msg, 6, input_id);
    share_off_proto->Handle(share_msg, node);

    std::vector<uint8_t> a2b_msg = {ProtocolType::A2B_OFF, 0, 0, result_start_id, bit_key};
    a2b_msg[1] = static_cast<uint8_t>(static_cast<uint16_t>(input_id) >> 8);
    a2b_msg[2] = static_cast<uint8_t>(static_cast<uint16_t>(input_id) & 0xFF);
    auto a2b_off_proto = new A2BOffProtocol();
    a2b_off_proto->Handle(a2b_msg, node, network_node, ctx);

    share_msg[0] = ProtocolType::SHARE_BETA;
    auto share_on_proto = new SharingBetaProtocol();
    share_on_proto->Handle(share_msg, node, network_node, ctx);
    ctx.operation_id++;

    a2b_msg[0] = ProtocolType::A2B_ON;
    auto a2b_on_proto = new A2BOnProtocol();
    for (int i = 0; i < 10; i++) {
        a2b_on_proto->Handle(a2b_msg, node, network_node, ctx);
    }

    /*
    std::vector<uint8_t> transform_msg = {ProtocolType::BIT_TRANSFORM, 0, 0, 1};
    auto bit_transform_proto = new BitTransformSharingProtocol();
    for (int value = result_start_id; value < result_start_id + 64; ++value) {
        transform_msg[1] = static_cast<uint8_t>(value >> 8);
        transform_msg[2] = static_cast<uint8_t>(value & 0xFF);
        bit_transform_proto->Handle(transform_msg, node);
    }

    std::vector<uint64_t> res{};
    auto rec_proto = new ReconstructionProtocol();
    std::vector<uint8_t> rec_msg = {ProtocolType::BIT_REC, 2, 3, 4, 5, 0};
    for (uint8_t bit_pos = result_start_id; bit_pos < result_start_id + 64; bit_pos++) {
        rec_msg[5] = bit_pos;
        rec_proto->Handle(rec_msg, node, network_node, ctx);
        ctx.operation_id++;
        if (node.ID() == 5) {
            res.push_back(node.Values(rec_msg[5]));
        }
    }

    if (node.ID() == 5) {
        uint64_t decimal = 0;
        std::bitset<64> bitset_result;
        for (size_t bit_pos = 0; bit_pos < res.size(); ++bit_pos) {
            bitset_result[bit_pos] = res[bit_pos];
        }
        decimal = bitset_result.to_ullong();
        SPDLOG_INFO("Task {} Node {} Reconstructed value after A2B protocol conversion: {}",
                    task_id, node.ID(), decimal);
    }
    if (node.ID() == 1) {
        SPDLOG_INFO("Task {} Original val: {}", task_id, random_value);
    }
    */
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./node <node_id>\n";
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

    std::vector<std::thread> tasks;
    for (int thread = 0; thread < 1000; thread++) {
        tasks.emplace_back(A2BTask, thread, 1, std::ref(network_node));
    }

    Timer timer;
    timer.start();
    for (auto& t : tasks) {
        t.join();
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
