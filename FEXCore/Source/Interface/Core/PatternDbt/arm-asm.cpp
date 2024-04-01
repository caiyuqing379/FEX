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

static ARMEmitter::Register GetHostRegMap(ARMRegister& reg)
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
      LOGMAN_MSG_A_FMT("Unsupported reg num");
      return reg_invalid;
  }
}

static uint64_t get_imm_map_wrapper(ARMImm *imm)
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
        case ARM_OPD_EXTEND_UXTW: return ARMEmitter::ExtendedMemOperand(Base.X(), GetHostRegMap(Index).X(), ARMEmitter::ExtendedType::UXTW, FEXCore::ilog2(amount) );
        case ARM_OPD_EXTEND_SXTW: return ARMEmitter::ExtendedMemOperand(Base.X(), GetHostRegMap(Index).X(), ARMEmitter::ExtendedType::SXTW, FEXCore::ilog2(amount) );
        case ARM_OPD_EXTEND_SXTX: return ARMEmitter::ExtendedMemOperand(Base.X(), GetHostRegMap(Index).X(), ARMEmitter::ExtendedType::SXTX, FEXCore::ilog2(amount) );
        default: LOGMAN_MSG_A_FMT("Unhandled GenerateExtMemOperand OffsetType: {}", OffsetScale.content.extend); break;
      }
    }
}

DEF_OPC(LDR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t    OpSize = instr->OpdSize;
    uint32_t   reg1size, reg2size;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
        auto ARMReg = get_guest_reg_map(opd0->content.reg.num, reg1size);
        auto Dst = GetHostRegMap(ARMReg);

        if (reg1size) OpSize = 1 << (reg1size-1);

        if (opd1->content.mem.base != ARM_REG_INVALID) {
          ARMRegister      Index = opd1->content.mem.index;
          ARMOperandScale  OffsetScale = opd1->content.mem.scale;
          ARMMemIndexType  PrePost = opd1->content.mem.pre_post;
          auto ARMReg = get_guest_reg_map(opd1->content.mem.base, reg2size);
          auto Base = GetHostRegMap(ARMReg);
          int32_t Imm = get_imm_map_wrapper(&opd1->content.mem.offset);

          auto MemSrc = GenerateExtMemOperand(Base, Index, Imm, OffsetScale, PrePost);

          if ((Index == ARM_REG_INVALID) && (PrePost == ARM_MEM_INDEX_TYPE_NONE) && (Imm < 0)) {
            sub(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Base, 0 - Imm);
            MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r20).X(), Index, 0, OffsetScale, PrePost);
          }

          if (instr->opc == ARM_OPC_LDRB || instr->opc == ARM_OPC_LDRH || instr->opc == ARM_OPC_LDR) {
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

          } else if (instr->opc == ARM_OPC_LDRSB) {
            if (OpSize == 4)
                ldrsb(Dst.W(), MemSrc);
            else
                ldrsb(Dst.X(), MemSrc);
          } else if (instr->opc == ARM_OPC_LDRSH) {
            if (OpSize == 4)
                ldrsh(Dst.W(), MemSrc);
            else
                ldrsh(Dst.X(), MemSrc);
          } else if (instr->opc == ARM_OPC_LDAR) {
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
    uint8_t    OpSize = instr->OpdSize;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_MEM) {
        auto RegPair1 = GetHostRegMap(opd0->content.reg.num);
        auto RegPair2 = GetHostRegMap(opd1->content.reg.num);
        auto Base     = GetHostRegMap(opd2->content.mem.base);
        ARMMemIndexType  PrePost = opd2->content.mem.pre_post;
        int32_t Imm = get_imm_map_wrapper(&opd2->content.mem.offset);

        if (OpSize == 8) {
          if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
            ldp<ARMEmitter::IndexType::PRE>(RegPair1.X(), RegPair2.X(), Base, Imm);
          else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
            ldp<ARMEmitter::IndexType::POST>(RegPair1.X(), RegPair2.X(), Base, Imm);
          else
            ldp<ARMEmitter::IndexType::OFFSET>(RegPair1.X(), RegPair2.X(), Base, Imm);
        } else if (OpSize == 4) {
          if (PrePost == ARM_MEM_INDEX_TYPE_PRE)
            ldp<ARMEmitter::IndexType::PRE>(RegPair1.W(), RegPair2.W(), Base, Imm);
          else if (PrePost == ARM_MEM_INDEX_TYPE_POST)
            ldp<ARMEmitter::IndexType::POST>(RegPair1.W(), RegPair2.W(), Base, Imm);
          else
            ldp<ARMEmitter::IndexType::OFFSET>(RegPair1.W(), RegPair2.W(), Base, Imm);
        } else
          LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
    } else
      LogMan::Msg::EFmt( "[arm] Unsupported operand type for ldp instruction.");
}

DEF_OPC(STR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t    OpSize = instr->OpdSize;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
        auto Src = GetHostRegMap(opd0->content.reg.num);

        if (opd1->content.mem.base != ARM_REG_INVALID) {
          ARMRegister      Index = opd1->content.mem.index;
          ARMOperandScale  OffsetScale = opd1->content.mem.scale;
          ARMMemIndexType  PrePost = opd1->content.mem.pre_post;
          auto Base = GetHostRegMap(opd1->content.mem.base);
          int32_t Imm = get_imm_map_wrapper(&opd1->content.mem.offset);

          auto MemSrc = GenerateExtMemOperand(Base, Index, Imm, OffsetScale, PrePost);

          if ((Index == ARM_REG_INVALID) && (PrePost == ARM_MEM_INDEX_TYPE_NONE) && (Imm < 0)) {
            sub(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, Base, 0 - Imm);
            MemSrc = GenerateExtMemOperand((ARMEmitter::Reg::r20).X(), Index, 0, OffsetScale, PrePost);
          }

          if (instr->opc == ARM_OPC_STRB || instr->opc == ARM_OPC_STRH || instr->opc == ARM_OPC_STR) {
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
        }
    } else
      LogMan::Msg::EFmt( "[arm] Unsupported operand type for str instruction.");
}

DEF_OPC(STP) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t    OpSize = instr->OpdSize;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_MEM) {
        auto RegPair1 = GetHostRegMap(opd0->content.reg.num);
        auto RegPair2 = GetHostRegMap(opd1->content.reg.num);
        auto Base = GetHostRegMap(opd2->content.mem.base);
        ARMMemIndexType  PrePost = opd2->content.mem.pre_post;
        int32_t Imm = get_imm_map_wrapper(&opd2->content.mem.offset);

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
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto Dst = GetHostRegMap(opd0->content.reg.num);
    const auto Src = GetHostRegMap(opd1->content.reg.num);

    if (OpSize == 4)
      sxtw(Dst.X(), Src.W());
    else
      LogMan::Msg::EFmt( "[arm] Unsupported operand size for SXTW instruction.");
}

DEF_OPC(MOV) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpdSize;

    ARMEmitter::Size EmitSize;
    uint32_t   reg1size, reg2size;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    auto ARMReg = get_guest_reg_map(opd0->content.reg.num, reg1size);
    auto Dst = GetHostRegMap(ARMReg);

    if ((reg1size & 0x3) || OpSize == 4)
        EmitSize = ARMEmitter::Size::i32Bit;
    else if (reg1size == 4 || OpSize == 8)
        EmitSize = ARMEmitter::Size::i64Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {

      if (opd1->content.reg.num != ARM_REG_INVALID) {
          ARMReg = get_guest_reg_map(opd1->content.reg.num, reg2size);
          auto SrcDst = GetHostRegMap(ARMReg);

          mov(EmitSize, Dst, SrcDst);  // move (register)
                                       // move (to/from SP) not support
      } else
          LogMan::Msg::EFmt( "[arm] Unsupported reg num for mov instruction.");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
        uint64_t Constant = get_imm_map_wrapper(&opd1->content.imm);
        LoadConstant(ARMEmitter::Size::i64Bit, Dst, Constant);
    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for mov instruction.");
}

DEF_OPC(MVN) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {

      if(opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto SrcDst = GetHostRegMap(opd1->content.reg.num);
        auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
        uint32_t amt = opd1->content.reg.scale.imm.content.val;
        mvn(EmitSize, Dst, SrcDst, Shift, amt);
      } else
        LogMan::Msg::EFmt( "[arm] Unsupported reg for mvn instruction.");

    } else
      LogMan::Msg::EFmt( "[arm] Unsupported operand type for mvn instruction.");
}

DEF_OPC(AND) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        int32_t Imm = get_imm_map_wrapper(&opd2->content.imm);

        if(instr->opc == ARM_OPC_AND)
          and_(EmitSize, Dst, Src1, Imm);  // and imm
        else
          ands(EmitSize, Dst, Src1, Imm);  // ands imm

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if(opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_AND)
          and_(EmitSize, Dst, Src1, Src2, Shift, amt);
        else
          ands(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else
        LogMan::Msg::EFmt( "[arm] Unsupported reg for and instruction.");

    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for and instruction.");
}

DEF_OPC(ORR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        uint64_t Imm = get_imm_map_wrapper(&opd2->content.imm);
        orr(EmitSize, Dst, Src1, Imm);

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;
        orr(EmitSize, Dst, Src1, Src2, Shift, amt);

      } else
        LogMan::Msg::EFmt( "[arm] Unsupported reg for orr instruction.");

    } else
      LogMan::Msg::EFmt( "[arm] Unsupported operand type for orr instruction.");
}

DEF_OPC(EOR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        int32_t Imm = get_imm_map_wrapper(&opd2->content.imm);
        eor(EmitSize, Dst, Src1, Imm);

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {
      auto Src2 = GetHostRegMap(opd2->content.reg.num);

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
          auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
          uint32_t amt = opd2->content.reg.scale.imm.content.val;
          eor(EmitSize, Dst, Src1, Src2, Shift, amt);
      } if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
          eor(EmitSize, Dst, Src1, Src2);
      } else
          LogMan::Msg::EFmt( "[arm] Unsupported reg for eor instruction.");

    } else
        LogMan::Msg::EFmt( "[arm] Unsupported operand type for eor instruction.");
}

DEF_OPC(BIC) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if(opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
          auto Src2 = GetHostRegMap(opd2->content.reg.num);
          auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
          uint32_t amt = opd2->content.reg.scale.imm.content.val;

          if(instr->opc == ARM_OPC_BIC)
              bic(EmitSize, Dst, Src1, Src2, Shift, amt);
          else
              bics(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else
          LogMan::Msg::IFmt( "[arm] Unsupported reg for bic instruction.");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for bic instruction.");
}

DEF_OPC(Shift) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    ARMOperand *opd2 = &instr->opd[2];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        int32_t shift = get_imm_map_wrapper(&opd2->content.imm);

        if(instr->opc == ARM_OPC_LSL)
          lsl(EmitSize, Dst, Src1, shift);
        else if (instr->opc == ARM_OPC_LSR)
          lsr(EmitSize, Dst, Src1, shift);
        else if (instr->opc == ARM_OPC_LSR)
          asr(EmitSize, Dst, Src1, shift);

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if(opd2->content.reg.num != ARM_REG_INVALID) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);

        if(instr->opc == ARM_OPC_LSL)
          lslv(EmitSize, Dst, Src1, Src2);
        else if (instr->opc == ARM_OPC_LSR)
          lsrv(EmitSize, Dst, Src1, Src2);
        else if (instr->opc == ARM_OPC_LSR)
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
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);
    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    auto Dst  = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        int32_t Imm = get_imm_map_wrapper(&opd2->content.imm);

        if(instr->opc == ARM_OPC_ADD)
          add(EmitSize, Dst, Src1, Imm);  // LSL12 default false
        else
          adds(EmitSize, Dst, Src1, Imm);

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE){
        auto Src2 = GetHostRegMap(opd2->content.reg.num);

        if(instr->opc == ARM_OPC_ADD)
          add(EmitSize, Dst, Src1, Src2);
        else
          adds(EmitSize, Dst, Src1, Src2);
      }
      else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto     Src2  = GetHostRegMap(opd2->content.reg.num);
        auto     Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt   = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_ADD)
          add(EmitSize, Dst, Src1, Src2, Shift, amt);
        else
          adds(EmitSize, Dst, Src1, Src2, Shift, amt);
      }
      else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
        auto     Src2   = GetHostRegMap(opd2->content.reg.num);
        auto     Option = GetExtendType(opd2->content.reg.scale.content.extend);
        uint32_t amt    = opd2->content.reg.scale.imm.content.val;

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
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {
      auto Dst = GetHostRegMap(opd0->content.reg.num);
      auto Src1 = GetHostRegMap(opd1->content.reg.num);
      auto Src2 = GetHostRegMap(opd2->content.reg.num);

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
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_IMM) {
        int32_t Imm = get_imm_map_wrapper(&opd2->content.imm);

        if(instr->opc == ARM_OPC_SUB)
          sub(EmitSize, Dst, Src1, Imm);  // LSL12 default false
        else {
          subs(EmitSize, Dst, Src1, Imm);
          FlipCF();
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_SUB)
          sub(EmitSize, Dst, Src1, Src2, Shift, amt);
        else {
          subs(EmitSize, Dst, Src1, Src2, Shift, amt);
          FlipCF();
        }
      } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Option = GetExtendType(opd2->content.reg.scale.content.extend);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_SUB)
          sub(EmitSize, Dst, Src1, Src2, Option, amt);
        else {
          subs(EmitSize, Dst, Src1, Src2, Option, amt);
          FlipCF();
        }
      } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE){
        auto Src2 = GetHostRegMap(opd2->content.reg.num);

        if(instr->opc == ARM_OPC_SUB)
          sub(EmitSize, Dst, Src1, Src2);
        else {
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
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {
      auto Dst = GetHostRegMap(opd0->content.reg.num);
      auto Src1 = GetHostRegMap(opd1->content.reg.num);
      auto Src2 = GetHostRegMap(opd2->content.reg.num);

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
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {
        auto Dst = GetHostRegMap(opd0->content.reg.num);
        auto Src1 = GetHostRegMap(opd1->content.reg.num);
        auto Src2 = GetHostRegMap(opd2->content.reg.num);

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
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
        auto Dst = GetHostRegMap(opd0->content.reg.num);
        auto Src = GetHostRegMap(opd1->content.reg.num);

        clz(EmitSize, Dst, Src);
    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for CLZ instruction.");
}


DEF_OPC(TST) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpdSize;

    bool isRegSym = false;
    ARMEmitter::Size EmitSize;
    uint32_t reg1size, reg2size;
    auto ARMReg = get_guest_reg_map(opd0->content.reg.num, reg1size);
    auto Dst = GetHostRegMap(ARMReg);

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    if ((reg1size & 0x3) || OpSize == 4)
        EmitSize = ARMEmitter::Size::i32Bit;
    else if (reg1size == 4 || OpSize == 8)
        EmitSize = ARMEmitter::Size::i64Bit;

    if (reg1size == 1 || reg1size == 2) isRegSym = true;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
        ARMReg = get_guest_reg_map(opd1->content.reg.num, reg2size);
        auto Src = GetHostRegMap(ARMReg);

        if (!isRegSym && opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
            auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
            uint32_t amt = opd1->content.reg.scale.imm.content.val;
            tst(EmitSize, Dst, Src, Shift, amt);
        } else if (!isRegSym && opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
            tst(EmitSize, Dst, Src);
        } else if (isRegSym && (Dst == Src)) {
            unsigned Shift = 32 - (reg1size * 8);
            cmn(EmitSize, ARMEmitter::Reg::zr, Dst, ARMEmitter::ShiftType::LSL, Shift);
        } else
            LogMan::Msg::EFmt("[arm] Unsupported reg for TST instruction.");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
        uint64_t Imm = get_imm_map_wrapper(&opd1->content.imm);
        const auto IsImm = vixl::aarch64::Assembler::IsImmLogical(Imm, RegSizeInBits(EmitSize));

        if (!isRegSym) {
            if (IsImm)
                tst(EmitSize, Dst, Imm);
            else {
                mov((ARMEmitter::Reg::r20).W(), Imm);
                and_(EmitSize, ARMEmitter::Reg::r26, Dst, ARMEmitter::Reg::r20);
                tst(EmitSize, ARMEmitter::Reg::r26, ARMEmitter::Reg::r26);
            }
        } else {
            unsigned Shift = 32 - (reg1size * 8);
            and_(EmitSize, ARMEmitter::Reg::r26, Dst, Imm);
            cmn(EmitSize, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, Shift);
        }

    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for TST instruction.");
}


DEF_OPC(COMPARE) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpdSize;

    bool isRegSym = false;
    ARMEmitter::Size EmitSize;
    uint32_t reg1size, reg2size;
    auto ARMReg = get_guest_reg_map(opd0->content.reg.num, reg1size);
    auto Dst = GetHostRegMap(ARMReg);

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    if ((reg1size & 0x3) || OpSize == 4)
        EmitSize = ARMEmitter::Size::i32Bit;
    else if (reg1size == 4 || OpSize == 8)
        EmitSize = ARMEmitter::Size::i64Bit;

    if (reg1size == 1 || reg1size == 2) isRegSym = true;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {
        uint64_t Imm = get_imm_map_wrapper(&opd1->content.imm);

        if (!isRegSym && instr->opc == ARM_OPC_CMP) {
            if ((Imm >> 12) != 0) {
                LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r20).X(), Imm);
                cmp(EmitSize, Dst, (ARMEmitter::Reg::r20).X());
            } else
                cmp(EmitSize, Dst, Imm);
            FlipCF();
        } else if (instr->opc == ARM_OPC_CMPB || isRegSym) {
            if (reg1size == 2) {
              uxth(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r27, Dst);
              LoadConstant(ARMEmitter::Size::i32Bit, (ARMEmitter::Reg::r20).X(), Imm);
              sub(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27, ARMEmitter::Reg::r20);
              cmn(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, 16);
            } else {
              if (reg1size == 1)
                uxtb(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r27, Dst);
              sub(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r26, ARMEmitter::Reg::r27, Imm);
              cmn(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, 24);
            }
            mrs((ARMEmitter::Reg::r20).X(), ARMEmitter::SystemRegister::NZCV);
            ubfx(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, ARMEmitter::Reg::r26, 8, 1);
            orr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::ShiftType::LSL, 29);
            bic(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r21, ARMEmitter::Reg::r27, ARMEmitter::Reg::r26);
            ubfx(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r21, ARMEmitter::Reg::r21, 7, 1);
            orr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::ShiftType::LSL, 28);
            msr(ARMEmitter::SystemRegister::NZCV, (ARMEmitter::Reg::r20).X());
        } else if (instr->opc == ARM_OPC_CMN) {
            cmn(EmitSize, Dst, Imm);
        }

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {
        ARMReg = get_guest_reg_map(opd1->content.reg.num, reg2size);
        auto Src = GetHostRegMap(ARMReg);

        if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
            auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
            uint32_t amt = opd1->content.reg.scale.imm.content.val;

            if (instr->opc == ARM_OPC_CMP) {
                cmp(EmitSize, Dst, Src, Shift, amt);
                FlipCF();
            } else
                cmn(EmitSize, Dst, Src, Shift, amt);

        } else if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
            auto Option = GetExtendType(opd1->content.reg.scale.content.extend);
            uint32_t Shift = opd1->content.reg.scale.imm.content.val;

            if (instr->opc == ARM_OPC_CMP) {
                cmp(EmitSize, Dst, Src, Option, Shift);
                FlipCF();
            } else
                cmn(EmitSize, Dst, Src, Option, Shift);

        } else if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_NONE) {
            if (instr->opc == ARM_OPC_CMP) {
                if (!isRegSym) {
                    cmp(EmitSize, Dst, Src);
                    FlipCF();
                } else {
                    uxtb(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20, Src);
                    uxtb(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r21, Dst);
                    sub(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r26, ARMEmitter::Reg::r21, ARMEmitter::Reg::r20);
                    //eor(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r27, ARMEmitter::Reg::r21, ARMEmitter::Reg::r20);
                    if (reg1size == 2)
                        cmn(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, 16);
                    else
                        cmn(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::zr, ARMEmitter::Reg::r26, ARMEmitter::ShiftType::LSL, 24);
                    mrs((ARMEmitter::Reg::r22).X(), ARMEmitter::SystemRegister::NZCV);
                    ubfx(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r23, ARMEmitter::Reg::r26, 8, 1);
                    orr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r22, ARMEmitter::Reg::r22, ARMEmitter::Reg::r23, ARMEmitter::ShiftType::LSL, 29);
                    eor(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::Reg::r20);
                    eor(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r21, ARMEmitter::Reg::r26, ARMEmitter::Reg::r21);
                    and_(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r21, ARMEmitter::Reg::r20);
                    ubfx(ARMEmitter::Size::i64Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r20, 7, 1);
                    orr(ARMEmitter::Size::i32Bit, ARMEmitter::Reg::r20, ARMEmitter::Reg::r22, ARMEmitter::Reg::r20, ARMEmitter::ShiftType::LSL, 28);
                    msr(ARMEmitter::SystemRegister::NZCV, (ARMEmitter::Reg::r20).X());
                }

            } else
                cmn(EmitSize, Dst, Src);
        }

    } else
        LogMan::Msg::EFmt("[arm] Unsupported operand type for compare instruction.");
}


IR::IROp_Header const * FEXCore::CPU::Arm64JITCore::FindIROp(IR::IROps tIROp)
{
    for (auto [BlockNode, BlockHeader] : this->IR->GetBlocks()) {
      for (auto [CodeNode, IROp] : this->IR->GetCode(BlockNode))
        if (IROp->Op == tIROp) return IROp;
      break;
    }
    return nullptr;
}


DEF_OPC(B) {
    IR::IROp_Header const *IROp = nullptr;
    ARMConditionCode cond = instr->cc;
    const auto EmitSize = ARMEmitter::Size::i64Bit;

    if (cond == ARM_CC_AL) {
      IROp = FindIROp(IR::IROps::OP_JUMP);

      const auto Op = IROp->C<IR::IROp_Jump>();

      PendingTargetLabel = &JumpTargets.try_emplace(Op->TargetBlock.ID()).first->second;
    } else {

      IROp = FindIROp(IR::IROps::OP_CONDJUMP);

      const auto Op = IROp->C<IR::IROp_CondJump>();

      auto TrueTargetLabel = &JumpTargets.try_emplace(Op->TrueBlock.ID()).first->second;

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

        PendingTargetLabel = &JumpTargets.try_emplace(Op->FalseBlock.ID()).first->second;
    }
}


DEF_OPC(CBNZ) {
    ARMOperand *opd0 = &instr->opd[0];
    uint8_t OpSize = instr->OpdSize;
    auto Src = GetHostRegMap(opd0->content.reg.num);

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);
    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    IR::IROp_Header const *IROp = FindIROp(IR::IROps::OP_CONDJUMP);

    auto Op = IROp->C<IR::IROp_CondJump>();

    auto TrueTargetLabel = &JumpTargets.try_emplace(Op->TrueBlock.ID()).first->second;

    if (instr->opc == ARM_OPC_CBNZ)
      cbnz(EmitSize, Src, TrueTargetLabel);
    else
      cbz(EmitSize, Src, TrueTargetLabel);

    PendingTargetLabel = &JumpTargets.try_emplace(Op->FalseBlock.ID()).first->second;
}

DEF_OPC(SET_JUMP) {
    ARMOperand *opd = &instr->opd[0];
    uint8_t  OpSize = instr->OpdSize;
    uint64_t target, fallthrough, Mask = ~0ULL;

    if (OpSize == 4) {
      Mask = 0xFFFF'FFFFULL;
    }

    if (opd->type == ARM_OPD_TYPE_IMM) {
      get_label_map(opd->content.imm.content.sym, &target, &fallthrough);

      LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r20).X(), fallthrough & Mask);
      LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(), target);
      add(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r20).X(), (ARMEmitter::Reg::r21).X(), (ARMEmitter::Reg::r20).X());
    }

    // operand type is reg don't need to be processed.
}


DEF_OPC(SET_CALL) {
    ARMOperand *opd = &instr->opd[0];
    uint8_t  OpSize = instr->OpdSize;
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
    } else if (instr->opd_num && opd->type == ARM_OPD_TYPE_REG) {
      LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r20).X(), rrule->target_pc & Mask);
      str((ARMEmitter::Reg::r20).X(), MemSrc);
    } else if (!instr->opd_num) { // only push
      LoadConstant(ARMEmitter::Size::i64Bit, (ARMEmitter::Reg::r21).X(), rrule->target_pc & Mask);
      str((ARMEmitter::Reg::r21).X(), MemSrc);
    }
}


DEF_OPC(PC_L) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t  OpSize = instr->OpdSize;
    uint64_t target, fallthrough, Mask = ~0ULL;

    auto Dst = GetHostRegMap(opd0->content.reg.num);

    if (OpSize == 4) {
      Mask = 0xFFFF'FFFFULL;
    }

    if (opd1->type == ARM_OPD_TYPE_REG) {
        ARMOperand *opd2 = &instr->opd[2];
        auto RIPDst = GetHostRegMap(opd1->content.reg.num);

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
    uint8_t  OpSize = instr->OpdSize;
    uint64_t target, fallthrough, Mask = ~0ULL;

    auto Src = GetHostRegMap(opd0->content.reg.num);

    if (OpSize == 4) {
      Mask = 0xFFFF'FFFFULL;
    }

    if (opd1->type == ARM_OPD_TYPE_REG) {
      ARMOperand *opd2 = &instr->opd[2];
      auto RIPDst = GetHostRegMap(opd1->content.reg.num);

      get_label_map(opd2->content.imm.content.sym, &target, &fallthrough);
      LoadConstant(ARMEmitter::Size::i64Bit, RIPDst.X(), (fallthrough + target) & Mask);

      auto MemSrc = ARMEmitter::ExtendedMemOperand(RIPDst.X(), ARMEmitter::IndexType::OFFSET, 0x0);
      if (instr->opc == ARM_OPC_PC_SB)
        strb(Src, MemSrc);
      else
        str(Src.X(), MemSrc);
    }
}


void FEXCore::CPU::Arm64JITCore::assemble_arm_instruction(ARMInstruction *instr, RuleRecord *rrule)
{
    print_arm_instr(instr);

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
        case ARM_OPC_CMN:
            Opc_COMPARE(instr, rrule);
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
        default:
            LogMan::Msg::IFmt("Unsupported x86 instruction in the assembler: {}, rule index: {}.",
                    get_arm_instr_opc(instr->opc), rrule->rule->index);
    }
}

void FEXCore::CPU::Arm64JITCore::assemble_arm_exit_tb(uint64_t target_pc)
{
  IR::IROp_Header const *IROp = FindIROp(IR::IROps::OP_EXITFUNCTION);

  if (!IROp) return;

  auto Op = IROp->C<IR::IROp_ExitFunction>();

  ResetStack();

  uint64_t NewRIP;

  if (IsInlineConstant(Op->NewRIP, &NewRIP) || IsInlineEntrypointOffset(Op->NewRIP, &NewRIP)) {
    ARMEmitter::SingleUseForwardLabel l_BranchHost;

    ldr(TMP1, &l_BranchHost);
    blr(TMP1);

    Bind(&l_BranchHost);
    dc64(ThreadState->CurrentFrame->Pointers.Common.ExitFunctionLinker);
    dc64(NewRIP);
  } else {

    ARMEmitter::SingleUseForwardLabel FullLookup;
    auto RipReg = GetReg(Op->NewRIP.ID());

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
