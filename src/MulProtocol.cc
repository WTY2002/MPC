#include <numeric>

#include "MulProtocol.h"
#include "Type.h"
#include "Util.h"

void MulOffProtocol::Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                            const TaskContext& ctx) {
    switch (data[0]) {
        case ProtocolType::MUL_OFF:
            HandleImpl<DefaultCalculator>(data, node, network_node, ctx);
            break;
        case ProtocolType::BIT_MUL_OFF:
            HandleImpl<Mod2Calculator>(data, node, network_node, ctx);
            break;
        default:
            HandleImpl<DefaultCalculator>(data, node, network_node, ctx);
            break;
    }
}

template <typename Calculator>
void MulOffProtocol::HandleImpl(const std::vector<uint8_t>& data, Node& node,
                                NetworkNode& network_node, const TaskContext& ctx) {
    const uint8_t node_id = node.ID();
    Matrix& matrix = node.MatrixRef();
    const uint32_t x_id = readUint32(data, 1);
    const uint32_t y_id = readUint32(data, 5);
    const CipherData cipher_x = node.BetaShares(x_id);
    const CipherData cipher_y = node.BetaShares(y_id);

    for (uint32_t row = 1; row <= 5; ++row) {
        if (row == node_id) {
            continue;
        }
        for (uint32_t col = 1; col <= 5; ++col) {
            if (col != node_id && col != row) {
                matrix.Set(row, col, cipher_x.Alpha(row) * cipher_y.Alpha(col));
            }
        }
    }

    if (node_id == 1 || node_id == 2) {
        const uint64_t alpha45 = matrix.Get(4, 5);
        const uint64_t alpha45_n =
            Calculator::add(alpha45, Calculator::add(cipher_x.Alpha(4) * cipher_y.Alpha(4),
                                                     cipher_x.Alpha(5) * cipher_y.Alpha(5)));
        matrix.Set(4, 5, alpha45_n);
        const uint64_t alpha35 = matrix.Get(3, 5);
        const uint64_t alpha35_n = Calculator::add(alpha35, cipher_x.Alpha(3) * cipher_y.Alpha(3));
        matrix.Set(3, 5, alpha35_n);
    } else if (node_id == 3) {
        const uint64_t alpha45 = matrix.Get(4, 5);
        const uint64_t alpha45_n =
            Calculator::add(alpha45, Calculator::add(cipher_x.Alpha(4) * cipher_y.Alpha(4),
                                                     cipher_x.Alpha(5) * cipher_y.Alpha(5)));
        matrix.Set(4, 5, alpha45_n);
        const uint64_t alpha12 = matrix.Get(1, 2);
        const uint64_t alpha12_n =
            Calculator::add(alpha12, Calculator::add(cipher_x.Alpha(1) * cipher_y.Alpha(1),
                                                     cipher_x.Alpha(2) * cipher_y.Alpha(2)));
        matrix.Set(1, 2, alpha12_n);
    } else if (node_id == 4) {
        const uint64_t alpha12 = matrix.Get(1, 2);
        const uint64_t alpha12_n =
            Calculator::add(alpha12, Calculator::add(cipher_x.Alpha(1) * cipher_y.Alpha(1),
                                                     cipher_x.Alpha(2) * cipher_y.Alpha(2)));
        matrix.Set(1, 2, alpha12_n);
        const uint64_t alpha35 = matrix.Get(3, 5);
        const uint64_t alpha35_n = Calculator::add(alpha35, cipher_x.Alpha(3) * cipher_y.Alpha(3));
        matrix.Set(3, 5, alpha35_n);
    } else if (node_id == 5) {
        const uint64_t alpha12 = matrix.Get(1, 2);
        const uint64_t alpha12_n =
            Calculator::add(alpha12, Calculator::add(cipher_x.Alpha(1) * cipher_y.Alpha(1),
                                                     cipher_x.Alpha(2) * cipher_y.Alpha(2)));
        matrix.Set(1, 2, alpha12_n);
    }

    bool is_bit_mul = std::is_same_v<Calculator, Mod2Calculator>;
    MulOffJointSharingPrepareProtocol::Handle(node, is_bit_mul);
    MulJointSharingProtocol::Handle<Calculator>(node, network_node, ctx);
}

void MulOffJointSharingPrepareProtocol::Handle(Node& node, bool is_bit_mul) {
    const auto& conditions = node.GetConditions();
    for (std::size_t condition_id = 0; condition_id < conditions.size(); ++condition_id) {
        const auto& condition = conditions[condition_id];

        for (uint8_t id = 1; id <= 5; ++id) {
            if (id == condition.node_idx[4]) {
                node.SetMulShares(0, id, condition_id);
                continue;
            }

            if (node.ID() != condition.node_idx[0] && node.ID() != condition.node_idx[1] &&
                node.ID() != condition.node_idx[2] && id == node.ID()) {
                node.SetMulShares(0, id, condition_id);
                continue;
            }

            uint64_t alpha_xy_share = node.PRFEval(id + condition_id * 5);
            if (is_bit_mul) {
                alpha_xy_share &= 1;
            }
            node.SetMulShares(alpha_xy_share, id, condition_id);
        }
    }
}

template <typename Calculator>
void MulJointSharingProtocol::Handle(Node& node, NetworkNode& network_node,
                                     const TaskContext& ctx) {
    const uint8_t node_id = node.ID();
    const auto& conditions = node.GetConditions();
    const Matrix& matrix = node.MatrixRef();

    for (uint8_t i = 0; i < static_cast<uint8_t>(conditions.size()); ++i) {
        const auto& condition = conditions[i];
        if (node_id == condition.node_idx[0] || node_id == condition.node_idx[1] ||
            node_id == condition.node_idx[2]) {
            uint64_t share_sum = Calculator::zero();
            for (uint8_t id = 1; id <= 5; ++id) {
                share_sum = Calculator::add(share_sum, node.MulShares(id, i));
            }

            const uint64_t val = Calculator::sub(
                matrix.Get(condition.node_idx[3], condition.node_idx[4]), share_sum);
            node.SetMulShares(val, condition.node_idx[4], i);

            network_node.AddMessage(condition.node_idx[3], ctx.task_id, ctx.operation_id + i, val);
        }
    }

    const auto& condition_map = node.GetConditionMap();
    auto& receive_vector = condition_map.at(node_id);
    for (const auto& [index, share_id] : receive_vector) {
        uint64_t val = network_node.Receive(ctx.task_id, ctx.operation_id + index, 3);
        node.SetMulShares(val, share_id, index);
    }

    uint64_t xy_share[5] = {};
    for (uint8_t i = 0; i < static_cast<uint8_t>(conditions.size()); ++i) {
        for (uint8_t id = 1; id <= 5; ++id) {
            if (id != node_id) {
                xy_share[id - 1] = Calculator::add(xy_share[id - 1], node.MulShares(id, i));
            }
        }
    }
    node.SetAlphaXY(xy_share);
}

void MulOnProtocol::Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                           const TaskContext& ctx) {
    switch (data[0]) {
        case ProtocolType::MUL_ON:
            HandleImpl<DefaultCalculator>(data, node, network_node, ctx);
            break;
        case ProtocolType::BIT_MUL_ON:
            HandleImpl<Mod2Calculator>(data, node, network_node, ctx);
            break;
        default:
            HandleImpl<DefaultCalculator>(data, node, network_node, ctx);
            break;
    }
}

template <typename Calculator>
void MulOnProtocol::HandleImpl(const std::vector<uint8_t>& data, Node& node,
                               NetworkNode& network_node, const TaskContext& ctx) {
    const uint8_t node_id = node.ID();
    const uint32_t x_id = readUint32(data, 1);
    const uint32_t y_id = readUint32(data, 5);
    const uint32_t z_id = readUint32(data, 9);

    const CipherData cipher_x = node.BetaShares(x_id);
    const CipherData cipher_y = node.BetaShares(y_id);
    CipherData& cipher_z = node.BetaShares(z_id);
    const uint64_t beta_x = cipher_x.Beta();
    const uint64_t beta_y = cipher_y.Beta();

    uint64_t beta_z[5] = {};
    for (uint8_t id = 1; id <= 5; ++id) {
        if (id != node_id) {
            beta_z[id - 1] = Calculator::add(
                Calculator::add(Calculator::sub(Calculator::zero(), beta_x * cipher_y.Alpha(id)),
                                Calculator::sub(Calculator::zero(), beta_y * cipher_x.Alpha(id))),
                Calculator::add(node.AlphaXY(id), cipher_z.Alpha(id)));
        }
    }

    if (node_id <= 3) {
        network_node.AddMessage(4, ctx.task_id, ctx.operation_id + 3, beta_z[3]);
        network_node.AddMessage(5, ctx.task_id, ctx.operation_id + 4, beta_z[4]);
    }

    if (node_id == 1 || node_id == 4 || node_id == 5) {
        network_node.AddMessage(2, ctx.task_id, ctx.operation_id + 1, beta_z[1]);
        network_node.AddMessage(3, ctx.task_id, ctx.operation_id + 2, beta_z[2]);
    }

    if (node_id == 3 || node_id == 4 || node_id == 5) {
        network_node.AddMessage(1, ctx.task_id, ctx.operation_id, beta_z[0]);
    }

    uint64_t receive_beta = network_node.Receive(ctx.task_id, ctx.operation_id + node_id - 1, 3);
    beta_z[node_id - 1] = receive_beta;
    const uint64_t sum = Calculator::accumulate(std::begin(beta_z), std::end(beta_z));
    const uint64_t val = Calculator::add(sum, beta_x * beta_y);
    cipher_z.SetBeta(val);
    if (data[0] == ProtocolType::BIT_MUL_ON) {
        node.BitBetaToAdditive(z_id);
    }
    // SPDLOG_INFO("Node {} final beta_z: {}", node_id, fmt::join(beta_z, ", "));
}
