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

#define DEF_OPC(x) void FEXCore::CPU::Arm64JITCore::Opc_##x(ARMInstruction *instr, \
                                uint32_t *reg_liveness, RuleRecord *rrule)

static int h_reg_map_buf_index = 0;

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

static ARMEmitter::Register GetHostRegMap(ARMRegister reg)
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
                                            ARMImm Offset,
                                            ARMOperandScale OffsetScale,
                                            ARMMemIndexType OffsetType) {

    uint8_t amount = static_cast<uint8_t>(OffsetScale.imm.content.val);

    if (Offset.type != ARM_IMM_TYPE_NONE) {

        if (OffsetType == ARM_MEM_INDEX_TYPE_PRE)
          return ARMEmitter::ExtendedMemOperand(Base.X(), ARMEmitter::IndexType::PRE, Offset.content.val);
        else if (OffsetType == ARM_MEM_INDEX_TYPE_POST)
          return ARMEmitter::ExtendedMemOperand(Base.X(), ARMEmitter::IndexType::POST, Offset.content.val);
        else
          return ARMEmitter::ExtendedMemOperand(Base.X(), ARMEmitter::IndexType::OFFSET, Offset.content.val);

    } else if (Index != ARM_REG_INVALID) {

      switch(OffsetScale.content.extend) {
        case ARM_OPD_EXTEND_UXTW: return ARMEmitter::ExtendedMemOperand(Base.X(), GetHostRegMap(Index).X(), ARMEmitter::ExtendedType::UXTW, FEXCore::ilog2(amount) );
        case ARM_OPD_EXTEND_SXTW: return ARMEmitter::ExtendedMemOperand(Base.X(), GetHostRegMap(Index).X(), ARMEmitter::ExtendedType::SXTW, FEXCore::ilog2(amount) );
        case ARM_OPD_EXTEND_SXTX: return ARMEmitter::ExtendedMemOperand(Base.X(), GetHostRegMap(Index).X(), ARMEmitter::ExtendedType::SXTX, FEXCore::ilog2(amount) );
        default: LOGMAN_MSG_A_FMT("Unhandled GenerateExtMemOperand OffsetType: {}", OffsetScale.content.extend); break;
      }

    } else
       LOGMAN_MSG_A_FMT("Error GenerateExtMemOperand AddrOp!");
}

DEF_OPC(LDR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t    OpSize = instr->OpdSize;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
        auto Src = GetHostRegMap(opd0->content.reg.num);

        if (opd1->content.mem.base != ARM_REG_INVALID) {
          ARMRegister      Index = opd1->content.mem.index;
          ARMImm           Offset = opd1->content.mem.offset;
          ARMOperandScale  OffsetScale = opd1->content.mem.scale;
          ARMMemIndexType  PrePost = opd1->content.mem.pre_post;
          auto Base = GetHostRegMap(opd1->content.mem.base);

          auto MemSrc = GenerateExtMemOperand(Base, Index, Offset, OffsetScale, PrePost);

          if (instr->opc == ARM_OPC_LDRB || instr->opc == ARM_OPC_LDRH || instr->opc == ARM_OPC_LDR) {
            switch (OpSize) {
              case 1:
                ldrb(Src, MemSrc);
                break;
              case 2:
                ldrh(Src, MemSrc);
                break;
              case 4:
                ldr(Src.W(), MemSrc);
                break;
              case 8:
                ldr(Src.X(), MemSrc); // LDR（literal) not support
                break;
              default:
                LOGMAN_MSG_A_FMT("Unhandled LoadMem size: {}", OpSize);
                break;
            }

          } else if (instr->opc == ARM_OPC_LDRSB) {
            if (OpSize == 4)
                ldrsb(Src.W(), MemSrc);
            else
                ldrsb(Src.X(), MemSrc);
          } else if (instr->opc == ARM_OPC_LDRSH) {
            if (OpSize == 4)
                ldrsh(Src.W(), MemSrc);
            else
                ldrsh(Src.X(), MemSrc);
          }
        }
    } else
      LogMan::Msg::IFmt( "[arm] Unsupported operand type for ldr instruction.\n");
}

DEF_OPC(STR) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t    OpSize = instr->OpdSize;

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_MEM) {
        auto Src = GetHostRegMap(opd0->content.reg.num);

        if (opd1->content.mem.base != ARM_REG_INVALID) {
          auto Base = GetHostRegMap(opd1->content.mem.base);
          ARMRegister      Index = opd1->content.mem.index;
          ARMImm           Offset = opd1->content.mem.offset;
          ARMOperandScale  OffsetScale = opd1->content.mem.scale;
          ARMMemIndexType  PrePost = opd1->content.mem.pre_post;

          auto MemSrc = GenerateExtMemOperand(Base, Index, Offset, OffsetScale, PrePost);

          if (instr->opc == ARM_OPC_LDRB || instr->opc == ARM_OPC_LDRH || instr->opc == ARM_OPC_LDR) {
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
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for str instruction.\n");
}

DEF_OPC(MOV) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {

      if(opd1->content.reg.num != ARM_REG_INVALID) {
        auto SrcDst = GetHostRegMap(opd1->content.reg.num);
        mov(EmitSize, Dst, SrcDst);  // move (register)
                                     // move (to/from SP) not support
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported reg num for mov instruction.\n");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {

      if (opd1->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint32_t Imm = opd1->content.imm.content.val;
        mov(EmitSize, Dst, Imm);  //  Move wide immediate, by movz
      } else {
        LogMan::Msg::IFmt( "[arm] MOV instr imm type is sym.");
        uint64_t Constant = get_imm_map_wrapper(&opd1->content.imm);
        LoadConstant(ARMEmitter::Size::i64Bit, Dst, Constant);
      }

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for mov instruction.\n");
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
        LogMan::Msg::IFmt( "[arm] Unsupported reg for mvn instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for mvn instruction.\n");
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

      if (opd2->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint64_t Imm = opd2->content.imm.content.val;

        if(instr->opc == ARM_OPC_AND)
          and_(EmitSize, Dst, Src1, Imm);  // and imm
        else
          ands(EmitSize, Dst, Src1, Imm);  // adds imm
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for and instruction.\n");

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
        LogMan::Msg::IFmt( "[arm] Unsupported reg for and instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for and instruction.\n");
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

      if (opd2->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint64_t Imm = opd2->content.imm.content.val;

        orr(EmitSize, Dst, Src1, Imm);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for orr instruction.\n");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if(opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        orr(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported reg for orr instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for orr instruction.\n");
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

      if (opd2->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint64_t Imm = opd2->content.imm.content.val;

        eor(EmitSize, Dst, Src1, Imm);  // and imm
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for eor instruction.\n");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if(opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        eor(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported reg for eor instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for eor instruction.\n");
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
        LogMan::Msg::IFmt( "[arm] Unsupported reg for bic instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for bic instruction.\n");
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

      if (opd2->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint64_t shift = opd2->content.imm.content.val;

        if(instr->opc == ARM_OPC_LSL)
          lsl(EmitSize, Dst, Src1, shift);
        else if (instr->opc == ARM_OPC_LSR)
          lsr(EmitSize, Dst, Src1, shift);
        else if (instr->opc == ARM_OPC_LSR)
          asr(EmitSize, Dst, Src1, shift);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for shift instruction.\n");

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
        LogMan::Msg::IFmt( "[arm] Unsupported reg for shift instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for shift instruction.\n");
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

      if (opd2->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint64_t Imm = opd2->content.imm.content.val;

        if(instr->opc == ARM_OPC_ADD)
          add(EmitSize, Dst, Src1, Imm);  // LSL12 default false
        else
          adds(EmitSize, Dst, Src1, Imm);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for add instruction.\n");

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
        LogMan::Msg::IFmt( "[arm] Unsupported reg for add instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for add instruction.\n");
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
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for ADC instruction.\n");
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

      if (opd2->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint64_t Imm = opd2->content.imm.content.val;

        if(instr->opc == ARM_OPC_SUB)
          sub(EmitSize, Dst, Src1, Imm);  // LSL12 default false
        else
          subs(EmitSize, Dst, Src1, Imm);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for SUB instruction.\n");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG && opd2->type == ARM_OPD_TYPE_REG) {

      if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Shift = GetShiftType(opd2->content.reg.scale.content.direct);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_SUB)
          sub(EmitSize, Dst, Src1, Src2, Shift, amt);
        else
          subs(EmitSize, Dst, Src1, Src2, Shift, amt);
      } else if (opd2->content.reg.num != ARM_REG_INVALID && opd2->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
        auto Src2 = GetHostRegMap(opd2->content.reg.num);
        auto Option = GetExtendType(opd2->content.reg.scale.content.extend);
        uint32_t amt = opd2->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_SUB)
          sub(EmitSize, Dst, Src1, Src2, Option, amt);
        else
          subs(EmitSize, Dst, Src1, Src2, Option, amt);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported reg for SUB instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for SUB instruction.\n");
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

      if(instr->opc == ARM_OPC_SUB)
        sbc(EmitSize, Dst, Src1, Src2);
      else
        sbcs(EmitSize, Dst, Src1, Src2);

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for SBC instruction.\n");
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
      else if(instr->opc == ARM_OPC_SUB)
        smull(Dst.X(), Src1.W(), Src2.W());
    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for MUL instruction.\n");
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
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for CLZ instruction.\n");
}

DEF_OPC(TST) {
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

        tst(EmitSize, Dst, SrcDst, Shift, amt);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported reg for mvn instruction.\n");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {

      if (opd1->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint32_t Imm = opd1->content.imm.content.val;

        tst(EmitSize, Dst, Imm);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for TST instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for TST instruction.\n");
}

DEF_OPC(COMPARE) {
    ARMOperand *opd0 = &instr->opd[0];
    ARMOperand *opd1 = &instr->opd[1];
    uint8_t     OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);

    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;
    auto Dst = GetHostRegMap(opd0->content.reg.num);
    auto Src1 = GetHostRegMap(opd1->content.reg.num);

    if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_IMM) {

      if (opd1->content.imm.type == ARM_IMM_TYPE_VAL) {
        uint64_t Imm = opd1->content.imm.content.val;

        if(instr->opc == ARM_OPC_CMP)
          cmp(EmitSize, Dst, Imm);
        else
          cmn(EmitSize, Dst, Imm);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported imm type for compare instruction.\n");

    } else if (opd0->type == ARM_OPD_TYPE_REG && opd1->type == ARM_OPD_TYPE_REG) {

      if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_SHIFT) {
        auto Shift = GetShiftType(opd1->content.reg.scale.content.direct);
        uint32_t amt = opd1->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_CMP)
          cmp(EmitSize, Dst, Src1, Shift, amt);
        else
          cmn(EmitSize, Dst, Src1, Shift, amt);
      } else if (opd1->content.reg.num != ARM_REG_INVALID && opd1->content.reg.scale.type == ARM_OPD_SCALE_TYPE_EXT) {
        auto Option = GetExtendType(opd1->content.reg.scale.content.extend);
        uint32_t amt = opd1->content.reg.scale.imm.content.val;

        if(instr->opc == ARM_OPC_CMP)
          cmp(EmitSize, Dst, Src1, Option, amt);
        else
          cmn(EmitSize, Dst, Src1, Option, amt);
      } else
        LogMan::Msg::IFmt( "[arm] Unsupported reg for compare instruction.\n");

    } else
        LogMan::Msg::IFmt( "[arm] Unsupported operand type for compare instruction.\n");
}

IR::IROp_Header const * FEXCore::CPU::Arm64JITCore::FindIROp(IR::IROps tIROp)
{
  FEXCore::IR::IRListView const *IR = this->IR;
  for (auto [BlockNode, BlockHeader] : IR->GetBlocks()) {
    for (auto [CodeNode, IROp] : IR->GetCode(BlockNode)) {
      if (IROp->Op == tIROp) {
        return IROp;
      }
    }
  }
  return nullptr;
}

DEF_OPC(B) {
    ARMConditionCode cond = instr->cc;

    IR::IROp_Header const *IROp = nullptr;

    if (cond == ARM_CC_AL) {
      IROp = FindIROp(IR::IROps::OP_JUMP);

      const auto Op = IROp->C<IR::IROp_Jump>();

      PendingTargetLabel = &JumpTargets.try_emplace(Op->TargetBlock.ID()).first->second;
    } else {

      IROp = FindIROp(IR::IROps::OP_CONDJUMP);

      const auto Op = IROp->C<IR::IROp_CondJump>();

      auto TrueTargetLabel = &JumpTargets.try_emplace(Op->TrueBlock.ID()).first->second;

      b(MapBranchCC(cond), TrueTargetLabel);

      PendingTargetLabel = &JumpTargets.try_emplace(Op->FalseBlock.ID()).first->second;
    }
}

DEF_OPC(BL) {
    IR::IROp_Header const *IROp = FindIROp(IR::IROps::OP_JUMP);

    auto Op = IROp->C<IR::IROp_Jump>();

    auto Label = &JumpTargets.try_emplace(Op->TargetBlock.ID()).first->second;

    bl(Label);
}

DEF_OPC(CBNZ) {
    ARMOperand *opd0 = &instr->opd[0];
    auto     Src = GetHostRegMap(opd0->content.reg.num);
    uint8_t  OpSize = instr->OpdSize;

    LOGMAN_THROW_AA_FMT(OpSize == 4 || OpSize == 8, "Unsupported {} size: {}", __func__, OpSize);
    const auto EmitSize = OpSize == 8 ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

    IR::IROp_Header const *IROp = FindIROp(IR::IROps::OP_CONDJUMP);

    auto Op = IROp->C<IR::IROp_CondJump>();

    auto TrueTargetLabel = &JumpTargets.try_emplace(Op->TrueBlock.ID()).first->second;

    cbnz(EmitSize, Src, TrueTargetLabel);

    PendingTargetLabel = &JumpTargets.try_emplace(Op->FalseBlock.ID()).first->second;
}

void reset_asm_buffer(void)
{
    h_reg_map_buf_index = 0;
}

void FEXCore::CPU::Arm64JITCore::assemble_arm_instruction(ARMInstruction *instr, uint32_t *reg_liveness, RuleRecord *rrule)
{
    print_arm_instr(instr);

    switch (instr->opc) {
        case ARM_OPC_LDRB:
        case ARM_OPC_LDRSB:
        case ARM_OPC_LDRH:
        case ARM_OPC_LDRSH:
        case ARM_OPC_LDR:
            Opc_LDR(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_STR:
        case ARM_OPC_STRB:
        case ARM_OPC_STRH:
            Opc_STR(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_MOV:
            Opc_MOV(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_MVN:
            Opc_MVN(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_AND:
        case ARM_OPC_ANDS:
            Opc_AND(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_ORR:
            Opc_ORR(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_EOR:
            Opc_EOR(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_BIC:
        case ARM_OPC_BICS:
            Opc_BIC(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_LSL:
        case ARM_OPC_LSR:
        case ARM_OPC_ASR:
            Opc_Shift(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_ADD:
        case ARM_OPC_ADDS:
            Opc_ADD(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_ADC:
        case ARM_OPC_ADCS:
            Opc_ADC(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_SUB:
        case ARM_OPC_SUBS:
            Opc_SUB(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_SBC:
        case ARM_OPC_SBCS:
            Opc_SBC(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_MUL:
        case ARM_OPC_UMULL:
        case ARM_OPC_SMULL:
            Opc_MUL(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_CLZ:
            Opc_CLZ(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_TST:
            Opc_TST(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_CMP:
        case ARM_OPC_CMN:
            Opc_COMPARE(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_B:
            Opc_B(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_BL:
            Opc_BL(instr, reg_liveness, rrule);
            break;
        case ARM_OPC_CBNZ:
            Opc_CBNZ(instr, reg_liveness, rrule);
            break;
        default:
            LogMan::Msg::IFmt( "Unsupported x86 instruction in the assembler: {}, rule index: {}.\n",
                    get_arm_instr_opc(instr->opc), rrule->rule->index);
    }
}

void FEXCore::CPU::Arm64JITCore::assemble_arm_exit_tb(uint64_t target_pc)
{
  IR::IROp_Header const *IROp = FindIROp(IR::IROps::OP_EXITFUNCTION);

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
