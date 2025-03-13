#include "PCNode.h"

Node::Node(const uint8_t id, const uint32_t share_count) : id_(id) {
    std::vector<uint8_t> key(16, 1);
    SetKey(key);
    InitializeMaps(share_count);
    GenerateAllConditions();
    GenerateConditionMap();
}

void Node::SetKey(const std::vector<uint8_t> &key) {
    if (AES_set_encrypt_key(key.data(), 128, &aes_key_) != 0) {
        throw std::runtime_error("Failed to set AES key");
    }
}

uint64_t Node::PRFEval(const uint64_t input) const {
    std::vector<uint8_t> input_block(AES_BLOCK_SIZE, 0);
    *reinterpret_cast<uint64_t *>(input_block.data()) = input;

    std::vector<uint8_t> output_block(AES_BLOCK_SIZE);
    AES_encrypt(input_block.data(), output_block.data(), &aes_key_);
    return *reinterpret_cast<uint64_t *>(output_block.data());
}

void Node::InitializeMaps(const uint32_t count) {
    uint64_t initial_shares[5] = {0, 0, 0, 0, 0};
    for (uint32_t i = 1; i <= count; ++i) {
        values_map_[i] = 0;
        beta_shares_map_[i] = CipherData{};
        SetAdditiveShares(i, initial_shares);
    }
}

CipherData Node::AdditiveToBeta(const std::array<uint64_t, 5> &shares) {
    CipherData result{};
    for (uint8_t id = 1; id <= 5; ++id) {
        result.SetAlpha(-shares[id - 1], id);
    }
    result.SetBeta(0);
    return result;
}

std::array<uint64_t, 5> Node::BetaToAdditive(const CipherData &beta_data, const uint8_t node_id) {
    std::array<uint64_t, 5> result{};
    if (node_id != 1) {
        result[0] = beta_data.Beta() - beta_data.Alpha(1);
    }
    for (uint8_t id = 2; id <= 5; ++id) {
        result[id - 1] = -beta_data.Alpha(id);
    }
    return result;
}

void Node::AdditiveToBetaUsingKey(const uint32_t key) {
    auto &beta_share = beta_shares_map_[key];
    const auto &additive_share = additive_shares_map_[key];
    for (uint8_t id = 1; id <= 5; ++id) {
        beta_share.SetAlpha(-additive_share[id - 1], id);
    }
    beta_share.SetBeta(0);
}

void Node::BetaToAdditiveUsingKey(const uint32_t key) {
    auto &additive_share = additive_shares_map_[key];
    const auto &beta_share = beta_shares_map_[key];
    if (id_ != 1) {
        additive_share[0] = beta_share.Beta() - beta_share.Alpha(1);
    }
    for (uint8_t id = 2; id <= 5; ++id) {
        additive_share[id - 1] = -beta_share.Alpha(id);
    }
}

void Node::BitAdditiveToBeta(const uint32_t key) {
    auto &beta_share = beta_shares_map_[key];
    const auto &additive_share = additive_shares_map_[key];

    for (uint8_t id = 1; id <= 5; ++id) {
        beta_share.SetAlpha((id != id_) ? additive_share[id - 1] : 0, id);
    }
    beta_share.SetBeta(0);
}

void Node::BitBetaToAdditive(const uint32_t key) {
    auto &additive_share = additive_shares_map_[key];
    const auto &beta_share = beta_shares_map_[key];

    additive_share[0] = (id_ != 1) ? beta_share.Beta() ^ beta_share.Alpha(1) : 0;
    for (uint8_t id = 2; id <= 5; ++id) {
        additive_share[id - 1] = (id != id_) ? beta_share.Alpha(id) : 0;
    }
}

void Node::PrintShares(const uint32_t key) {
    const auto &beta_share = beta_shares_map_[key];
    const auto &additive_share = additive_shares_map_[key];

    SPDLOG_INFO("Key {}: Beta form - Beta: {}, Alpha: {}", key, beta_share.Beta(),
                fmt::join(beta_share.GetFullAlpha(), ", "));

    SPDLOG_INFO("Key {}: Additive form: {}", key, fmt::join(additive_share, ", "));
}

void Node::GenerateAllConditions() {
    conditions_.reserve(20);
    for (uint8_t i = 1; i <= 3; ++i) {
        for (uint8_t j = i + 1; j <= 4; ++j) {
            for (uint8_t k = j + 1; k <= 5; ++k) {
                std::array<uint8_t, 2> remaining{};
                int idx = 0;
                for (uint8_t n = 1; n <= 5; ++n) {
                    if (n != i && n != j && n != k) {
                        remaining[idx++] = n;
                    }
                }
                conditions_.push_back({{i, j, k, remaining[0], remaining[1]}});
                conditions_.push_back({{i, j, k, remaining[1], remaining[0]}});
            }
        }
    }
}

void Node::GenerateConditionMap() {
    for (size_t index = 0; index < conditions_.size(); ++index) {
        const auto &cond = conditions_[index];
        condition_map_[cond.node_idx[3]].emplace_back(index, cond.node_idx[4]);
    }
}
