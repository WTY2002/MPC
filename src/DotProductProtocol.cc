#include <numeric>

#include "DotProductProtocol.h"
#include "MulProtocol.h"
#include "Util.h"

void DotProductOffProtocol::Handle(const std::vector<uint8_t>& data, Node& node,
                                   NetworkNode& network_node, const TaskContext& ctx) {
    HandleImpl<DefaultCalculator>(data, node, network_node, ctx);
}

template <typename Calculator>
void DotProductOffProtocol::HandleImpl(const std::vector<uint8_t>& data, Node& node,
                                       NetworkNode& network_node, const TaskContext& ctx) {
    const uint8_t node_id = node.ID();
    Matrix& matrix = node.MatrixRef();
    std::vector<CipherData> cipher_x_vec;
    std::vector<CipherData> cipher_y_vec;

    const uint32_t dimension = readUint32(data, 1);
    const uint32_t x_start_idx = readUint32(data, 5);
    const uint32_t y_start_idx = readUint32(data, 9);

    cipher_x_vec.reserve(dimension);
    for (uint32_t t = 0; t < dimension; t++) {
        cipher_x_vec.push_back(node.BetaShares(x_start_idx + t));
    }
    cipher_y_vec.reserve(dimension);
    for (uint32_t t = 0; t < dimension; t++) {
        cipher_y_vec.push_back(node.BetaShares(y_start_idx + t));
    }

    // For each dimension, compute partial products
    for (uint32_t t = 0; t < dimension; t++) {
        const CipherData& cipher_x = cipher_x_vec[t];
        const CipherData& cipher_y = cipher_y_vec[t];

        for (uint32_t row = 1; row <= 5; ++row) {
            if (row == node_id) {
                continue;
            }
            for (uint32_t col = 1; col <= 5; ++col) {
                if (col != node_id && col != row) {
                    // Accumulate products for each dimension
                    const uint64_t current = matrix.Get(row, col);
                    matrix.Set(row, col, current + cipher_x.Alpha(row) * cipher_y.Alpha(col));
                }
            }
        }

        // Special cases handling for each node's responsibility
        if (node_id == 1 || node_id == 2) {
            const uint64_t alpha45 = matrix.Get(4, 5);
            const uint64_t alpha45_n = alpha45 + cipher_x.Alpha(4) * cipher_y.Alpha(4) +
                                       cipher_x.Alpha(5) * cipher_y.Alpha(5);
            matrix.Set(4, 5, alpha45_n);

            const uint64_t alpha35 = matrix.Get(3, 5);
            const uint64_t alpha35_n = alpha35 + cipher_x.Alpha(3) * cipher_y.Alpha(3);
            matrix.Set(3, 5, alpha35_n);
        } else if (node_id == 3) {
            const uint64_t alpha45 = matrix.Get(4, 5);
            const uint64_t alpha45_n = alpha45 + cipher_x.Alpha(4) * cipher_y.Alpha(4) +
                                       cipher_x.Alpha(5) * cipher_y.Alpha(5);
            matrix.Set(4, 5, alpha45_n);

            const uint64_t alpha12 = matrix.Get(1, 2);
            const uint64_t alpha12_n = alpha12 + cipher_x.Alpha(1) * cipher_y.Alpha(1) +
                                       cipher_x.Alpha(2) * cipher_y.Alpha(2);
            matrix.Set(1, 2, alpha12_n);
        } else if (node_id == 4) {
            const uint64_t alpha12 = matrix.Get(1, 2);
            const uint64_t alpha12_n = alpha12 + cipher_x.Alpha(1) * cipher_y.Alpha(1) +
                                       cipher_x.Alpha(2) * cipher_y.Alpha(2);
            matrix.Set(1, 2, alpha12_n);

            const uint64_t alpha35 = matrix.Get(3, 5);
            const uint64_t alpha35_n = alpha35 + cipher_x.Alpha(3) * cipher_y.Alpha(3);
            matrix.Set(3, 5, alpha35_n);
        } else if (node_id == 5) {
            const uint64_t alpha12 = matrix.Get(1, 2);
            const uint64_t alpha12_n = alpha12 + cipher_x.Alpha(1) * cipher_y.Alpha(1) +
                                       cipher_x.Alpha(2) * cipher_y.Alpha(2);
            matrix.Set(1, 2, alpha12_n);
        }
    }

    // Continue with joint sharing protocols
    MulOffJointSharingPrepareProtocol::Handle(node);
    MulJointSharingProtocol::Handle<Calculator>(node, network_node, ctx);
}

void DotProductOnProtocol::Handle(const std::vector<uint8_t>& data, Node& node,
                                  NetworkNode& network_node, const TaskContext& ctx) {
    const uint8_t node_id = node.ID();
    std::vector<CipherData> cipher_x_vec;
    std::vector<CipherData> cipher_y_vec;

    const uint32_t dimension = readUint32(data, 1);
    const uint32_t x_start_idx = readUint32(data, 5);
    const uint32_t y_start_idx = readUint32(data, 9);
    const uint32_t z_idx = readUint32(data, 13);

    cipher_x_vec.reserve(dimension);
    for (uint32_t t = 0; t < dimension; t++) {
        cipher_x_vec.push_back(node.BetaShares(x_start_idx + t));
    }
    cipher_y_vec.reserve(dimension);
    for (uint32_t t = 0; t < dimension; t++) {
        cipher_y_vec.push_back(node.BetaShares(y_start_idx + t));
    }
    CipherData& cipher_z = node.BetaShares(z_idx);

    uint64_t beta_z[5] = {};
    // Compute for all dimensions
    for (uint32_t t = 0; t < dimension; t++) {
        const CipherData& cipher_x = cipher_x_vec[t];
        const CipherData& cipher_y = cipher_y_vec[t];
        const uint64_t beta_x = cipher_x.Beta();
        const uint64_t beta_y = cipher_y.Beta();

        for (uint8_t id = 1; id <= 5; ++id) {
            if (id != node_id) {
                beta_z[id - 1] += -beta_x * cipher_y.Alpha(id) - beta_y * cipher_x.Alpha(id);
            }
        }
        if (t == 0) {
            for (uint8_t id = 1; id <= 5; ++id) {
                if (id != node_id) {
                    beta_z[id - 1] += cipher_z.Alpha(id);
                    beta_z[id - 1] += node.AlphaXY(id);
                }
            }
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

    uint64_t beta_dot_product = 0;
    for (uint32_t t = 0; t < dimension; t++) {
        beta_dot_product += cipher_x_vec[t].Beta() * cipher_y_vec[t].Beta();
    }

    const uint64_t sum =
        std::accumulate(std::begin(beta_z), std::end(beta_z), static_cast<uint64_t>(0));
    const uint64_t val = sum + beta_dot_product;
    cipher_z.SetBeta(val);
}
