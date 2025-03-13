#ifndef TYPE_H
#define TYPE_H

#include <cstdint>

enum ProtocolType : uint8_t {
    JMP = 1,
    SHARE_BETA_OFF = 2,
    SHARE_BETA = 3,
    JOINT_SHARE_BETA_OFF = 4,
    JOINT_SHARE_BETA = 5,
    JOINT_SHARE_OFF = 6,
    JOINT_SHARE = 7,
    RES_OFF = 8,
    RES = 9,
    REC = 10,
    ADD = 11,
    TRANSFORM = 12,
    MUL_OFF = 13,
    MUL_ON = 14,
    DOT_PRODUCT_OFF = 15,
    DOT_PRODUCT_ON = 16,
    TRUN_OFF = 17,
    TRUN_OFF_PREPARE = 18,
    LINEAR_COMBINE = 19,
    TRUN_ON = 20,
    TRUN_ON_PREPARE = 21,
    RIGHT_SHIFT = 22,
    TRUN_ON_RECOVERY = 23,
    BIT_SHARE_BETA_OFF = 24,
    BIT_SHARE_BETA = 25,
    BIT_REC = 26,
    BIT_MUL_OFF = 27,
    BIT_MUL_ON = 28,
    BIT_FULL_ADD = 29,
    A2B_OFF = 30,
    A2B_OFF_PREPARE = 31,
    INIT_CARRY = 32,
    BIT_XOR = 33,
    BIT_TRANSFORM = 34,
    A2B_OFF_STORE = 35,
    A2B_ON = 36,
    A2B_ON_PREPARE = 37,
    B2A_OFF = 38,
    B2A_ON = 39,
};

#endif
