#include "A2BProtocol.h"
#include "MulProtocol.h"
#include "Type.h"
#include "Util.h"

void SingleBitFullAdder::Handle(const std::vector<uint8_t>& data, Node& node,
                                NetworkNode& network_node, TaskContext& ctx) {
    auto current_key_high = data[1];
    auto current_key_low = data[2];
    auto prev_key_high = data[3];
    auto prev_key_low = data[4];
    auto carry_key_high = data[5];
    auto carry_key_low = data[6];

    auto temp_space_start = bytesToUint16(data[7], data[8]);
    auto current_key = bytesToUint16(current_key_high, current_key_low);
    auto prev_key = bytesToUint16(prev_key_high, prev_key_low);
    auto carry_key = bytesToUint16(carry_key_high, carry_key_low);

    // Temporary storage keys (temp_space_start to temp_space_start + 2)
    // temp_space_start: Ai ⊕ Bi result
    // temp_space_start + 1: Ai ∧ Bi result
    // temp_space_start + 2: (Ai ⊕ Bi) ∧ Cin result
    auto [xor_result_high, xor_result_low] = uint16ToBytes(temp_space_start);
    auto [and_result_high, and_result_low] = uint16ToBytes(temp_space_start + 1);
    auto [and2_result_high, and2_result_low] = uint16ToBytes(temp_space_start + 2);

    // 1. Calculate Ai ⊕ Bi
    std::vector<uint8_t> xor_msg1 = {ProtocolType::BIT_XOR, current_key_high, current_key_low,
                                     prev_key_high,         prev_key_low,     xor_result_high,
                                     xor_result_low};
    auto bit_xor_proto = new BitXorProtocol();
    bit_xor_proto->Handle(xor_msg1, node);

    // 2. Calculate Ai ∧ Bi
    std::vector<uint8_t> transform_msg1 = {ProtocolType::BIT_TRANSFORM, 0, 0, 1};
    auto bit_transform_proto = new BitTransformSharingProtocol();
    for (int j = 0; j < 2; ++j) {
        transform_msg1[1] = j == 0 ? current_key_high : prev_key_high;
        transform_msg1[2] = j == 0 ? current_key_low : prev_key_low;
        bit_transform_proto->Handle(transform_msg1, node);
    }

    auto mul_off_proto = new MulOffProtocol();
    auto mul_on_proto = new MulOnProtocol();
    std::vector<uint8_t> mul_msg1 = {ProtocolType::BIT_MUL_OFF};
    mul_msg1.insert(mul_msg1.end(), 12, 0);
    writeUint32(mul_msg1, 1, current_key);
    writeUint32(mul_msg1, 5, prev_key);
    writeUint32(mul_msg1, 9, temp_space_start + 1);

    // MUL_OFF phase
    mul_off_proto->Handle(mul_msg1, node, network_node, ctx);
    ctx.operation_id += 20;

    // MUL_ON phase
    mul_msg1[0] = ProtocolType::BIT_MUL_ON;
    mul_on_proto->Handle(mul_msg1, node, network_node, ctx);
    ctx.operation_id += 5;

    // 3. Calculate (Ai ⊕ Bi) ∧ Cin
    std::vector<uint8_t> transform_msg2 = {ProtocolType::BIT_TRANSFORM, 0, 0, 1};
    for (int j = 0; j < 2; ++j) {
        transform_msg2[1] = j == 0 ? xor_result_high : carry_key_high;
        transform_msg2[2] = j == 0 ? xor_result_low : carry_key_low;
        bit_transform_proto->Handle(transform_msg2, node);
    }

    std::vector<uint8_t> mul_msg2 = {ProtocolType::BIT_MUL_OFF};
    mul_msg2.insert(mul_msg2.end(), 12, 0);
    writeUint32(mul_msg2, 1, temp_space_start);
    writeUint32(mul_msg2, 5, carry_key);
    writeUint32(mul_msg2, 9, temp_space_start + 2);

    // MUL_OFF phase
    mul_off_proto->Handle(mul_msg2, node, network_node, ctx);
    ctx.operation_id += 20;

    // MUL_ON phase
    mul_msg2[0] = ProtocolType::BIT_MUL_ON;
    mul_on_proto->Handle(mul_msg2, node, network_node, ctx);
    ctx.operation_id += 5;

    // 4. Calculate S = (Ai ⊕ Bi) ⊕ Cin
    std::vector<uint8_t> xor_msg2 = {ProtocolType::BIT_XOR, xor_result_high, xor_result_low,
                                     carry_key_high,        carry_key_low,   current_key_high,
                                     current_key_low};
    bit_xor_proto->Handle(xor_msg2, node);

    // 5. Calculate Cout = (Ai ∧ Bi) ⊕ ((Ai ⊕ Bi) ∧ Cin)
    // In fact, it should be (Ai ∧ Bi) ∨ ((Ai ⊕ Bi) ∧ Cin),
    // but since both terms can never be 1 simultaneously, ⊕ achieves the same result.
    std::vector<uint8_t> xor_msg3 = {ProtocolType::BIT_XOR, and_result_high, and_result_low,
                                     and2_result_high,      and2_result_low, carry_key_high,
                                     carry_key_low};
    bit_xor_proto->Handle(xor_msg3, node);
}

void A2BOffProtocol::Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                            TaskContext& ctx) {
    auto target_id = bytesToUint16(data[1], data[2]);
    auto start_id = data[3];
    auto bit_key = data[4];

    std::vector<uint8_t> a2b_off_msg = {ProtocolType::A2B_OFF_PREPARE, 0, 0, start_id};
    a2b_off_msg[1] = static_cast<uint8_t>(target_id >> 8);
    a2b_off_msg[2] = static_cast<uint8_t>(target_id & 0xFF);
    auto a2b_off_prepare_proto = new A2BOffPreProtocol();
    a2b_off_prepare_proto->Handle(a2b_off_msg, node);

    // Process each alpha value
    auto inint_carry_proto = new InitializeCarryProtocol();
    std::vector<uint8_t> init_msg = {ProtocolType::INIT_CARRY, 0, 0};
    auto bit_full_adder = new SingleBitFullAdder();
    std::vector<uint8_t> bit_add_msg{ProtocolType::BIT_FULL_ADD};
    bit_add_msg.insert(bit_add_msg.end(), 8, 0);

    for (uint8_t alpha_idx = 2; alpha_idx <= 5; alpha_idx++) {
        // Initialize carry with 0 for lowest bit
        init_msg[1] = static_cast<uint8_t>((start_id + 328) >> 8);
        init_msg[2] = static_cast<uint8_t>((start_id + 328) & 0xFF);
        inint_carry_proto->Handle(init_msg, node);

        // Process each bit position
        for (uint8_t bit_pos = 0; bit_pos < 64; bit_pos++) {
            uint16_t current_key = start_id + 5 + (alpha_idx - 1) * 64 + bit_pos;
            uint16_t prev_key = start_id + 5 + (alpha_idx - 2) * 64 + bit_pos;
            uint16_t carry_key = start_id + 328;  // Reuse the same key for next iteration
            uint16_t temp_space_start = start_id + 325;

            // Call the single bit full adder function
            std::vector<uint16_t> keys = {current_key, prev_key, carry_key, temp_space_start};
            size_t idx = 1;
            for (auto key : keys) {
                auto [high, low] = uint16ToBytes(key);
                bit_add_msg[idx++] = high;
                bit_add_msg[idx++] = low;
            }
            bit_full_adder->Handle(bit_add_msg, node, network_node, ctx);
        }
        // SPDLOG_INFO("Completed accumulation up to alpha{}", alpha_idx);
    }

    std::vector<uint8_t> a2b_store_msg = {ProtocolType::A2B_OFF_STORE, bit_key, start_id};
    auto a2b_store_proto = new A2BOffStoreProtocol();
    a2b_store_proto->Handle(a2b_store_msg, node);
}

void A2BOnProtocol::Handle(const std::vector<uint8_t>& data, Node& node, NetworkNode& network_node,
                           TaskContext& ctx) {
    auto target_id = bytesToUint16(data[1], data[2]);
    auto start_id = data[3];
    auto bit_key = data[4];

    std::vector<uint8_t> a2b_on_msg = {ProtocolType::A2B_ON_PREPARE, 0, 0, bit_key, start_id};
    a2b_on_msg[1] = static_cast<uint8_t>(target_id >> 8);
    a2b_on_msg[2] = static_cast<uint8_t>(target_id & 0xFF);
    auto a2b_on_prepare_proto = new A2BOnPreProtocol();
    a2b_on_prepare_proto->Handle(a2b_on_msg, node);

    // Initialize carry with 0 for lowest bit
    std::vector<uint8_t> init_msg = {ProtocolType::INIT_CARRY, 0, 0};
    init_msg[1] = static_cast<uint8_t>((start_id + 131) >> 8);
    init_msg[2] = static_cast<uint8_t>((start_id + 131) & 0xFF);
    auto inint_carry_proto = new InitializeCarryProtocol();
    inint_carry_proto->Handle(init_msg, node);

    auto bit_full_adder = new SingleBitFullAdder();
    std::vector<uint8_t> bit_add_msg{ProtocolType::BIT_FULL_ADD};
    bit_add_msg.insert(bit_add_msg.end(), 8, 0);

    // Process each bit position
    for (uint8_t bit_pos = 0; bit_pos < 64; bit_pos++) {
        uint16_t current_key = start_id + bit_pos;
        uint16_t prev_key = start_id + 64 + bit_pos;
        uint16_t carry_key = start_id + 131;
        uint16_t temp_space_start = start_id + 128;

        // Call the single bit full adder function
        std::vector<uint16_t> keys = {current_key, prev_key, carry_key, temp_space_start};
        size_t idx = 1;
        for (auto key : keys) {
            auto [high, low] = uint16ToBytes(key);
            bit_add_msg[idx++] = high;
            bit_add_msg[idx++] = low;
        }
        bit_full_adder->Handle(bit_add_msg, node, network_node, ctx);
    }
}
