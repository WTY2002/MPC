#include "B2AProtocol.h"

#include "AddProtocol.h"
#include "MulProtocol.h"
#include "Type.h"

void B2AOffProtocol::Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                            TaskContext& ctx) {
    uint16_t idx = bytesToUint16(data[1], data[2]);
    uint16_t start_id = bytesToUint16(data[3], data[4]);
    const CipherData& beta_share = node.BetaShares(idx);
    uint8_t node_id = node.ID();
    uint64_t shares[5]{};

    for (uint8_t alpha_idx = 1; alpha_idx <= 5; alpha_idx++) {
        std::memset(shares, 0, sizeof(shares));
        uint16_t share_key = start_id + alpha_idx - 1;
        if (alpha_idx != node_id) {
            uint64_t alpha_value = beta_share.Alpha(alpha_idx);
            shares[alpha_idx - 1] = alpha_value;
            node.SetAdditiveShares(share_key, shares);
        } else {
            node.SetAdditiveShares(share_key, shares);
        }
    }

    auto mul_off_proto = new MulOffProtocol();
    auto mul_on_proto = new MulOnProtocol();
    std::vector<uint8_t> mul_msg{ProtocolType::MUL_OFF};
    mul_msg.insert(mul_msg.end(), 12, 0);
    uint32_t mul_result_idx = start_id + 5;
    writeUint32(mul_msg, 9, mul_result_idx);

    for (uint8_t alpha_idx = 5; alpha_idx >= 2; alpha_idx--) {
        uint16_t current_key = start_id + alpha_idx - 1;
        uint16_t prev_key = start_id + alpha_idx - 2;

        node.AdditiveToBetaUsingKey(current_key);
        node.AdditiveToBetaUsingKey(prev_key);
        writeUint32(mul_msg, 1, current_key);
        writeUint32(mul_msg, 5, prev_key);

        mul_msg[0] = ProtocolType::MUL_OFF;
        mul_off_proto->Handle(mul_msg, node, network_node, ctx);
        ctx.operation_id += 20;

        mul_msg[0] = ProtocolType::MUL_ON;
        mul_on_proto->Handle(mul_msg, node, network_node, ctx);
        ctx.operation_id += 5;

        auto current_share = node.AdditiveShares(current_key);
        auto previous_share = node.AdditiveShares(prev_key);
        auto mul_result_share = Node::BetaToAdditive(node.BetaShares(mul_result_idx), node_id);

        uint64_t result[5]{};
        for (uint8_t id = 0; id < 5; ++id) {
            if (id != node_id - 1) {
                result[id] = current_share[id] + previous_share[id] - 2 * mul_result_share[id];
            }
        }
        node.SetAdditiveShares(prev_key, result);
    }
}

void B2AOnProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    uint16_t idx = bytesToUint16(data[1], data[2]);
    uint16_t start_id = bytesToUint16(data[3], data[4]);
    uint16_t result_id = bytesToUint16(data[5], data[6]);
    const CipherData& beta_share = node.BetaShares(idx);

    uint8_t node_id = node.ID();
    auto beta = beta_share.Beta();
    auto share = node.AdditiveShares(start_id);

    uint64_t result[5]{};
    for (uint8_t id = 0; id < 5; ++id) {
        if (id != node_id - 1) {
            result[id] = share[id] - 2 * beta * share[id];
        }
    }
    if (node_id != 1) {
        result[0] += beta;
    }
    node.SetAdditiveShares(result_id, result);
    node.AdditiveToBetaUsingKey(result_id);
}