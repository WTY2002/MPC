#ifndef TRUNCATIONPROTOCOL_H
#define TRUNCATIONPROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

constexpr uint8_t kTruncatedBit = 13;

class TrunOffProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       TaskContext &ctx);
};

class TrunOffPrepareProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class LinearCombineProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class TrunOnProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       TaskContext &ctx);
};

class TrunOnPrepareProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class RightShiftProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class TrunOnRecoveryProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

#endif
