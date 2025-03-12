#include <spdlog/spdlog.h>
#include <iostream>
#include <random>

#include "MulProtocol.h"
#include "NetworkNode.h"
#include "PCNode.h"
#include "RecProtocol.h"
#include "SharingProtocol.h"
#include "Timer.h"
#include "TruncationProtocol.h"
#include "Type.h"
#include "Util.h"

void TrunTask(int task_id, int operation_id, NetworkNode &network_node) {
    TaskContext ctx = {task_id, operation_id};
    uint32_t share_count = 500;
    Node node(network_node.ID(), share_count);

    uint8_t r_key = 1;
    uint32_t occupancy_start_id = 10;
    uint32_t input_id = 1;
    uint8_t result_id = 2;

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

    std::vector<uint8_t> trun_off_msg = {ProtocolType::TRUN_OFF, r_key, 0, 0, 0, 0};
    writeUint32(trun_off_msg, 2, occupancy_start_id);
    auto trun_off_proto = new TrunOffProtocol();
    trun_off_proto->Handle(trun_off_msg, node, network_node, ctx);

    share_msg[0] = ProtocolType::SHARE_BETA;
    auto share_on_proto = new SharingBetaProtocol();
    share_on_proto->Handle(share_msg, node, network_node, ctx);
    ctx.operation_id++;

    std::vector<uint8_t> trun_on_msg = {ProtocolType::TRUN_ON, static_cast<uint8_t>(input_id),
                                        r_key, result_id};
    auto trun_on_proto = new TrunOnProtocol();
    for (int i = 0; i < 1000; i++) {
        trun_on_proto->Handle(trun_on_msg, node, network_node, ctx);
    }

    /*
    std::vector<uint8_t> rec_msg = {ProtocolType::REC, 2, 3, 4, 5, 2};
    auto rec_proto = new ReconstructionProtocol();
    rec_proto->Handle(rec_msg, node, network_node, ctx);
    ctx.operation_id++;
    if (node.ID() == 5) {
        SPDLOG_INFO("Task {} Node {} Reconstruct val {}", task_id, node.ID(),
                    node.Values(rec_msg[5]));
    }

    if (node.ID() == 1) {
        uint64_t expected_result = static_cast<int64_t>(random_value) >> kTruncatedBit;
        SPDLOG_INFO("Task {} Original val: {}, Expected truncated val: {}", task_id, random_value,
                    expected_result);
    }
    */
}

int main(int argc, char *argv[]) {
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
        tasks.emplace_back(TrunTask, thread, 1, std::ref(network_node));
    }

    Timer timer;
    timer.start();
    for (auto &t : tasks) {
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