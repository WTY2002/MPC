#include "RecProtocol.h"
#include "JMPProtocol.h"
#include "Type.h"

void ReconstructionProtocol::Handle(const std::vector<uint8_t> &data, Node &node,
                                    NetworkNode &network_node, const TaskContext &ctx) {
    CipherData &beta_share = node.BetaShares(data[5]);
    const uint8_t node_id = node.ID();
    JMPProtocol jMPProtocol;

    if (node_id == data[1] || node_id == data[2] || node_id == data[3]) {
        node.SetT(beta_share.Alpha(data[4]));
        jMPProtocol.Handle(data, node, network_node, ctx);
    } else if (node_id == data[4]) {
        jMPProtocol.Handle(data, node, network_node, ctx);
        beta_share.SetAlpha(node.T(), data[4]);
        if (data[0] == ProtocolType::REC) {
            if (beta_share.Beta() < beta_share.AlphaSum()) {
                node.SetTruncationWrap(true);
            }
            node.SetValues(data[5], beta_share.Beta() - beta_share.AlphaSum());
        } else if (data[0] == ProtocolType::BIT_REC) {
            node.SetValues(data[5], beta_share.Beta() ^ beta_share.AlphaXor());
        }
    }
}
