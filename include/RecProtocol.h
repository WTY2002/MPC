#ifndef RECPROTOCOL_H
#define RECPROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

class ReconstructionProtocol final {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);
};

#endif