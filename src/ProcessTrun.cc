#include "MulProtocol.h"
#include "RecProtocol.h"
#include "SharingProtocol.h"
#include "TruncationProtocol.h"
#include "Type.h"
#include "Util.h"

void TrunOffProtocol::Handle(const std::vector<uint8_t> &data, Node &node,
                             NetworkNode &network_node, TaskContext &ctx) {
    uint8_t r_key = data[1];
    uint32_t start_id = readUint32(data, 2);

    // Step 1: TrunOffPrepare协议生成r1,r2,r3的分片（0-191）
    std::vector<uint8_t> trun_msg = {ProtocolType::TRUN_OFF_PREPARE, 1, 2, 3, 4, 5, 0, 0, 0, 0};
    writeUint32(trun_msg, 6, start_id);

    auto trun_off_prepare_proto = new TrunOffPrepareProtocol();
    trun_off_prepare_proto->Handle(trun_msg, node);

    // Step 2: 对每个bit位进行乘法运算
    auto mul_off_proto = new MulOffProtocol();
    auto mul_on_proto = new MulOnProtocol();
    std::vector<uint8_t> mul_msg{ProtocolType::MUL_OFF};
    mul_msg.insert(mul_msg.end(), 12, 0);

    for (uint16_t bit_pos = 0; bit_pos < 64; ++bit_pos) {
        // 1. 计算该bit位的三个双乘积: r1*r2, r2*r3, r1*r3
        for (int i = 0; i < 3; ++i) {
            uint16_t id1 = 0, id2 = 0, result_id = 0;
            switch (i) {
                case 0: {                                      // r1*r2
                    id1 = start_id + bit_pos * 3;              // r1的ID
                    id2 = start_id + bit_pos * 3 + 1;          // r2的ID
                    result_id = start_id + 192 + bit_pos * 3;  // 结果存储ID
                    break;
                }
                case 1: {                                          // r2*r3
                    id1 = start_id + bit_pos * 3 + 1;              // r2的ID
                    id2 = start_id + bit_pos * 3 + 2;              // r3的ID
                    result_id = start_id + 192 + bit_pos * 3 + 1;  // 结果存储ID
                    break;
                }
                case 2: {                                          // r1*r3
                    id1 = start_id + bit_pos * 3;                  // r1的ID
                    id2 = start_id + bit_pos * 3 + 2;              // r3的ID
                    result_id = start_id + 192 + bit_pos * 3 + 2;  // 结果存储ID
                    break;
                }
                default:;
            }

            writeUint32(mul_msg, 1, id1);
            writeUint32(mul_msg, 5, id2);
            writeUint32(mul_msg, 9, result_id);

            // MUL_OFF阶段
            mul_msg[0] = ProtocolType::MUL_OFF;
            mul_off_proto->Handle(mul_msg, node, network_node, ctx);
            ctx.operation_id += 20;

            // MUL_ON阶段
            mul_msg[0] = ProtocolType::MUL_ON;
            mul_on_proto->Handle(mul_msg, node, network_node, ctx);
            ctx.operation_id += 5;
        }

        // 2. 计算r1*r2*r3, 用(r1*r2)*r3计算三元乘积
        const uint16_t mul_id = start_id + 192 + bit_pos * 3;  // r1*r2的ID
        const uint16_t r3_id = start_id + bit_pos * 3 + 2;     // r3的ID
        const uint16_t result_id = start_id + 384 + bit_pos;   // 三元乘积的结果ID

        writeUint32(mul_msg, 1, mul_id);
        writeUint32(mul_msg, 5, r3_id);
        writeUint32(mul_msg, 9, result_id);

        // MUL_OFF阶段
        mul_msg[0] = ProtocolType::MUL_OFF;
        mul_off_proto->Handle(mul_msg, node, network_node, ctx);
        ctx.operation_id += 20;

        // MUL_ON阶段
        mul_msg[0] = ProtocolType::MUL_ON;
        mul_on_proto->Handle(mul_msg, node, network_node, ctx);
        ctx.operation_id += 5;
        // SPDLOG_INFO("The calculation of the {}-th bit is completed", bit_pos + 1);
    }

    // Step 3: 线性组合
    // msg[0]-协议号，msg[1-4]-起始编号，msg[5]-截断位数，msg[6]-存储编号
    std::vector<uint8_t> combine_msg = {
        ProtocolType::LINEAR_COMBINE, 0, 0, 0, 0, kTruncatedBit, r_key};
    writeUint32(combine_msg, 1, start_id);
    auto linear_combine_proto = new LinearCombineProtocol();
    linear_combine_proto->Handle(combine_msg, node);
}

void TrunOnProtocol::Handle(const std::vector<uint8_t> &data, Node &node, NetworkNode &network_node,
                            TaskContext &ctx) {
    uint8_t input_id = data[1];
    uint8_t r_key = data[2];
    uint8_t result_id = data[3];

    auto trun_on_prepare_proto = new TrunOnPrepareProtocol();
    const std::vector<uint8_t> trun_msg = {ProtocolType::TRUN_ON_PREPARE, input_id, r_key,
                                           result_id};
    trun_on_prepare_proto->Handle(trun_msg, node);

    auto rec_proto = new ReconstructionProtocol();
    for (uint8_t id = 1; id <= 3; ++id) {
        // msg[0]-协议号，msg[1-3]-联合数据发送者，msg[4]-数据重构者, msg[5]-数据编号
        std::vector<uint8_t> rec_msg;
        if (id == 1) {
            rec_msg = {ProtocolType::REC, 3, 4, 5, 1, result_id};
        } else if (id == 2) {
            rec_msg = {ProtocolType::REC, 3, 4, 5, 2, result_id};
        } else {
            rec_msg = {ProtocolType::REC, 2, 4, 5, 3, result_id};
        }
        rec_proto->Handle(rec_msg, node, network_node, ctx);
        ctx.operation_id++;
    }

    auto right_shift_msg = new RightShiftProtocol();
    const std::vector<uint8_t> shift_msg = {ProtocolType::RIGHT_SHIFT, 1, 2, 3, result_id};
    right_shift_msg->Handle(shift_msg, node);

    // msg[0]-协议号，msg[1-3]-数据拥有者，msg[4,5]-接收秘密共享份额
    std::vector<uint8_t> joint_share_msg = {ProtocolType::JOINT_SHARE_BETA_OFF, 1, 2, 3, 4, 5};
    auto joint_share_off_proto = new JointSharingBetaOfflineProtocol();
    joint_share_off_proto->Handle(joint_share_msg, node);

    joint_share_msg[0] = ProtocolType::JOINT_SHARE_BETA;
    auto joint_share_on_proto = new JointSharingBetaProtocol();
    joint_share_on_proto->Handle(joint_share_msg, node, network_node, ctx);
    ctx.operation_id++;

    const std::vector<uint8_t> recovery_msg = {ProtocolType::TRUN_ON_RECOVERY, r_key, result_id};
    auto trun_on_recovery_proto = new TrunOnRecoveryProtocol();
    trun_on_recovery_proto->Handle(recovery_msg, node);
}
