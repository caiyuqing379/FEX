#include "Interface/Context/Context.h"
#include "FEXCore/IR/IR.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Emitter.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Registers.h"
#include "Interface/Core/LookupCache.h"
#include "Interface/Core/JIT/Arm64/JITClass.h"

#include <FEXCore/Utils/CompilerDefs.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/Utils/MathUtils.h>

#include "Interface/Core/Frontend.h"
#include "Interface/Core/PatternDbt/arm-instr.h"
#include "Interface/Core/PatternDbt/rule-translate.h"
#include "Interface/Core/PatternDbt/rule-debug-log.h"

using namespace FEXCore;

#define DEF_OPC(x) void FEXCore::CPU::Arm64JITCore::Opc_##x(ARMInstruction *instr, RuleRecord *rrule)


static ARMEmitter::Condition MapBranchCC(ARMConditionCode Cond)
{
  switch (Cond) {
    case ARM_CC_EQ: return ARMEmitter::Condition::CC_EQ;
    case ARM_CC_NE: return ARMEmitter::Condition::CC_NE;
    case ARM_CC_GE: return ARMEmitter::Condition::CC_GE;
    case ARM_CC_LT: return ARMEmitter::Condition::CC_LT;
    case ARM_CC_GT: return ARMEmitter::Condition::CC_GT;
    case ARM_CC_LE: return ARMEmitter::Condition::CC_LE;
    case ARM_CC_CS: return ARMEmitter::Condition::CC_CS;
    case ARM_CC_CC: return ARMEmitter::Condition::CC_CC;
    case ARM_CC_HI: return ARMEmitter::Condition::CC_HI;
    case ARM_CC_LS: return ARMEmitter::Condition::CC_LS;
    case ARM_CC_VS: return ARMEmitter::Condition::CC_VS;
    case ARM_CC_VC: return ARMEmitter::Condition::CC_VC;
    case ARM_CC_MI: return ARMEmitter::Condition::CC_MI;
    case ARM_CC_PL: return ARMEmitter::Condition::CC_PL;
    default:
      LOGMAN_MSG_A_FMT("Unsupported compare type");
      return ARMEmitter::Condition::CC_NV;
  }
}

static ARMEmitter::ShiftType GetShiftType(ARMOperandScaleDirect direct)
{
  switch(direct) {
    case ARM_OPD_DIRECT_LSL: return ARMEmitter::ShiftType::LSL;
    case ARM_OPD_DIRECT_LSR: return ARMEmitter::ShiftType::LSR;
    case ARM_OPD_DIRECT_ASR: return ARMEmitter::ShiftType::ASR;
    case ARM_OPD_DIRECT_ROR: return ARMEmitter::ShiftType::ROR;
    default:
      LOGMAN_MSG_A_FMT("Unsupported Shift type");
  }
}

static ARMEmitter::ExtendedType GetExtendType(ARMOperandScaleExtend extend)
{
  switch(extend) {
    case ARM_OPD_EXTEND_UXTB: return ARMEmitter::ExtendedType::UXTB;
    case ARM_OPD_EXTEND_UXTH: return ARMEmitter::ExtendedType::UXTH;
    case ARM_OPD_EXTEND_UXTW: return ARMEmitter::ExtendedType::UXTW;
    case ARM_OPD_EXTEND_UXTX: return ARMEmitter::ExtendedType::UXTX;
    case ARM_OPD_EXTEND_SXTB: return ARMEmitter::ExtendedType::SXTB;
    case ARM_OPD_EXTEND_SXTH: return ARMEmitter::ExtendedType::SXTH;
    case ARM_OPD_EXTEND_SXTW: return ARMEmitter::ExtendedType::SXTW;
    case ARM_OPD_EXTEND_SXTX: return ARMEmitter::ExtendedType::SXTX;
    default:
      LOGMAN_MSG_A_FMT("Unsupported Extend type");
  }
}

static ARMEmitter::Register GetRegMap(ARMRegister& reg)
{
  ARMEmitter::Register reg_invalid(255);
  switch (reg) {
    case ARM_REG_R0:  return ARMEmitter::Reg::r0;
    case ARM_REG_R1:  return ARMEmitter::Reg::r1;
    case ARM_REG_R2:  return ARMEmitter::Reg::r2;
    case ARM_REG_R3:  return ARMEmitter::Reg::r3;
    case ARM_REG_R4:  return ARMEmitter::Reg::r4;
    case ARM_REG_R5:  return ARMEmitter::Reg::r5;
    case ARM_REG_R6:  return ARMEmitter::Reg::r6;
    case ARM_REG_R7:  return ARMEmitter::Reg::r7;
    case ARM_REG_R8:  return ARMEmitter::Reg::r8;
    case ARM_REG_R9:  return ARMEmitter::Reg::r9;
    case ARM_REG_R10: return ARMEmitter::Reg::r10;
    case ARM_REG_R11: return ARMEmitter::Reg::r11;
    case ARM_REG_R12: return ARMEmitter::Reg::r12;
    case ARM_REG_R13: return ARMEmitter::Reg::r13;
    case ARM_REG_R14: return ARMEmitter::Reg::r14;
    case ARM_REG_R15: return ARMEmitter::Reg::r15;
    case ARM_REG_R16: return ARMEmitter::Reg::r16;
    case ARM_REG_R17: return ARMEmitter::Reg::r17;
    case ARM_REG_R18: return ARMEmitter::Reg::r18;
    case ARM_REG_R19: return ARMEmitter::Reg::r19;
    case ARM_REG_R20: return ARMEmitter::Reg::r20;
    case ARM_REG_R21: return ARMEmitter::Reg::r21;
    case ARM_REG_R22: return ARMEmitter::Reg::r22;
    case ARM_REG_R23: return ARMEmitter::Reg::r23;
    case ARM_REG_R24: return ARMEmitter::Reg::r24;
    case ARM_REG_R25: return ARMEmitter::Reg::r25;
    case ARM_REG_R26: return ARMEmitter::Reg::r26;
    case ARM_REG_R27: return ARMEmitter::Reg::r27;
    case ARM_REG_R28: return ARMEmitter::Reg::r28;
    case ARM_REG_R29: return ARMEmitter::Reg::r29;
    case ARM_REG_R30: return ARMEmitter::Reg::r30;
    case ARM_REG_R31: return ARMEmitter::Reg::r31;

    case ARM_REG_FP: return ARMEmitter::Reg::fp;
    case ARM_REG_LR: return ARMEmitter::Reg::lr;
    case ARM_REG_RSP: return ARMEmitter::Reg::rsp;
    case ARM_REG_ZR: return ARMEmitter::Reg::zr;
    default:
      LOGMAN_MSG_A_FMT("Unsupported host reg num");
  }
  return reg_invalid;
}

static ARMEmitter::VRegister GetVRegMap(ARMRegister& reg)
{
  ARMEmitter::VRegister reg_invalid(255);
  switch (reg) {
    case ARM_REG_V0:  return ARMEmitter::VReg::v0;
    case ARM_REG_V1:  return ARMEmitter::VReg::v1;
    case ARM_REG_V2:  return ARMEmitter::VReg::v2;
    case ARM_REG_V3:  return ARMEmitter::VReg::v3;
    case ARM_REG_V4:  return ARMEmitter::VReg::v4;
    case ARM_REG_V5:  return ARMEmitter::VReg::v5;
    case ARM_REG_V6:  return ARMEmitter::VReg::v6;
    case ARM_REG_V7:  return ARMEmitter::VReg::v7;
    case ARM_REG_V8:  return ARMEmitter::VReg::v8;
    case ARM_REG_V9:  return ARMEmitter::VReg::v9;
    case ARM_REG_V10: return ARMEmitter::VReg::v10;
    case ARM_REG_V11: return ARMEmitter::VReg::v11;
    case ARM_REG_V12: return ARMEmitter::VReg::v12;
    case ARM_REG_V13: return ARMEmitter::VReg::v13;
    case ARM_REG_V14: return ARMEmitter::VReg::v14;
    case ARM_REG_V15: return ARMEmitter::VReg::v15;
    case ARM_REG_V16: return ARMEmitter::VReg::v16;
    case ARM_REG_V17: return ARMEmitter::VReg::v17;
    case ARM_REG_V18: return ARMEmitter::VReg::v18;
    case ARM_REG_V19: return ARMEmitter::VReg::v19;
    case ARM_REG_V20: return ARMEmitter::VReg::v20;
    case ARM_REG_V21: return ARMEmitter::VReg::v21;
    case ARM_REG_V22: return ARMEmitter::VReg::v22;
    case ARM_REG_V23: return ARMEmitter::VReg::v23;
    case ARM_REG_V24: return ARMEmitter::VReg::v24;
    case ARM_REG_V25: return ARMEmitter::VReg::v25;
    case ARM_REG_V26: return ARMEmitter::VReg::v26;
    case ARM_REG_V27: return ARMEmitter::VReg::v27;
    case ARM_REG_V28: return ARMEmitter::VReg::v28;
    case ARM_REG_V29: return ARMEmitter::VReg::v29;
    case ARM_REG_V30: return ARMEmitter::VReg::v30;
    case ARM_REG_V31: return ARMEmitter::VReg::v31;
    default:
      LOGMAN_MSG_A_FMT("Unsupported host vreg num");
  }
  return reg_invalid;
}

uint64_t FEXCore::CPU::Arm64JITCore::get_imm_map_wrapper(ARMImm *imm)
{
    if (imm->type == ARM_IMM_TYPE_NONE)
        return 0;

    if (imm->type == ARM_IMM_TYPE_VAL)
        return imm->content.val;

    return get_imm_map(imm->content.sym);
}

static ARMEmitter::ExtendedMemOperand  GenerateExtMemOperand(ARMEmitter::Register Base,
                                            ARMRegister Index,
                                            int32_t Imm,
                                            ARMOperandScale OffsetScale,
                                            ARMMemIndexType OffsetType) {

    uint8_t amount = static_cast<uint8_t>(OffsetScale.imm.content.val);

    if (Index == ARM_REG_INVALID) {
        if (OffsetType == ARM_MEM_INDEX_TYPE_PRE)
          return ARMEmitter::ExtendedMemOperand(Base.X(), ARMEmitter::IndexType::PRE, Imm);
        else if (OffsetType == ARM_MEM_INDEX_TYPE_POST)
          return ARMEmitter::ExtendedMemOperand(Base.X(), ARMEmitter::IndexType::POST, Imm);
        else
          return ARMEmitter::ExtendedMemOperand(Base.X(), ARMEmitter::IndexType::OFFSET, Imm);
    } else {
      switch(OffsetScale.content.extend) {
        case ARM_OPD_EXTEND_UXTW: return ARMEmitter::ExtendedMemOperand(Base.X(), GetRegMap(Index).X(), ARMEmitter::ExtendedType::UXTW, FEXCore::ilog2(amount) );
        case ARM_OPD_EXTEND_SXTW: return ARMEmitter::ExtendedMemOperand(Base.X(), GetRegMap(Index).X(), ARMEmitter::ExtendedType::SXTW, FEXCore::ilog2(amount) );
        case ARM_OPD_EXTEND_SXTX: return ARMEmitter::ExtendedMemOperand(Base.X(), GetRegMap(Index).X(), ARMEmitter::ExtendedType::SXTX, FEXCore::ilog2(amount) );
        default: LOGMAN_MSG_A_FMT("Unhandled GenerateExtMemOperand OffsetType"); break;
      }
    }
}

DEF_OPC(LDR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t    OpSize = instr->OpSize;
    uint32_t   Reg0Size, Reg1Size;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
        auto SrcReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);

        if (Reg0Size && !OpSize) OpSize = 1 << (Reg0Size-1);

        if (opd1->content.mem.base != ARM_REG_INVALID) {
          ARMRegister      Index = opd1->content.mem.index;
          ARMOperandScale  OffsetScale = opd1->content.mem.scale;
          ARMMemIndexType  PrePost = opd1->content.mem.pre_post;
          auto ARMReg = GetGuestRegMap(opd1->content.mem.base, Reg1Size);
          auto Base = GetRegMap(ARMReg);
          int32_t Imm = get_imm_map_wrapper(&opd1->content.mem.offset);

          auto MemSrc = GenerateExtMemOperand(Base, Index, Imm, OffsetScale, PrePost);

          if ((Index == ARM_REG_INVALID) && (PrePost == ARM_MEM_INDEX_TYPE_NONE) && ((Imm < 0) || (Imm > 0 && (Imm >> 12) != 0))) {
            if (Imm < 0) {
              sub(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, Base, 0 - Imm);
            } else {
              LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r22, Imm);
              add(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, Base, ARMEmitter::Reg::r22);
            }
            MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r21).X(), Index, 0, OffsetScale, PrePost);
          }

          if (instr->opc == ARM_OPC_LDRB || instr->opc == ARM_OPC_LDRH || instr->opc == ARM_OPC_LDR) {
            if (SrcReg >= ARM_REG_R0 && SrcReg <= ARM_REG_R31) {
              const auto Dst = GetRegMap(SrcReg);
              switch (OpSize) {
                case 1:
                  ldrb(Dst, MemSrc);
                  break;
                case 2:
                  ldrh(Dst, MemSrc);
                  break;
                case 4:
                  ldr(Dst.W(), MemSrc);
                  break;
                case 8:
                  ldr(Dst.X(), MemSrc); // LDR（literal) not support
                  break;
                default:
                  LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
                  break;
              }
            }
            else if (SrcReg >= ARM_REG_V0 && SrcReg <= ARM_REG_V31) {
              const auto Dst = GetVRegMap(SrcReg);
              switch (OpSize) {
                case 1:
                  ldrb(Dst, MemSrc);
                  break;
                case 2:
                  ldrh(Dst, MemSrc);
                  break;
                case 4:
                  ldr(Dst.S(), MemSrc);
                  break;
                case 8:
                  ldr(Dst.D(), MemSrc);
                  break;
                case 16:
                  ldr(Dst.Q(), MemSrc);
                  break;
                default:
                  LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
                  break;
              }
            }
          } else if (instr->opc == ARM_OPC_LDRSB) {
            const auto Dst = GetRegMap(SrcReg);
            if (OpSize == 4)
              ldrsb(Dst.W(), MemSrc);
            else
              ldrsb(Dst.X(), MemSrc);
          } else if (instr->opc == ARM_OPC_LDRSH) {
            const auto Dst = GetRegMap(SrcReg);
            if (OpSize == 4)
              ldrsh(Dst.W(), MemSrc);
            else
              ldrsh(Dst.X(), MemSrc);
          } else if (instr->opc == ARM_OPC_LDAR) {
            const auto Dst = GetRegMap(SrcReg);
            ldar(Dst.X(), Base);
            nop();
          }
        }
    } else
      LogMan::Msg::EFmt( "[arm] Unsupported operand type for ldr instruction.");
}

DEF_OPC(LDP) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint32_t   Reg0Size, Reg1Size, Reg2Size;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_MEM) {
        auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
        auto RegPair1 = GetRegMap(ARMReg);
        ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
        auto RegPair2 = GetRegMap(ARMReg);
        ARMReg = GetGuestRegMap(opd2->content.mem.base, Reg2Size);
        auto Base     = GetRegMap(ARMReg);
        ARMMemIndexType  PrePost = opd2->content.mem.pre_post;
        int32_t Imm = get_imm_map_wrapper(&opd2->content.mem.offset);

        if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
            ldp<ARMEmitter::IndexType::PRE>(RegPair1.X(), RegPair2.X(), Base, Imm);
        else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
            ldp<ARMEmitter::IndexType::POST>(RegPair1.X(), RegPair2.X(), Base, Imm);
        else
            ldp<ARMEmitter::IndexType::OFFSET>(RegPair1.X(), RegPair2.X(), Base, Imm);
    } else
      LogMan::Msg::EFmt( "[arm] Unsupported operand type for ldp instruction.");
}

DEF_OPC(STR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t    OpSize = instr->OpSize;
    uint32_t   Reg0Size, Reg1Size;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
        auto SrcReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);

        if (Reg0Size && !OpSize) OpSize = 1 << (Reg0Size-1);

        if (opd1->content.mem.base != ARM_REG_INVALID) {
          ARMRegister      Index = opd1->content.mem.index;
          ARMOperandScale  OffsetScale = opd1->content.mem.scale;
          ARMMemIndexType  PrePost = opd1->content.mem.pre_post;
          auto ARMReg = GetGuestRegMap(opd1->content.mem.base, Reg1Size);
          auto Base = GetRegMap(ARMReg);
          int32_t Imm = get_imm_map_wrapper(&opd1->content.mem.offset);

          auto MemSrc = GenerateExtMemOperand(Base, Index, Imm, OffsetScale, PrePost);

          if ((Index == ARM_REG_INVALID) && (PrePost == ARM_MEM_INDEX_TYPE_NONE) && ((Imm < 0) || (Imm > 0 && (Imm >> 12) != 0))) {
            if (Imm < 0) {
              sub(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, Base, 0 - Imm);
            } else {
              LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r22, Imm);
              add(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, Base, ARMEmitter::Reg::r22);
            }
            MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r21).X(), Index, 0, OffsetScale, PrePost);
          }

          if (SrcReg >= ARM_REG_R0 && SrcReg <= ARM_REG_R31) {
            const auto Src = GetRegMap(SrcReg);
            switch (OpSize) {
              case 1:
                strb(Src, MemSrc);
                break;
              case 2:
                strh(Src, MemSrc);
                break;
              case 4:
                str(Src.W(), MemSrc);
                break;
              case 8:
                str(Src.X(), MemSrc);
                break;
              default:
                LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
                break;
            }
          }
          else if (SrcReg >= ARM_REG_V0 && SrcReg <= ARM_REG_V31) {
            const auto Src = GetVRegMap(SrcReg);
            switch (OpSize) {
              case 1:
                strb(Src, MemSrc);
                break;
              case 2:
                strh(Src, MemSrc);
                break;
              case 4:
                str(Src.S(), MemSrc);
                break;
              case 8:
                str(Src.D(), MemSrc);
                break;
              case 16:
                str(Src.Q(), MemSrc);
                break;
              default:
                LOGMAN_MSG_A_FMT("Unhandled StoreMem size: {}", OpSize);
                break;
            }
          }
        }
    } else
      LogMan::Msg::EFmt( "[arm] Unsupported operand type for str instruction.");
}

DEF_OPC(STP) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t    OpSize = instr->OpSize;
    uint32_t   Reg0Size, Reg1Size, Reg2Size;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_MEM) {
        auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
        auto RegPair1 = GetRegMap(ARMReg);
        ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
        auto RegPair2 = GetRegMap(ARMReg);
        ARMReg = GetGuestRegMap(opd2->content.mem.base, Reg2Size);
        auto Base     = GetRegMap(ARMReg);
        ARMMemIndexType  PrePost = opd2->content.mem.pre_post;
        int32_t Imm = get_imm_map_wrapper(&opd2->content.mem.offset);

        if (Reg0Size) OpSize = 1 << (Reg0Size-1);

        if (OpSize == 8) {
          if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
            stp<ARMEmitter::IndexType::PRE>(RegPair1.X(), RegPair2.X(), Base, Imm);
          else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
            stp<ARMEmitter::IndexType::POST>(RegPair1.X(), RegPair2.X(), Base, Imm);
          else
            stp<ARMEmitter::IndexType::OFFSET>(RegPair1.X(), RegPair2.X(), Base, Imm);
        } else if (OpSize == 4) {
          if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
            stp<ARMEmitter::IndexType::PRE>(RegPair1.W(), RegPair2.W(), Base, Imm);
          else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
            stp<ARMEmitter::IndexType::POST>(RegPair1.W(), RegPair2.W(), Base, Imm);
          else
            stp<ARMEmitter::IndexType::OFFSET>(RegPair1.W(), RegPair2.W(), Base, Imm);
        } else
          LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for stp instruction.");
}


DEF_OPC(SXTW) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint32_t   Reg0Size, Reg1Size;

    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    const auto Dst = GetRegMap(DstReg);
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    const auto Src = GetRegMap(SrcReg);

    sxtw(Dst.X(), Src.W());
}


DEF_OPC(MOV) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size;
    bool HighBits = false;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
      auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size, std::forward<bool>(HighBits));

      const auto EmitSize =
        (Reg1Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
        (Reg1Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

      if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
        auto Dst = GetRegMap(DstReg);
        auto Src = GetRegMap(SrcReg);

        if (Reg1Size == 1 && HighBits)
          ubfx(EmitSize, Dst, Src, 8, 8);
        else if (Reg1Size == 1 && !HighBits)
          uxtb(EmitSize, Dst, Src);
        else if (Reg1Size == 2 && !HighBits)
          uxth(EmitSize, Dst, Src);
        else
          mov(EmitSize, Dst, Src);  // move (register)
                                    // move (to/from SP) not support
      } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
        auto Dst = GetVRegMap(DstReg);
        auto SrcDst = GetVRegMap(SrcReg);

        if (OpSize == 16) {
          if (HostSupportsSVE256 || Dst != SrcDst) {
            mov(Dst.Q(), SrcDst.Q());
          }
        } else if (OpSize == 8) {
          mov(Dst.D(), SrcDst.D());
        }
      } else
        LogMan::Msg::EFmt("Unsupported reg num for mov instr.");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
        auto Dst = GetRegMap(DstReg);
        uint64_t Constant = 0;
        if (opd1->content.imm.type == ARM_IMM_TYPE_SYM && !strcmp(opd1->content.imm.content.sym, "LVMask"))
          Constant = 0x80'40'20'10'08'04'02'01ULL;
        else
          Constant = get_imm_map_wrapper(&opd1->content.imm);
        LoadConstant(ARMEmitter::Size::i64Bit, Dst, Constant);
    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for mov instruction.");
}


DEF_OPC(MVN) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpSize;

    uint32_t   Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {

      if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
          ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
          auto SrcDst = GetRegMap(ARMReg);
          auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
          uint32_t amt = opd1->content.reg.scale.imm.content.val;
          mvn(EmitSize, Dst, SrcDst, Shift, amt);

      } else if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
          ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
          auto SrcDst = GetRegMap(ARMReg);
          mvn(EmitSize, Dst, SrcDst);
      } else
          LogMan::Msg::EFmt( "[arm] Unsupported reg for mvn instruction.");

    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for mvn instruction.");
}


DEF_OPC(AND) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        uint64_t Imm = get_imm_map_wrapper(&opd2->content.imm);
        const auto IsImm = vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

        if ((Imm >> 12) > 0 || !IsImm) {
          LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
          if(instr->opc == ARM_OPC_AND)
            and_(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);  // and imm
          else
            ands(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
        } else {
          if(instr->opc == ARM_OPC_AND)
            and_(EmitSize, Dst, Src1, Imm);  // and imm
          else
            ands(EmitSize, Dst, Src1, Imm);  // ands imm
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_AND)
          and_(EmitSize, Dst, Src1, Src2, Shift, amt);
        else
          ands(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
        auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

        if (instr->opc == ARM_OPC_AND) {
          if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
            auto Dst  = GetRegMap(DstReg);
            auto Src1 = GetRegMap(SrcReg);
            auto Src2 = GetRegMap(Src2Reg);
            and_(EmitSize, Dst, Src1, Src2);

          } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
            auto Dst  = GetVRegMap(DstReg);
            auto Vector1 = GetVRegMap(SrcReg);
            auto Vector2 = GetVRegMap(Src2Reg);

            if (HostSupportsSVE256 && Is256Bit) {
              and_(Dst.Z(), Vector1.Z(), Vector2.Z());
            } else {
              and_(Dst.Q(), Vector1.Q(), Vector2.Q());
            }
          } else
              LogMan::Msg::EFmt("Unsupported reg num for and instr.");

        } else {
          auto Dst  = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2 = GetRegMap(Src2Reg);
          ands(EmitSize, Dst, Src1, Src2);
        }

      } else
          LogMan::Msg::EFmt( "Unsupported reg type for and instr.");

    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for and instruction.");
}


DEF_OPC(ORR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Imm = get_imm_map_wrapper(&opd2->content.imm);
        const auto IsImm = vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

        if ((Imm >> 12) > 0 || !IsImm) {
          LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
          orr(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
        } else
          orr(EmitSize, Dst, Src1, Imm);

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

        if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
          auto Dst  = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
          auto Src2 = GetRegMap(Src2Reg);
          auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
          uint32_t amt = opd2->content.reg.scale.imm.content.val;
          orr(EmitSize, Dst, Src1, Src2, Shift, amt);

        } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
          auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
          auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

          if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
            auto Dst  = GetRegMap(DstReg);
            auto Src1 = GetRegMap(SrcReg);
            auto Src2 = GetRegMap(Src2Reg);
            orr(EmitSize, Dst, Src1, Src2);

          } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
            auto Dst  = GetVRegMap(DstReg);
            auto Vector1 = GetVRegMap(SrcReg);
            auto Vector2 = GetVRegMap(Src2Reg);

            if (HostSupportsSVE256 && Is256Bit) {
              orr(Dst.Z(), Vector1.Z(), Vector2.Z());
            } else {
              orr(Dst.Q(), Vector1.Q(), Vector2.Q());
            }
          } else
            LogMan::Msg::EFmt("Unsupported reg num for orr instr.");

        } else
          LogMan::Msg::EFmt( "[arm] Unsupported reg for orr instr.");

    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for orr instruction.");
}


DEF_OPC(EOR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        int32_t Imm = get_imm_map_wrapper(&opd2->content.imm);
        const auto IsImm = vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

        if ((Imm >> 12) > 0 || !IsImm) {
          LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
          eor(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
        } else {
          eor(EmitSize, Dst, Src1, Imm);
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;
        eor(EmitSize, Dst, Src1, Src2, Shift, amt);

      } if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
        auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

        if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
          auto Dst  = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2 = GetRegMap(Src2Reg);
          eor(EmitSize, Dst, Src1, Src2);

        } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
          auto Dst  = GetVRegMap(DstReg);
          auto Vector1 = GetVRegMap(SrcReg);
          auto Vector2 = GetVRegMap(Src2Reg);

          if (HostSupportsSVE256 && Is256Bit) {
            eor(Dst.Z(), Vector1.Z(), Vector2.Z());
          } else {
            eor(Dst.Q(), Vector1.Q(), Vector2.Q());
          }
        } else
          LogMan::Msg::EFmt("Unsupported reg num for eor instr.");

      } else
          LogMan::Msg::EFmt( "[arm] Unsupported reg for eor instruction.");

    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for eor instruction.");
}


DEF_OPC(BIC) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_BIC)
          bic(EmitSize, Dst, Src1, Src2, Shift, amt);
        else
          bics(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
        auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);

        if (instr->opc == ARM_OPC_BIC) {
          if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
            auto Dst  = GetRegMap(DstReg);
            auto Src1 = GetRegMap(SrcReg);
            auto Src2 = GetRegMap(Src2Reg);
            bic(EmitSize, Dst, Src1, Src2);

          } else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
            auto Dst  = GetVRegMap(DstReg);
            auto Vector1 = GetVRegMap(SrcReg);
            auto Vector2 = GetVRegMap(Src2Reg);

            if (HostSupportsSVE256 && Is256Bit) {
              bic(Dst.Z(), Vector1.Z(), Vector2.Z());
            } else {
              bic(Dst.Q(), Vector1.Q(), Vector2.Q());
            }
          } else
            LogMan::Msg::EFmt("Unsupported reg num for bic instr.");
        } else {
          auto Dst  = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2 = GetRegMap(Src2Reg);
          bics(EmitSize, Dst, Src1, Src2);
        }

      } else
          LogMan::Msg::IFmt( "[arm] Unsupported reg for bic instruction.");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for bic instruction.");
}


DEF_OPC(Shift) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Src1 = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        int32_t shift = get_imm_map_wrapper(&opd2->content.imm);

        if (instr->opc == ARM_OPC_LSL) {
            lsl(EmitSize, Dst, Src1, shift);
            mrs((ARMEmitter::Reg::r20).X(), ARMEmitter::SystemRegister::NZCV);
            ubfx(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, Src1, 64-shift, 1);
            orr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::ShiftType::LSL, 29);
            msr(ARMEmitter::SystemRegister::NZCV, (ARMEmitter::Reg::r20).X());
        } else if (instr->opc == ARM_OPC_LSR) {
            lsr(EmitSize, Dst, Src1, shift);
        } else if (instr->opc == ARM_OPC_ASR) {
            asr(EmitSize, Dst, Src1, shift);
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID) {
        ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(ARMReg);

        if (instr->opc == ARM_OPC_LSL)
          lslv(EmitSize, Dst, Src1, Src2);
        else if (instr->opc == ARM_OPC_LSR)
          lsrv(EmitSize, Dst, Src1, Src2);
        else if (instr->opc == ARM_OPC_ASR)
          asrv(EmitSize, Dst, Src1, Src2);
      } else
          LogMan::Msg::EFmt("[arm] Unsupported reg for shift instruction.");

    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for shift instruction.");
}


DEF_OPC(ADD) {
    ARMOperand *opd0   = &instr->opd[0];
    ARMOperand *opd1   = &instr->opd[1];
    ARMOperand *opd2   = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

    const auto EmitSize =
        (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
        (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        int32_t Imm = get_imm_map_wrapper(&opd2->content.imm);

        if (Imm < 0) {
          if (instr->opc == ARM_OPC_ADD)
            sub(EmitSize, Dst, Src1, 0 - Imm);
          else
            subs(EmitSize, Dst, Src1, 0 - Imm);
        } else if (Imm > 0 && ((Imm >> 12) > 0)) {
          LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
          if (instr->opc == ARM_OPC_ADD)
            add(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
          else
            adds(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
        } else {
          if (instr->opc == ARM_OPC_ADD)
            add(EmitSize, Dst, Src1, Imm);  // LSL12 default false
          else
            adds(EmitSize, Dst, Src1, Imm);
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
        if (instr->opc == ARM_OPC_ADD) {
          if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
            auto Dst  = GetRegMap(DstReg);
            auto Src1 = GetRegMap(SrcReg);
            auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
            auto Src2 = GetRegMap(Src2Reg);
            add(EmitSize, Dst, Src1, Src2);
          }
          else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
            auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
            auto ElementSize = instr->ElementSize;

            auto Dst  = GetVRegMap(DstReg);
            auto Vector1 = GetVRegMap(SrcReg);
            auto VecReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
            auto Vector2 = GetVRegMap(VecReg);

            LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8, "add Invalid size");
            const auto SubRegSize =
              ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
              ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
              ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
              ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i8Bit;

            if (HostSupportsSVE256 && Is256Bit) {
              add(SubRegSize, Dst.Z(), Vector1.Z(), Vector2.Z());
            } else {
              add(SubRegSize, Dst.Q(), Vector1.Q(), Vector2.Q());
            }
          } else
              LogMan::Msg::EFmt("Unsupported reg num for add instr.");

        } else {
          auto Dst  = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
          auto Src2 = GetRegMap(Src2Reg);
          adds(EmitSize, Dst, Src1, Src2);
        }
      }
      else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_ADD)
          add(EmitSize, Dst, Src1, Src2, Shift, amt);
        else
          adds(EmitSize, Dst, Src1, Src2, Shift, amt);
      }
      else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        auto Option = GetExtendType(opd2->content.reg.scale.content.extend);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_ADD)
          add(EmitSize, Dst, Src1, Src2, Option, amt);
        else
          adds(EmitSize, Dst, Src1, Src2, Option, amt);
      } else
          LogMan::Msg::EFmt("[arm] Unsupported reg for add instruction.");

    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for add instruction.");
}


DEF_OPC(ADC) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
      auto Src1 = GetRegMap(ARMReg);
      ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(ARMReg);

      if(instr->opc == ARM_OPC_ADC)
        adc(EmitSize, Dst, Src1, Src2);
      else
        adcs(EmitSize, Dst, Src1, Src2);

    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for ADC instruction.");
}


// behind cmp, sub instr
void FEXCore::CPU::Arm64JITCore::FlipCF() {
    mrs((ARMEmitter::Reg::r20).X(), ARMEmitter::SystemRegister::NZCV);
    eor(ARMEmitter::Size::i32Bit, (ARMEmitter::Reg::r20).W(), (ARMEmitter::Reg::r20).W(), 0x20000000);
    msr(ARMEmitter::SystemRegister::NZCV, (ARMEmitter::Reg::r20).X());
}

DEF_OPC(SUB) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto SrcReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

    const auto EmitSize =
        (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
        (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        int32_t Imm = get_imm_map_wrapper(&opd2->content.imm);

        if ((Imm >> 12) > 0) {
          LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
          if (instr->opc == ARM_OPC_SUB) {
            sub(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
          } else {
            subs(EmitSize, Dst, Src1, ARMEmitter::Reg::r20);
            FlipCF();
          }
        } else {
          if (instr->opc == ARM_OPC_SUB) {
            sub(EmitSize, Dst, Src1, Imm);  // LSL12 default false
          } else {
            subs(EmitSize, Dst, Src1, Imm);
            FlipCF();
          }
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_SUB) {
          sub(EmitSize, Dst, Src1, Src2, Shift, amt);
        } else {
          subs(EmitSize, Dst, Src1, Src2, Shift, amt);
          FlipCF();
        }

      } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
        auto Dst  = GetRegMap(DstReg);
        auto Src1 = GetRegMap(SrcReg);
        auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(Src2Reg);
        auto Option = GetExtendType(opd2->content.reg.scale.content.extend);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_SUB) {
          sub(EmitSize, Dst, Src1, Src2, Option, amt);
        } else {
          subs(EmitSize, Dst, Src1, Src2, Option, amt);
          FlipCF();
        }

      } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
        if (instr->opc == ARM_OPC_SUB) {
          if (DstReg >= ARM_REG_R0 && DstReg <= ARM_REG_R31) {
            auto Dst  = GetRegMap(DstReg);
            auto Src1 = GetRegMap(SrcReg);
            auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
            auto Src2 = GetRegMap(Src2Reg);
            sub(EmitSize, Dst, Src1, Src2);
          }
          else if (DstReg >= ARM_REG_V0 && DstReg <= ARM_REG_V31) {
            const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
            const auto ElementSize = instr->ElementSize;

            auto Dst  = GetVRegMap(DstReg);
            auto Vector1 = GetVRegMap(SrcReg);
            auto VecReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
            auto Vector2 = GetVRegMap(VecReg);

            LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8, "sub Invalid size");
            const auto SubRegSize =
              ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
              ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
              ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
              ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i8Bit;

            if (HostSupportsSVE256 && Is256Bit) {
              sub(SubRegSize, Dst.Z(), Vector1.Z(), Vector2.Z());
            } else {
              sub(SubRegSize, Dst.Q(), Vector1.Q(), Vector2.Q());
            }
          } else
              LogMan::Msg::EFmt("Unsupported reg num for sub instr.");

        } else {
          auto Dst  = GetRegMap(DstReg);
          auto Src1 = GetRegMap(SrcReg);
          auto Src2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
          auto Src2 = GetRegMap(Src2Reg);
          subs(EmitSize, Dst, Src1, Src2);
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
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
      auto Src1 = GetRegMap(ARMReg);
      ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(ARMReg);

      if(instr->opc == ARM_OPC_SBC)
        sbc(EmitSize, Dst, Src1, Src2);
      else
        sbcs(EmitSize, Dst, Src1, Src2);

    } else
      LogMan::Msg::EFmt("[arm] Unsupported operand type for SBC instruction.");
}


DEF_OPC(MUL) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
      auto Src1 = GetRegMap(ARMReg);
      ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
      auto Src2 = GetRegMap(ARMReg);

      if (instr->opc == ARM_OPC_MUL)
        mul(EmitSize, Dst, Src1, Src2);
      else if(instr->opc == ARM_OPC_UMULL)
        umull(Dst.X(), Src1.W(), Src2.W());
      else if(instr->opc == ARM_OPC_SMULL)
        smull(Dst.X(), Src1.W(), Src2.W());
    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for MUL instruction.");
}


DEF_OPC(CLZ) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
        ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
        auto Src = GetRegMap(ARMReg);

        clz(EmitSize, Dst, Src);
    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for CLZ instruction.");
}


DEF_OPC(TST) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpSize;

    bool isRegSym = false;
    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (Reg0Size == 1 || Reg0Size == 2) isRegSym = true;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
        ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
        auto Src = GetRegMap(ARMReg);

        if (!isRegSym && opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
            auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
            uint32_t amt = opd1->content.reg.scale.imm.content.val;
            tst(EmitSize, Dst, Src, Shift, amt);
        } else if (!isRegSym && opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
            tst(EmitSize, Dst, Src);
        } else if (isRegSym) {
            unsigned Shift = 32 - (Reg0Size * 8);
            if (Dst == Src) {
              cmn(EmitSize, ARMEmitter::Reg::zr, Dst, ARMEmitter::ShiftType::LSL, Shift);
            } else {
              and_(EmitSize, ARMEmitter::Reg::r26, Dst, Src);
              cmn(EmitSize, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, Shift);
            }
        } else
            LogMan::Msg::EFmt("[arm] Unsupported reg for TST instruction.");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
        uint64_t Imm = get_imm_map_wrapper(&opd1->content.imm);
        const auto IsImm = vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

        if (!isRegSym) {
            if (IsImm) {
              tst(EmitSize, Dst, Imm);
            } else {
              LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
              and_(EmitSize, ARMEmitter::Reg::r26, Dst, ARMEmitter::Reg::r20);
              tst(EmitSize, ARMEmitter::Reg::r26, ARMEmitter::Reg::r26);
            }
        } else {
            unsigned Shift = 32 - (Reg0Size * 8);
            if (IsImm) {
              and_(EmitSize, ARMEmitter::Reg::r26, Dst, Imm);
            } else {
              LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
              and_(EmitSize, ARMEmitter::Reg::r26, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20);
            }
            cmn(EmitSize, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, Shift);
        }

    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for TST instruction.");
}


DEF_OPC(COMPARE) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpSize;

    bool isRegSym = false;
    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    if (Reg0Size == 1 || Reg0Size == 2) isRegSym = true;

    /*
      lambda expression:
      [ capture_clause ] ( parameters ) -> return_type {
          function_body
      }
    }*/
    auto adjust_8_16_cmp = [=] (bool isImm = false, uint32_t regsize = 0, uint64_t Imm = 0, ARMEmitter::Register Src = ARMEmitter::Reg::r20) -> void {
        const auto s32 = ARMEmitter::Size::i32Bit;
        const auto s64 = ARMEmitter::Size::i64Bit;
        unsigned Shift = 32 - (regsize * 8);
        uint32_t lsb = regsize * 8;

        if (isImm) {
            if (regsize == 2) {
              uxth(s32, ARMEmitter::Reg::r27, Dst);
              LoadConstant(s32, (ARMEmitter::Reg::r20).X(), Imm);
              sub(s32, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27, ARMEmitter::Reg::r20);
            } else {
              if (regsize == 1)
                uxtb(s32, ARMEmitter::Reg::r27, Dst);
              sub(s32, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27, Imm);
            }
            cmn(s32, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, Shift);
            mrs((ARMEmitter::Reg::r20).X(), ARMEmitter::SystemRegister::NZCV);
            ubfx(s64, ARMEmitter::Reg::r21, ARMEmitter::Reg::r26, lsb, 1);
            orr(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::ShiftType::LSL, 29);
            bic(s32, ARMEmitter::Reg::r21, ARMEmitter::Reg::r27, ARMEmitter::Reg::r26);
            ubfx(s64, ARMEmitter::Reg::r21, ARMEmitter::Reg::r21, lsb - 1, 1);
            orr(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::ShiftType::LSL, 28);
        } else {
            sub(s32, ARMEmitter::Reg::r26, Dst, Src);
            //eor(s32, ARMEmitter::Reg::r27, Dst, Src);
            cmn(s32, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, Shift);
            mrs((ARMEmitter::Reg::r22).X(), ARMEmitter::SystemRegister::NZCV);
            ubfx(s64, ARMEmitter::Reg::r23, ARMEmitter::Reg::r26, lsb, 1);
            orr(s32, ARMEmitter::Reg::r22, ARMEmitter::Reg::r22, ARMEmitter::Reg::r23, ARMEmitter::ShiftType::LSL, 29);
            eor(s32, ARMEmitter::Reg::r20, Dst, Src);
            eor(s32, ARMEmitter::Reg::r21, ARMEmitter::Reg::r26, Dst);
            and_(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::Reg::r20);
            ubfx(s64, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20, lsb - 1, 1);
            orr(s32, ARMEmitter::Reg::r20, ARMEmitter::Reg::r22, ARMEmitter::Reg::r20, ARMEmitter::ShiftType::LSL, 28);
        }
        msr(ARMEmitter::SystemRegister::NZCV, (ARMEmitter::Reg::r20).X());
    };

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
        uint64_t Imm = get_imm_map_wrapper(&opd1->content.imm);

        const auto EmitSize =
          (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
          (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

        if (!isRegSym && instr->opc == ARM_OPC_CMP) {
            if ((Imm >> 12) != 0) {
              LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Imm);
              cmp(EmitSize, Dst, (ARMEmitter::Reg::r20).X());
            } else
              cmp(EmitSize, Dst, Imm);
            FlipCF();

        } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW || isRegSym) {
            if (!isRegSym && instr->opc == ARM_OPC_CMPB)
              Reg0Size = 1;
            else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
              Reg0Size = 2;
            adjust_8_16_cmp(true, Reg0Size, Imm);

        } else if (instr->opc == ARM_OPC_CMN) {
            cmn(EmitSize, Dst, Imm);
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
        ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
        auto Src = GetRegMap(ARMReg);

        const auto EmitSize =
          (Reg1Size & 0x3) ? ARMEmitter::Size::i32Bit :
          (Reg1Size == 4) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

        if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
            auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
            uint32_t amt = opd1->content.reg.scale.imm.content.val;

            if (!isRegSym && instr->opc == ARM_OPC_CMP) {
                cmp(EmitSize, Dst, Src, Shift, amt);
                FlipCF();
            } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW || isRegSym) {
                if (!isRegSym && instr->opc == ARM_OPC_CMPB)
                  Reg0Size = 1;
                else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
                  Reg0Size = 2;
                adjust_8_16_cmp(false, Reg0Size, 0, Src);
            } else {
                cmn(EmitSize, Dst, Src, Shift, amt);
            }

        } else if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
            auto Option = GetExtendType(opd1->content.reg.scale.content.extend);
            uint32_t Shift = opd1->content.reg.scale.imm.content.val;

            if (!isRegSym && instr->opc == ARM_OPC_CMP) {
                cmp(EmitSize, Dst, Src, Option, Shift);
                FlipCF();
            } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW || isRegSym) {
                if (!isRegSym && instr->opc == ARM_OPC_CMPB)
                  Reg0Size = 1;
                else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
                  Reg0Size = 2;
                adjust_8_16_cmp(false, Reg0Size, 0, Src);
            } else {
                cmn(EmitSize, Dst, Src, Option, Shift);
            }

        } else if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
            if (!isRegSym && instr->opc == ARM_OPC_CMP) {
                cmp(EmitSize, Dst, Src);
                FlipCF();
            } else if (instr->opc == ARM_OPC_CMPB || instr->opc == ARM_OPC_CMPW || isRegSym) {
                if (!isRegSym && instr->opc == ARM_OPC_CMPB)
                  Reg0Size = 1;
                else if (!isRegSym && instr->opc == ARM_OPC_CMPW)
                  Reg0Size = 2;
                adjust_8_16_cmp(false, Reg0Size, 0, Src);
            } else {
                cmn(EmitSize, Dst, Src);
            }
        }

    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for compare instruction.");
}


DEF_OPC(CSEX) {
    ARMOperand *opd0 = &instr->opd[0];
    uint8_t     OpSize = instr->OpSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    const auto EmitSize =
      (Reg0Size & 0x3 || OpSize == 4) ? ARMEmitter::Size::i32Bit :
      (Reg0Size == 4 || OpSize == 8) ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (instr->opd_num == 1 && opd0->type == ARM_OPD_TYPE_REG) {
        cset(EmitSize, Dst, MapBranchCC(instr->cc));
    } else if (instr->opd_num == 3 && opd0->type == ARM_OPD_TYPE_REG) {
      ARMOperand *opd1 = &instr->opd[1];
      ARMOperand *opd2 = &instr->opd[2];

      if (opd2->content.reg.num != ARM_REG_INVALID) {
        ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
        auto Src1 = GetRegMap(ARMReg);
        ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
        auto Src2 = GetRegMap(ARMReg);

        csel(EmitSize, Dst, Src1, Src2, MapBranchCC(instr->cc));
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

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    const auto Dst = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    const auto Src = GetRegMap(ARMReg);
    uint32_t lsb = get_imm_map_wrapper(&opd2->content.imm);
    uint32_t width = get_imm_map_wrapper(&opd3->content.imm);

    // If Dst and SrcDst match then this turns in to a single instruction.
    bfxil(EmitSize, Dst, Src, lsb, width);
}


DEF_OPC(B) {
    ARMOperand *opd = &instr->opd[0];
    ARMConditionCode cond = instr->cc;
    const auto EmitSize = ARMEmitter::Size::i64Bit;

    // get new rip
    uint64_t target, fallthrough;
    get_label_map(opd->content.imm.content.sym, &target, &fallthrough);
    this->TrueNewRip = fallthrough + target;
    this->FalseNewRip = fallthrough;

    if (cond == ARM_CC_AL) {
      PendingTargetLabel = &JumpTargets2.try_emplace(this->TrueNewRip).first->second;
    } else {

      auto TrueTargetLabel = &JumpTargets2.try_emplace(this->TrueNewRip).first->second;

      if (cond == ARM_CC_LS) {
        // b.ls (CF=0 || ZF=1)
        mov((ARMEmitter::Reg::r20).W(), 1);
        cset(EmitSize, (ARMEmitter::Reg::r21).X(), MapBranchCC(ARM_CC_CS));
        csel(EmitSize, (ARMEmitter::Reg::r20).X(), (ARMEmitter::Reg::r20).X(), (ARMEmitter::Reg::r21).X(), MapBranchCC(ARM_CC_EQ));
        cbnz(EmitSize, (ARMEmitter::Reg::r20).X(), TrueTargetLabel);
      } else if (cond == ARM_CC_HI) {
        // b.hi (CF=1 && ZF=0)
        cset(EmitSize, (ARMEmitter::Reg::r20).X(), MapBranchCC(ARM_CC_CC));
        csel(EmitSize, (ARMEmitter::Reg::r20).X(), (ARMEmitter::Reg::r20).X(), (ARMEmitter::Reg::zr).X(), MapBranchCC(ARM_CC_NE));
        cbnz(EmitSize, (ARMEmitter::Reg::r20).X(), TrueTargetLabel);
      } else
        b(MapBranchCC(cond), TrueTargetLabel);

      PendingTargetLabel = &JumpTargets2.try_emplace(this->FalseNewRip).first->second;
    }

    PendingTargetLabel = nullptr;
}


DEF_OPC(CBNZ) {
    ARMOperand *opd = &instr->opd[0];
    uint8_t OpSize = instr->OpSize;

    uint32_t Reg0Size;
    auto ARMReg = GetGuestRegMap(opd->content.reg.num, Reg0Size);
    auto Src = GetRegMap(ARMReg);

    // get new rip
    uint64_t target, fallthrough;
    get_label_map(opd->content.imm.content.sym, &target, &fallthrough);
    this->TrueNewRip = fallthrough + target;
    this->FalseNewRip = fallthrough;

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    auto TrueTargetLabel = &JumpTargets2.try_emplace(this->TrueNewRip).first->second;

    if (instr->opc == ARM_OPC_CBNZ)
      cbnz(EmitSize, Src, TrueTargetLabel);
    else
      cbz(EmitSize, Src, TrueTargetLabel);

    // PendingTargetLabel = &JumpTargets.try_emplace(Op->FalseBlock.ID()).first->second;
    PendingTargetLabel = &JumpTargets2.try_emplace(this->FalseNewRip).first->second;
}


DEF_OPC(SET_JUMP) {
    ARMOperand *opd = &instr->opd[0];
    uint8_t  OpSize = instr->OpSize;
    uint64_t target, fallthrough, Mask = ~0ULL;

    if (OpSize == 4) {
      Mask = 0xFFFF'FFFFULL;
    }

    if (opd->type == ARM_OPD_TYPE_IMM) {
        get_label_map(opd->content.imm.content.sym, &target, &fallthrough);

        LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, fallthrough & Mask);
        LoadConstant(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, target);
        add(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::Reg::r20);
        // Set New RIP Reg
        RipReg = ARM_REG_R20;
    } else if (opd->type == ARM_OPD_TYPE_REG) {
        uint32_t Reg0Size;
        auto Src = GetGuestRegMap(opd->content.reg.num, Reg0Size);
        // Set New RIP Reg
        RipReg = Src;
    }

    // operand type is reg don't need to be processed.
}


DEF_OPC(SET_CALL) {
    ARMOperand *opd = &instr->opd[0];
    uint8_t  OpSize = instr->OpSize;
    uint64_t target, fallthrough, Mask = ~0ULL;

    if (OpSize == 4) {
      Mask = 0xFFFF'FFFFULL;
    }

    auto MemSrc = ARMEmitter::ExtendedMemOperand((ARMEmitter::Reg::r8).X(), ARMEmitter::IndexType::PRE, -0x8);

    if (instr->opd_num && opd->type == ARM_OPD_TYPE_IMM) {
        get_label_map(opd->content.imm.content.sym, &target, &fallthrough);
        LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r20).X(), fallthrough & Mask);

        if ((target >> 12) != 0) {
          LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(), target);
          add(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(), (ARMEmitter::Reg::r20).X(), (ARMEmitter::Reg::r21).X());
        } else {
          add(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(), (ARMEmitter::Reg::r20).X(), target);
        }
        str((ARMEmitter::Reg::r20).X(), MemSrc);
        // Set New RIP Reg
        RipReg = ARM_REG_R21;
    } else if (instr->opd_num && opd->type == ARM_OPD_TYPE_REG) {
        LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r20).X(), rrule->target_pc & Mask);
        str((ARMEmitter::Reg::r20).X(), MemSrc);
        // Set New RIP Reg
        uint32_t Reg0Size;
        auto Src = GetGuestRegMap(opd->content.reg.num, Reg0Size);
        RipReg = Src;
    } else if (!instr->opd_num) { // only push
        LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(), rrule->target_pc & Mask);
        str((ARMEmitter::Reg::r21).X(), MemSrc);
        // Set New RIP Reg
        RipReg = ARM_REG_R20;
    }
}


DEF_OPC(PC_L) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint64_t target, fallthrough, Mask = ~0ULL;

    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);

    if (instr->OpSize == 4) {
      Mask = 0xFFFF'FFFFULL;
    }

    if (opd1->type == ARM_OPD_TYPE_REG) {
        ARMOperand *opd2 = &instr->opd[2];
        ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
        auto RIPDst = GetRegMap(ARMReg);

        get_label_map(opd2->content.imm.content.sym, &target, &fallthrough);
        LoadConstant(ARMEmitter::Size::i64Bit, RIPDst.X(), (fallthrough + target) & Mask);

        auto MemSrc = ARMEmitter::ExtendedMemOperand(RIPDst.X(), ARMEmitter::IndexType::OFFSET, 0x0);
        if (instr->opc == ARM_OPC_PC_LB)
          ldrb(Dst, MemSrc);
        else
          ldr(Dst.X(), MemSrc);

    } else if (opd1->type == ARM_OPD_TYPE_IMM) {
        get_label_map(opd1->content.imm.content.sym, &target, &fallthrough);
        LoadConstant(ARMEmitter::Size::i64Bit, Dst.X(), (fallthrough + target) & Mask);
    }
}


DEF_OPC(PC_S) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint64_t target, fallthrough, Mask = ~0ULL;

    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Src = GetRegMap(ARMReg);

    if (instr->OpSize == 4) {
      Mask = 0xFFFF'FFFFULL;
    }

    if (opd1->type == ARM_OPD_TYPE_REG) {
      ARMOperand *opd2 = &instr->opd[2];
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
      auto RIPDst = GetRegMap(ARMReg);

      get_label_map(opd2->content.imm.content.sym, &target, &fallthrough);
      LoadConstant(ARMEmitter::Size::i64Bit, RIPDst.X(), (fallthrough + target) & Mask);

      auto MemSrc = ARMEmitter::ExtendedMemOperand(RIPDst.X(), ARMEmitter::IndexType::OFFSET, 0x0);
      if (instr->opc == ARM_OPC_PC_SB)
        strb(Src, MemSrc);
      else
        str(Src.X(), MemSrc);
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
    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto VectorLower = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto VectorUpper = GetVRegMap(ARMReg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8, "addp Invalid size");
    const auto SubRegSize =
      ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
      ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
      ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
      ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i8Bit;

    if (HostSupportsSVE256 && Is256Bit) {
      const auto Pred = PRED_TMP_32B.Merging();

      // SVE ADDP is a destructive operation, so we need a temporary
      movprfx(VTMP1.Z(), VectorLower.Z());

      // Unlike Adv. SIMD's version of ADDP, which acts like it concats the
      // upper vector onto the end of the lower vector and then performs
      // pairwise addition, the SVE version actually interleaves the
      // results of the pairwise addition (gross!), so we need to undo that.
      addp(SubRegSize, VTMP1.Z(), Pred, VTMP1.Z(), VectorUpper.Z());
      uzp1(SubRegSize, Dst.Z(), VTMP1.Z(), VTMP1.Z());
      uzp2(SubRegSize, VTMP2.Z(), VTMP1.Z(), VTMP1.Z());

      // Merge upper half with lower half.
      splice<ARMEmitter::OpType::Destructive>(ARMEmitter::SubRegSize::i64Bit, Dst.Z(), PRED_TMP_16B, Dst.Z(), VTMP2.Z());
    } else {
      if (IsScalar) {
        addp(SubRegSize, Dst.D(), VectorLower.D(), VectorUpper.D());
      } else {
        addp(SubRegSize, Dst.Q(), VectorLower.Q(), VectorUpper.Q());
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
    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(DstReg);
    auto Vec1Reg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Vector1 = GetVRegMap(Vec1Reg);
    auto Vec2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto Vector2 = GetVRegMap(Vec2Reg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8 || ElementSize == 16, "cmeq Invalid size");
    const auto SubRegSize = ARMEmitter::ToVectorSizePair(
      ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
      ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
      ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
      ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i128Bit);

    if (HostSupportsSVE256 && Is256Bit) {
      // FIXME: We should rework this op to avoid the NZCV spill/fill dance.
      mrs(TMP1, ARMEmitter::SystemRegister::NZCV);

      const auto Mask = PRED_TMP_32B.Zeroing();
      const auto ComparePred = ARMEmitter::PReg::p0;

      // General idea is to compare for equality, not the equal vals
      // from one of the registers, then or both together to make the
      // relevant equal entries all 1s.
      cmpeq(SubRegSize.Vector, ComparePred, Mask, Vector1.Z(), Vector2.Z());
      not_(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(), Vector1.Z());
      movprfx(SubRegSize.Vector, Dst.Z(), ComparePred.Zeroing(), Vector1.Z());
      orr(SubRegSize.Vector, Dst.Z(), ComparePred.Merging(), Dst.Z(), VTMP1.Z());

      // Restore NZCV
      msr(ARMEmitter::SystemRegister::NZCV, TMP1);
    } else {
      if (IsScalar) {
        cmeq(SubRegSize.Scalar, Dst, Vector1, Vector2);
      } else {
        cmeq(SubRegSize.Vector, Dst.Q(), Vector1.Q(), Vector2.Q());
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
    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(DstReg);
    auto Vec1Reg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Vector1 = GetVRegMap(Vec1Reg);
    auto Vec2Reg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto Vector2 = GetVRegMap(Vec2Reg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8 || ElementSize == 16, "cmgt Invalid size");
    const auto SubRegSize = ARMEmitter::ToVectorSizePair(
      ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
      ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
      ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
      ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i128Bit);

    if (HostSupportsSVE256 && Is256Bit) {
      const auto Mask = PRED_TMP_32B.Zeroing();
      const auto ComparePred = ARMEmitter::PReg::p0;

      // FIXME: We should rework this op to avoid the NZCV spill/fill dance.
      mrs(TMP1, ARMEmitter::SystemRegister::NZCV);

      // General idea is to compare for greater-than, bitwise NOT
      // the valid values, then ORR the NOTed values with the original
      // values to form entries that are all 1s.
      cmpgt(SubRegSize.Vector, ComparePred, Mask, Vector1.Z(), Vector2.Z());
      not_(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(), Vector1.Z());
      movprfx(SubRegSize.Vector, Dst.Z(), ComparePred.Zeroing(), Vector1.Z());
      orr(SubRegSize.Vector, Dst.Z(), ComparePred.Merging(), Dst.Z(), VTMP1.Z());

      // Restore NZCV
      msr(ARMEmitter::SystemRegister::NZCV, TMP1);
    } else {
      if (IsScalar) {
        cmgt(SubRegSize.Scalar, Dst, Vector1, Vector2);
      } else {
        cmgt(SubRegSize.Vector, Dst.Q(), Vector1.Q(), Vector2.Q());
      }
    }
}


DEF_OPC(CMLT) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    const auto OpSize = instr->OpSize;

    const auto ElementSize = instr->ElementSize;
    const auto IsScalar = ElementSize == OpSize;
    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

    uint32_t Reg0Size, Reg1Size;
    auto DstReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(DstReg);
    auto VecReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Vector = GetVRegMap(VecReg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8 || ElementSize == 16, "cmlt Invalid size");
    const auto SubRegSize = ARMEmitter::ToVectorSizePair(
      ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
      ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
      ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
      ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i128Bit);


    if (HostSupportsSVE256 && Is256Bit) {
      const auto Mask = PRED_TMP_32B.Zeroing();
      const auto ComparePred = ARMEmitter::PReg::p0;

      // FIXME: We should rework this op to avoid the NZCV spill/fill dance.
      mrs(TMP1, ARMEmitter::SystemRegister::NZCV);

      // Ensure no junk is in the temp (important for ensuring
      // non less-than values remain as zero).
      mov_imm(ARMEmitter::SubRegSize::i64Bit, VTMP1.Z(), 0);
      cmplt(SubRegSize.Vector, ComparePred, Mask, Vector.Z(), 0);
      not_(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(), Vector.Z());
      orr(SubRegSize.Vector, VTMP1.Z(), ComparePred.Merging(), VTMP1.Z(), Vector.Z());
      mov(Dst.Z(), VTMP1.Z());

      // Restore NZCV
      msr(ARMEmitter::SystemRegister::NZCV, TMP1);
    } else {
      if (IsScalar) {
        cmlt(SubRegSize.Scalar, Dst, Vector);
      } else {
        cmlt(SubRegSize.Vector, Dst.Q(), Vector.Q());
      }
    }
}


DEF_OPC(DUP) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    const auto OpSize = instr->OpSize;

    const auto Index = opd1->content.reg.Index;
    const auto ElementSize = instr->ElementSize;
    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
    const auto Is128Bit = OpSize == Core::CPUState::XMM_SSE_REG_SIZE;

    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(ARMReg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8, "dup Invalid size");
    const auto SubRegSize =
      ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
      ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
      ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
      ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i128Bit;

    if (opd1->type == ARM_OPD_TYPE_REG) {
      ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);

      if (ARMReg >= ARM_REG_R0 && ARMReg <= ARM_REG_R31) {
        const auto Src = GetRegMap(ARMReg);
        if (HostSupportsSVE256 && Is256Bit) {
          dup(SubRegSize, Dst.Z(), Src);
        } else {
          dup(SubRegSize, Dst.Q(), Src);
        }
      }
      else if (ARMReg >= ARM_REG_V0 && ARMReg <= ARM_REG_V31) {
        const auto Vector = GetVRegMap(ARMReg);
        if (HostSupportsSVE256 && Is256Bit) {
          dup(SubRegSize, Dst.Z(), Vector.Z(), Index);
        } else {
          if (Is128Bit)
            dup(SubRegSize, Dst.Q(), Vector.Q(), Index);
          else
            dup(SubRegSize, Dst.D(), Vector.D(), Index);
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

    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
    const auto ElementSize = instr->ElementSize;
    auto Index = get_imm_map_wrapper(&opd3->content.imm);

    // AArch64 ext op has bit arrangement as [Vm:Vn] so arguments need to be swapped
    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto UpperBits = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto LowerBits = GetVRegMap(ARMReg);

    if (Index >= OpSize) {
      // Upper bits have moved in to the lower bits
      LowerBits = UpperBits;

      // Upper bits are all now zero
      UpperBits = VTMP1;
      movi(ARMEmitter::SubRegSize::i64Bit, VTMP1.Q(), 0);
      Index -= OpSize;
    }

    const auto CopyFromByte = Index * ElementSize;

    if (HostSupportsSVE256 && Is256Bit) {
      if (Dst == LowerBits) {
        // Trivial case where we don't need to do any moves
        ext<ARMEmitter::OpType::Destructive>(Dst.Z(), Dst.Z(), UpperBits.Z(), CopyFromByte);
      } else if (Dst == UpperBits) {
        movprfx(VTMP2.Z(), LowerBits.Z());
        ext<ARMEmitter::OpType::Destructive>(VTMP2.Z(), VTMP2.Z(), UpperBits.Z(), CopyFromByte);
        mov(Dst.Z(), VTMP2.Z());
      } else {
        // No registers alias the destination, so we can safely move into it.
        movprfx(Dst.Z(), LowerBits.Z());
        ext<ARMEmitter::OpType::Destructive>(Dst.Z(), Dst.Z(), UpperBits.Z(), CopyFromByte);
      }
    } else {
      if (OpSize == 8) {
        ext(Dst.D(), LowerBits.D(), UpperBits.D(), CopyFromByte);
      } else {
        ext(Dst.Q(), LowerBits.Q(), UpperBits.Q(), CopyFromByte);
      }
    }
}


DEF_OPC(FMOV) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    auto ElementSize = instr->ElementSize;

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

      if (Reg1Size && !ElementSize) ElementSize = 1 << (Reg1Size-1);

      switch (ElementSize) {
        case 1:
          uxtb(ARMEmitter::Size::i32Bit, TMP1, Src);
          fmov(ARMEmitter::Size::i32Bit, Dst.S(), TMP1);
          break;
        case 2:
          uxth(ARMEmitter::Size::i32Bit, TMP1, Src);
          fmov(ARMEmitter::Size::i32Bit, Dst.S(), TMP1);
          break;
        case 4:
          fmov(ARMEmitter::Size::i32Bit, Dst.S(), Src);
          break;
        case 8:
          fmov(ARMEmitter::Size::i64Bit, Dst.D(), Src);
          break;
        default: LOGMAN_MSG_A_FMT("Unknown castGPR element size: {}", ElementSize);
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
        ins(ARMEmitter::SubRegSize::i8Bit, guest, gregOffs, host, hregOffs);
        break;
      case 2:
        ins(ARMEmitter::SubRegSize::i16Bit, guest, gregOffs, host, hregOffs);
        break;
      case 4:
        // XXX: This had a bug with insert of size 16bit
        ins(ARMEmitter::SubRegSize::i32Bit, guest, gregOffs, host, hregOffs);
        break;
      case 8:
        // XXX: This had a bug with insert of size 16bit
        ins(ARMEmitter::SubRegSize::i64Bit, guest, gregOffs, host, hregOffs);
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

    constexpr auto SSERegBitSize = Core::CPUState::XMM_SSE_REG_SIZE * 8;
    const auto ElementSizeBits = ElementSize * 8;
    const auto Offset = ElementSizeBits * opd1->content.reg.Index;

    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto Vector = GetVRegMap(ARMReg);

    if (Reg0Size && !ElementSize) ElementSize = 1 << (Reg0Size-1);

    const auto PerformMove = [&](const ARMEmitter::VRegister reg, int index) {
      switch (ElementSize) {
        case 1:
          umov<ARMEmitter::SubRegSize::i8Bit>(Dst, Vector, index);
          break;
        case 2:
          umov<ARMEmitter::SubRegSize::i16Bit>(Dst, Vector, index);
          break;
        case 4:
          umov<ARMEmitter::SubRegSize::i32Bit>(Dst, Vector, index);
          break;
        case 8:
          umov<ARMEmitter::SubRegSize::i64Bit>(Dst, Vector, index);
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
    ARMOperand *opd2 = &instr->opd[2];
    const auto OpSize = instr->OpSize;

    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;
    const auto ElementSize = instr->ElementSize;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto DstSrc = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto MemReg = GetRegMap(ARMReg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 ||
                        ElementSize == 4 || ElementSize == 8 ||
                        ElementSize == 16, "Invalid element size");

    if (Is256Bit) {
      LOGMAN_MSG_A_FMT("Unsupported 256-bit VLoadVectorElement");
    } else {
      if (Dst != DstSrc && ElementSize != 16) {
        mov(Dst.Q(), DstSrc.Q());
      }
      switch (ElementSize) {
      case 1:
        ld1<ARMEmitter::SubRegSize::i8Bit>(Dst.Q(), opd0->content.reg.Index, MemReg);
        break;
      case 2:
        ld1<ARMEmitter::SubRegSize::i16Bit>(Dst.Q(), opd0->content.reg.Index, MemReg);
        break;
      case 4:
        ld1<ARMEmitter::SubRegSize::i32Bit>(Dst.Q(), opd0->content.reg.Index, MemReg);
        break;
      case 8:
        ld1<ARMEmitter::SubRegSize::i64Bit>(Dst.Q(), opd0->content.reg.Index, MemReg);
        break;
      case 16:
        ldr(Dst.Q(), MemReg);
        break;
      default:
        LOGMAN_MSG_A_FMT("Unhandled {} size: {}", __func__, ElementSize);
        return;
      }
    }

    // Emit a half-barrier if TSO is enabled.
    if (CTX->IsAtomicTSOEnabled()) {
      dmb(ARMEmitter::BarrierScope::ISHLD);
    }
}


DEF_OPC(SQXTUN) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    const auto OpSize = instr->OpSize;

    const auto ElementSize = instr->ElementSize;
    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

    uint32_t Reg0Size, Reg1Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(ARMReg);
    auto VectorLower = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto VectorUpper = GetVRegMap(ARMReg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8, "aqxtun Invalid size");
    const auto SubRegSize =
      ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
      ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
      ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
      ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i8Bit;

    if (HostSupportsSVE256 && Is256Bit) {
      // This combines the SVE versions of VSQXTUN/VSQXTUN2.
      // Upper VSQXTUN2 handling.
      // Doing upper first to ensure it doesn't get overwritten by lower calculation.
      const auto Mask = PRED_TMP_16B;

      sqxtunb(SubRegSize, VTMP2.Z(), VectorUpper.Z());
      uzp1(SubRegSize, VTMP2.Z(), VTMP2.Z(), VTMP2.Z());

      // Look at those implementations for details about this.
      // Lower VSQXTUN handling.
      sqxtunb(SubRegSize, Dst.Z(), VectorLower.Z());
      uzp1(SubRegSize, Dst.Z(), Dst.Z(), Dst.Z());

      // Merge.
      splice<ARMEmitter::OpType::Destructive>(SubRegSize, Dst.Z(), Mask, Dst.Z(), VTMP2.Z());
    } else {
      if (OpSize == 8) {
        zip1(ARMEmitter::SubRegSize::i64Bit, Dst.Q(), VectorLower.Q(), VectorUpper.Q());
        sqxtun(SubRegSize, Dst, Dst);
      } else {
        if (Dst == VectorUpper) {
          // If the destination overlaps the upper then we need to move it temporarily.
          mov(VTMP1.Q(), VectorUpper.Q());
          VectorUpper = VTMP1;
        }
        sqxtun(SubRegSize, Dst, VectorLower);
        sqxtun2(SubRegSize, Dst, VectorUpper);
      }
    }
}


DEF_OPC(ZIP) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    const auto OpSize = instr->OpSize;

    const auto ElementSize = instr->ElementSize;
    const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

    uint32_t Reg0Size, Reg1Size, Reg2Size;
    auto ARMReg = GetGuestRegMap(opd0->content.reg.num, Reg0Size);
    auto Dst = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd1->content.reg.num, Reg1Size);
    auto VectorLower = GetVRegMap(ARMReg);
    ARMReg = GetGuestRegMap(opd2->content.reg.num, Reg2Size);
    auto VectorUpper = GetVRegMap(ARMReg);

    LOGMAN_THROW_AA_FMT(ElementSize == 1 || ElementSize == 2 || ElementSize == 4 || ElementSize == 8, "zip Invalid size");
    const auto SubRegSize =
      ElementSize == 1 ? ARMEmitter::SubRegSize::i8Bit :
      ElementSize == 2 ? ARMEmitter::SubRegSize::i16Bit :
      ElementSize == 4 ? ARMEmitter::SubRegSize::i32Bit :
      ElementSize == 8 ? ARMEmitter::SubRegSize::i64Bit : ARMEmitter::SubRegSize::i8Bit;

    if (HostSupportsSVE256 && Is256Bit) {
      if (instr->opc == ARM_OPC_ZIP1)
        zip1(SubRegSize, Dst.Z(), VectorLower.Z(), VectorUpper.Z());
      else
        zip2(SubRegSize, Dst.Z(), VectorLower.Z(), VectorUpper.Z());
    } else {
      if (OpSize == 8) {
        if (instr->opc == ARM_OPC_ZIP1)
          zip1(SubRegSize, Dst.D(), VectorLower.D(), VectorUpper.D());
        else
          zip2(SubRegSize, Dst.D(), VectorLower.D(), VectorUpper.D());
      } else {
        if (instr->opc == ARM_OPC_ZIP1)
          zip1(SubRegSize, Dst.Q(), VectorLower.Q(), VectorUpper.Q());
        else
          zip2(SubRegSize, Dst.Q(), VectorLower.Q(), VectorUpper.Q());
      }
    }
}


void FEXCore::CPU::Arm64JITCore::assemble_arm_instr(ARMInstruction *instr, RuleRecord *rrule)
{
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
            Opc_PC_L(instr, rrule);
            break;
        case ARM_OPC_PC_S:
        case ARM_OPC_PC_SB:
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
            LogMan::Msg::EFmt("Unsupported arm instruction in the assembler: {}, rule index: {}.",
                    get_arm_instr_opc(instr->opc), rrule->rule->index);
    }
}

void FEXCore::CPU::Arm64JITCore::assemble_arm_exit(uint64_t target_pc)
{
    ResetStack();

    if (target_pc != 0) {
      ARMEmitter::SingleUseForwardLabel l_BranchHost;
      ldr(TMP1, &l_BranchHost);
      blr(TMP1);

      Bind(&l_BranchHost);
      dc64(ThreadState->CurrentFrame->Pointers.Common.ExitFunctionLinker);
      dc64(target_pc);
    } else {
      ARMEmitter::SingleUseForwardLabel FullLookup;
      auto RipReg = GetRegMap(this->RipReg);

      // L1 Cache
      ldr(TMP1, STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.L1Pointer));

      and_(ARMEmitter::Size::i64Bit, TMP4, RipReg, LookupCache::L1_ENTRIES_MASK);
      add(TMP1, TMP1, TMP4, ARMEmitter::ShiftType::LSL, 4);

      // Note: sub+cbnz used over cmp+br to preserve flags.
      ldp<ARMEmitter::IndexType::OFFSET>(TMP2, TMP1, TMP1, 0);
      sub(TMP1, TMP1, RipReg.X());
      cbnz(ARMEmitter::Size::i64Bit, TMP1, &FullLookup);
      br(TMP2);

      Bind(&FullLookup);
      ldr(TMP1, STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.DispatcherLoopTop));
      str(RipReg.X(), STATE, offsetof(FEXCore::Core::CpuStateFrame, State.rip));
      br(TMP1);
    }
}

#undef DEF_OP
