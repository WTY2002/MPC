#include "AddProtocol.h"
#include "Util.h"

void AddProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    const uint8_t node_id = node.ID();
    const uint32_t x_id = readUint32(data, 1);
    const uint32_t y_id = readUint32(data, 5);
    const uint32_t z_id = readUint32(data, 9);

    const std::array<uint64_t, 5> x = Node::BetaToAdditive(node.BetaShares(x_id), node_id);
    const std::array<uint64_t, 5> y = Node::BetaToAdditive(node.BetaShares(y_id), node_id);
    CipherData& result_cipher = node.BetaShares(z_id);

    std::array<uint64_t, 5> result = {};
    for (uint8_t id = 0; id < 5; ++id) {
        if (id != node_id - 1) {
            result[id] = x[id] + y[id];
        }
    }
    result_cipher = Node::AdditiveToBeta(result);
}

void TransformSharingProtocol::Handle(const std::vector<uint8_t>& data, Node& node) {
    // transform == 0: Beta form to additive form, otherwise, the opposite
    const uint8_t transform = data[1];
    const uint32_t key = readUint32(data, 2);
    transform != 0U ? node.AdditiveToBetaUsingKey(key) : node.BetaToAdditiveUsingKey(key);
}