#include <cstdint>

#define FLAG_OFFSET_CF 704
#define FLAG_OFFSET_PF 706
#define FLAG_OFFSET_AF 708
#define FLAG_OFFSET_ZF 710
#define FLAG_OFFSET_SF 711
#define FLAG_OFFSET_OF 715

#define XMM1_OFFSET_LOW 224
#define XMM1_OFFSET_HIGH 232

public:
  DEF_RV_OPC(Shifts) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];
    RISCVOperand *opd2 = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        int32_t shamt = GetRVImmMapWrapper(&opd2->content.imm);

        // shamt[4:0], x86 shift > riscv shamt?
        if (instr->opc == RISCV_OPC_SLLI) {
            RVAssembler->SLLI(rd, rs1, shamt);
        } else if (instr->opc == RISCV_OPC_SLLIW) {
            RVAssembler->SLLIW(rd, rs1, shamt);
        } else if (instr->opc == RISCV_OPC_SRLI) {
            RVAssembler->SRLI(rd, rs1, shamt);
        } else if (instr->opc == RISCV_OPC_SRLIW) {
            RVAssembler->SRLIW(rd, rs1, shamt);
        } else if (instr->opc == RISCV_OPC_SRAI) {
            RVAssembler->SRAI(rd, rs1, shamt);
        } else if (instr->opc == RISCV_OPC_SRAIW) {
            RVAssembler->SRAIW(rd, rs1, shamt);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_SLL) {
            RVAssembler->SLL(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_SLLW) {
            RVAssembler->SLLW(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_SRL) {
            RVAssembler->SRL(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_SRLW) {
            RVAssembler->SRLW(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_SRA) {
            RVAssembler->SRA(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_SRAW) {
            RVAssembler->SRAW(rd, rs1, rs2);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for shift instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for shift instruction.");
  }

  DEF_RV_OPC(ADD) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (instr->opc == RISCV_OPC_MV && opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {
        // Move (ADDI rd, rs, 0)
        RVAssembler->ADDI(rd, rs1, 0);

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        if (instr->opc == RISCV_OPC_ADDI && opd2->content.imm.pcrel == RISCV_IMM_PCREL_LO) {
            // get new rip
            uint64_t target, fallthrough;
            GetLabelMap(opd2->content.imm.content.sym, &target, &fallthrough);
            int32_t imm = target + fallthrough - rrule->entry;
            auto [new_lower, new_upper] = ProcessImmediate(imm);

            RVAssembler->LUI(biscuit::x5, static_cast<int32_t>(new_upper));
            RVAssembler->ADDI(biscuit::x5, biscuit::x5, new_lower);
            RVAssembler->ADD(rd, biscuit::x5, biscuit::x31);
            return;
        }

        int32_t Imm = GetRVImmMapWrapper(&opd2->content.imm);
        if (!biscuit::IsValidSigned12BitImm(Imm)) {
            auto [new_lower, new_upper] = ProcessImmediate(Imm);
            RVAssembler->LUI(biscuit::x5, static_cast<int32_t>(new_upper));
            RVAssembler->ADDI(biscuit::x5, biscuit::x5, new_lower);
            RVAssembler->ADD(rd, biscuit::x5, rs1);
            return;
        }

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_ADDI) {
          RVAssembler->ADDI(rd, rs1, Imm);
        } else if (instr->opc == RISCV_OPC_ADDIW) {
          RVAssembler->ADDIW(rd, rs1, Imm);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_ADD) {
            RVAssembler->ADD(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_ADDW) {
            RVAssembler->ADDW(rd, rs1, rs2);
        }
      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for ADD instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for ADD instruction.");
  }

  DEF_RV_OPC(SUB) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        // CF Flag?
        if (instr->opc == RISCV_OPC_SUB) {
            RVAssembler->SUB(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_SUBW) {
            RVAssembler->SUBW(rd, rs1, rs2);
        }
      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for sub instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for sub instruction.");
  }

  DEF_RV_OPC(LI) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
        int32_t Imm = GetRVImmMapWrapper(&opd1->content.imm);
        // I-immediate[11:0], x86 imm > riscv imm?
        RVAssembler->LI(rd, Imm);
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for LI instruction.");
  }

  DEF_RV_OPC(LUI) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    uint64_t target, fallthrough;

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
        // get new rip
        GetLabelMap(opd1->content.imm.content.sym, &target, &fallthrough);
        int32_t imm = fallthrough + target;

        if (instr->opc == RISCV_OPC_LUI) {
          RVAssembler->LUI(rd, imm);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for LUI instruction.");
  }

  DEF_RV_OPC(Logical) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];
    RISCVOperand *opd2 = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        int32_t imm = GetRVImmMapWrapper(&opd2->content.imm);

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_XORI) {
            RVAssembler->XORI(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_ORI) {
            RVAssembler->ORI(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_ANDI) {
            RVAssembler->ANDI(rd, rs1, imm);
            RVAssembler->NOT(biscuit::x5, rd);      // PF
            RVAssembler->SEQZ(biscuit::x6, rd);     // ZF
            RVAssembler->SB(biscuit::x0, FLAG_OFFSET_CF, biscuit::x9);
            RVAssembler->SB(biscuit::x5, FLAG_OFFSET_PF, biscuit::x9);
            RVAssembler->SB(biscuit::x6, FLAG_OFFSET_ZF, biscuit::x9);
            RVAssembler->SB(biscuit::x0, FLAG_OFFSET_SF, biscuit::x9);
            RVAssembler->SB(biscuit::x0, FLAG_OFFSET_OF, biscuit::x9);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_XOR) {
            RVAssembler->XOR(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_OR) {
            RVAssembler->OR(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_AND) {
            RVAssembler->AND(rd, rs1, rs2);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for Logical instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Logical instruction.");
  }

  DEF_RV_OPC(Compare) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];
    RISCVOperand *opd2 = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        int32_t imm = GetRVImmMapWrapper(&opd2->content.imm);

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_SLTI) {
            RVAssembler->SLTI(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_SLTIU) {
            RVAssembler->SLTIU(rd, rs1, imm);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_SLT) {
            RVAssembler->SLT(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_SLTU) {
            RVAssembler->SLTU(rd, rs1, rs2);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for Compare instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Compare instruction.");
  }

  DEF_RV_OPC(SAVE_FLAGS) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
    auto rs0 = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    RVAssembler->SLTU(biscuit::x6, rs0, rs1);
    RVAssembler->XOR(biscuit::x7, rs1, rs0);
    RVAssembler->SLLI(biscuit::x7, biscuit::x7, 59);
    RVAssembler->SRLI(biscuit::x7, biscuit::x7, 63);
    RVAssembler->NOT(biscuit::x28, rs0);
    RVAssembler->SRLI(biscuit::x29, rs0, 63);
    RVAssembler->SEQZ(biscuit::x25, rs0);
    RVAssembler->SLT(biscuit::x26, rs0, rs1);
    RVAssembler->SB(biscuit::x6, FLAG_OFFSET_CF, biscuit::x9);
    RVAssembler->SB(biscuit::x28, FLAG_OFFSET_PF, biscuit::x9);
    RVAssembler->SB(biscuit::x7, FLAG_OFFSET_AF, biscuit::x9);
    RVAssembler->SB(biscuit::x25, FLAG_OFFSET_ZF, biscuit::x9);
    RVAssembler->SB(biscuit::x29, FLAG_OFFSET_SF, biscuit::x9);
    RVAssembler->SB(biscuit::x26, FLAG_OFFSET_OF, biscuit::x9);
    RVAssembler->MV(rs1, rs0);
  }

  DEF_RV_OPC(CMP) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];

    uint32_t OpSize;
    auto rvreg0 = GetRiscvReg(opd0->content.reg.num, OpSize);
    auto rs = GetRiscvGPR(rvreg0);

    if (!OpSize && instr->opc == RISCV_OPC_CMPQ) {
      OpSize = 4;
    } else if (!OpSize && instr->opc == RISCV_OPC_CMP) {
      OpSize = 3;
    }

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
      int32_t imm = GetRVImmMapWrapper(&opd1->content.imm);

      if (instr->opc == RISCV_OPC_CMPQ && imm == 0) {
        GPRTempRes = rs;
        Opc_CMPB(instr,rrule);
        return;
      }

      if (biscuit::IsValidSigned12BitImm(imm)) {
        if (OpSize >= 4) {
          RVAssembler->ADDI(biscuit::x26, rs, -imm);
        } else {
          RVAssembler->ADDIW(biscuit::x25, rs, 0);
          RVAssembler->ADDIW(biscuit::x26, rs, -imm);
        }
        GPRTempRes = biscuit::x26;
        RVAssembler->XOR(biscuit::x5, rs, biscuit::x26);
        RVAssembler->SLLI(biscuit::x5, biscuit::x5, 59);
        RVAssembler->SRLI(biscuit::x5, biscuit::x5, 63);  // AF
        RVAssembler->NOT(biscuit::x7, biscuit::x26);      // PF
        RVAssembler->SLTI(biscuit::x29, biscuit::x26, 0);
        if (OpSize >= 4) {
          RVAssembler->SRLI(biscuit::x6, biscuit::x26, (1 << (OpSize + 2))-1);
        } else {
          RVAssembler->SRLIW(biscuit::x6, biscuit::x26, (1 << (OpSize + 2))-1);   // SF
        }
        RVAssembler->SEQZ(biscuit::x28, biscuit::x26);    // ZF
        if (OpSize >= 4) {
          RVAssembler->SLTI(biscuit::x27, rs, imm);
          RVAssembler->XOR(biscuit::x29, biscuit::x29, biscuit::x27); // OF
          RVAssembler->SLTIU(biscuit::x25, rs, imm);
        } else {
          RVAssembler->SLTI(biscuit::x27, biscuit::x25, imm);
          RVAssembler->XOR(biscuit::x29, biscuit::x29, biscuit::x27); // OF
          RVAssembler->SLTIU(biscuit::x25, biscuit::x25, imm);
        }
      } else {
        if (OpSize != 4) {
          RVAssembler->ADDIW(biscuit::x25, rs, 0);
        }
        RVAssembler->LI(biscuit::x26, imm);
        if (imm < 0) {
          RVAssembler->ADDW(biscuit::x27, rs, biscuit::x26);
        } else {
          RVAssembler->SUBW(biscuit::x27, rs, biscuit::x26);
        }
        GPRTempRes = biscuit::x27;
        RVAssembler->XOR(biscuit::x5, rs, biscuit::x27);
        RVAssembler->NOT(biscuit::x5, biscuit::x5);      // PF
        RVAssembler->SLLI(biscuit::x5, biscuit::x5, 59);
        RVAssembler->SRLI(biscuit::x5, biscuit::x5, 63);  // AF
        RVAssembler->NOT(biscuit::x7, biscuit::x27);      // PF
        RVAssembler->SLTI(biscuit::x29, biscuit::x27, 0);
        if (OpSize >= 4) {
          RVAssembler->SRLI(biscuit::x6, biscuit::x27, (1 << (OpSize + 2))-1);
        } else {
          RVAssembler->SRLIW(biscuit::x6, biscuit::x27, (1 << (OpSize + 2))-1);
        }
        RVAssembler->SEQZ(biscuit::x28, biscuit::x27);    // ZF
        RVAssembler->SLT(biscuit::x8, biscuit::x25, biscuit::x26);
        RVAssembler->XOR(biscuit::x29, biscuit::x29, biscuit::x8);   // OF
        RVAssembler->SLTU(biscuit::x25, biscuit::x25, biscuit::x26);  // CF
      }
    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {
      auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
      auto rs1 = GetRiscvGPR(rvreg1);

      RVAssembler->SUBW(biscuit::x26, rs, rs1);
      GPRTempRes = biscuit::x26;
      RVAssembler->XOR(biscuit::x5, rs, rs1);
      RVAssembler->XOR(biscuit::x5, biscuit::x5, biscuit::x26);
      RVAssembler->SLLI(biscuit::x5, biscuit::x5, 59);
      RVAssembler->SRLI(biscuit::x5, biscuit::x5, 63);  // AF
      RVAssembler->NOT(biscuit::x7, biscuit::x26);      // PF
      RVAssembler->SLTI(biscuit::x29, biscuit::x26, 0);
      if (OpSize >= 4) {
        RVAssembler->SRLI(biscuit::x6, biscuit::x26, (1 << (OpSize + 2))-1);
      } else {
        RVAssembler->SRLIW(biscuit::x6, biscuit::x26, (1 << (OpSize + 2))-1);   // SF
      }
      RVAssembler->SEQZ(biscuit::x28, biscuit::x26);    // ZF
      RVAssembler->SLT(biscuit::x27, rs, rs1);
      RVAssembler->XOR(biscuit::x29, biscuit::x29, biscuit::x27); // OF
      RVAssembler->SLTU(biscuit::x25, rs, rs1);
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for CMP instruction.");

      RVAssembler->SB(biscuit::x25, FLAG_OFFSET_CF, biscuit::x9);
      RVAssembler->SB(biscuit::x7, FLAG_OFFSET_PF, biscuit::x9);
      RVAssembler->SB(biscuit::x5, FLAG_OFFSET_AF, biscuit::x9);
      RVAssembler->SB(biscuit::x28, FLAG_OFFSET_ZF, biscuit::x9);
      RVAssembler->SB(biscuit::x6, FLAG_OFFSET_SF, biscuit::x9);
      RVAssembler->SB(biscuit::x29, FLAG_OFFSET_OF, biscuit::x9);
  }

  DEF_RV_OPC(CMPB) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
      auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
      auto rs = GetRiscvGPR(rvreg0);

      RVAssembler->NOT(biscuit::x6, rs);      // PF
      if (instr->opc == RISCV_OPC_CMPB) {
        RVAssembler->SRLI(biscuit::x5, rs, 7);  // SF
      } else if (instr->opc == RISCV_OPC_CMPW) {
        RVAssembler->SRLI(biscuit::x5, rs, 15);
      } else if (instr->opc == RISCV_OPC_CMPQ) {
        RVAssembler->SRLI(biscuit::x5, rs, 63);
      }
      RVAssembler->SEQZ(biscuit::x7, rs);    // ZF

      RVAssembler->SB(biscuit::x0, FLAG_OFFSET_CF, biscuit::x9);
      RVAssembler->SB(biscuit::x6, FLAG_OFFSET_PF, biscuit::x9);
      RVAssembler->SB(biscuit::x0, FLAG_OFFSET_AF, biscuit::x9);
      RVAssembler->SB(biscuit::x7, FLAG_OFFSET_ZF, biscuit::x9);
      RVAssembler->SB(biscuit::x5, FLAG_OFFSET_SF, biscuit::x9);
      RVAssembler->SB(biscuit::x0, FLAG_OFFSET_OF, biscuit::x9);
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for CMP instruction.");
  }

  DEF_RV_OPC(TEST) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];

    uint32_t OpSize;
    auto rvreg0 = GetRiscvReg(opd0->content.reg.num, OpSize);
    auto rs0 = GetRiscvGPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {
      auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
      auto rs1 = GetRiscvGPR(rvreg1);

      if (rs0 != rs1) {
        RVAssembler->AND(biscuit::x5, rs0, rs1);
        rs0 = biscuit::x5;
      }

      if (OpSize == 1) {
          RVAssembler->ANDI(biscuit::x28, rs0, 255);
          GPRTempRes = biscuit::x28;
      } else if (OpSize == 2) {
          RVAssembler->SLLI(biscuit::x5, rs0, 48);
          RVAssembler->SRLI(biscuit::x28, biscuit::x5, 48);  // AF
          GPRTempRes = biscuit::x28;
      } else if (OpSize == 3) {
          RVAssembler->ADDIW(biscuit::x28, rs0, 0);
          GPRTempRes = biscuit::x28;
      } else {
          GPRTempRes = rs1;
      }

      RVAssembler->NOT(biscuit::x6, rs0);      // PF
      if (OpSize == 3) {
          RVAssembler->SRLIW(biscuit::x5, rs0, (1 << (OpSize + 2))-1);
      } else if (OpSize == 1 || OpSize == 2) {
          RVAssembler->SRLI(biscuit::x5, GPRTempRes, (1 << (OpSize + 2))-1);
      } else {
          RVAssembler->SRLI(biscuit::x5, rs0, (1 << (OpSize + 2))-1);
      }

      RVAssembler->SEQZ(biscuit::x7, GPRTempRes);    // ZF
      RVAssembler->SB(biscuit::x0, FLAG_OFFSET_CF, biscuit::x9);
      RVAssembler->SB(biscuit::x6, FLAG_OFFSET_PF, biscuit::x9);
      RVAssembler->SB(biscuit::x7, FLAG_OFFSET_ZF, biscuit::x9);
      RVAssembler->SB(biscuit::x5, FLAG_OFFSET_SF, biscuit::x9);
      RVAssembler->SB(biscuit::x0, FLAG_OFFSET_OF, biscuit::x9);
    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
        int32_t imm = GetRVImmMapWrapper(&opd1->content.imm);

        if (!biscuit::IsValidSigned12BitImm(imm)) {
            auto [new_lower, new_upper] = ProcessImmediate(imm);
            RVAssembler->LUI(biscuit::x5, static_cast<int32_t>(new_upper));
            RVAssembler->ADDI(biscuit::x5, biscuit::x5, new_lower);
            RVAssembler->AND(biscuit::x28, rs0, biscuit::x5);
            RVAssembler->SEQZ(biscuit::x6, biscuit::x28);     // ZF
            RVAssembler->SB(biscuit::x0, FLAG_OFFSET_CF, biscuit::x9);
            RVAssembler->LI(biscuit::x5, 255);
            RVAssembler->SB(biscuit::x5, FLAG_OFFSET_PF, biscuit::x9);
            RVAssembler->SB(biscuit::x6, FLAG_OFFSET_ZF, biscuit::x9);
            RVAssembler->SB(biscuit::x0, FLAG_OFFSET_SF, biscuit::x9);
            RVAssembler->SB(biscuit::x0, FLAG_OFFSET_OF, biscuit::x9);
            return;
        }

        if (imm >= 0) {
          RVAssembler->ANDI(biscuit::x28, rs0, imm);
          RVAssembler->NOT(biscuit::x5, biscuit::x28);      // PF
          RVAssembler->SEQZ(biscuit::x6, biscuit::x28);     // ZF
          RVAssembler->SB(biscuit::x0, FLAG_OFFSET_CF, biscuit::x9);
          RVAssembler->SB(biscuit::x5, FLAG_OFFSET_PF, biscuit::x9);
          RVAssembler->SB(biscuit::x6, FLAG_OFFSET_ZF, biscuit::x9);
          RVAssembler->SB(biscuit::x0, FLAG_OFFSET_SF, biscuit::x9);
          RVAssembler->SB(biscuit::x0, FLAG_OFFSET_OF, biscuit::x9);
        } else {
          RVAssembler->ANDI(biscuit::x5, rs0, imm);
          RVAssembler->ADDIW(biscuit::x28, biscuit::x5, 0);
          RVAssembler->NOT(biscuit::x6, biscuit::x5);      // PF
          RVAssembler->SRLIW(biscuit::x5, rs0, (1 << (OpSize + 2))-1);
          RVAssembler->SEQZ(biscuit::x7, biscuit::x28);     // ZF
          RVAssembler->SB(biscuit::x0, FLAG_OFFSET_CF, biscuit::x9);
          RVAssembler->SB(biscuit::x6, FLAG_OFFSET_PF, biscuit::x9);
          RVAssembler->SB(biscuit::x7, FLAG_OFFSET_ZF, biscuit::x9);
          RVAssembler->SB(biscuit::x5, FLAG_OFFSET_SF, biscuit::x9);
          RVAssembler->SB(biscuit::x0, FLAG_OFFSET_OF, biscuit::x9);
        }
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for TEST instruction.");
  }

  DEF_RV_OPC(Branch) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];
    RISCVOperand *opd2 = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    uint64_t target, fallthrough;

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
        GetLabelMap(opd1->content.imm.content.sym, &target, &fallthrough);
        int32_t imm = rrule->blocksize;

        if (!rrule->update_cc) {
            if (instr->opc == RISCV_OPC_BEQZ) {
                RVAssembler->LBU(biscuit::x5, FLAG_OFFSET_ZF, biscuit::x9);
                RVAssembler->ANDI(biscuit::x5, biscuit::x5, 1);
                RVAssembler->BEQZ(biscuit::x5, 10);
            } else if (instr->opc == RISCV_OPC_BGTZ) {
                RVAssembler->LBU(biscuit::x5, FLAG_OFFSET_CF, biscuit::x9);
                RVAssembler->LBU(biscuit::x6, FLAG_OFFSET_ZF, biscuit::x9);
                RVAssembler->XOR(biscuit::x5, biscuit::x5, biscuit::x6);
                RVAssembler->ANDI(biscuit::x5, biscuit::x5, 1);
                RVAssembler->BNEZ(biscuit::x5, 10);
            } else if (instr->opc == RISCV_OPC_BNEZ) {
                RVAssembler->LBU(biscuit::x5, FLAG_OFFSET_CF, biscuit::x9);
                RVAssembler->LBU(biscuit::x6, FLAG_OFFSET_ZF, biscuit::x9);
                RVAssembler->OR(biscuit::x5, biscuit::x5, biscuit::x6);
                RVAssembler->ANDI(biscuit::x5, biscuit::x5, 1);
                RVAssembler->BNEZ(biscuit::x5, 10);
            }
            RVAssembler->ADDI(biscuit::x31, biscuit::x31, target + imm);
            RVAssembler->C_JALR(biscuit::x1);
            RVAssembler->ADDI(biscuit::x31, biscuit::x31, imm);
            return;
        }

        if (rd == biscuit::x0 && GPRTempRes != biscuit::x0)
            rd = GPRTempRes;

        if (instr->opc == RISCV_OPC_BLTZ) {
            RVAssembler->BLTZ(rd, 3*4);
        } else if (instr->opc == RISCV_OPC_BLEZ) {
            RVAssembler->BLEZ(rd, 3*4);
        } else if (instr->opc == RISCV_OPC_BGEZ) {
            RVAssembler->BGEZ(rd, 3*4);
        } else if (instr->opc == RISCV_OPC_BGTZ) {
            RVAssembler->BGTZ(rd, 3*4);
        } else if (instr->opc == RISCV_OPC_BNEZ) {
            RVAssembler->BNEZ(rd, 3*4);
        } else if (instr->opc == RISCV_OPC_BEQZ) {
            RVAssembler->BEQZ(rd, 3*4);
        }

        RVAssembler->ADDI(biscuit::x31, biscuit::x31, imm);
        RVAssembler->RET();
        if (biscuit::IsValidSigned12BitImm(target + imm)) {
          RVAssembler->ADDI(biscuit::x31, biscuit::x31, target + imm);
        } else {
          RVAssembler->LI(biscuit::x28, target + imm);
          RVAssembler->ADD(biscuit::x31, biscuit::x31, biscuit::x28);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
        auto rs1 = GetRiscvGPR(rvreg1);

        GetLabelMap(opd2->content.imm.content.sym, &target, &fallthrough);
        int32_t imm = rrule->blocksize;

        if (!rrule->update_cc) {
            if (instr->opc == RISCV_OPC_BLE) {
                RVAssembler->LBU(biscuit::x5, FLAG_OFFSET_CF, biscuit::x9);
                RVAssembler->ANDI(biscuit::x5, biscuit::x5, 1);
                RVAssembler->BNEZ(biscuit::x5, 5*4);
                RVAssembler->LBU(biscuit::x5, FLAG_OFFSET_ZF, biscuit::x9);
                RVAssembler->ANDI(biscuit::x5, biscuit::x5, 1);
                RVAssembler->BNEZ(biscuit::x5, 8);
                RVAssembler->C_ADDI(biscuit::x31, imm);
                RVAssembler->C_JALR(biscuit::x1);
                RVAssembler->ADDI(biscuit::x31, biscuit::x31, target + imm);
            } else if (instr->opc == RISCV_OPC_BNE) {
                RVAssembler->LBU(biscuit::x5, FLAG_OFFSET_ZF, biscuit::x9);
                RVAssembler->ANDI(biscuit::x5, biscuit::x5, 1);
                RVAssembler->BEQZ(biscuit::x5, 10);
                RVAssembler->ADDI(biscuit::x31, biscuit::x31, target + imm);
                RVAssembler->C_JALR(biscuit::x1);
                RVAssembler->ADDI(biscuit::x31, biscuit::x31, imm);
            } else if (instr->opc == RISCV_OPC_BGT) {
                RVAssembler->LBU(biscuit::x5, FLAG_OFFSET_SF, biscuit::x9);
                RVAssembler->LBU(biscuit::x6, FLAG_OFFSET_OF, biscuit::x9);
                RVAssembler->LBU(biscuit::x7, FLAG_OFFSET_ZF, biscuit::x9);
                RVAssembler->XOR(biscuit::x5, biscuit::x5, biscuit::x6);
                RVAssembler->OR(biscuit::x5, biscuit::x7, biscuit::x5);
                RVAssembler->ANDI(biscuit::x5, biscuit::x5, 1);
                RVAssembler->BNEZ(biscuit::x5, 10);
                RVAssembler->ADDI(biscuit::x31, biscuit::x31, target + imm);
                RVAssembler->C_JALR(biscuit::x1);
                RVAssembler->ADDI(biscuit::x31, biscuit::x31, imm);
            }
            return;
        }

        if (instr->opc == RISCV_OPC_BEQ) {
            RVAssembler->BEQ(rd, rs1, 3*4);
        } else if (instr->opc == RISCV_OPC_BNE) {
            RVAssembler->BNE(rd, rs1, 3*4);
        } else if (instr->opc == RISCV_OPC_BLT) {
            RVAssembler->BLT(rd, rs1, 3*4);
        } else if (instr->opc == RISCV_OPC_BLE) {
            RVAssembler->BLE(rd, rs1, 3*4);
        } else if (instr->opc == RISCV_OPC_BGT) {
            RVAssembler->BGT(rd, rs1, 3*4);
        } else if (instr->opc == RISCV_OPC_BGE) {
            RVAssembler->BGE(rd, rs1, 3*4);
        } else if (instr->opc == RISCV_OPC_BLTU) {
            RVAssembler->BLTU(rd, rs1, 3*4);
        } else if (instr->opc == RISCV_OPC_BGEU) {
            RVAssembler->BGEU(rd, rs1, 3*4);
        }

        RVAssembler->ADDI(biscuit::x31, biscuit::x31, imm);
        RVAssembler->RET();
        if (biscuit::IsValidSigned12BitImm(target + imm)) {
          RVAssembler->ADDI(biscuit::x31, biscuit::x31, target + imm);
        } else {
          RVAssembler->LI(biscuit::x28, target + imm);
          RVAssembler->ADD(biscuit::x31, biscuit::x31, biscuit::x28);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Branch instruction.");
  }

  DEF_RV_OPC(Load) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_MEM) {
        if (opd1->content.mem.offset.pcrel == RISCV_IMM_PCREL_LO) {
            // get new rip
            uint64_t target, fallthrough;
            GetLabelMap(opd1->content.mem.offset.content.sym, &target, &fallthrough);
            int32_t imm = target + fallthrough - rrule->entry;
            auto [new_lower, new_upper] = ProcessImmediate(imm);

            RVAssembler->LUI(biscuit::x5, static_cast<int32_t>(new_upper));
            RVAssembler->ADD(biscuit::x5, biscuit::x5, biscuit::x31);
            if (instr->opc == RISCV_OPC_LD)
                RVAssembler->LD(rd, new_lower, biscuit::x5);
            else if (instr->opc == RISCV_OPC_LB)
                RVAssembler->LB(rd, new_lower, biscuit::x5);
            else if (instr->opc == RISCV_OPC_LW)
                RVAssembler->LW(rd, new_lower, biscuit::x5);
            return;
        }

        auto rvreg1 = GetRiscvReg(opd1->content.mem.base);
        auto rs1 = GetRiscvGPR(rvreg1);
        int32_t imm = GetRVImmMapWrapper(&opd1->content.mem.offset);

        if (!biscuit::IsValidSigned12BitImm(imm)) {
            auto [new_lower, new_upper] = ProcessImmediate(imm);
            RVAssembler->LUI(biscuit::x5, static_cast<int32_t>(new_upper));
            RVAssembler->ADD(biscuit::x5, biscuit::x5, biscuit::x31);
            rs1 = biscuit::x5;
            imm = new_lower;
        }

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_LB) {
            RVAssembler->LB(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_LBU) {
            RVAssembler->LBU(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_LH) {
            RVAssembler->LH(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_LHU) {
            RVAssembler->LHU(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_LW) {
            RVAssembler->LW(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_LWU) {
            RVAssembler->LWU(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_LD) {
            RVAssembler->LD(rd, imm, rs1);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Load instruction.");
  }

  DEF_RV_OPC(Store) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_MEM) {
        if (opd1->content.mem.offset.pcrel == RISCV_IMM_PCREL_LO) {
            // get new rip
            uint64_t target, fallthrough;
            GetLabelMap(opd1->content.mem.offset.content.sym, &target, &fallthrough);
            int32_t imm = target + fallthrough - rrule->entry;
            auto [new_lower, new_upper] = ProcessImmediate(imm);

            RVAssembler->LUI(biscuit::x5, static_cast<int32_t>(new_upper));
            RVAssembler->ADD(biscuit::x5, biscuit::x5, biscuit::x31);
            if (instr->opc == RISCV_OPC_SD)
                RVAssembler->SD(rd, new_lower, biscuit::x5);
            else if (instr->opc == RISCV_OPC_SB)
                RVAssembler->SB(rd, new_lower, biscuit::x5);
            return;
        }

        auto rvreg1 = GetRiscvReg(opd1->content.mem.base);
        auto rs1 = GetRiscvGPR(rvreg1);
        int32_t imm = GetRVImmMapWrapper(&opd1->content.mem.offset);

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_SB) {
            RVAssembler->SB(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_SH) {
            RVAssembler->SH(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_SW) {
            RVAssembler->SW(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_SD) {
            RVAssembler->SD(rd, imm, rs1);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Store instruction.");
  }

  DEF_RV_OPC(Jump) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];
    RISCVOperand *opd2 = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    uint64_t target, fallthrough;

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {

        if (instr->opc == RISCV_OPC_JAL) {
            // get new rip
            GetLabelMap(opd1->content.imm.content.sym, &target, &fallthrough);
            int32_t imm = rrule->blocksize;
            if (biscuit::IsValidSigned12BitImm(target + imm)) {
                RVAssembler->ADDI(biscuit::x31, biscuit::x31, target + imm);
            } else {
                RVAssembler->LI(biscuit::x5, target + imm);
                RVAssembler->ADD(biscuit::x31, biscuit::x31, biscuit::x5);
            }
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {

        if (instr->opc == RISCV_OPC_JALR) {
            auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
            auto rs1 = GetRiscvGPR(rvreg1);

            GetLabelMap(opd2->content.imm.content.sym, &target, &fallthrough);
            int32_t imm = (fallthrough + target) & 0xFFF;
            RVAssembler->JALR(rd, imm, rs1);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Jump instruction.");
  }

  DEF_RV_OPC(CALL) {
    RISCVOperand *opd0 = &instr->opd[0];

    uint64_t target, fallthrough;
    int32_t imm = rrule->blocksize;

    if (opd0->type == RISCV_OPD_TYPE_IMM) {
        if (instr->opc == RISCV_OPC_CALL) {
            GetLabelMap(opd0->content.imm.content.sym, &target, &fallthrough);

            RVAssembler->ADDI(biscuit::x5, biscuit::x31, imm);  // retval = rip + InstSize
            RVAssembler->SD(biscuit::x5, -8, biscuit::x17);   // push retval
            RVAssembler->LI(biscuit::x5, target + imm);
            RVAssembler->ADD(biscuit::x31, biscuit::x31, biscuit::x5);
            RVAssembler->ADDI(biscuit::x17, biscuit::x17, -8);// rsp = rsp - 8
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for CALL instruction.");
  }

  DEF_RV_OPC(Multiply) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_MUL) {
            RVAssembler->MUL(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_MULH) {
            RVAssembler->MULH(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_MULW) {
            RVAssembler->MULW(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_MULHSU) {
            RVAssembler->MULHSU(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_MULHU) {
            RVAssembler->MULHU(rd, rs1, rs2);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for Multiply instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Multiply instruction.");
  }

  DEF_RV_OPC(Divide) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_DIV) {
            RVAssembler->DIV(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_DIVU) {
            RVAssembler->DIVU(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_DIVW) {
            RVAssembler->DIVW(rd, rs1, rs2);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for Divide instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Divide instruction.");
  }

  DEF_RV_OPC(Remainder) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_REM) {
            RVAssembler->REM(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_REMU) {
            RVAssembler->REMU(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_REMW) {
            RVAssembler->REMW(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_REMUW) {
            RVAssembler->REMUW(rd, rs1, rs2);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for Remainder instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Remainder instruction.");
  }

  DEF_RV_OPC(LoadAPS) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_MEM) {
        auto rvreg1 = GetRiscvReg(opd1->content.mem.base);
        auto rs1 = GetRiscvGPR(rvreg1);
        int32_t imm = GetRVImmMapWrapper(&opd1->content.mem.offset);

        if (!biscuit::IsValidSigned12BitImm(imm)) {
            auto [new_lower, new_upper] = ProcessImmediate(imm);
            RVAssembler->LUI(biscuit::x5, static_cast<int32_t>(new_upper));
            RVAssembler->ADD(biscuit::x5, biscuit::x5, biscuit::x31);
            rs1 = biscuit::x5;
            imm = new_lower;
        }

        if (instr->opc == RISCV_OPC_LDAPS) {
            if (rvreg0 == RISCV_REG_V1) {
                RVAssembler->LD(biscuit::x25, imm - 8, rs1);
                RVAssembler->LD(biscuit::x26, imm, rs1);
                RVAssembler->SD(biscuit::x25, XMM1_OFFSET_LOW, biscuit::x9);
                RVAssembler->SD(biscuit::x26, XMM1_OFFSET_HIGH, biscuit::x9);
            }
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for LoadAPS instruction.");
  }