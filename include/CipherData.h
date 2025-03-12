#ifndef CIPHERDATA_H
#define CIPHERDATA_H

#include <cstdint>

struct CipherData {
    uint64_t alpha_[5];
    uint64_t beta_;

    uint64_t Alpha(const std::size_t index) const {
        return alpha_[index - 1];
    }

    uint64_t Beta() const {
        return beta_;
    }

    void SetAlpha(const uint64_t val, const std::size_t index) {
        alpha_[index - 1] = val;
    }

    void SetBeta(const uint64_t val) {
        beta_ = val;
    }

    const uint64_t (&GetFullAlpha() const)[5] {
        return alpha_;
    }

    uint64_t AlphaSum() const {
        return alpha_[0] + alpha_[1] + alpha_[2] + alpha_[3] + alpha_[4];
    }

    uint64_t AlphaXor() const {
        return alpha_[0] ^ alpha_[1] ^ alpha_[2] ^ alpha_[3] ^ alpha_[4];
    }
};

#endif
