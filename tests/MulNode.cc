#include <spdlog/spdlog.h>
#include <iostream>

#include "MulProtocol.h"
#include "NetworkNode.h"
#include "PCNode.h"
#include "RecProtocol.h"
#include "SharingProtocol.h"
#include "Timer.h"
#include "Type.h"
#include "Util.h"

void MulTask(int task_id, int operation_id, NetworkNode &network_node) {
    TaskContext ctx = {task_id, operation_id};
    uint32_t share_count = 3;
    Node node(network_node.ID(), share_count);

    if (node.ID() == 1) {
        node.SetValues(1, 12345);
        node.SetValues(2, 67890);
    }

    auto share_off_proto = new SharingBetaOfflineProtocol();
    std::vector<uint8_t> share_msg = {ProtocolType::SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    for (int i = 1; i <= share_count; i++) {
        writeUint32(share_msg, 6, i);
        share_off_proto->Handle(share_msg, node);
    }

    share_msg[0] = ProtocolType::SHARE_BETA;
    auto share_on_proto = new SharingBetaProtocol();
    for (int i = 1; i <= 2; i++) {
        writeUint32(share_msg, 6, i);
        share_on_proto->Handle(share_msg, node, network_node, ctx);
        ctx.operation_id++;
    }

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
    for (int i = 0; i < 1000; i++) {
        mul_on_proto->Handle(mul_msg, node, network_node, ctx);
        ctx.operation_id += 5;
    }

    /*
    std::vector<uint8_t> rec_msg = {ProtocolType::REC, 2, 3, 4, 5, 3};
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

    std::vector<std::thread> tasks;
    for (int thread = 0; thread < 1000; thread++) {
        tasks.emplace_back(MulTask, thread, 1, std::ref(network_node));
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