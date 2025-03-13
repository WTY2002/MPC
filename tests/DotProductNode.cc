#include <spdlog/spdlog.h>
#include <future>
#include <iostream>

#include "DotProductProtocol.h"
#include "NetworkNode.h"
#include "PCNode.h"
#include "RecProtocol.h"
#include "SharingProtocol.h"
#include "Timer.h"
#include "Type.h"
#include "Util.h"

void DotProductTask(int task_id, int operation_id, NetworkNode &network_node) {
    TaskContext ctx = {task_id, operation_id};

    // Set up test case
    const uint32_t dimension = 5;
    const uint32_t x_start_idx = 1;
    const uint32_t y_start_idx = x_start_idx + dimension;
    const uint32_t z_idx = y_start_idx + dimension;

    const std::vector<uint64_t> x_values = {3122, 463, 734, 2553, 1065};
    const std::vector<uint64_t> y_values = {524, 1605, 4357, 578, 394};

    uint32_t share_count = z_idx;
    Node node(network_node.ID(), share_count);

    if (node.ID() == 1) {
        for (uint32_t i = 0; i < dimension; i++) {
            node.SetValues(x_start_idx + i, x_values[i]);
            node.SetValues(y_start_idx + i, y_values[i]);
        }
    }

    auto share_off_proto = new SharingBetaOfflineProtocol();
    std::vector<uint8_t> share_msg = {ProtocolType::SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    for (int i = x_start_idx; i <= z_idx; i++) {
        writeUint32(share_msg, 6, i);
        share_off_proto->Handle(share_msg, node);
    }

    share_msg[0] = ProtocolType::SHARE_BETA;
    auto share_on_proto = new SharingBetaProtocol();
    for (int i = x_start_idx; i < z_idx; i++) {
        writeUint32(share_msg, 6, i);
        share_on_proto->Handle(share_msg, node, network_node, ctx);
        ctx.operation_id++;
    }

    std::vector<uint8_t> dot_msg{ProtocolType::DOT_PRODUCT_OFF};
    dot_msg.insert(dot_msg.end(), 16, 0);
    writeUint32(dot_msg, 1, dimension);
    writeUint32(dot_msg, 5, x_start_idx);
    writeUint32(dot_msg, 9, y_start_idx);
    writeUint32(dot_msg, 13, z_idx);

    auto dot_off_proto = new DotProductOffProtocol();
    dot_off_proto->Handle(dot_msg, node, network_node, ctx);
    ctx.operation_id += 20;

    dot_msg[0] = ProtocolType::DOT_PRODUCT_ON;
    auto dot_on_proto = new DotProductOnProtocol();
    for (int i = 0; i < 1000; i++) {
        dot_on_proto->Handle(dot_msg, node, network_node, ctx);
        ctx.operation_id += 5;
    }

    /*
    std::vector<uint8_t> rec_msg = {ProtocolType::REC, 2, 3, 4, 5, 11};
    auto rec_proto = new ReconstructionProtocol();
    rec_proto->Handle(rec_msg, node, network_node, ctx);
    ctx.operation_id++;
    SPDLOG_INFO("Node {} Reconstruct val {}", node.ID(), node.Values(rec_msg[5]));
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

    std::vector<std::future<void>> tasks;
    for (int thread = 0; thread < 1000; thread++) {
        tasks.push_back(
            std::async(std::launch::async, DotProductTask, thread, 1, std::ref(network_node)));
    }

    Timer timer;
    timer.start();
    for (auto &t : tasks) {
        t.wait();
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