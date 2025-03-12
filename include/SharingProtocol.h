#ifndef SHARINGPROTOCOL_H
#define SHARINGPROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

class SharingBetaOfflineProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class SharingBetaProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);
};

class JointSharingBetaOfflineProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class JointSharingBetaProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);
};

class JointSharingOfflineProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node);
};

class JointSharingProtocol {
  public:
    static void Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                       const TaskContext &ctx);
};

#endif
