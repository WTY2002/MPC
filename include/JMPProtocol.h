#ifndef JMPPROTOCOL_H
#define JMPPROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

class JMPProtocol final {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);
};

uint64_t JMPFunc(uint64_t va, uint64_t vb, uint64_t vc);

#endif
