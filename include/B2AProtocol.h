#ifndef B2APROTOCOL_H
#define B2APROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

class B2AOffProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                       TaskContext& ctx);
};

class B2AOnProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

#endif  // B2APROTOCOL_H
