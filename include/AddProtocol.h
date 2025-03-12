#ifndef ADDPROTOCOL_H
#define ADDPROTOCOL_H

#include <cstdint>
#include <vector>

#include "PCNode.h"

class AddProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

class TransformSharingProtocol {
  public:
    static void Handle(const std::vector<uint8_t>& data, Node& node);
};

#endif  // ADDPROTOCOL_H
