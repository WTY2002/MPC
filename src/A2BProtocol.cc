#include <bitset>

#include "A2BProtocol.h"
#include "PCNode.h"

void A2BOffPreProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint16_t idx = bytesToUint16(data[1], data[2]);
    const uint16_t start_id = data[3];
    const CipherData& beta_share = node.BetaShares(idx);
    const uint8_t node_id = node.ID();
    uint64_t shares[5]{};

    for (uint8_t alpha_idx = 1; alpha_idx <= 5; alpha_idx++) {
        std::memset(shares, 0, sizeof(shares));
        if (alpha_idx != node_id) {
            const uint64_t alpha_value = -beta_share.Alpha(alpha_idx);
            shares[alpha_idx - 1] = alpha_value;

            std::bitset<64> bits(alpha_value);
            uint16_t share_key = start_id + alpha_idx - 1;
            node.SetAdditiveShares(share_key, shares);

            for (uint8_t bit_pos = 0; bit_pos < 64; bit_pos++) {
                uint64_t bit_shares[5]{};
                bit_shares[alpha_idx - 1] = static_cast<uint64_t>(bits[bit_pos]);

                uint16_t bit_key = start_id + 5 + (alpha_idx - 1) * 64 + bit_pos;
                node.SetAdditiveShares(bit_key, bit_shares);
            }
        } else {
            uint16_t share_key = start_id + alpha_idx - 1;
            node.SetAdditiveShares(share_key, shares);

            for (uint8_t bit_pos = 0; bit_pos < 64; bit_pos++) {
                uint16_t bit_key = start_id + 5 + (alpha_idx - 1) * 64 + bit_pos;
                node.SetAdditiveShares(bit_key, shares);
            }
        }
    }
}

void InitializeCarryProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint16_t idx = bytesToUint16(data[1], data[2]);
    auto& [alpha_, beta_] = node.BetaShares(idx);
    std::memset(alpha_, 0, sizeof(alpha_));
    beta_ = 0ULL;
    auto& additive_shares = node.AdditiveShares(idx);
    std::memset(additive_shares, 0, sizeof(additive_shares));
}

void BitXorProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint16_t a_key = bytesToUint16(data[1], data[2]);
    const uint16_t b_key = bytesToUint16(data[3], data[4]);
    const uint16_t result_key = bytesToUint16(data[5], data[6]);

    auto a_shares = node.AdditiveShares(a_key);
    auto b_shares = node.AdditiveShares(b_key);

    uint64_t xor_result[5]{};
    for (int i = 0; i < 5; i++) {
        xor_result[i] = a_shares[i] ^ b_shares[i];
    }
    node.SetAdditiveShares(result_key, xor_result);
}

void BitTransformSharingProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint16_t key = bytesToUint16(data[1], data[2]);
    const uint8_t transform = data[3];
    transform != 0U ? node.BitAdditiveToBeta(key) : node.BitBetaToAdditive(key);
}

void A2BOffStoreProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint8_t key = data[1];
    const uint16_t start_id = data[2];
    uint64_t(*alpha_vec)[5] = node.A2BShares()[key];

    for (uint8_t bit_pos = 0; bit_pos < 64; bit_pos++) {
        const uint16_t bit_key = start_id + 5 + 4 * 64 + bit_pos;
        const auto* const shares = node.AdditiveShares(bit_key);
        std::memcpy(alpha_vec[bit_pos], shares, sizeof(uint64_t) * 5);
    }
}

void A2BOnPreProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint16_t idx = bytesToUint16(data[1], data[2]);
    const uint8_t bit_key = data[3];
    const uint16_t start_id = data[4];

    const CipherData& beta_share = node.BetaShares(idx);
    const uint64_t beta = beta_share.Beta();
    const auto& alpha_vec = node.A2BShares()[bit_key];

    for (uint8_t bit_pos = 0; bit_pos < 64; bit_pos++) {
        uint64_t bit_shares[5]{};
        bit_shares[0] = (node.ID() != 1) ? (beta >> bit_pos) & 1ULL : 0;
        const uint16_t beta_key = start_id + bit_pos;
        node.SetAdditiveShares(beta_key, bit_shares);

        const uint16_t alpha_key = beta_key + 64;
        node.SetAdditiveShares(alpha_key, alpha_vec[bit_pos]);
    }
}
