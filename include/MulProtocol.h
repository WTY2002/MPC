#ifndef MULPROTOCOL_H
#define MULPROTOCOL_H

#include <cstdint>
#include <map>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

struct ShareCondition {
    std::array<uint8_t, 5> node_idx;  // [sender1, sender2, sender3, receiver, zero_share]
};

// Operations over a boolean ring
struct Mod2Policy {
    static uint64_t add(const uint64_t a, const uint64_t b) {
        return a ^ b;
    }
    static uint64_t sub(const uint64_t a, const uint64_t b) {
        return a ^ b;
    }
    static uint64_t zero() {
        return 0;
    }
};

// Operations over an arithmetic ring (l=64)
struct Mod2_64Policy {
    static uint64_t add(const uint64_t a, const uint64_t b) {
        return a + b;
    }
    static uint64_t sub(const uint64_t a, const uint64_t b) {
        return a - b;
    }
    static uint64_t zero() {
        return 0;
    }
};

// Calculator classes
template <typename Policy>
class ModularCalculator {
  public:
    static uint64_t add(uint64_t a, uint64_t b) {
        return Policy::add(a, b);
    }
    static uint64_t sub(uint64_t a, uint64_t b) {
        return Policy::sub(a, b);
    }
    static uint64_t zero() {
        return Policy::zero();
    }

    template <typename Iterator>
    static uint64_t accumulate(Iterator begin, Iterator end) {
        uint64_t result = zero();
        for (Iterator it = begin; it != end; ++it) {
            result = add(result, *it);
        }
        return result;
    }
};

using DefaultCalculator = ModularCalculator<Mod2_64Policy>;
using Mod2Calculator = ModularCalculator<Mod2Policy>;

class MulOffProtocol {
  public:
    void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                const TaskContext &ctx);

    template <class Calculator>
    void HandleImpl(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                    const TaskContext &ctx);
};

class MulOnProtocol {
  public:
    void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                const TaskContext &ctx);

    template <class Calculator>
    void HandleImpl(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                    const TaskContext &ctx);
};

class MulOffJointSharingPrepareProtocol {
  public:
    static void Handle(Node &node, bool is_bit_mul = false);
    static void PrintConditions();
    static const std::vector<ShareCondition> &GetConditions();
    static std::map<uint8_t, std::vector<std::pair<uint8_t, uint8_t>>> GenerateConditionMap(
        const std::vector<ShareCondition> &conditions);

  private:
    static std::vector<ShareCondition> GenerateAllConditions();
};

class MulJointSharingProtocol {
  public:
    template <class Calculator>
    static void Handle(Node &node, NetworkNode &network_node, const TaskContext &ctx);
};

#endif