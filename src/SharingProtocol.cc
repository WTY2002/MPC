#include <random>

#include "SharingProtocol.h"
#include "Type.h"
#include "Util.h"

void SharingBetaOfflineProtocol::Handle(const std::vector<uint8_t> &data, Node &node) {
    const uint32_t idx = readUint32(data, 6);
    CipherData &beta_share = node.BetaShares(idx, true);
    const uint8_t node_id = node.ID();
    for (uint8_t id = 1; id <= 5; ++id) {
        if (node_id != data[1] && id == node_id) {
            beta_share.SetAlpha(0, id);
            continue;
        }
        uint64_t alpha_share = node.PRFEval(id + 5 * idx);
        if (data[0] == ProtocolType::BIT_SHARE_BETA_OFF) {
            alpha_share &= 1;
        }
        beta_share.SetAlpha(alpha_share, id);
    }
    // SPDLOG_INFO("Node {} Data {} Alpha {}", node_id, idx,
    // fmt::join(beta_share.GetFullAlpha(), ", "));
}

void SharingBetaProtocol::Handle(const std::vector<uint8_t> &data, Node &node,
                                 NetworkNode &network_node, const TaskContext &ctx) {
    uint32_t idx = readUint32(data, 6);
    CipherData &beta_share = node.BetaShares(idx);
    uint8_t node_id = node.ID();

    if (node_id == data[1]) {
        uint64_t beta = 0;
        uint64_t value = node.Values(idx);
        if (data[0] == ProtocolType::SHARE_BETA) {
            beta = beta_share.AlphaSum() + value;
        } else if (data[0] == ProtocolType::BIT_SHARE_BETA) {
            beta = beta_share.AlphaXor() ^ value;
        }
        beta_share.SetBeta(beta);

        for (int index = 2; index <= 5; ++index) {
            network_node.AddMessage(data[index], ctx.task_id, ctx.operation_id, beta_share.Beta());
        }
    } else if (node_id == data[2] || node_id == data[3] || node_id == data[4] ||
               node_id == data[5]) {
        uint64_t beta = network_node.Receive(ctx.task_id, ctx.operation_id, 1);
        beta_share.SetBeta(beta);
        // SPDLOG_INFO("Node {} Data {} Beta value: {}", node_id, idx, beta_share.Beta());
    }
    if (data[0] == ProtocolType::BIT_SHARE_BETA) {
        node.BitBetaToAdditive(idx);
    }
}

void JointSharingBetaOfflineProtocol::Handle(const std::vector<uint8_t> &data, Node &node) {
    const uint8_t node_id = node.ID();
    for (uint8_t id = 1; id <= 5; ++id) {
        if (node_id != data[1] && node_id != data[2] && node_id != data[3] && id == node_id) {
            node.SetAlpha(0, id);
            continue;
        }
        const uint64_t alpha_share = node.PRFEval(id);
        node.SetAlpha(alpha_share, id);
    }
}

void JointSharingBetaProtocol::Handle(const std::vector<uint8_t> &data, Node &node,
                                      NetworkNode &network_node, const TaskContext &ctx) {
    const uint8_t node_id = node.ID();

    if (node_id == data[1] || node_id == data[2] || node_id == data[3]) {
        const uint64_t val = node.AlphaSum() + node.Val();
        node.SetBeta(val);

        for (std::size_t index = 4; index <= 5; ++index) {
            network_node.AddMessage(data[index], ctx.task_id, ctx.operation_id, node.Beta());
        }
    } else if (node_id == data[4] || node_id == data[5]) {
        uint64_t beta = network_node.Receive(ctx.task_id, ctx.operation_id, 3);
        node.SetBeta(beta);
        // SPDLOG_INFO("Node {} Beta value: {}", node_id, node.Beta());
    }
}

void JointSharingOfflineProtocol::Handle(const std::vector<uint8_t> &data, Node &node) {
    const uint8_t node_id = node.ID();
    for (uint8_t id = 1; id <= 5; ++id) {
        if (id == data[5]) {
            node.SetShare(0, id - 1);
            continue;
        }
        if (node_id != data[1] && node_id != data[2] && node_id != data[3] && id == node_id) {
            node.SetShare(0, id - 1);
            continue;
        }
        const uint64_t x_share = node.PRFEval(id);
        node.SetShare(x_share, id - 1);
    }
    // SPDLOG_INFO("Node {} Share {}", node_id, fmt::join(node.GetFullShare(), ", "));
}

void JointSharingProtocol::Handle(const std::vector<uint8_t> &data, Node &node,
                                  NetworkNode &network_node, const TaskContext &ctx) {
    const uint8_t node_id = node.ID();

    if (node_id == data[1] || node_id == data[2] || node_id == data[3]) {
        const uint64_t val = node.Val() - node.ShareSum();
        node.SetShare(val, data[5] - 1);

        network_node.AddMessage(data[4], ctx.task_id, ctx.operation_id, node.Share(data[5] - 1));
    } else if (node_id == data[4]) {
        uint64_t share = network_node.Receive(ctx.task_id, ctx.operation_id, 3);
        node.SetShare(share, data[5] - 1);
    }
}
