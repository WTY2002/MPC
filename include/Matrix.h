#ifndef SYMMETRIC_MATRIX_H
#define SYMMETRIC_MATRIX_H

#include <cstdint>
#include <iostream>
#include <vector>

class Matrix {
  public:
    explicit Matrix(const uint32_t size)
        : size_(size), elements_(size, std::vector<uint64_t>(size, 0)) {}

    void Set(const uint32_t row, const uint32_t col, const uint64_t val) {
        elements_[row - 1][col - 1] = val;
    }

    uint64_t Get(const uint32_t row, const uint32_t col) const {
        return elements_[row - 1][col - 1];
    }

    void Print() const {
        for (uint32_t row = 1; row <= size_; ++row) {
            for (uint32_t col = 1; col <= size_; ++col) {
                std::cout << Get(row, col) << " ";
            }
            std::cout << std::endl;
        }
    }

  private:
    uint32_t size_;
    std::vector<std::vector<uint64_t>> elements_;
};

#endif