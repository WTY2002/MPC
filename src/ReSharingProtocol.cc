#include "ReSharingProtocol.h"
#include "Util.h"

void ReSharingOfflineProtocol::Handle(const std::vector<uint8_t> &data, Node &node) {
    const uint8_t node_id = node.ID();
    if (node_id != data[5]) {
        uint64_t sum = 0;
        for (uint8_t i = 1; i <= 4; ++i) {
            const uint64_t reshare = node.PRFEval(i);
            node.SetReshare(reshare, i - 1);
            sum += reshare;
        }
        node.SetReshare(std::numeric_limits<uint64_t>::max() - sum + 1, 4);
        // SPDLOG_INFO("Node {} Reshare {}", node_id, fmt::join(node.GetFullReshare(), ", "));
    }
}

void ReSharingProtocol::Handle(const std::vector<uint8_t> &data, Node &node,
                               NetworkNode &network_node, const TaskContext &ctx) {
    const uint8_t node_id = node.ID();
    const uint32_t key = readUint32(data, 6);
    auto share = node.AdditiveShares(key);

    if (node_id == data[1] || node_id == data[2] || node_id == data[3] || node_id == data[4]) {
        share[0] = node.Reshare(0) + share[0];
        share[1] = node.Reshare(1) + share[1];
        share[2] = node.Reshare(2) + share[2];
        share[3] = node.Reshare(3) + share[3];
        share[4] = node.Reshare(4) + share[4];

        for (uint8_t id = 1; id <= 5; ++id) {
            if (id != node_id && id != data[5]) {
                network_node.AddMessage(data[5], ctx.task_id, ctx.operation_id + id - 1,
                                        share[id - 1]);
            }
        }
    } else if (node_id == data[5]) {
        // SPDLOG_INFO("Node {} Original Share {}", node_id, fmt::join(share, share + 5, ", "));
        for (uint8_t id = 1; id <= 5; ++id) {
            if (id != node_id && id != data[5]) {
                uint64_t val = network_node.Receive(ctx.task_id, ctx.operation_id + id - 1, 3);
                share[id - 1] = val;
            }
        }
        // SPDLOG_INFO("Node {} New Share {}", node_id, fmt::join(share, share + 5, ", "));
    }
    node.AdditiveToBetaUsingKey(key);
}