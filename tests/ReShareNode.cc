#include <spdlog/spdlog.h>
#include <iostream>

#include "AddProtocol.h"
#include "NetworkNode.h"
#include "PCNode.h"
#include "ReSharingProtocol.h"
#include "RecProtocol.h"
#include "SharingProtocol.h"
#include "Timer.h"
#include "Type.h"
#include "Util.h"

void ReShareTask(int task_id, int operation_id, NetworkNode &network_node) {
    TaskContext ctx = {task_id, operation_id};
    uint32_t share_count = 1;
    Node node(network_node.ID(), share_count);

    if (node.ID() == 1) {
        node.SetValues(1, 123456789);
    }

    std::vector<uint8_t> share_msg = {ProtocolType::SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    writeUint32(share_msg, 6, 1);
    auto share_off_proto = new SharingBetaOfflineProtocol();
    share_off_proto->Handle(share_msg, node);

    std::vector<uint8_t> res_msg = {ProtocolType::RES_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    writeUint32(res_msg, 6, 1);
    auto res_off_proto = new ReSharingOfflineProtocol();
    res_off_proto->Handle(res_msg, node);

    share_msg[0] = ProtocolType::SHARE_BETA;
    auto share_on_proto = new SharingBetaProtocol();
    share_on_proto->Handle(share_msg, node, network_node, ctx);
    ctx.operation_id++;

    // Not necessary, determine conversion based on the existing sharding format.
    // Currently, Beta shares are used and need to be converted to Additive shares.
    std::vector<uint8_t> transform_msg = {ProtocolType::TRANSFORM, 0, 0, 0, 0, 0};
    writeUint32(transform_msg, 2, 1);
    auto trandform_proto = new TransformSharingProtocol();
    trandform_proto->Handle(transform_msg, node);

    res_msg[0] = ProtocolType::RES;
    auto res_on_proto = new ReSharingProtocol();
    for (int i = 0; i < 1000; i++) {
        res_on_proto->Handle(res_msg, node, network_node, ctx);
        ctx.operation_id += 5;
    }

    /*
    // Verify Code
    std::vector<uint8_t> rec_msg = {ProtocolType::REC, 2, 3, 4, 5, 1};
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
        tasks.emplace_back(ReShareTask, thread, 1, std::ref(network_node));
    }

    Timer timer;
    timer.start();
    for (auto &t : tasks) {
        t.join();
    }
    timer.stop();
    std::cout << "Total time: " << timer.elapsedMicroseconds() << " us" << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(60));
    network_node.Stop();

    receiver.join();
    sender.join();
    std::cout << "[Node " << network_node.ID() << "] Stopped.\n";
    return 0;
}