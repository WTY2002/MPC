#ifndef A2BPROTOCOL_H
#define A2BPROTOCOL_H

#include <cstdint>
#include <vector>

#include "NetworkNode.h"
#include "PCNode.h"

class SingleBitFullAdder {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                       TaskContext& ctx);
};

class A2BOffProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                       TaskContext& ctx);
};

class A2BOffPreProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

class InitializeCarryProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

class BitXorProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

class BitTransformSharingProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

class A2BOffStoreProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

class A2BOnProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                       TaskContext& ctx);
};

class A2BOnPreProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

#endif  // A2BPROTOCOL_H
