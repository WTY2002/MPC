#ifndef PCNODE_H
#define PCNODE_H

#include <cstdint>
#include <iostream>
#include <string>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <spdlog/spdlog.h>

#include "CipherData.h"
#include "Matrix.h"
#include "Util.h"
#include "spdlog/fmt/ranges.h"

class Node {
  public:
    Node(uint8_t id, uint32_t share_count);

    void SetKey(const std::vector<uint8_t> &key);

    uint64_t PRFEval(uint64_t input) const;

    void InitializeMaps(uint32_t count);

    static CipherData AdditiveToBeta(const std::array<uint64_t, 5> &shares);

    static std::array<uint64_t, 5> BetaToAdditive(const CipherData &beta_data, uint8_t node_id);

    void AdditiveToBetaUsingKey(uint32_t key);

    void BetaToAdditiveUsingKey(uint32_t key);

    void BitAdditiveToBeta(uint32_t key);

    void BitBetaToAdditive(uint32_t key);

    void PrintShares(uint32_t key);

    uint64_t Alpha(const std::size_t index) const {
        return cipher_.Alpha(index);
    }

    const uint64_t (&GetFullAlpha() const)[5] {
        return cipher_.alpha_;
    }

    uint64_t AlphaSum() const {
        return cipher_.AlphaSum();
    }

    uint64_t Beta() const {
        return cipher_.Beta();
    }

    const CipherData &Cipher() const {
        return cipher_;
    }

    uint8_t ID() const {
        return id_;
    }

    Matrix &MatrixRef() {
        return matrix_;
    }

    uint64_t Share(const std::size_t index) const {
        return share_[index];
    }

    const uint64_t (&GetFullShare() const)[5] {
        return share_;
    }

    uint64_t ShareSum() const {
        return share_[0] + share_[1] + share_[2] + share_[3] + share_[4];
    }

    uint64_t Reshare(const std::size_t index) const {
        return reshare_[index];
    }

    const uint64_t (&GetFullReshare() const)[5] {
        return reshare_;
    }

    uint64_t T() const {
        return t_;
    }

    uint64_t Val() const {
        return val_;
    }

    uint64_t &Values(const uint32_t id, bool create_if_missing = false) {
        if (create_if_missing) {
            return values_map_[id];
        }
        const auto it = values_map_.find(id);
        if (it != values_map_.end()) {
            return it->second;
        }
        throw std::runtime_error("Value not found for ID: " + std::to_string(id));
    }

    CipherData &BetaShares(const uint32_t id, bool create_if_missing = false) {
        if (create_if_missing) {
            return beta_shares_map_[id];
        }
        const auto it = beta_shares_map_.find(id);
        if (it != beta_shares_map_.end()) {
            return it->second;
        }
        throw std::runtime_error("Beta share not found for ID: " + std::to_string(id));
    }

    uint64_t (&AdditiveShares(const uint32_t id, bool create_if_missing = false))[5] {
        if (create_if_missing) {
            return additive_shares_map_[id];
        }
        const auto it = additive_shares_map_.find(id);
        if (it != additive_shares_map_.end()) {
            return it->second;
        }
        throw std::runtime_error("Additive share not found for ID: " + std::to_string(id));
    }

    const CipherData &GetFullTruncationParams(const uint8_t key) const {
        const auto it = truncation_params_.find(key);
        if (it == truncation_params_.end()) {
            throw std::runtime_error("Truncation parameter not found for key: " +
                                     std::to_string(key));
        }
        return it->second.first;
    }

    const CipherData &GetTruncatedTruncationParams(const uint8_t key) const {
        const auto it = truncation_params_.find(key);
        if (it == truncation_params_.end()) {
            throw std::runtime_error("Truncation parameter not found for key: " +
                                     std::to_string(key));
        }
        return it->second.second;
    }

    uint64_t MulShares(const uint8_t index, const uint8_t condition_id) const {
        return mul_shares_[condition_id][index - 1];
    }

    uint64_t AlphaXY(const std::size_t index) const {
        return alpha_xy_.Alpha(index);
    }

    std::unordered_map<uint8_t, uint64_t[64][5]> &A2BShares() {
        return a2b_shares_map_;
    }

    bool TruncationWrap() const {
        return truncation_wrap_;
    }

    void SetAlpha(const uint64_t val, const std::size_t index) {
        cipher_.SetAlpha(val, index);
    }

    void SetBeta(const uint64_t val) {
        cipher_.SetBeta(val);
    }

    void SetID(const uint8_t id) {
        id_ = id;
    }

    void SetShare(const uint64_t val, const std::size_t index) {
        share_[index] = val;
    }

    void SetReshare(const uint64_t val, const std::size_t index) {
        reshare_[index] = val;
    }

    void SetT(const uint64_t val) {
        t_ = val;
    }

    void SetVal(const uint64_t val) {
        val_ = val;
    }

    void SetValues(const uint32_t id, const uint64_t value) {
        values_map_[id] = value;
    }

    void SetBetaShares(const uint32_t id, const CipherData &beta_share) {
        beta_shares_map_[id] = beta_share;
    }

    void SetAdditiveShares(const uint32_t id, const uint64_t (&shares)[5]) {
        std::copy(std::begin(shares), std::end(shares), additive_shares_map_[id]);
    }

    void SetMulShares(const uint64_t share, const uint8_t index, const uint8_t condition_id) {
        mul_shares_[condition_id][index - 1] = share;
    }

    void SetAlphaXY(const uint64_t (&xy_share)[5]) {
        for (uint8_t id = 1; id <= 5; ++id) {
            alpha_xy_.SetAlpha(xy_share[id - 1], id);
        }
    }

    void PrintMulShares() {
        for (size_t i = 0; i < mul_shares_.size(); ++i) {
            SPDLOG_INFO("Node {} mul_shares_[{}]: [{}]", id_, i, fmt::join(mul_shares_[i], ", "));
        }
    }

    void SetTruncationParams(const std::array<uint64_t, 5> &r_full,
                             const std::array<uint64_t, 5> &r_truncated, const uint8_t key) {
        CipherData beta_r_full = AdditiveToBeta(r_full);
        CipherData beta_r_truncated = AdditiveToBeta(r_truncated);

        truncation_params_[key] = {beta_r_full, beta_r_truncated};
    }

    void SetTruncationWrap(const bool truncation_wrap) {
        truncation_wrap_ = truncation_wrap;
    }

  private:
    uint8_t id_;
    AES_KEY aes_key_{};

    uint64_t t_{};           // JMP protocol buffer
    uint64_t reshare_[5]{};  // ReSharing protocol buffer
    uint64_t val_{};
    uint64_t share_[5]{};          // Sharing protocol buffer
    struct CipherData cipher_ {};  // [[alpha]] and beta
    Matrix matrix_{5};             // Mul protocol buffer

    std::unordered_map<uint32_t, uint64_t> values_map_;
    std::unordered_map<uint32_t, CipherData> beta_shares_map_;
    std::unordered_map<uint32_t, uint64_t[5]> additive_shares_map_;

    std::unordered_map<uint8_t, std::pair<CipherData, CipherData>> truncation_params_;
    std::unordered_map<uint8_t, uint64_t[64][5]> a2b_shares_map_;

    std::vector<std::array<uint64_t, 5>> mul_shares_ = std::vector(20, std::array<uint64_t, 5>{});
    struct CipherData alpha_xy_ {};  // alpha_x_y

    bool truncation_wrap_ = false;
};

#endif
