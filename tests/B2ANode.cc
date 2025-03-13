#include <spdlog/spdlog.h>
#include <future>
#include <iostream>

#include "B2AProtocol.h"
#include "NetworkNode.h"
#include "PCNode.h"
#include "RecProtocol.h"
#include "SharingProtocol.h"
#include "Timer.h"
#include "Type.h"
#include "Util.h"

void B2ATask(int task_id, int operation_id, NetworkNode &network_node) {
    TaskContext ctx = {task_id, operation_id};
    uint32_t share_count = 15;
    Node node(network_node.ID(), share_count);

    uint16_t input_id = 1;
    uint16_t start_id = 10;
    uint16_t result_id = 2;

    if (node.ID() == 1) {
        node.SetValues(1, 1);
    }

    std::vector<uint8_t> share_msg = {ProtocolType::BIT_SHARE_BETA_OFF, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    writeUint32(share_msg, 6, input_id);

    auto share_off_proto = new SharingBetaOfflineProtocol();
    share_off_proto->Handle(share_msg, node);

    std::vector<uint8_t> b2a_msg = {ProtocolType::B2A_OFF};
    b2a_msg.insert(b2a_msg.end(), 6, 0);
    std::vector<uint16_t> keys = {input_id, start_id, result_id};
    size_t idx = 1;
    for (auto key : keys) {
        auto [high, low] = uint16ToBytes(key);
        b2a_msg[idx++] = high;
        b2a_msg[idx++] = low;
    }

    auto b2a_off_proto = new B2AOffProtocol();
    b2a_off_proto->Handle(b2a_msg, node, network_node, ctx);

    share_msg[0] = ProtocolType::BIT_SHARE_BETA;
    auto share_on_proto = new SharingBetaProtocol();

    share_on_proto->Handle(share_msg, node, network_node, ctx);
    ctx.operation_id++;

    b2a_msg[0] = ProtocolType::B2A_ON;
    for (int i = 0; i < 1000; i++) {
        auto b2a_on_proto = new B2AOnProtocol();
        b2a_on_proto->Handle(b2a_msg, node);
    }

    /*
    std::vector<uint8_t> rec_msg = {ProtocolType::REC, 2, 3, 4, 5, 2};
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
        tasks.push_back(std::async(std::launch::async, B2ATask, thread, 1, std::ref(network_node)));
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