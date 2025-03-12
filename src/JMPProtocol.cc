#include "JMPProtocol.h"

uint64_t JMPFunc(const uint64_t va, const uint64_t vb, const uint64_t vc) {
    return (va == vb || va == vc) ? va : vb;
}

void JMPProtocol::Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                         const TaskContext &ctx) {
    const uint8_t node_id = node.ID();

    if (node_id == data[1] || node_id == data[2] || node_id == data[3]) {
        network_node.AddMessage(data[4], ctx.task_id, ctx.operation_id, node.T());
    } else if (node_id == data[4]) {
        uint64_t t = network_node.Receive(ctx.task_id, ctx.operation_id, 3);
        node.SetT(t);
    }
}
