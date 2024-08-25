// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef AMX_USABILITY_H
#define AMX_USABILITY_H

// From https://github.com/corsix/amx/blob/main/aarch64.h
#define AMX_NOP_OP_IMM5(op, imm5) \
    __asm("nop\nnop\nnop\n.word (0x201000 + (%0 << 5) + %1)" : : "i"(op), "i"(imm5) : "memory")

#define AMX_OP_GPR(op, gpr) \
    __asm(".word (0x201000 + (%0 << 5) + 0%1 - ((0%1 >> 4) * 6))" : : "i"(op), "r"((uint64_t)(gpr)) : "memory")

#define AMX_LDX(gpr)    AMX_OP_GPR( 0, gpr)
#define AMX_LDY(gpr)    AMX_OP_GPR( 1, gpr)
#define AMX_STX(gpr)    AMX_OP_GPR( 2, gpr)
#define AMX_STY(gpr)    AMX_OP_GPR( 3, gpr)
#define AMX_LDZ(gpr)    AMX_OP_GPR( 4, gpr)
#define AMX_STZ(gpr)    AMX_OP_GPR( 5, gpr)
#define AMX_LDZI(gpr)   AMX_OP_GPR( 6, gpr)
#define AMX_STZI(gpr)   AMX_OP_GPR( 7, gpr)
#define AMX_EXTRX(gpr)  AMX_OP_GPR( 8, gpr)
#define AMX_EXTRY(gpr)  AMX_OP_GPR( 9, gpr)
#define AMX_FMA64(gpr)  AMX_OP_GPR(10, gpr)
#define AMX_FMS64(gpr)  AMX_OP_GPR(11, gpr)
#define AMX_FMA32(gpr)  AMX_OP_GPR(12, gpr)
#define AMX_FMS32(gpr)  AMX_OP_GPR(13, gpr)
#define AMX_MAC16(gpr)  AMX_OP_GPR(14, gpr)
#define AMX_FMA16(gpr)  AMX_OP_GPR(15, gpr)
#define AMX_FMS16(gpr)  AMX_OP_GPR(16, gpr)
#define AMX_SET()       AMX_NOP_OP_IMM5(17, 0)
#define AMX_CLR()       AMX_NOP_OP_IMM5(17, 1)
#define AMX_VECINT(gpr) AMX_OP_GPR(18, gpr)
#define AMX_VECFP(gpr)  AMX_OP_GPR(19, gpr)
#define AMX_MATINT(gpr) AMX_OP_GPR(20, gpr)
#define AMX_MATFP(gpr)  AMX_OP_GPR(21, gpr)
#define AMX_GENLUT(gpr) AMX_OP_GPR(22, gpr)

#endif // AMX_USABILITY_H