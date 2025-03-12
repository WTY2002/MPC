#ifndef DOTPRODUCTPROTOCOL_H
#define DOTPRODUCTPROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

class DotProductOffProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);

    template <class Calculator>
    static void HandleImpl(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                           const TaskContext &ctx);
};

class DotProductOnProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);
};

#endif
