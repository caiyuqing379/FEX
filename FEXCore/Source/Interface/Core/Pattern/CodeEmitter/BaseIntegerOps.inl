#include <cstdint>
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

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        int32_t Imm = GetRVImmMapWrapper(&opd2->content.imm);

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_ADDI) {
          RVAssembler->ADDI(rd, rs1, Imm);
        } else if (instr->opc == RISCV_OPC_MV) {
          RVAssembler->ADDI(rd, rs1, 0);
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

  DEF_RV_OPC(LI) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {
        int32_t Imm = GetRVImmMapWrapper(&opd1->content.imm);
        // I-immediate[11:0], x86 imm > riscv imm?
        RVAssembler->LI(rd, Imm);
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for LI instruction.");
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

  DEF_RV_OPC(LUI) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
        int32_t Imm = GetRVImmMapWrapper(&opd1->content.imm);

        // I-immediate[11:0]
        if (instr->opc == RISCV_OPC_LUI) {
          RVAssembler->LUI(rd, Imm);
        } else if (instr->opc == RISCV_OPC_AUIPC) {
          RVAssembler->AUIPC(rd, Imm);
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

  DEF_RV_OPC(Branch) {
    RISCVOperand *opd0 = &instr->opd[0];
    RISCVOperand *opd1 = &instr->opd[1];
    RISCVOperand *opd2 = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvGPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
        int32_t imm = GetRVImmMapWrapper(&opd1->content.imm);

        if (instr->opc == RISCV_OPC_BLTZ) {
            RVAssembler->BLTZ(rd, imm);
        } else if (instr->opc == RISCV_OPC_BLEZ) {
            RVAssembler->BLEZ(rd, imm);
        } else if (instr->opc == RISCV_OPC_BGEZ) {
            RVAssembler->BGEZ(rd, imm);
        } else if (instr->opc == RISCV_OPC_BGTZ) {
            RVAssembler->BGTZ(rd, imm);
        } else if (instr->opc == RISCV_OPC_BNEZ) {
            RVAssembler->BNEZ(rd, imm);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
        auto rs1 = GetRiscvGPR(rvreg1);
        int32_t imm = GetRVImmMapWrapper(&opd2->content.imm);

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_BEQ) {
            RVAssembler->BEQ(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_BNE) {
            RVAssembler->BNE(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_BLT) {
            RVAssembler->BLT(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_BLE) {
            RVAssembler->BLE(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_BGT) {
            RVAssembler->BGT(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_BGE) {
            RVAssembler->BGE(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_BLTU) {
            RVAssembler->BLTU(rd, rs1, imm);
        } else if (instr->opc == RISCV_OPC_BGEU) {
            RVAssembler->BGEU(rd, rs1, imm);
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
        auto rvreg1 = GetRiscvReg(opd1->content.mem.base);
        auto rs1 = GetRiscvGPR(rvreg1);
        int32_t imm = GetRVImmMapWrapper(&opd1->content.mem.offset);

        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_LB) {
            RVAssembler->LB(rd, imm, rs1);
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
        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_JAL) {
            // get new rip
            GetLabelMap(opd1->content.imm.content.sym, &target, &fallthrough);
            RVAssembler->JAL(rd, fallthrough + target);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_IMM) {
        // I-immediate[11:0], x86 imm > riscv imm?
        if (instr->opc == RISCV_OPC_JALR) {
            auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
            auto rs1 = GetRiscvGPR(rvreg1);

            GetLabelMap(opd2->content.imm.content.sym, &target, &fallthrough);
            RVAssembler->JALR(rd, fallthrough + target, rs1);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Jump instruction.");
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