#ifndef RESHARINGPROTOCOL_H
#define RESHARINGPROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

class ReSharingOfflineProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class ReSharingProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);
};

#endif