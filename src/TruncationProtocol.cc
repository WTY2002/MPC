#include <bitset>

#include "TruncationProtocol.h"

void TrunOffPrepareProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint8_t node_id = node.ID();
    const uint16_t start_id = readUint32(data, 6);
    std::array<uint64_t, 5> r1_shares = {}, r2_shares = {}, r3_shares = {};
    const std::map<uint8_t, std::vector<uint8_t>> share_mapping = {{data[1], {2, 3}},
                                                                   {data[2], {1, 3}},
                                                                   {data[3], {1, 2}},
                                                                   {data[4], {1, 2, 3}},
                                                                   {data[5], {1, 2, 3}}};
    const auto it = share_mapping.find(node_id);
    if (it != share_mapping.end()) {
        std::array<uint64_t, 3> shares{0, 0, 0};
        for (uint8_t i = 1; i <= 3; ++i) {
            if (i != node_id) {
                shares[i - 1] = node.PRFEval(start_id * 3 + i);
            }
        }

        for (const unsigned char j : it->second) {
            switch (j) {
                case 1:
                    r1_shares[0] = shares[0];
                    break;
                case 2:
                    r2_shares[1] = shares[1];
                    break;
                case 3:
                    r3_shares[2] = shares[2];
                    break;
                default:;
            }
        }
    }

    std::array<std::array<std::bitset<64>, 5>, 3> r_binary;
    for (uint8_t i = 0; i < 3; ++i) {
        for (uint8_t j = 0; j < 5; ++j) {
            switch (i) {
                case 0:
                    r_binary[i][j] = std::bitset<64>(r1_shares[j]);
                    break;
                case 1:
                    r_binary[i][j] = std::bitset<64>(r2_shares[j]);
                    break;
                case 2:
                    r_binary[i][j] = std::bitset<64>(r3_shares[j]);
                    break;
                default:;
            }
        }
    }

    for (uint16_t bit_pos = 0; bit_pos < 64; ++bit_pos) {
        for (uint8_t r_idx = 0; r_idx < 3; ++r_idx) {
            std::array<uint64_t, 5> bit_shares{};
            for (uint8_t share_idx = 0; share_idx < 5; ++share_idx) {
                bit_shares[share_idx] = r_binary[r_idx][share_idx][bit_pos] ? 1ULL : 0ULL;
            }
            CipherData cipher_data = Node::AdditiveToBeta(bit_shares);
            node.SetBetaShares(start_id + bit_pos * 3 + r_idx, cipher_data);
        }
    }
}

void LinearCombineProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint8_t node_id = node.ID();
    const uint32_t start_id = readUint32(data, 1);
    const uint8_t truncated_bit = data[5];
    const uint8_t key = data[6];
    std::vector<std::array<uint64_t, 5>> r_vec;

    for (uint16_t bit_pos = 0; bit_pos < 64; ++bit_pos) {
        std::array<uint64_t, 5> cipher_r1 =
            Node::BetaToAdditive(node.BetaShares(start_id + bit_pos * 3), node_id);
        std::array<uint64_t, 5> cipher_r2 =
            Node::BetaToAdditive(node.BetaShares(start_id + bit_pos * 3 + 1), node_id);
        std::array<uint64_t, 5> cipher_r3 =
            Node::BetaToAdditive(node.BetaShares(start_id + bit_pos * 3 + 2), node_id);
        std::array<uint64_t, 5> cipher_r12 =
            Node::BetaToAdditive(node.BetaShares(start_id + 192 + bit_pos * 3), node_id);
        std::array<uint64_t, 5> cipher_r23 =
            Node::BetaToAdditive(node.BetaShares(start_id + 192 + bit_pos * 3 + 1), node_id);
        std::array<uint64_t, 5> cipher_r13 =
            Node::BetaToAdditive(node.BetaShares(start_id + 192 + bit_pos * 3 + 2), node_id);
        std::array<uint64_t, 5> cipher_r123 =
            Node::BetaToAdditive(node.BetaShares(start_id + 384 + bit_pos), node_id);

        // r[t] = ∑(ri[t]) - ∑2(ri[t]*rj[t]) + 4(r1[t]*r2[t]*r3[t])
        std::array<uint64_t, 5> r_bit = {};
        for (uint8_t id = 0; id < 5; ++id) {
            if (id != node_id - 1) {
                r_bit[id] = cipher_r1[id] + cipher_r2[id] + cipher_r3[id] -
                            2 * (cipher_r12[id] + cipher_r23[id] + cipher_r13[id]) +
                            4 * cipher_r123[id];
            }
        }
        r_vec.push_back(r_bit);
    }

    std::array<uint64_t, 5> r_full = {};
    for (size_t t = 0; t < 64; ++t) {
        for (uint8_t id = 0; id < 5; ++id) {
            if (id != node_id - 1) {
                r_full[id] += r_vec[t][id] * (1ULL << t);
            }
        }
    }

    std::array<uint64_t, 5> r_truncated = {};
    for (size_t t = truncated_bit; t < 64; ++t) {
        for (uint8_t id = 0; id < 5; ++id) {
            if (id != node_id - 1) {
                r_truncated[id] += r_vec[t][id] * (1ULL << (t - truncated_bit));
            }
        }
    }

    node.SetTruncationParams(r_full, r_truncated, key);
}

void TrunOnPrepareProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint8_t node_id = node.ID();
    const std::array<uint64_t, 5> input_share =
        Node::BetaToAdditive(node.BetaShares(data[1]), node_id);
    const std::array<uint64_t, 5> r_full =
        Node::BetaToAdditive(node.GetFullTruncationParams(data[2]), node_id);
    CipherData& result_cipher = node.BetaShares(data[3]);

    std::array<uint64_t, 5> result = {};
    for (uint8_t id = 0; id < 5; ++id) {
        if (id != node_id - 1) {
            if (input_share[id] < r_full[id]) {
                node.SetTruncationWrap(true);
            }
            result[id] = input_share[id] - r_full[id];
        }
    }
    result_cipher = Node::AdditiveToBeta(result);
}

void RightShiftProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint8_t node_id = node.ID();
    if (node_id == data[1] || node_id == data[2] || node_id == data[3]) {
        const uint64_t input = node.Values(data[4]);
        uint64_t result = 0;
        if (node.TruncationWrap()) {
            result = (input >> kTruncatedBit) | (UINT64_MAX << (64 - kTruncatedBit));
            node.SetTruncationWrap(false);
        } else {
            result = input >> kTruncatedBit;
        }
        node.SetVal(result);
    }
}

void TrunOnRecoveryProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint8_t node_id = node.ID();
    const std::array<uint64_t, 5> r_truncated =
        Node::BetaToAdditive(node.GetTruncatedTruncationParams(data[1]), node_id);
    const std::array<uint64_t, 5> intermediate = Node::BetaToAdditive(node.Cipher(), node_id);
    CipherData& result_cipher = node.BetaShares(data[2]);

    std::array<uint64_t, 5> result = {};
    for (uint8_t id = 0; id < 5; ++id) {
        if (id != node_id - 1) {
            result[id] = intermediate[id] + r_truncated[id];
        }
    }
    result_cipher = Node::AdditiveToBeta(result);
}
