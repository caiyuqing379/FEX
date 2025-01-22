/**
 * @file arm-asm.cpp
 * @brief ARM指令汇编器的实现
 *
 * 本文件包含了用于将ARM指令转换为机器码的各种函数。
 * 主要功能包括条件码映射、寄存器映射、以及各种ARM指令的汇编实现。
 */

#include "Interface/Core/ArchHelpers/Arm64Emitter.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Emitter.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Registers.h"

#include <FEXCore/Utils/CompilerDefs.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/Utils/MathUtils.h>

// #include "Interface/Core/Frontend.h"
#include "Interface/Core/Pattern/RuleDebug.h"
#include "Interface/Core/Pattern/arm-instr.h"
#include "Interface/Core/Pattern/rule-translate.h"
#include "Interface/Core/PatternMatcher.h"

using namespace FEXCore;
using namespace FEXCore::Pattern;

constexpr static bool HostSupportsSVE256 = false;
constexpr static int XMM_AVX_REG_SIZE = 32;
constexpr static int XMM_SSE_REG_SIZE = 16;

void PatternMatcher::LoadConstant(FEXCore::ARMEmitter::Size s,
                                  FEXCore::ARMEmitter::Register Reg,
                                  uint64_t Constant, bool NOPPad) {
  bool Is64Bit = s == ARMEmitter::Size::i64Bit;
  int Segments = Is64Bit ? 4 : 2;

  if (Is64Bit && ((~Constant) >> 16) == 0) {
    ARMAssembler->movn(s, Reg, (~Constant) & 0xFFFF);

    if (NOPPad) {
      ARMAssembler->nop();
      ARMAssembler->nop();
      ARMAssembler->nop();
    }
    return;
  }

  if ((Constant >> 32) == 0) {
    // If the upper 32-bits is all zero, we can now switch to a 32-bit move.
    s = ARMEmitter::Size::i32Bit;
    Is64Bit = false;
    Segments = 2;
  }

  int RequiredMoveSegments{};

  // Count the number of move segments
  // We only want to use ADRP+ADD if we have more than 1 segment
  for (size_t i = 0; i < Segments; ++i) {
    uint16_t Part = (Constant >> (i * 16)) & 0xFFFF;
    if (Part != 0) {
      ++RequiredMoveSegments;
    }
  }

  // If this can be loaded with a mov bitmask.
  if (RequiredMoveSegments > 1) {
    // Only try to use this path if the number of segments is > 1.
    // `movz` is better than `orr` since hardware will rename or merge if
    // possible when `movz` is used.
    const auto IsImm =
        vixl::aarch64::Assembler::IsImmLogical(Constant, RegSizeInBits(s));
    if (IsImm) {
      ARMAssembler->orr(s, Reg, ARMEmitter::Reg::zr, Constant);
      if (NOPPad) {
        ARMAssembler->nop();
        ARMAssembler->nop();
        ARMAssembler->nop();
      }
      return;
    }
  }

  // ADRP+ADD is specifically optimized in hardware
  // Check if we can use this
  auto PC = ARMAssembler->GetCursorAddress<uint64_t>();

  // PC aligned to page
  uint64_t AlignedPC = PC & ~0xFFFULL;

  // Offset from aligned PC
  int64_t AlignedOffset =
      static_cast<int64_t>(Constant) - static_cast<int64_t>(AlignedPC);

  int NumMoves = 0;

  // If the aligned offset is within the 4GB window then we can use ADRP+ADD
  // and the number of move segments more than 1
  if (RequiredMoveSegments > 1 && vixl::IsInt32(AlignedOffset)) {
    // If this is 4k page aligned then we only need ADRP
    if ((AlignedOffset & 0xFFF) == 0) {
      ARMAssembler->adrp(Reg, AlignedOffset >> 12);
    } else {
      // If the constant is within 1MB of PC then we can still use ADR to load
      // in a single instruction 21-bit signed integer here
      int64_t SmallOffset =
          static_cast<int64_t>(Constant) - static_cast<int64_t>(PC);
      if (vixl::IsInt21(SmallOffset)) {
        ARMAssembler->adr(Reg, SmallOffset);
      } else {
        // Need to use ADRP + ADD
        ARMAssembler->adrp(Reg, AlignedOffset >> 12);
        ARMAssembler->add(s, Reg, Reg, Constant & 0xFFF);
        NumMoves = 2;
      }
    }
  } else {
    int CurrentSegment = 0;
    for (; CurrentSegment < Segments; ++CurrentSegment) {
      uint16_t Part = (Constant >> (CurrentSegment * 16)) & 0xFFFF;
      if (Part) {
        ARMAssembler->movz(s, Reg, Part, CurrentSegment * 16);
        ++CurrentSegment;
        ++NumMoves;
        break;
      }
    }

    for (; CurrentSegment < Segments; ++CurrentSegment) {
      uint16_t Part = (Constant >> (CurrentSegment * 16)) & 0xFFFF;
      if (Part) {
        ARMAssembler->movk(s, Reg, Part, CurrentSegment * 16);
        ++NumMoves;
      }
    }

    if (NumMoves == 0) {
      // If we didn't move anything that means this is a zero move. Special case
      // this.
      ARMAssembler->movz(s, Reg, 0);
      ++NumMoves;
    }
  }

  if (NOPPad) {
    for (int i = NumMoves; i < Segments; ++i) {
      ARMAssembler->nop();
    }
  }
}

#define DEF_OPC(x)                                                             \
  void FEXCore::Pattern::PatternMatcher::Opc_##x(ARMInstruction *instr,        \
                                                 RuleRecord *rrule)

/**
 * @brief 将ARM条件码映射到ARMEmitter中的条件码枚举
 *
 * @param Cond ARM条件码
 * @return ARMEmitter::Condition 对应的ARMEmitter条件码
 */
static ARMEmitter::Condition MapBranchCC(ARMConditionCode Cond) {
  switch (Cond) {
  case ARM_CC_EQ:
    return ARMEmitter::Condition::CC_EQ;
  case ARM_CC_NE:
    return ARMEmitter::Condition::CC_NE;
  case ARM_CC_GE:
    return ARMEmitter::Condition::CC_GE;
  case ARM_CC_LT:
    return ARMEmitter::Condition::CC_LT;
  case ARM_CC_GT:
    return ARMEmitter::Condition::CC_GT;
  case ARM_CC_LE:
    return ARMEmitter::Condition::CC_LE;
  case ARM_CC_CS:
    return ARMEmitter::Condition::CC_CS;
  case ARM_CC_CC:
    return ARMEmitter::Condition::CC_CC;
  case ARM_CC_HI:
    return ARMEmitter::Condition::CC_HI;
  case ARM_CC_LS:
    return ARMEmitter::Condition::CC_LS;
  case ARM_CC_VS:
    return ARMEmitter::Condition::CC_VS;
  case ARM_CC_VC:
    return ARMEmitter::Condition::CC_VC;
  case ARM_CC_MI:
    return ARMEmitter::Condition::CC_MI;
  case ARM_CC_PL:
    return ARMEmitter::Condition::CC_PL;
  default:
    LOGMAN_MSG_A_FMT("Unsupported compare type");
    return ARMEmitter::Condition::CC_NV;
  }
}

static ARMEmitter::ShiftType GetShiftType(ARMOperandScaleDirect direct) {
  switch (direct) {
  case ARM_OPD_DIRECT_LSL:
    return ARMEmitter::ShiftType::LSL;
  case ARM_OPD_DIRECT_LSR:
    return ARMEmitter::ShiftType::LSR;
  case ARM_OPD_DIRECT_ASR:
    return ARMEmitter::ShiftType::ASR;
  case ARM_OPD_DIRECT_ROR:
    return ARMEmitter::ShiftType::ROR;
  default:
    LOGMAN_MSG_A_FMT("Unsupported Shift type");
  }
}

/**
 * @brief 获取指定的扩展类型
 *
 * @param extend ARM操作数比例扩展类型
 * @return ARMEmitter::ExtendedType 对应的ARMEmitter扩展类型
 */
static ARMEmitter::ExtendedType GetExtendType(ARMOperandScaleExtend extend) {
  switch (extend) {
  case ARM_OPD_EXTEND_UXTB:
    return ARMEmitter::ExtendedType::UXTB;
  case ARM_OPD_EXTEND_UXTH:
    return ARMEmitter::ExtendedType::UXTH;
  case ARM_OPD_EXTEND_UXTW:
    return ARMEmitter::ExtendedType::UXTW;
  case ARM_OPD_EXTEND_UXTX:
    return ARMEmitter::ExtendedType::UXTX;
  case ARM_OPD_EXTEND_SXTB:
    return ARMEmitter::ExtendedType::SXTB;
  case ARM_OPD_EXTEND_SXTH:
    return ARMEmitter::ExtendedType::SXTH;
  case ARM_OPD_EXTEND_SXTW:
    return ARMEmitter::ExtendedType::SXTW;
  case ARM_OPD_EXTEND_SXTX:
    return ARMEmitter::ExtendedType::SXTX;
  default:
    LOGMAN_MSG_A_FMT("Unsupported Extend type");
  }
}

/**
 * @brief 将ARM寄存器映射到ARMEmitter寄存器
 *
 * @param reg ARM寄存器
 * @return ARMEmitter::Register 对应的ARMEmitter寄存器
 */
static ARMEmitter::Register GetRegMap(ARMRegister &reg) {
  ARMEmitter::Register reg_invalid(255);
  switch (reg) {
  case ARM_REG_R0:
    return ARMEmitter::Reg::r0;
  case ARM_REG_R1:
    return ARMEmitter::Reg::r1;
  case ARM_REG_R2:
    return ARMEmitter::Reg::r2;
  case ARM_REG_R3:
    return ARMEmitter::Reg::r3;
  case ARM_REG_R4:
    return ARMEmitter::Reg::r4;
  case ARM_REG_R5:
    return ARMEmitter::Reg::r5;
  case ARM_REG_R6:
    return ARMEmitter::Reg::r6;
  case ARM_REG_R7:
    return ARMEmitter::Reg::r7;
  case ARM_REG_R8:
    return ARMEmitter::Reg::r8;
  case ARM_REG_R9:
    return ARMEmitter::Reg::r9;
  case ARM_REG_R10:
    return ARMEmitter::Reg::r10;
  case ARM_REG_R11:
    return ARMEmitter::Reg::r11;
  case ARM_REG_R12:
    return ARMEmitter::Reg::r12;
  case ARM_REG_R13:
    return ARMEmitter::Reg::r13;
  case ARM_REG_R14:
    return ARMEmitter::Reg::r14;
  case ARM_REG_R15:
    return ARMEmitter::Reg::r15;
  case ARM_REG_R16:
    return ARMEmitter::Reg::r16;
  case ARM_REG_R17:
    return ARMEmitter::Reg::r17;
  case ARM_REG_R18:
    return ARMEmitter::Reg::r18;
  case ARM_REG_R19:
    return ARMEmitter::Reg::r19;
  case ARM_REG_R20:
    return ARMEmitter::Reg::r20;
  case ARM_REG_R21:
    return ARMEmitter::Reg::r21;
  case ARM_REG_R22:
    return ARMEmitter::Reg::r22;
  case ARM_REG_R23:
    return ARMEmitter::Reg::r23;
  case ARM_REG_R24:
    return ARMEmitter::Reg::r24;
  case ARM_REG_R25:
    return ARMEmitter::Reg::r25;
  case ARM_REG_R26:
    return ARMEmitter::Reg::r26;
  case ARM_REG_R27:
    return ARMEmitter::Reg::r27;
  case ARM_REG_R28:
    return ARMEmitter::Reg::r28;
  case ARM_REG_R29:
    return ARMEmitter::Reg::r29;
  case ARM_REG_R30:
    return ARMEmitter::Reg::r30;
  case ARM_REG_R31:
    return ARMEmitter::Reg::r31;

  case ARM_REG_FP:
    return ARMEmitter::Reg::fp;
  case ARM_REG_LR:
    return ARMEmitter::Reg::lr;
  case ARM_REG_RSP:
    return ARMEmitter::Reg::rsp;
  case ARM_REG_ZR:
    return ARMEmitter::Reg::zr;
  default:
    LOGMAN_MSG_A_FMT("Unsupported host reg num");
  }
  return reg_invalid;
}

/**
 * @brief 将ARM向量寄存器映射到ARMEmitter向量寄存器
 *
 * @param reg ARM向量寄存器
 * @return ARMEmitter::VRegister 对应的ARMEmitter向量寄存器
 */
static ARMEmitter::VRegister GetVRegMap(ARMRegister &reg) {
  ARMEmitter::VRegister reg_invalid(255);
  switch (reg) {
  case ARM_REG_V0:
    return ARMEmitter::VReg::v0;
  case ARM_REG_V1:
    return ARMEmitter::VReg::v1;
  case ARM_REG_V2:
    return ARMEmitter::VReg::v2;
  case ARM_REG_V3:
    return ARMEmitter::VReg::v3;
  case ARM_REG_V4:
    return ARMEmitter::VReg::v4;
  case ARM_REG_V5:
    return ARMEmitter::VReg::v5;
  case ARM_REG_V6:
    return ARMEmitter::VReg::v6;
  case ARM_REG_V7:
    return ARMEmitter::VReg::v7;
  case ARM_REG_V8:
    return ARMEmitter::VReg::v8;
  case ARM_REG_V9:
    return ARMEmitter::VReg::v9;
  case ARM_REG_V10:
    return ARMEmitter::VReg::v10;
  case ARM_REG_V11:
    return ARMEmitter::VReg::v11;
  case ARM_REG_V12:
    return ARMEmitter::VReg::v12;
  case ARM_REG_V13:
    return ARMEmitter::VReg::v13;
  case ARM_REG_V14:
    return ARMEmitter::VReg::v14;
  case ARM_REG_V15:
    return ARMEmitter::VReg::v15;
  case ARM_REG_V16:
    return ARMEmitter::VReg::v16;
  case ARM_REG_V17:
    return ARMEmitter::VReg::v17;
  case ARM_REG_V18:
    return ARMEmitter::VReg::v18;
  case ARM_REG_V19:
    return ARMEmitter::VReg::v19;
  case ARM_REG_V20:
    return ARMEmitter::VReg::v20;
  case ARM_REG_V21:
    return ARMEmitter::VReg::v21;
  case ARM_REG_V22:
    return ARMEmitter::VReg::v22;
  case ARM_REG_V23:
    return ARMEmitter::VReg::v23;
  case ARM_REG_V24:
    return ARMEmitter::VReg::v24;
  case ARM_REG_V25:
    return ARMEmitter::VReg::v25;
  case ARM_REG_V26:
    return ARMEmitter::VReg::v26;
  case ARM_REG_V27:
    return ARMEmitter::VReg::v27;
  case ARM_REG_V28:
    return ARMEmitter::VReg::v28;
  case ARM_REG_V29:
    return ARMEmitter::VReg::v29;
  case ARM_REG_V30:
    return ARMEmitter::VReg::v30;
  case ARM_REG_V31:
    return ARMEmitter::VReg::v31;
  default:
    LOGMAN_MSG_A_FMT("Unsupported host vreg num");
  }
  return reg_invalid;
}

/**
 * @brief 生成扩展内存操作数
 *
 * @param Base 基址寄存器
 * @param Index 索引寄存器
 * @param Imm 立即数偏移
 * @param OffsetScale 偏移比例
 * @param OffsetType 偏移类型
 * @return ARMEmitter::ExtendedMemOperand 生成的扩展内存操作数
 */
static ARMEmitter::ExtendedMemOperand
GenerateExtMemOperand(ARMEmitter::Register Base, ARMRegister Index, int32_t Imm,
                      ARMOperandScale OffsetScale, ARMMemIndexType OffsetType) {

  uint8_t amount = static_cast<uint8_t>(OffsetScale.imm.content.val);

  if (Index == ARM_REG_INVALID) {
    if (OffsetType == ARM_MEM_INDEX_TYPE_PRE)
      return ARMEmitter::ExtendedMemOperand(Base.X(),
                                            ARMEmitter::IndexType::PRE, Imm);
    else if (OffsetType == ARM_MEM_INDEX_TYPE_POST)
      return ARMEmitter::ExtendedMemOperand(Base.X(),
                                            ARMEmitter::IndexType::POST, Imm);
    else
      return ARMEmitter::ExtendedMemOperand(Base.X(),
                                            ARMEmitter::IndexType::OFFSET, Imm);
  } else {
    switch (OffsetScale.content.extend) {
    case ARM_OPD_EXTEND_UXTW:
      return ARMEmitter::ExtendedMemOperand(Base.X(), GetRegMap(Index).X(),
                                            ARMEmitter::ExtendedType::UXTW,
                                            FEXCore::ilog2(amount));
    case ARM_OPD_EXTEND_SXTW:
      return ARMEmitter::ExtendedMemOperand(Base.X(), GetRegMap(Index).X(),
                                            ARMEmitter::ExtendedType::SXTW,
                                            FEXCore::ilog2(amount));
    case ARM_OPD_EXTEND_SXTX:
      return ARMEmitter::ExtendedMemOperand(Base.X(), GetRegMap(Index).X(),
                                            ARMEmitter::ExtendedType::SXTX,
                                            FEXCore::ilog2(amount));
    default:
      LOGMAN_MSG_A_FMT("Unhandled GenerateExtMemOperand OffsetType");
      break;
    }
  }
}

/**
 * @brief 实现LDR(加载)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(LDR) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;
  uint32_t Reg0Size, Reg1Size;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
    auto SrcReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);

    if (Reg0Size && !OpSize)
      OpSize = 1 << (Reg0Size - 1);

    if (opd1->content.mem.base != ARM_REG_INVALID) {
      ARMRegister Index = opd1->content.mem.index;
      ARMOperandScale OffsetScale = opd1->content.mem.scale;
      ARMMemIndexType PrePost = opd1->content.mem.pre_post;
      auto ARMReg = GetGuestRegMap(opd1->content.mem.base, Reg1Size);
      auto Base = GetRegMap(ARMReg);
      int64_t Imm = GetARMImmMapWrapper(&opd1->content.mem.offset);

      auto MemSrc =
          GenerateExtMemOperand(Base, Index, Imm, OffsetScale, PrePost);

      // 处理特殊情况：负立即数、大立即数等
      if ((Index == ARM_REG_INVALID) && (PrePost == ARM_MEM_INDEX_TYPE_NONE)) {
        // 负立即数且位数小于12位
        if (Imm < 0 && !((0 - Imm) >> 12)) {
          int32_t s = static_cast<int32_t>(Imm);
          ARMAssembler->sub(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21,
                            Base, 0 - s);
          MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r21).X(), Index, 0,
                                         OffsetScale, PrePost);
        } else if ((Imm < 0 && ((0 - Imm) >> 12)) ||
                   ((instr->opc == ARM_OPC_LDRH) && (Imm & 0b1)) ||
                   ((instr->opc == ARM_OPC_LDR) && ((Imm & 0b11)))) {
          OffsetScale.imm.content.val = 1;
          OffsetScale.content.extend = ARM_OPD_EXTEND_SXTX;
          LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(),
                       Imm);
          MemSrc =
              GenerateExtMemOperand(Base, ARM_REG_R21, 0, OffsetScale, PrePost);
        } else if (Imm > 0 && (Imm >> 12)) {
          LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r22, Imm);
          ARMAssembler->add(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21,
                            Base, ARMEmitter::Reg::r22);
          MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r21).X(), Index, 0,
                                         OffsetScale, PrePost);
        }
      }

      // 根据不同的加载指令和操作数类型生成相应的加载指令
      if (instr->opc == ARM_OPC_LDRB || instr->opc == ARM_OPC_LDRH ||
          instr->opc == ARM_OPC_LDR) {
        if (SrcReg >= ARM_REG_R0 && SrcReg <= ARM_REG_R31) {
          const auto Dst = GetRegMap(SrcReg);
          switch (OpSize) {
          case 1:
            ARMAssembler->ldrb(Dst, MemSrc);
            break;
          case 2:
            ARMAssembler->ldrh(Dst, MemSrc);
            break;
          case 4:
            ARMAssembler->ldr(Dst.W(), MemSrc);
            break;
          case 8:
            ARMAssembler->ldr(Dst.X(), MemSrc); // LDR（literal) not support
            break;
          default:
            LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
            break;
          }
        } else if (SrcReg >= ARM_REG_V0 && SrcReg <= ARM_REG_V31) {
          const auto Dst = GetVRegMap(SrcReg);
          switch (OpSize) {
          case 1:
            ARMAssembler->ldrb(Dst, MemSrc);
            break;
          case 2:
            ARMAssembler->ldrh(Dst, MemSrc);
            break;
          case 4:
            ARMAssembler->ldr(Dst.S(), MemSrc);
            break;
          case 8:
            ARMAssembler->ldr(Dst.D(), MemSrc);
            break;
          case 16:
            ARMAssembler->ldr(Dst.Q(), MemSrc);
            break;
          default:
            LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
            break;
          }
        }
      } else if (instr->opc == ARM_OPC_LDRSB) {
        const auto Dst = GetRegMap(SrcReg);
        if (OpSize == 4)
          ARMAssembler->ldrsb(Dst.W(), MemSrc);
        else
          ARMAssembler->ldrsb(Dst.X(), MemSrc);
      } else if (instr->opc == ARM_OPC_LDRSH) {
        const auto Dst = GetRegMap(SrcReg);
        if (OpSize == 4)
          ARMAssembler->ldrsh(Dst.W(), MemSrc);
        else
          ARMAssembler->ldrsh(Dst.X(), MemSrc);
      } else if (instr->opc == ARM_OPC_LDAR) {
        const auto Dst = GetRegMap(SrcReg);
        ARMAssembler->ldar(Dst.X(), Base);
        ARMAssembler->nop();
      }
    }
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for ldr instruction.");
}

/**
 * @brief 实现LDP(加载对)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(LDP) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint32_t Reg0Size, Reg1Size, Reg2Size;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_MEM) {
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto RegPair1 = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto RegPair2 = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.mem.base, Reg2Size);
    auto Base = GetRegMap(ARMReg);
    ARMMemIndexType PrePost = opd2->content.mem.pre_post;
    int32_t Imm = GetARMImmMapWrapper(&opd2->content.mem.offset);

    // 根据不同的索引类型生成相应的LDP指令
    if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
      ARMAssembler->ldp<ARMEmitter::IndexType::PRE>(RegPair1.X(), RegPair2.X(),
                                                    Base, Imm);
    else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
      ARMAssembler->ldp<ARMEmitter::IndexType::POST>(RegPair1.X(), RegPair2.X(),
                                                     Base, Imm);
    else
      ARMAssembler->ldp<ARMEmitter::IndexType::OFFSET>(RegPair1.X(),
                                                       RegPair2.X(), Base, Imm);
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for ldp instruction.");
}

/**
 * @brief 实现STR(存储)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(STR) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;
  uint32_t Reg0Size, Reg1Size;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
    auto SrcReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);

    if (Reg0Size && !OpSize)
      OpSize = 1 << (Reg0Size - 1);

    if (opd1->content.mem.base != ARM_REG_INVALID) {
      ARMRegister Index = opd1->content.mem.index;
      ARMOperandScale OffsetScale = opd1->content.mem.scale;
      ARMMemIndexType PrePost = opd1->content.mem.pre_post;
      auto ARMReg = GetGuestRegMap(opd1->content.mem.base, Reg1Size);
      auto Base = GetRegMap(ARMReg);
      int64_t Imm = GetARMImmMapWrapper(&opd1->content.mem.offset);

      auto MemSrc =
          GenerateExtMemOperand(Base, Index, Imm, OffsetScale, PrePost);

      // 处理特殊情况：负立即数、大立即数等
      if ((Index == ARM_REG_INVALID) && (PrePost == ARM_MEM_INDEX_TYPE_NONE)) {
        // 负立即数且位数小于12位
        if (Imm < 0 && !((0 - Imm) >> 12)) {
          int32_t s = static_cast<int32_t>(Imm);
          ARMAssembler->sub(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21,
                            Base, 0 - s);
          MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r21).X(), Index, 0,
                                         OffsetScale, PrePost);
        } else if ((Imm < 0 && ((0 - Imm) >> 12)) ||
                   ((instr->opc == ARM_OPC_STRH) && (Imm & 0b1)) ||
                   ((instr->opc == ARM_OPC_STR) && ((Imm & 0b11)))) {
          OffsetScale.imm.content.val = 1;
          OffsetScale.content.extend = ARM_OPD_EXTEND_SXTX;
          LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(),
                       Imm);
          MemSrc =
              GenerateExtMemOperand(Base, ARM_REG_R21, 0, OffsetScale, PrePost);
        } else if (Imm > 0 && (Imm >> 12)) {
          LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r22, Imm);
          ARMAssembler->add(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21,
                            Base, ARMEmitter::Reg::r22);
          MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r21).X(), Index, 0,
                                         OffsetScale, PrePost);
        }
      }

      // 根据不同的存储指令和操作数类型生成相应的存储指令
      if (SrcReg >= ARM_REG_R0 && SrcReg <= ARM_REG_R31) {
        const auto Src = GetRegMap(SrcReg);
        switch (OpSize) {
        case 1:
          ARMAssembler->strb(Src, MemSrc);
          break;
        case 2:
          ARMAssembler->strh(Src, MemSrc);
          break;
        case 4:
          ARMAssembler->str(Src.W(), MemSrc);
          break;
        case 8:
          ARMAssembler->str(Src.X(), MemSrc);
          break;
        default:
          LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
          break;
        }
      } else if (SrcReg >= ARM_REG_V0 && SrcReg <= ARM_REG_V31) {
        const auto Src = GetVRegMap(SrcReg);
        switch (OpSize) {
        case 1:
          ARMAssembler->strb(Src, MemSrc);
          break;
        case 2:
          ARMAssembler->strh(Src, MemSrc);
          break;
        case 4:
          ARMAssembler->str(Src.S(), MemSrc);
          break;
        case 8:
          ARMAssembler->str(Src.D(), MemSrc);
          break;
        case 16:
          ARMAssembler->str(Src.Q(), MemSrc);
          break;
        default:
          LOGMAN_MSG_A_FMT("Unhandled StoreMem size: {}", OpSize);
          break;
        }
      }
    }
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for str instruction.");
}

/**
 * @brief 实现STP(存储对)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(STP) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;
  uint32_t Reg0Size, Reg1Size, Reg2Size;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_MEM) {
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto RegPair1 = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto RegPair2 = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.mem.base, Reg2Size);
    auto Base = GetRegMap(ARMReg);
    ARMMemIndexType PrePost = opd2->content.mem.pre_post;
    int32_t Imm = GetARMImmMapWrapper(&opd2->content.mem.offset);

    if (Reg0Size)
      OpSize = 1 << (Reg0Size - 1);

    // 根据操作数大小和索引类型生成相应的STP指令
    if (OpSize == 8) {
      if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
        ARMAssembler->stp<ARMEmitter::IndexType::PRE>(RegPair1.X(),
                                                      RegPair2.X(), Base, Imm);
      else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
        ARMAssembler->stp<ARMEmitter::IndexType::POST>(RegPair1.X(),
                                                       RegPair2.X(), Base, Imm);
      else
        ARMAssembler->stp<ARMEmitter::IndexType::OFFSET>(
            RegPair1.X(), RegPair2.X(), Base, Imm);
    } else if (OpSize == 4) {
      if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
        ARMAssembler->stp<ARMEmitter::IndexType::PRE>(RegPair1.W(),
                                                      RegPair2.W(), Base, Imm);
      else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
        ARMAssembler->stp<ARMEmitter::IndexType::POST>(RegPair1.W(),
                                                       RegPair2.W(), Base, Imm);
      else
        ARMAssembler->stp<ARMEmitter::IndexType::OFFSET>(
            RegPair1.W(), RegPair2.W(), Base, Imm);
    } else
      LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for stp instruction.");
}

/**
 * @brief 实现SXTW(符号扩展字到双字)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(SXTW) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint32_t Reg0Size, Reg1Size;

  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  const auto Dst = GetRegMap(DstReg);
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  const auto Src = GetRegMap(SrcReg);

  ARMAssembler->sxtw(Dst.X(), Src.W());
}

/**
 * @brief 实现MOV(移动)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(MOV) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size;
  bool HighBits = false;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size,
                                 std::forward<bool>(HighBits));

    const auto EmitSize =
        (Reg1Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
        : (Reg1Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                         : ARMEmitter::Size::i32Bit;

    if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
      auto Dst = GetRegMap(DstReg);
      auto Src = GetRegMap(SrcReg);

      if (Reg1Size == 1 && HighBits)
        ARMAssembler->ubfx(EmitSize, Dst, Src, 8, 8);
      else if (Reg1Size == 1 && !HighBits)
        ARMAssembler->uxtb(EmitSize, Dst, Src);
      else if (Reg1Size == 2 && !HighBits)
        ARMAssembler->uxth(EmitSize, Dst, Src);
      else
        ARMAssembler->mov(EmitSize, Dst, Src); // move (register)
                                               // move (to/from SP) not support
    } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
      auto Dst = GetVRegMap(DstReg);
      auto SrcDst = GetVRegMap(SrcReg);

      if (OpSize == 16) {
        if (HostSupportsSVE256 || Dst != SrcDst) {
          ARMAssembler->mov(Dst.Q(), SrcDst.Q());
        }
      } else if (OpSize == 8) {
        ARMAssembler->mov(Dst.D(), SrcDst.D());
      }
    } else
      LogMan::Msg::EFmt("Unsupported reg num for mov instr.");

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
    auto Dst = GetRegMap(DstReg);
    uint64_t Constant = 0;
    if (opd1->content.imm.type == ARM_IMM_TYPE_SYM &&
        !strcmp(opd1->content.imm.content.sym, "LVMask"))
      Constant = 0x80'40'20'10'08'04'02'01ULL;
    else
      Constant = GetARMImmMapWrapper(&opd1->content.imm);
    LoadConstant(ARMEmitter::Size::i64Bit, Dst, Constant);
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for mov instruction.");
}

/**
 * @brief 实现MVN(按位取反后移动)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(MVN) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {

    if (opd1->content.reg.num != ARM_REG_INVALID &&
        opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
      auto SrcDst = GetRegMap(ARMReg);
      auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
      uint32_t amt = opd1->content.reg.scale.imm.content.val;
      ARMAssembler->mvn(EmitSize, Dst, SrcDst, Shift, amt);

    } else if (opd1->content.reg.num != ARM_REG_INVALID &&
               opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
      auto SrcDst = GetRegMap(ARMReg);
      ARMAssembler->mvn(EmitSize, Dst, SrcDst);
    } else
      LogMan::Msg::EFmt("[arm] Unsupported reg for mvn instruction.");

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for mvn instruction.");
}

/**
 * @brief 实现AND(按位与)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(AND) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  bool HighBits = false;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size,
                               std::forward<bool>(HighBits));

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_IMM) {
    auto Dst = GetRegMap(DstReg);
    auto Src1 = GetRegMap(SrcReg);
    uint64_t Imm = GetARMImmMapWrapper(&opd2->content.imm);
    const auto IsImm =
        vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

    if (HighBits) {
      ARMAssembler->lsr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r21, Src1,
                        8);
      Src1 = ARMEmitter::Reg::r21;
    }

    if ((Imm >> 12) != 0 || !IsImm) {
      LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
      if (instr->opc == ARM_OPC_AND)
        ARMAssembler->and_(EmitSize, Dst, Src1,
                           ARMEmitter::Reg::r20); // and imm
      else
        ARMAssembler->ands(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
    } else {
      if (instr->opc == ARM_OPC_AND)
        ARMAssembler->and_(EmitSize, Dst, Src1, Imm); // and imm
      else
        ARMAssembler->ands(EmitSize, Dst, Src1, Imm); // ands imm
    }

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
             opd2->type == ARM_OPD_TYPE_REG) {

    if (opd2->content.reg.num != ARM_REG_INVALID &&
        opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;

      if (instr->opc == ARM_OPC_AND)
        ARMAssembler->and_(EmitSize, Dst, Src1, Src2, Shift, amt);
      else
        ARMAssembler->ands(EmitSize, Dst, Src1, Src2, Shift, amt);
    } else if (opd2->content.reg.num != ARM_REG_INVALID &&
               opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

      if (instr->opc == ARM_OPC_AND) {
        if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
          auto Dst = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2 = GetRegMap(Src2Reg);
          ARMAssembler->and_(EmitSize, Dst, Src1, Src2);

        } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
          auto Dst = GetVRegMap(DstReg);
          auto Vector1 = GetVRegMap(SrcReg);
          auto Vector2 = GetVRegMap(Src2Reg);

          if (HostSupportsSVE256 && Is256Bit) {
            ARMAssembler->and_(Dst.Z(), Vector1.Z(), Vector2.Z());
          } else {
            ARMAssembler->and_(Dst.Q(), Vector1.Q(), Vector2.Q());
          }
        } else
          LogMan::Msg::EFmt("Unsupported reg num for and instr.");

      } else {
        auto Dst = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2 = GetRegMap(Src2Reg);
        ARMAssembler->ands(EmitSize, Dst, Src1, Src2);
      }

    } else
      LogMan::Msg::EFmt("Unsupported reg type for and instr.");

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for and instruction.");
}

/**
 * @brief 实现ORR(按位或)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(ORR) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  bool HighBits = false;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size,
                               std::forward<bool>(HighBits));

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_IMM) {
    auto Dst = GetRegMap(DstReg);
    auto Src1 = GetRegMap(SrcReg);
    auto Imm = GetARMImmMapWrapper(&opd2->content.imm);
    const auto IsImm =
        vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

    if (HighBits) {
      ARMAssembler->lsr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r21, Src1,
                        8);
      Src1 = ARMEmitter::Reg::r21;
    }

    if ((Imm >> 12) != 0 || !IsImm) {
      LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
      ARMAssembler->orr(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
    } else {
      ARMAssembler->orr(EmitSize, Dst, Src1, Imm);
    }

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
             opd2->type == ARM_OPD_TYPE_REG) {

    if (opd2->content.reg.num != ARM_REG_INVALID &&
        opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;
      ARMAssembler->orr(EmitSize, Dst, Src1, Src2, Shift, amt);

    } else if (opd2->content.reg.num != ARM_REG_INVALID &&
               opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

      if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
        auto Dst = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2 = GetRegMap(Src2Reg);
        ARMAssembler->orr(EmitSize, Dst, Src1, Src2);

      } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
        auto Dst = GetVRegMap(DstReg);
        auto Vector1 = GetVRegMap(SrcReg);
        auto Vector2 = GetVRegMap(Src2Reg);

        if (HostSupportsSVE256 && Is256Bit) {
          ARMAssembler->orr(Dst.Z(), Vector1.Z(), Vector2.Z());
        } else {
          ARMAssembler->orr(Dst.Q(), Vector1.Q(), Vector2.Q());
        }
      } else
        LogMan::Msg::EFmt("Unsupported reg num for orr instr.");

    } else
      LogMan::Msg::EFmt("[arm] Unsupported reg for orr instr.");

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for orr instruction.");
}

/**
 * @brief 实现EOR(按位异或)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(EOR) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  bool HighBits = false;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size,
                               std::forward<bool>(HighBits));

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_IMM) {
    auto Dst = GetRegMap(DstReg);
    auto Src1 = GetRegMap(SrcReg);
    int32_t Imm = GetARMImmMapWrapper(&opd2->content.imm);
    const auto IsImm =
        vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

    if (HighBits) {
      ARMAssembler->lsr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r21, Src1,
                        8);
      Src1 = ARMEmitter::Reg::r21;
    }

    if ((Imm >> 12) != 0 || !IsImm) {
      LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
      ARMAssembler->eor(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
    } else {
      ARMAssembler->eor(EmitSize, Dst, Src1, Imm);
    }

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
             opd2->type == ARM_OPD_TYPE_REG) {

    if (opd2->content.reg.num != ARM_REG_INVALID &&
        opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;
      ARMAssembler->eor(EmitSize, Dst, Src1, Src2, Shift, amt);
    }
    if (opd2->content.reg.num != ARM_REG_INVALID &&
        opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

      if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
        auto Dst = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2 = GetRegMap(Src2Reg);
        ARMAssembler->eor(EmitSize, Dst, Src1, Src2);

      } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
        auto Dst = GetVRegMap(DstReg);
        auto Vector1 = GetVRegMap(SrcReg);
        auto Vector2 = GetVRegMap(Src2Reg);

        if (HostSupportsSVE256 && Is256Bit) {
          ARMAssembler->eor(Dst.Z(), Vector1.Z(), Vector2.Z());
        } else {
          ARMAssembler->eor(Dst.Q(), Vector1.Q(), Vector2.Q());
        }
      } else
        LogMan::Msg::EFmt("Unsupported reg num for eor instr.");

    } else
      LogMan::Msg::EFmt("[arm] Unsupported reg for eor instruction.");

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for eor instruction.");
}

/**
 * @brief 实现BIC(位清除)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(BIC) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_REG) {

    if (opd2->content.reg.num != ARM_REG_INVALID &&
        opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;

      if (instr->opc == ARM_OPC_BIC)
        ARMAssembler->bic(EmitSize, Dst, Src1, Src2, Shift, amt);
      else
        ARMAssembler->bics(EmitSize, Dst, Src1, Src2, Shift, amt);
    } else if (opd2->content.reg.num != ARM_REG_INVALID &&
               opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

      if (instr->opc == ARM_OPC_BIC) {
        if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
          auto Dst = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2 = GetRegMap(Src2Reg);
          ARMAssembler->bic(EmitSize, Dst, Src1, Src2);

        } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
          auto Dst = GetVRegMap(DstReg);
          auto Vector1 = GetVRegMap(SrcReg);
          auto Vector2 = GetVRegMap(Src2Reg);

          if (HostSupportsSVE256 && Is256Bit) {
            ARMAssembler->bic(Dst.Z(), Vector1.Z(), Vector2.Z());
          } else {
            ARMAssembler->bic(Dst.Q(), Vector1.Q(), Vector2.Q());
          }
        } else
          LogMan::Msg::EFmt("Unsupported reg num for bic instr.");
      } else {
        auto Dst = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2 = GetRegMap(Src2Reg);
        ARMAssembler->bics(EmitSize, Dst, Src1, Src2);
      }

    } else
      LogMan::Msg::IFmt("[arm] Unsupported reg for bic instruction.");

  } else
    LogMan::Msg::IFmt("[arm] Unsupported operand type for bic instruction.");
}

/**
 * @brief 实现移位指令(LSL, LSR, ASR)
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(Shift) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto Src1 = GetRegMap(ARMReg);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_IMM) {
    int32_t shift = GetARMImmMapWrapper(&opd2->content.imm);

    if (instr->opc == ARM_OPC_LSL) {
      ARMAssembler->lsl(EmitSize, Dst, Src1, shift);
      ARMAssembler->mrs((ARMEmitter::Reg::r20).X(),
                        ARMEmitter::SystemRegister::NZCV);
      ARMAssembler->ubfx(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, Src1,
                         64 - shift, 1);
      ARMAssembler->orr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20,
                        ARMEmitter::Reg::r20, ARMEmitter::Reg::r21,
                        ARMEmitter::ShiftType::LSL, 29);
      ARMAssembler->msr(ARMEmitter::SystemRegister::NZCV,
                        (ARMEmitter::Reg::r20).X());
    } else if (instr->opc == ARM_OPC_LSR) {
      ARMAssembler->lsr(EmitSize, Dst, Src1, shift);
    } else if (instr->opc == ARM_OPC_ASR) {
      ARMAssembler->asr(EmitSize, Dst, Src1, shift);
    }

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
             opd2->type == ARM_OPD_TYPE_REG) {

    if (opd2->content.reg.num != ARM_REG_INVALID) {
      ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(ARMReg);

      if (instr->opc == ARM_OPC_LSL)
        ARMAssembler->lslv(EmitSize, Dst, Src1, Src2);
      else if (instr->opc == ARM_OPC_LSR)
        ARMAssembler->lsrv(EmitSize, Dst, Src1, Src2);
      else if (instr->opc == ARM_OPC_ASR)
        ARMAssembler->asrv(EmitSize, Dst, Src1, Src2);
    } else
      LogMan::Msg::EFmt("[arm] Unsupported reg for shift instruction.");

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for shift instruction.");
}

/**
 * @brief 实现ADD(加法)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(ADD) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_IMM) {
    auto Dst = GetRegMap(DstReg);
    auto Src1 = GetRegMap(SrcReg);
    int32_t Imm = GetARMImmMapWrapper(&opd2->content.imm);

    if (Imm < 0 && !((0 - Imm) >> 12)) {
      if (instr->opc == ARM_OPC_ADD)
        ARMAssembler->sub(EmitSize, Dst, Src1, 0 - Imm);
      else
        ARMAssembler->subs(EmitSize, Dst, Src1, 0 - Imm);
    } else if (Imm > 0 && !(Imm >> 12)) {
      if (instr->opc == ARM_OPC_ADD)
        ARMAssembler->add(EmitSize, Dst, Src1, Imm); // LSL12 default false
      else
        ARMAssembler->adds(EmitSize, Dst, Src1, Imm);
    } else {
      LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
      if (instr->opc == ARM_OPC_ADD)
        ARMAssembler->add(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
      else
        ARMAssembler->adds(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
    }

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
             opd2->type == ARM_OPD_TYPE_REG) {

    if (opd2->content.reg.num != ARM_REG_INVALID &&
        opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      if (instr->opc == ARM_OPC_ADD) {
        if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
          auto Dst = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
          auto Src2 = GetRegMap(Src2Reg);
          ARMAssembler->add(EmitSize, Dst, Src1, Src2);
        } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
          auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
          auto ElementSize = instr->ElementSize;

          auto Dst = GetVRegMap(DstReg);
          auto Vector1 = GetVRegMap(SrcReg);
          auto VecReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
          auto Vector2 = GetVRegMap(VecReg);

          LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                                  ElementSize == 4 || ElementSize == 8,
                              "add Invalid size");
          const auto SubRegSize =
              ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
              : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
              : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
              : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                                 : ARMEmitter::SubRegSize::i8Bit;

          if (HostSupportsSVE256 && Is256Bit) {
            ARMAssembler->add(SubRegSize, Dst.Z(), Vector1.Z(), Vector2.Z());
          } else {
            ARMAssembler->add(SubRegSize, Dst.Q(), Vector1.Q(), Vector2.Q());
          }
        } else
          LogMan::Msg::EFmt("Unsupported reg num for add instr.");

      } else {
        auto Dst = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        ARMAssembler->adds(EmitSize, Dst, Src1, Src2);
      }
    } else if (opd2->content.reg.num != ARM_REG_INVALID &&
               opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;

      if (instr->opc == ARM_OPC_ADD)
        ARMAssembler->add(EmitSize, Dst, Src1, Src2, Shift, amt);
      else
        ARMAssembler->adds(EmitSize, Dst, Src1, Src2, Shift, amt);
    } else if (opd2->content.reg.num != ARM_REG_INVALID &&
               opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Option = GetExtendType(opd2->content.reg.scale.content.extend);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;

      if (instr->opc == ARM_OPC_ADD)
        ARMAssembler->add(EmitSize, Dst, Src1, Src2, Option, amt);
      else
        ARMAssembler->adds(EmitSize, Dst, Src1, Src2, Option, amt);
    } else
      LogMan::Msg::EFmt("[arm] Unsupported reg for add instruction.");

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for add instruction.");
}

DEF_OPC(ADC) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src1 = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto Src2 = GetRegMap(ARMReg);

    if (instr->opc == ARM_OPC_ADC)
      ARMAssembler->adc(EmitSize, Dst, Src1, Src2);
    else
      ARMAssembler->adcs(EmitSize, Dst, Src1, Src2);

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for ADC instruction.");
}

// behind cmp, sub instr
void PatternMatcher::FlipCF() {
  ARMAssembler->mrs((ARMEmitter::Reg::r20).X(),
                    ARMEmitter::SystemRegister::NZCV);
  ARMAssembler->eor(ARMEmitter::Size::i32Bit, (ARMEmitter::Reg::r20).W(),
                    (ARMEmitter::Reg::r20).W(), 0x20000000);
  ARMAssembler->msr(ARMEmitter::SystemRegister::NZCV,
                    (ARMEmitter::Reg::r20).X());
}

/**
 * @brief 实现SUB(减法)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(SUB) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_IMM) {
    auto Dst = GetRegMap(DstReg);
    auto Src1 = GetRegMap(SrcReg);
    int32_t Imm = GetARMImmMapWrapper(&opd2->content.imm);

    if ((Imm >> 12) != 0) {
      LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
      if (instr->opc == ARM_OPC_SUB) {
        ARMAssembler->sub(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
      } else {
        ARMAssembler->subs(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
        FlipCF();
      }
    } else {
      if (instr->opc == ARM_OPC_SUB) {
        ARMAssembler->sub(EmitSize, Dst, Src1, Imm); // LSL12 default false
      } else {
        ARMAssembler->subs(EmitSize, Dst, Src1, Imm);
        FlipCF();
      }
    }

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
             opd2->type == ARM_OPD_TYPE_REG) {

    if (opd2->content.reg.num != ARM_REG_INVALID &&
        opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;

      if (instr->opc == ARM_OPC_SUB) {
        ARMAssembler->sub(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else {
        ARMAssembler->subs(EmitSize, Dst, Src1, Src2, Shift, amt);
        FlipCF();
      }

    } else if (opd2->content.reg.num != ARM_REG_INVALID &&
               opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
      auto Dst = GetRegMap(DstReg);
      auto Src1 = GetRegMap(SrcReg);
      auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(Src2Reg);
      auto Option = GetExtendType(opd2->content.reg.scale.content.extend);
      uint32_t amt = opd2->content.reg.scale.imm.content.val;

      if (instr->opc == ARM_OPC_SUB) {
        ARMAssembler->sub(EmitSize, Dst, Src1, Src2, Option, amt);
      } else {
        ARMAssembler->subs(EmitSize, Dst, Src1, Src2, Option, amt);
        FlipCF();
      }

    } else if (opd2->content.reg.num != ARM_REG_INVALID &&
               opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      if (instr->opc == ARM_OPC_SUB) {
        if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
          auto Dst = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
          auto Src2 = GetRegMap(Src2Reg);
          ARMAssembler->sub(EmitSize, Dst, Src1, Src2);
        } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
          const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
          const auto ElementSize = instr->ElementSize;

          auto Dst = GetVRegMap(DstReg);
          auto Vector1 = GetVRegMap(SrcReg);
          auto VecReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
          auto Vector2 = GetVRegMap(VecReg);

          LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                                  ElementSize == 4 || ElementSize == 8,
                              "sub Invalid size");
          const auto SubRegSize =
              ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
              : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
              : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
              : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                                 : ARMEmitter::SubRegSize::i8Bit;

          if (HostSupportsSVE256 && Is256Bit) {
            ARMAssembler->sub(SubRegSize, Dst.Z(), Vector1.Z(), Vector2.Z());
          } else {
            ARMAssembler->sub(SubRegSize, Dst.Q(), Vector1.Q(), Vector2.Q());
          }
        } else
          LogMan::Msg::EFmt("Unsupported reg num for sub instr.");

      } else {
        auto Dst = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        ARMAssembler->subs(EmitSize, Dst, Src1, Src2);
        FlipCF();
      }
    }
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for sub instruction.");
}

DEF_OPC(SBC) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src1 = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto Src2 = GetRegMap(ARMReg);

    if (instr->opc == ARM_OPC_SBC)
      ARMAssembler->sbc(EmitSize, Dst, Src1, Src2);
    else
      ARMAssembler->sbcs(EmitSize, Dst, Src1, Src2);

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for SBC instruction.");
}

/**
 * @brief 实现MUL(乘法)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(MUL) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG &&
      opd2->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src1 = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto Src2 = GetRegMap(ARMReg);

    if (instr->opc == ARM_OPC_MUL)
      ARMAssembler->mul(EmitSize, Dst, Src1, Src2);
    else if (instr->opc == ARM_OPC_UMULL)
      ARMAssembler->umull(Dst.X(), Src1.W(), Src2.W());
    else if (instr->opc == ARM_OPC_SMULL)
      ARMAssembler->smull(Dst.X(), Src1.W(), Src2.W());
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for MUL instruction.");
}

/**
 * @brief 实现CLZ(计数前导零)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(CLZ) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src = GetRegMap(ARMReg);

    ARMAssembler->clz(EmitSize, Dst, Src);
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for CLZ instruction.");
}

/**
 * @brief 实现TST(测试)指令
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(TST) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;

  bool isRegSym = false;
  uint32_t Reg0Size, Reg1Size;
  bool HighBits = false;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size,
                               std::forward<bool>(HighBits));
  auto Dst = GetRegMap(ARMReg);

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src = GetRegMap(ARMReg);

    const auto EmitSize = ((Reg0Size & 0x3) || (Reg1Size & 0x3) || OpSize == 4)
                              ? ARMEmitter::Size::i32Bit
                          : ((Reg0Size == 4) || (Reg1Size == 4) || OpSize == 8)
                              ? ARMEmitter::Size::i64Bit
                              : ARMEmitter::Size::i32Bit;

    if (Reg0Size == 1 || Reg0Size == 2 || Reg1Size == 1 || Reg1Size == 2)
      isRegSym = true;

    if (!isRegSym && opd1->content.reg.num != ARM_REG_INVALID &&
        opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
      uint32_t amt = opd1->content.reg.scale.imm.content.val;
      ARMAssembler->tst(EmitSize, Dst, Src, Shift, amt);
    } else if (!isRegSym && opd1->content.reg.num != ARM_REG_INVALID &&
               opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      ARMAssembler->tst(EmitSize, Dst, Src);
    } else if (isRegSym) {
      unsigned Shift = 32 - (Reg0Size * 8);
      if (Dst == Src) {
        ARMAssembler->cmn(EmitSize, ARMEmitter::Reg::zr, Dst,
                          ARMEmitter::ShiftType::LSL, Shift);
      } else {
        ARMAssembler->and_(EmitSize, ARMEmitter::Reg::r26, Dst, Src);
        ARMAssembler->cmn(EmitSize, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26,
                          ARMEmitter::ShiftType::LSL, Shift);
      }
    } else
      LogMan::Msg::EFmt("[arm] Unsupported reg for TST instruction.");

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
    uint64_t Imm = GetARMImmMapWrapper(&opd1->content.imm);

    const auto EmitSize =
        (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
        : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                         : ARMEmitter::Size::i32Bit;

    if (Reg0Size == 1 || Reg0Size == 2)
      isRegSym = true;

    const auto IsImm =
        vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

    if (HighBits) {
      ARMAssembler->lsr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r21, Dst, 8);
      Dst = ARMEmitter::Reg::r21;
    }

    if (!isRegSym) {
      if (IsImm) {
        ARMAssembler->tst(EmitSize, Dst, Imm);
      } else {
        LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
        ARMAssembler->and_(EmitSize, ARMEmitter::Reg::r26, Dst,
                           ARMEmitter::Reg::r20);
        ARMAssembler->tst(EmitSize, ARMEmitter::Reg::r26, ARMEmitter::Reg::r26);
      }
    } else {
      unsigned Shift = 32 - (Reg0Size * 8);
      if (IsImm) {
        ARMAssembler->and_(EmitSize, ARMEmitter::Reg::r26, Dst, Imm);
      } else {
        LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
        ARMAssembler->and_(EmitSize, ARMEmitter::Reg::r26, Dst,
                           ARMEmitter::Reg::r20);
      }
      ARMAssembler->cmn(EmitSize, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26,
                        ARMEmitter::ShiftType::LSL, Shift);
    }

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for TST instruction.");
}

/**
 * @brief 实现比较指令(CMP, CMPB, CMPW, CMN)
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(COMPARE) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;

  bool isRegSym = false, preIsMem = false;
  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  /*
    lambda expression:
    [ capture_clause ] ( parameters ) -> return_type {
        function_body
    }
  */
  auto adjust_8_16_cmp =
      [=, this](bool isImm = false, uint32_t regsize = 0, uint64_t Imm = 0,
                ARMEmitter::Register Src = ARMEmitter::Reg::r20) -> void {
    const auto s32 = ARMEmitter::Size::i32Bit;
    const auto s64 = ARMEmitter::Size::i64Bit;
    unsigned Shift = 32 - (regsize * 8);
    uint32_t lsb = regsize * 8;

    if (isImm) {
      if (regsize == 2) {
        if (!preIsMem)
          ARMAssembler->uxth(s32, ARMEmitter::Reg::r27, Dst);
        else
          ARMAssembler->mov(s32, ARMEmitter::Reg::r27, Dst);
        uint16_t u16 = static_cast<uint16_t>(Imm);
        if (u16 >> 12) {
          LoadConstant(s32, (ARMEmitter::Reg::r20).X(), u16);
          ARMAssembler->sub(s32, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27,
                            ARMEmitter::Reg::r20);
        } else {
          ARMAssembler->sub(s32, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27,
                            u16);
        }
      } else {
        if (regsize == 1) {
          if (!preIsMem)
            ARMAssembler->uxtb(s32, ARMEmitter::Reg::r27, Dst);
          else
            ARMAssembler->mov(s32, ARMEmitter::Reg::r27, Dst);
          ARMAssembler->sub(s32, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27,
                            Imm);
        } else {
          LoadConstant(s32, (ARMEmitter::Reg::r20).X(), Imm);
          ARMAssembler->sub(s32, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27,
                            ARMEmitter::Reg::r20);
        }
      }
      ARMAssembler->cmn(s32, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26,
                        ARMEmitter::ShiftType::LSL, Shift);
      ARMAssembler->mrs((ARMEmitter::Reg::r20).X(),
                        ARMEmitter::SystemRegister::NZCV);
      ARMAssembler->ubfx(s64, ARMEmitter::Reg::r21, ARMEmitter::Reg::r26, lsb,
                         1);
      ARMAssembler->orr(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20,
                        ARMEmitter::Reg::r21, ARMEmitter::ShiftType::LSL, 29);
      ARMAssembler->bic(s32, ARMEmitter::Reg::r21, ARMEmitter::Reg::r27,
                        ARMEmitter::Reg::r26);
      ARMAssembler->ubfx(s64, ARMEmitter::Reg::r21, ARMEmitter::Reg::r21,
                         lsb - 1, 1);
      ARMAssembler->orr(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20,
                        ARMEmitter::Reg::r21, ARMEmitter::ShiftType::LSL, 28);
    } else {
      ARMAssembler->sub(s32, ARMEmitter::Reg::r26, Dst, Src);
      // ARMAssembler->eor(s32, ARMEmitter::Reg::r27, Dst, Src);
      ARMAssembler->cmn(s32, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26,
                        ARMEmitter::ShiftType::LSL, Shift);
      ARMAssembler->mrs((ARMEmitter::Reg::r22).X(),
                        ARMEmitter::SystemRegister::NZCV);
      ARMAssembler->ubfx(s64, ARMEmitter::Reg::r23, ARMEmitter::Reg::r26, lsb,
                         1);
      ARMAssembler->orr(s32, ARMEmitter::Reg::r22, ARMEmitter::Reg::r22,
                        ARMEmitter::Reg::r23, ARMEmitter::ShiftType::LSL, 29);
      ARMAssembler->eor(s32, ARMEmitter::Reg::r20, Dst, Src);
      ARMAssembler->eor(s32, ARMEmitter::Reg::r21, ARMEmitter::Reg::r26, Dst);
      ARMAssembler->and_(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21,
                         ARMEmitter::Reg::r20);
      ARMAssembler->ubfx(s64, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20,
                         lsb - 1, 1);
      ARMAssembler->orr(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r22,
                        ARMEmitter::Reg::r20, ARMEmitter::ShiftType::LSL, 28);
    }
    ARMAssembler->msr(ARMEmitter::SystemRegister::NZCV,
                      (ARMEmitter::Reg::r20).X());
  };

  if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
    uint64_t Imm = GetARMImmMapWrapper(&opd1->content.imm);

    const auto EmitSize =
        (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
        : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                         : ARMEmitter::Size::i32Bit;

    if (Reg0Size == 1 || Reg0Size == 2)
      isRegSym = true;

    if (!isRegSym && instr->opc == ARM_OPC_CMP) {
      if ((Imm >> 12) != 0) {
        LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
        ARMAssembler->cmp(EmitSize, Dst, (ARMEmitter::Reg::r20).X());
      } else
        ARMAssembler->cmp(EmitSize, Dst, Imm);
      FlipCF();

    } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW ||
               isRegSym) {
      if (!isRegSym && instr->opc == ARM_OPC_CMPB)
        Reg0Size = 1;
      else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
        Reg0Size = 2;
      adjust_8_16_cmp(true, Reg0Size, Imm);

    } else if (instr->opc == ARM_OPC_CMN) {
      ARMAssembler->cmn(EmitSize, Dst, Imm);
    }

  } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src = GetRegMap(ARMReg);

    const auto EmitSize = ((Reg0Size & 0x3) || (Reg1Size & 0x3) || OpSize == 4)
                              ? ARMEmitter::Size::i32Bit
                          : (Reg0Size == 4 || Reg1Size == 4 || OpSize == 8)
                              ? ARMEmitter::Size::i64Bit
                              : ARMEmitter::Size::i32Bit;

    if (Reg0Size == 1 || Reg0Size == 2 || Reg1Size == 1 || Reg1Size == 2)
      isRegSym = true;

    if (opd1->content.reg.num != ARM_REG_INVALID &&
        opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
      auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
      uint32_t amt = opd1->content.reg.scale.imm.content.val;

      if (!isRegSym && instr->opc == ARM_OPC_CMP) {
        ARMAssembler->cmp(EmitSize, Dst, Src, Shift, amt);
        FlipCF();
      } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW ||
                 isRegSym) {
        if (!isRegSym && instr->opc == ARM_OPC_CMPB)
          Reg0Size = 1;
        else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
          Reg0Size = 2;
        adjust_8_16_cmp(false, Reg0Size, 0, Src);
      } else {
        ARMAssembler->cmn(EmitSize, Dst, Src, Shift, amt);
      }

    } else if (opd1->content.reg.num != ARM_REG_INVALID &&
               opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
      auto Option = GetExtendType(opd1->content.reg.scale.content.extend);
      uint32_t Shift = opd1->content.reg.scale.imm.content.val;

      if (!isRegSym && instr->opc == ARM_OPC_CMP) {
        ARMAssembler->cmp(EmitSize, Dst, Src, Option, Shift);
        FlipCF();
      } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW ||
                 isRegSym) {
        if (!isRegSym && instr->opc == ARM_OPC_CMPB)
          Reg0Size = 1;
        else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
          Reg0Size = 2;
        adjust_8_16_cmp(false, Reg0Size, 0, Src);
      } else {
        ARMAssembler->cmn(EmitSize, Dst, Src, Option, Shift);
      }

    } else if (opd1->content.reg.num != ARM_REG_INVALID &&
               opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
      if (!isRegSym && instr->opc == ARM_OPC_CMP) {
        ARMAssembler->cmp(EmitSize, Dst, Src);
        FlipCF();
      } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW ||
                 isRegSym) {
        if (!isRegSym && instr->opc == ARM_OPC_CMPB)
          Reg0Size = 1;
        else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
          Reg0Size = 2;
        adjust_8_16_cmp(false, Reg0Size, 0, Src);
      } else {
        ARMAssembler->cmn(EmitSize, Dst, Src);
      }
    }

  } else
    LogMan::Msg::EFmt(
        "[arm] Unsupported operand type for compare instruction.");
}

/**
 * @brief 实现条件选择指令(CSEL, CSET)
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param rrule 指向RuleRecord结构的指针
 */
DEF_OPC(CSEX) {
  ARMOperand *opd0 = &instr->opd[0];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4)  ? ARMEmitter::Size::i32Bit
      : (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit
                                       : ARMEmitter::Size::i32Bit;

  if (instr->opd_num == 1 && opd0->type == ARM_OPD_TYPE_REG) {
    ARMAssembler->cset(EmitSize, Dst, MapBranchCC(instr->cc));
  } else if (instr->opd_num == 3 && opd0->type == ARM_OPD_TYPE_REG) {
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];

    if (opd2->content.reg.num != ARM_REG_INVALID) {
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
      auto Src1 = GetRegMap(ARMReg);
      ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(ARMReg);

      ARMAssembler->csel(EmitSize, Dst, Src1, Src2, MapBranchCC(instr->cc));
    } else
      LogMan::Msg::EFmt("[arm] Unsupported opd for csex instruction.");

  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for csex instruction.");
}

DEF_OPC(BFXIL) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  ARMOperand *opd3 = &instr->opd[3];
  const uint8_t OpSize = instr->OpSize;

  const auto EmitSize =
      OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  const auto Dst = GetRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  const auto Src = GetRegMap(ARMReg);
  uint32_t lsb = GetARMImmMapWrapper(&opd2->content.imm);
  uint32_t width = GetARMImmMapWrapper(&opd3->content.imm);

  // If Dst and SrcDst match then this turns in to a single instruction.
  ARMAssembler->bfxil(EmitSize, Dst, Src, lsb, width);
}

DEF_OPC(B) {
  ARMOperand *opd = &instr->opd[0];
  ARMConditionCode cond = instr->cc;
  const auto EmitSize = ARMEmitter::Size::i64Bit;

  // get new rip
  uint64_t target, fallthrough;
  GetLabelMap(opd->content.imm.content.sym, &target, &fallthrough);
  // this->TrueNewRip = fallthrough + target;
  // this->FalseNewRip = fallthrough;
  FEXCore::ARMEmitter::Register TMPReg1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMPReg2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::Register RIPReg(GPRMappedIdx[16]);
  {
    // store nzcv to eflags before return;
    StoreNZCV();
    if (cond == ARM_CC_LS) {
      // b.ls (CF=0 || ZF=1)
      ARMAssembler->mov(TMPReg1.W(), 1);
      ARMAssembler->cset(EmitSize, TMPReg2.X(), MapBranchCC(ARM_CC_CS));
      ARMAssembler->csel(EmitSize, TMPReg1.X(), TMPReg1.X(), TMPReg2.X(),
                         MapBranchCC(ARM_CC_EQ));
      ARMAssembler->cmp(EmitSize, TMPReg1, 0);
      LoadConstant(EmitSize, TMPReg1, fallthrough + target);
      LoadConstant(EmitSize, RIPReg, fallthrough);
      ARMAssembler->csel(EmitSize, RIPReg, TMPReg1, RIPReg,
                         ARMEmitter::Condition::CC_NE);
    } else if (cond == ARM_CC_HI) {
      // b.hi (CF=1 && ZF=0)
      ARMAssembler->cset(EmitSize, TMPReg1.X(), MapBranchCC(ARM_CC_CC));
      ARMAssembler->csel(EmitSize, TMPReg1.X(), TMPReg1.X(),
                         (ARMEmitter::Reg::zr).X(), MapBranchCC(ARM_CC_NE));
      ARMAssembler->cmp(EmitSize, TMPReg1, 0);

      LoadConstant(EmitSize, TMPReg1, fallthrough + target);
      LoadConstant(EmitSize, RIPReg, fallthrough);
      ARMAssembler->csel(EmitSize, RIPReg, TMPReg1, RIPReg,
                         ARMEmitter::Condition::CC_NE);
    } else {
      LoadConstant(EmitSize, TMPReg1, fallthrough + target);
      LoadConstant(EmitSize, RIPReg, fallthrough);
      ARMAssembler->csel(EmitSize, RIPReg, TMPReg1, RIPReg, MapBranchCC(cond));
    }
  }
}

DEF_OPC(CBNZ) {
  ARMOperand *opd = &instr->opd[0];
  uint8_t OpSize = instr->OpSize;

  uint32_t Reg0Size;
  auto ARMReg = GetGuestRegMap(opd->content.reg.num, Reg0Size);
  auto Src = GetRegMap(ARMReg);

  // get new rip
  uint64_t target, fallthrough;
  GetLabelMap(opd->content.imm.content.sym, &target, &fallthrough);
  // this->TrueNewRip = fallthrough + target;
  // this->FalseNewRip = fallthrough;

  const auto EmitSize =
      OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

  // auto TrueTargetLabel =
  //     &JumpTargets2.try_emplace(this->TrueNewRip).first->second;

  // if (instr->opc == ARM_OPC_CBNZ)
  //   ARMAssembler->cbnz(EmitSize, Src, TrueTargetLabel);
  // else
  //   cbz(EmitSize, Src, TrueTargetLabel);
}

DEF_OPC(SET_JUMP) {
  ARMOperand *opd = &instr->opd[0];
  uint8_t OpSize = instr->OpSize;
  uint64_t target, fallthrough, Mask = ~0ULL;

  if (OpSize == 4) {
    Mask = 0xFFFF'FFFFULL;
  }

  if (opd->type == ARM_OPD_TYPE_IMM) {
    GetLabelMap(opd->content.imm.content.sym, &target, &fallthrough);

    LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20,
                 fallthrough & Mask);
    LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, target);
    ARMAssembler->add(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20,
                      ARMEmitter::Reg::r21, ARMEmitter::Reg::r20);
    // Set New RIP Reg
    // RipReg = ARM_REG_R20;
  } else if (opd->type == ARM_OPD_TYPE_REG) {
    uint32_t Reg0Size;
    auto Src = GetGuestRegMap(opd->content.reg.num, Reg0Size);
    // Set New RIP Reg
    // RipReg = Src;
  }

  // operand type is reg don't need to be processed.
}

DEF_OPC(SET_CALL) {
  ARMOperand *opd = &instr->opd[0];
  uint8_t OpSize = instr->OpSize;
  uint64_t target, fallthrough, Mask = ~0ULL;

  if (OpSize == 4) {
    Mask = 0xFFFF'FFFFULL;
  }
  FEXCore::ARMEmitter::Register TMPReg1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMPReg2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::Register RSPReg(GPRTempIdx[X86_GPR::RSP]);
  FEXCore::ARMEmitter::Register RIPReg(GPRMappedIdx[X86_GPR::RIP]);
  auto MemSrc = ARMEmitter::ExtendedMemOperand(
      RSPReg.X(), ARMEmitter::IndexType::PRE, -0x8);

  if (instr->opd_num && opd->type == ARM_OPD_TYPE_IMM) {
    GetLabelMap(opd->content.imm.content.sym, &target, &fallthrough);
    LoadConstant(ARMEmitter::Size::i64Bit, RIPReg.X(),
                 (fallthrough + target) & Mask);

    LoadConstant(ARMEmitter::Size::i64Bit, TMPReg1.X(), fallthrough & Mask);

    ARMAssembler->str((TMPReg1).X(), MemSrc);
  } else if (instr->opd_num && opd->type == ARM_OPD_TYPE_REG) {
    LoadConstant(ARMEmitter::Size::i64Bit, TMPReg1.X(),
                 rrule->target_pc & Mask);
    ARMAssembler->str((TMPReg1).X(), MemSrc);
    // Set New RIP Reg
    uint32_t Reg0Size;
    auto Src = GetGuestRegMap(opd->content.reg.num, Reg0Size);
    ARMAssembler->mov(ARMEmitter::Size::i64Bit, RIPReg, Src);
  } else if (!instr->opd_num) { // only push
    LoadConstant(ARMEmitter::Size::i64Bit, TMPReg1.X(),
                 rrule->target_pc & Mask);
    ARMAssembler->str(TMPReg1.X(), MemSrc);
  }
}

DEF_OPC(PC_L) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;
  uint64_t target, fallthrough, Mask = ~0ULL;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);

  if (opd1->type == ARM_OPD_TYPE_REG) {
    ARMOperand *opd2 = &instr->opd[2];
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto RIPDst = GetRegMap(ARMReg);

    GetLabelMap(opd2->content.imm.content.sym, &target, &fallthrough);
    // 32bit this isn't RIP relative but instead absolute
    int64_t s = static_cast<int64_t>(static_cast<int32_t>(target));
    LoadConstant(ARMEmitter::Size::i64Bit, RIPDst.X(),
                 (fallthrough + s) & Mask);

    auto MemSrc = ARMEmitter::ExtendedMemOperand(
        RIPDst.X(), ARMEmitter::IndexType::OFFSET, 0x0);
    if (instr->opc == ARM_OPC_PC_LB)
      ARMAssembler->ldrb(Dst, MemSrc);
    else if (instr->opc == ARM_OPC_PC_LW)
      ARMAssembler->ldrh(Dst, MemSrc);
    else if (Reg0Size & 0x3 || OpSize == 4)
      ARMAssembler->ldr(Dst.W(), MemSrc);
    else
      ARMAssembler->ldr(Dst.X(), MemSrc);

  } else if (opd1->type == ARM_OPD_TYPE_IMM) {
    GetLabelMap(opd1->content.imm.content.sym, &target, &fallthrough);
    // 32bit this isn't RIP relative but instead absolute
    int64_t s = static_cast<int64_t>(static_cast<int32_t>(target));
    LoadConstant(ARMEmitter::Size::i64Bit, Dst.X(), (fallthrough + s) & Mask);
  }
}

DEF_OPC(PC_S) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  uint8_t OpSize = instr->OpSize;
  uint64_t target, fallthrough, Mask = ~0ULL;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Src = GetRegMap(ARMReg);

  if (opd1->type == ARM_OPD_TYPE_REG) {
    ARMOperand *opd2 = &instr->opd[2];
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto RIPDst = GetRegMap(ARMReg);

    GetLabelMap(opd2->content.imm.content.sym, &target, &fallthrough);
    LoadConstant(ARMEmitter::Size::i64Bit, RIPDst.X(),
                 (fallthrough + target) & Mask);

    auto MemSrc = ARMEmitter::ExtendedMemOperand(
        RIPDst.X(), ARMEmitter::IndexType::OFFSET, 0x0);
    if (instr->opc == ARM_OPC_PC_SB)
      ARMAssembler->strb(Src, MemSrc);
    else if (instr->opc == ARM_OPC_PC_SW)
      ARMAssembler->strh(Src, MemSrc);
    else if (Reg0Size & 0x3 || OpSize == 4)
      ARMAssembler->str(Src.W(), MemSrc);
    else
      ARMAssembler->str(Src.X(), MemSrc);
  }
}

/*
  SIMD&FP
*/
DEF_OPC(ADDP) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  const auto OpSize = instr->OpSize;

  const auto ElementSize = instr->ElementSize;
  const auto IsScalar = OpSize == 8;
  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto VectorLower = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
  auto VectorUpper = GetVRegMap(ARMReg);

  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);
  FEXCore::ARMEmitter::PRegister PRED_TMP_16B(6); // p6
  FEXCore::ARMEmitter::PRegister PRED_TMP_32B(7); // p7

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8,
                      "addp Invalid size");
  const auto SubRegSize = ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
                          : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
                          : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
                          : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                                             : ARMEmitter::SubRegSize::i8Bit;

  if (HostSupportsSVE256 && Is256Bit) {
    const auto Pred = PRED_TMP_32B.Merging();

    // SVE ADDP is a destructive operation, so we need a temporary
    ARMAssembler->movprfx(VTMP1.Z(), VectorLower.Z());

    // Unlike Adv. SIMD's version of ADDP, which acts like it concats the
    // upper vector onto the end of the lower vector and then performs
    // pairwise addition, the SVE version actually interleaves the
    // results of the pairwise addition (gross!), so we need to undo that.
    ARMAssembler->addp(SubRegSize, VTMP1.Z(), Pred, VTMP1.Z(), VectorUpper.Z());
    ARMAssembler->uzp1(SubRegSize, Dst.Z(), VTMP1.Z(), VTMP1.Z());
    ARMAssembler->uzp2(SubRegSize, VTMP2.Z(), VTMP1.Z(), VTMP1.Z());

    // Merge upper half with lower half.
    ARMAssembler->splice<ARMEmitter::OpType::Destructive>(
        ARMEmitter::SubRegSize::i64Bit, Dst.Z(), PRED_TMP_16B, Dst.Z(),
        VTMP2.Z());
  } else {
    if (IsScalar) {
      ARMAssembler->addp(SubRegSize, Dst.D(), VectorLower.D(), VectorUpper.D());
    } else {
      ARMAssembler->addp(SubRegSize, Dst.Q(), VectorLower.Q(), VectorUpper.Q());
    }
  }
}

DEF_OPC(CMEQ) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  const auto OpSize = instr->OpSize;

  const auto ElementSize = instr->ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(DstReg);
  auto Vec1Reg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto Vector1 = GetVRegMap(Vec1Reg);
  auto Vec2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
  auto Vector2 = GetVRegMap(Vec2Reg);

  FEXCore::ARMEmitter::Register TMP1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMP2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);
  FEXCore::ARMEmitter::PRegister PRED_TMP_32B(7); // p7

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8 ||
                          ElementSize == 16,
                      "cmeq Invalid size");
  const auto SubRegSize = ARMEmitter::ToVectorSizePair(
      ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
      : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
      : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
      : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                         : ARMEmitter::SubRegSize::i128Bit);

  if (HostSupportsSVE256 && Is256Bit) {
    // FIXME: We should rework this op to avoid the NZCV spill/fill dance.
    ARMAssembler->mrs(TMP1, ARMEmitter::SystemRegister::NZCV);

    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = ARMEmitter::PReg::p0;

    // General idea is to compare for equality, not the equal vals
    // from one of the registers, then or both together to make the
    // relevant equal entries all 1s.
    ARMAssembler->cmpeq(SubRegSize.Vector, ComparePred, Mask, Vector1.Z(),
                        Vector2.Z());
    ARMAssembler->not_(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(),
                       Vector1.Z());
    ARMAssembler->movprfx(SubRegSize.Vector, Dst.Z(), ComparePred.Zeroing(),
                          Vector1.Z());
    ARMAssembler->orr(SubRegSize.Vector, Dst.Z(), ComparePred.Merging(),
                      Dst.Z(), VTMP1.Z());

    // Restore NZCV
    ARMAssembler->msr(ARMEmitter::SystemRegister::NZCV, TMP1);
  } else {
    if (IsScalar) {
      ARMAssembler->cmeq(SubRegSize.Scalar, Dst, Vector1, Vector2);
    } else {
      ARMAssembler->cmeq(SubRegSize.Vector, Dst.Q(), Vector1.Q(), Vector2.Q());
    }
  }
}

DEF_OPC(CMGT) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  const auto OpSize = instr->OpSize;

  const auto ElementSize = instr->ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(DstReg);
  auto Vec1Reg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto Vector1 = GetVRegMap(Vec1Reg);
  auto Vec2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
  auto Vector2 = GetVRegMap(Vec2Reg);

  FEXCore::ARMEmitter::Register TMP1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMP2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);
  FEXCore::ARMEmitter::PRegister PRED_TMP_32B(7); // p7

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8 ||
                          ElementSize == 16,
                      "cmgt Invalid size");
  const auto SubRegSize = ARMEmitter::ToVectorSizePair(
      ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
      : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
      : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
      : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                         : ARMEmitter::SubRegSize::i128Bit);

  if (HostSupportsSVE256 && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = ARMEmitter::PReg::p0;

    // FIXME: We should rework this op to avoid the NZCV spill/fill dance.
    ARMAssembler->mrs(TMP1, ARMEmitter::SystemRegister::NZCV);

    // General idea is to compare for greater-than, bitwise NOT
    // the valid values, then ORR the NOTed values with the original
    // values to form entries that are all 1s.
    ARMAssembler->cmpgt(SubRegSize.Vector, ComparePred, Mask, Vector1.Z(),
                        Vector2.Z());
    ARMAssembler->not_(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(),
                       Vector1.Z());
    ARMAssembler->movprfx(SubRegSize.Vector, Dst.Z(), ComparePred.Zeroing(),
                          Vector1.Z());
    ARMAssembler->orr(SubRegSize.Vector, Dst.Z(), ComparePred.Merging(),
                      Dst.Z(), VTMP1.Z());

    // Restore NZCV
    ARMAssembler->msr(ARMEmitter::SystemRegister::NZCV, TMP1);
  } else {
    if (IsScalar) {
      ARMAssembler->cmgt(SubRegSize.Scalar, Dst, Vector1, Vector2);
    } else {
      ARMAssembler->cmgt(SubRegSize.Vector, Dst.Q(), Vector1.Q(), Vector2.Q());
    }
  }
}

DEF_OPC(CMLT) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  const auto OpSize = instr->OpSize;

  const auto ElementSize = instr->ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;

  uint32_t Reg0Size, Reg1Size;
  auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(DstReg);
  auto VecReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto Vector = GetVRegMap(VecReg);

  FEXCore::ARMEmitter::Register TMP1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMP2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);
  FEXCore::ARMEmitter::PRegister PRED_TMP_32B(7); // p7

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8 ||
                          ElementSize == 16,
                      "cmlt Invalid size");
  const auto SubRegSize = ARMEmitter::ToVectorSizePair(
      ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
      : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
      : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
      : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                         : ARMEmitter::SubRegSize::i128Bit);

  if (HostSupportsSVE256 && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = ARMEmitter::PReg::p0;

    // FIXME: We should rework this op to avoid the NZCV spill/fill dance.
    ARMAssembler->mrs(TMP1, ARMEmitter::SystemRegister::NZCV);

    // Ensure no junk is in the temp (important for ensuring
    // non less-than values remain as zero).
    ARMAssembler->mov_imm(ARMEmitter::SubRegSize::i64Bit, VTMP1.Z(), 0);
    ARMAssembler->cmplt(SubRegSize.Vector, ComparePred, Mask, Vector.Z(), 0);
    ARMAssembler->not_(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(),
                       Vector.Z());
    ARMAssembler->orr(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(),
                      VTMP1.Z(), Vector.Z());
    ARMAssembler->mov(Dst.Z(), VTMP1.Z());

    // Restore NZCV
    ARMAssembler->msr(ARMEmitter::SystemRegister::NZCV, TMP1);
  } else {
    if (IsScalar) {
      ARMAssembler->cmlt(SubRegSize.Scalar, Dst, Vector);
    } else {
      ARMAssembler->cmlt(SubRegSize.Vector, Dst.Q(), Vector.Q());
    }
  }
}

DEF_OPC(DUP) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  const auto OpSize = instr->OpSize;

  const auto Index = opd1->content.reg.Index;
  const auto ElementSize = instr->ElementSize;
  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
  const auto Is128Bit = OpSize == XMM_SSE_REG_SIZE;

  FEXCore::ARMEmitter::Register TMP1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMP2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(ARMReg);

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8,
                      "dup Invalid size");
  const auto SubRegSize = ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
                          : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
                          : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
                          : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                                             : ARMEmitter::SubRegSize::i128Bit;

  if (opd1->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

    if (ARMReg >= ARM_REG_R0 && ARMReg <= ARM_REG_R31) {
      const auto Src = GetRegMap(ARMReg);
      if (HostSupportsSVE256 && Is256Bit) {
        ARMAssembler->dup(SubRegSize, Dst.Z(), Src);
      } else {
        ARMAssembler->dup(SubRegSize, Dst.Q(), Src);
      }
    } else if (ARMReg >= ARM_REG_V0 && ARMReg <= ARM_REG_V31) {
      const auto Vector = GetVRegMap(ARMReg);
      if (HostSupportsSVE256 && Is256Bit) {
        ARMAssembler->dup(SubRegSize, Dst.Z(), Vector.Z(), Index);
      } else {
        if (Is128Bit)
          ARMAssembler->dup(SubRegSize, Dst.Q(), Vector.Q(), Index);
        else
          ARMAssembler->dup(SubRegSize, Dst.D(), Vector.D(), Index);
      }
    }
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for dup instruction.");
}

DEF_OPC(EXT) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  ARMOperand *opd3 = &instr->opd[3];
  const auto OpSize = instr->OpSize;

  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
  const auto ElementSize = instr->ElementSize;
  auto Index = GetARMImmMapWrapper(&opd3->content.imm);

  // AArch64 ext op has bit arrangement as [Vm:Vn] so arguments need to be
  // swapped
  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
  auto UpperBits = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto LowerBits = GetVRegMap(ARMReg);

  FEXCore::ARMEmitter::Register TMP1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMP2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);

  if (Index >= OpSize) {
    // Upper bits have moved in to the lower bits
    LowerBits = UpperBits;

    // Upper bits are all now zero
    UpperBits = VTMP1;
    ARMAssembler->movi(ARMEmitter::SubRegSize::i64Bit, VTMP1.Q(), 0);
    Index -= OpSize;
  }

  const auto CopyFromByte = Index * ElementSize;

  if (HostSupportsSVE256 && Is256Bit) {
    if (Dst == LowerBits) {
      // Trivial case where we don't need to do any moves
      ARMAssembler->ext<ARMEmitter::OpType::Destructive>(
          Dst.Z(), Dst.Z(), UpperBits.Z(), CopyFromByte);
    } else if (Dst == UpperBits) {
      ARMAssembler->movprfx(VTMP2.Z(), LowerBits.Z());
      ARMAssembler->ext<ARMEmitter::OpType::Destructive>(
          VTMP2.Z(), VTMP2.Z(), UpperBits.Z(), CopyFromByte);
      ARMAssembler->mov(Dst.Z(), VTMP2.Z());
    } else {
      // No registers alias the destination, so we can safely move into it.
      ARMAssembler->movprfx(Dst.Z(), LowerBits.Z());
      ARMAssembler->ext<ARMEmitter::OpType::Destructive>(
          Dst.Z(), Dst.Z(), UpperBits.Z(), CopyFromByte);
    }
  } else {
    if (OpSize == 8) {
      ARMAssembler->ext(Dst.D(), LowerBits.D(), UpperBits.D(), CopyFromByte);
    } else {
      ARMAssembler->ext(Dst.Q(), LowerBits.Q(), UpperBits.Q(), CopyFromByte);
    }
  }
}

DEF_OPC(FMOV) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  auto ElementSize = instr->ElementSize;

  FEXCore::ARMEmitter::Register TMP1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMP2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  if (ARMReg >= ARM_REG_R0 && ARMReg <= ARM_REG_R31) {
    instr->opc = ARM_OPC_UMOV;
    Opc_UMOV(instr, rrule);
    return;
  }
  auto Dst = GetVRegMap(ARMReg);

  if (opd1->type == ARM_OPD_TYPE_REG) {
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src = GetRegMap(ARMReg);

    if (Reg1Size && !ElementSize)
      ElementSize = 1 << (Reg1Size - 1);

    switch (ElementSize) {
    case 1:
      ARMAssembler->uxtb(ARMEmitter::Size::i32Bit, TMP1, Src);
      ARMAssembler->fmov(ARMEmitter::Size::i32Bit, Dst.S(), TMP1);
      break;
    case 2:
      ARMAssembler->uxth(ARMEmitter::Size::i32Bit, TMP1, Src);
      ARMAssembler->fmov(ARMEmitter::Size::i32Bit, Dst.S(), TMP1);
      break;
    case 4:
      ARMAssembler->fmov(ARMEmitter::Size::i32Bit, Dst.S(), Src);
      break;
    case 8:
      ARMAssembler->fmov(ARMEmitter::Size::i64Bit, Dst.D(), Src);
      break;
    default:
      LOGMAN_MSG_A_FMT("Unknown castGPR element size: {}", ElementSize);
    }
  } else
    LogMan::Msg::EFmt("[arm] Unsupported operand type for fmov instruction.");
}

DEF_OPC(INS) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  auto ElementSize = instr->ElementSize;

  uint32_t Reg0Size, Reg1Size;
  auto GuestReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto guest = GetVRegMap(GuestReg);
  auto gregOffs = opd0->content.reg.Index;
  auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto host = GetVRegMap(SrcReg);
  auto hregOffs = opd1->content.reg.Index;

  switch (ElementSize) {
  case 1:
    ARMAssembler->ins(ARMEmitter::SubRegSize::i8Bit, guest, gregOffs, host,
                      hregOffs);
    break;
  case 2:
    ARMAssembler->ins(ARMEmitter::SubRegSize::i16Bit, guest, gregOffs, host,
                      hregOffs);
    break;
  case 4:
    // XXX: This had a bug with insert of size 16bit
    ARMAssembler->ins(ARMEmitter::SubRegSize::i32Bit, guest, gregOffs, host,
                      hregOffs);
    break;
  case 8:
    // XXX: This had a bug with insert of size 16bit
    ARMAssembler->ins(ARMEmitter::SubRegSize::i64Bit, guest, gregOffs, host,
                      hregOffs);
    break;
  default:
    LOGMAN_MSG_A_FMT("Unhandled INS FPR size: {}", ElementSize);
    break;
  }
}

DEF_OPC(UMOV) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  auto ElementSize = instr->ElementSize;

  constexpr auto SSERegBitSize = XMM_SSE_REG_SIZE * 8;
  const auto ElementSizeBits = ElementSize * 8;
  const auto Offset = ElementSizeBits * opd1->content.reg.Index;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto Vector = GetVRegMap(ARMReg);

  if (Reg0Size && !ElementSize)
    ElementSize = 1 << (Reg0Size - 1);

  const auto PerformMove = [&](const ARMEmitter::VRegister reg, int index) {
    switch (ElementSize) {
    case 1:
      ARMAssembler->umov<ARMEmitter::SubRegSize::i8Bit>(Dst, Vector, index);
      break;
    case 2:
      ARMAssembler->umov<ARMEmitter::SubRegSize::i16Bit>(Dst, Vector, index);
      break;
    case 4:
      ARMAssembler->umov<ARMEmitter::SubRegSize::i32Bit>(Dst, Vector, index);
      break;
    case 8:
      ARMAssembler->umov<ARMEmitter::SubRegSize::i64Bit>(Dst, Vector, index);
      break;
    default:
      LOGMAN_MSG_A_FMT("Unhandled ExtractElementSize: {}", ElementSize);
      break;
    }
  };

  if (Offset < SSERegBitSize) {
    // Desired data lies within the lower 128-bit lane, so we
    // can treat the operation as a 128-bit operation, even
    // when acting on larger register sizes.
    PerformMove(Vector, opd1->content.reg.Index);
  } else
    LogMan::Msg::EFmt("[ARM] Offset >= SSERegBitSize for umov instr.");
}

DEF_OPC(LD1) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  const auto OpSize = instr->OpSize;

  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;
  const auto ElementSize = instr->ElementSize;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto MemReg = GetRegMap(ARMReg);

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8 ||
                          ElementSize == 16,
                      "Invalid element size");

  if (Is256Bit) {
    LOGMAN_MSG_A_FMT("Unsupported 256-bit VLoadVectorElement");
  } else {
    switch (ElementSize) {
    case 1:
      ARMAssembler->ld1<ARMEmitter::SubRegSize::i8Bit>(
          Dst.Q(), opd0->content.reg.Index, MemReg);
      break;
    case 2:
      ARMAssembler->ld1<ARMEmitter::SubRegSize::i16Bit>(
          Dst.Q(), opd0->content.reg.Index, MemReg);
      break;
    case 4:
      ARMAssembler->ld1<ARMEmitter::SubRegSize::i32Bit>(
          Dst.Q(), opd0->content.reg.Index, MemReg);
      break;
    case 8:
      ARMAssembler->ld1<ARMEmitter::SubRegSize::i64Bit>(
          Dst.Q(), opd0->content.reg.Index, MemReg);
      break;
    case 16:
      ARMAssembler->ldr(Dst.Q(), MemReg);
      break;
    default:
      LOGMAN_MSG_A_FMT("Unhandled {} size: {}", __func__, ElementSize);
      return;
    }
  }
}

DEF_OPC(SQXTUN) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  const auto OpSize = instr->OpSize;

  const auto ElementSize = instr->ElementSize;
  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;

  uint32_t Reg0Size, Reg1Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(ARMReg);
  auto VectorLower = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto VectorUpper = GetVRegMap(ARMReg);

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8,
                      "aqxtun Invalid size");
  const auto SubRegSize = ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
                          : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
                          : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
                          : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                                             : ARMEmitter::SubRegSize::i8Bit;

  FEXCore::ARMEmitter::Register TMP1(GPRTempIdx[0]);
  FEXCore::ARMEmitter::Register TMP2(GPRTempIdx[1]);
  FEXCore::ARMEmitter::VRegister VTMP1(XMMTempIdx[0]);
  FEXCore::ARMEmitter::VRegister VTMP2(XMMTempIdx[1]);
  FEXCore::ARMEmitter::PRegister PRED_TMP_16B(6); // p6

  if (HostSupportsSVE256 && Is256Bit) {
    // This combines the SVE versions of VSQXTUN/VSQXTUN2.
    // Upper VSQXTUN2 handling.
    // Doing upper first to ensure it doesn't get overwritten by lower
    // calculation.
    const auto Mask = PRED_TMP_16B;

    ARMAssembler->sqxtunb(SubRegSize, VTMP2.Z(), VectorUpper.Z());
    ARMAssembler->uzp1(SubRegSize, VTMP2.Z(), VTMP2.Z(), VTMP2.Z());

    // Look at those implementations for details about this.
    // Lower VSQXTUN handling.
    ARMAssembler->sqxtunb(SubRegSize, Dst.Z(), VectorLower.Z());
    ARMAssembler->uzp1(SubRegSize, Dst.Z(), Dst.Z(), Dst.Z());

    // Merge.
    ARMAssembler->splice<ARMEmitter::OpType::Destructive>(
        SubRegSize, Dst.Z(), Mask, Dst.Z(), VTMP2.Z());
  } else {
    if (OpSize == 8) {
      ARMAssembler->zip1(ARMEmitter::SubRegSize::i64Bit, Dst.Q(),
                         VectorLower.Q(), VectorUpper.Q());
      ARMAssembler->sqxtun(SubRegSize, Dst, Dst);
    } else {
      if (Dst == VectorUpper) {
        // If the destination overlaps the upper then we need to move it
        // temporarily.
        ARMAssembler->mov(VTMP1.Q(), VectorUpper.Q());
        VectorUpper = VTMP1;
      }
      ARMAssembler->sqxtun(SubRegSize, Dst, VectorLower);
      ARMAssembler->sqxtun2(SubRegSize, Dst, VectorUpper);
    }
  }
}

DEF_OPC(ZIP) {
  ARMOperand *opd0 = &instr->opd[0];
  ARMOperand *opd1 = &instr->opd[1];
  ARMOperand *opd2 = &instr->opd[2];
  const auto OpSize = instr->OpSize;

  const auto ElementSize = instr->ElementSize;
  const auto Is256Bit = OpSize == XMM_AVX_REG_SIZE;

  uint32_t Reg0Size, Reg1Size, Reg2Size;
  auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
  auto Dst = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
  auto VectorLower = GetVRegMap(ARMReg);
  ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
  auto VectorUpper = GetVRegMap(ARMReg);

  LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                          ElementSize == 4 || ElementSize == 8,
                      "zip Invalid size");
  const auto SubRegSize = ElementSize == 1   ? ARMEmitter::SubRegSize::i8Bit
                          : ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit
                          : ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit
                          : ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit
                                             : ARMEmitter::SubRegSize::i8Bit;

  if (HostSupportsSVE256 && Is256Bit) {
    if (instr->opc == ARM_OPC_ZIP1)
      ARMAssembler->zip1(SubRegSize, Dst.Z(), VectorLower.Z(), VectorUpper.Z());
    else
      ARMAssembler->zip2(SubRegSize, Dst.Z(), VectorLower.Z(), VectorUpper.Z());
  } else {
    if (OpSize == 8) {
      if (instr->opc == ARM_OPC_ZIP1)
        ARMAssembler->zip1(SubRegSize, Dst.D(), VectorLower.D(),
                           VectorUpper.D());
      else
        ARMAssembler->zip2(SubRegSize, Dst.D(), VectorLower.D(),
                           VectorUpper.D());
    } else {
      if (instr->opc == ARM_OPC_ZIP1)
        ARMAssembler->zip1(SubRegSize, Dst.Q(), VectorLower.Q(),
                           VectorUpper.Q());
      else
        ARMAssembler->zip2(SubRegSize, Dst.Q(), VectorLower.Q(),
                           VectorUpper.Q());
    }
  }
}

void PatternMatcher::assemble_arm_instr(ARMInstruction *instr,
                                        RuleRecord *rrule) {
  LogMan::Msg::IFmt("ARM instr in the asm: {}.", get_arm_instr_opc(instr->opc));

  switch (instr->opc) {
  case ARM_OPC_LDRB:
  case ARM_OPC_LDRSB:
  case ARM_OPC_LDRH:
  case ARM_OPC_LDRSH:
  case ARM_OPC_LDAR:
  case ARM_OPC_LDR:
    Opc_LDR(instr, rrule);
    break;
  case ARM_OPC_LDP:
    Opc_LDP(instr, rrule);
    break;
  case ARM_OPC_STR:
  case ARM_OPC_STRB:
  case ARM_OPC_STRH:
    Opc_STR(instr, rrule);
    break;
  case ARM_OPC_STP:
    Opc_STP(instr, rrule);
    break;
  case ARM_OPC_SXTW:
    Opc_SXTW(instr, rrule);
    break;
  case ARM_OPC_MOV:
    Opc_MOV(instr, rrule);
    break;
  case ARM_OPC_MVN:
    Opc_MVN(instr, rrule);
    break;
  case ARM_OPC_AND:
  case ARM_OPC_ANDS:
    Opc_AND(instr, rrule);
    break;
  case ARM_OPC_ORR:
    Opc_ORR(instr, rrule);
    break;
  case ARM_OPC_EOR:
    Opc_EOR(instr, rrule);
    break;
  case ARM_OPC_BIC:
  case ARM_OPC_BICS:
    Opc_BIC(instr, rrule);
    break;
  case ARM_OPC_LSL:
  case ARM_OPC_LSR:
  case ARM_OPC_ASR:
    Opc_Shift(instr, rrule);
    break;
  case ARM_OPC_ADD:
  case ARM_OPC_ADDS:
    Opc_ADD(instr, rrule);
    break;
  case ARM_OPC_ADC:
  case ARM_OPC_ADCS:
    Opc_ADC(instr, rrule);
    break;
  case ARM_OPC_SUB:
  case ARM_OPC_SUBS:
    Opc_SUB(instr, rrule);
    break;
  case ARM_OPC_SBC:
  case ARM_OPC_SBCS:
    Opc_SBC(instr, rrule);
    break;
  case ARM_OPC_MUL:
  case ARM_OPC_UMULL:
  case ARM_OPC_SMULL:
    Opc_MUL(instr, rrule);
    break;
  case ARM_OPC_CLZ:
    Opc_CLZ(instr, rrule);
    break;
  case ARM_OPC_TST:
    Opc_TST(instr, rrule);
    break;
  case ARM_OPC_CMP:
  case ARM_OPC_CMPB:
  case ARM_OPC_CMPW:
  case ARM_OPC_CMN:
    Opc_COMPARE(instr, rrule);
    break;
  case ARM_OPC_CSEL:
  case ARM_OPC_CSET:
    Opc_CSEX(instr, rrule);
    break;
  case ARM_OPC_BFXIL:
    Opc_BFXIL(instr, rrule);
    break;
  case ARM_OPC_B:
    Opc_B(instr, rrule);
    break;
  case ARM_OPC_CBZ:
  case ARM_OPC_CBNZ:
    Opc_CBNZ(instr, rrule);
    break;
  case ARM_OPC_SET_JUMP:
    Opc_SET_JUMP(instr, rrule);
    break;
  case ARM_OPC_SET_CALL:
    Opc_SET_CALL(instr, rrule);
    break;
  case ARM_OPC_PC_L:
  case ARM_OPC_PC_LB:
  case ARM_OPC_PC_LW:
    Opc_PC_L(instr, rrule);
    break;
  case ARM_OPC_PC_S:
  case ARM_OPC_PC_SB:
  case ARM_OPC_PC_SW:
    Opc_PC_S(instr, rrule);
    break;
  case ARM_OPC_ADDP:
    Opc_ADDP(instr, rrule);
    break;
  case ARM_OPC_CMEQ:
    Opc_CMEQ(instr, rrule);
    break;
  case ARM_OPC_CMGT:
    Opc_CMGT(instr, rrule);
    break;
  case ARM_OPC_CMLT:
    Opc_CMLT(instr, rrule);
    break;
  case ARM_OPC_DUP:
    Opc_DUP(instr, rrule);
    break;
  case ARM_OPC_EXT:
    Opc_EXT(instr, rrule);
    break;
  case ARM_OPC_FMOV:
    Opc_FMOV(instr, rrule);
    break;
  case ARM_OPC_INS:
    Opc_INS(instr, rrule);
    break;
  case ARM_OPC_LD1:
    Opc_LD1(instr, rrule);
    break;
  case ARM_OPC_SQXTUN:
  case ARM_OPC_SQXTUN2:
    Opc_SQXTUN(instr, rrule);
    break;
  case ARM_OPC_UMOV:
    Opc_UMOV(instr, rrule);
    break;
  case ARM_OPC_ZIP1:
  case ARM_OPC_ZIP2:
    Opc_ZIP(instr, rrule);
    break;
  default:
    LogMan::Msg::EFmt(
        "Unsupported arm instruction in the assembler: {}, rule index: {}.",
        get_arm_instr_opc(instr->opc), rrule->rule->index);
  }
}

/**
 * @brief 生成退出代码
 *
 * @param target_pc 目标PC值
 */
void PatternMatcher::assemble_arm_exit(uint64_t target_pc) {
  ARMAssembler->ret();
}

#undef DEF_OP
