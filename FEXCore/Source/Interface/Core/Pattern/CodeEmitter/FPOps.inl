public:
/*
  RISC-V Flaoting-Point Extenssions: RVF & RVD
*/
  DEF_RV_OPC(FMV) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {

        if (instr->opc == RISCV_OPC_FMV_W_X) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FMV_W_X(rd, rs1);
        } else if (instr->opc == RISCV_OPC_FMV_H_X) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FMV_H_X(rd, rs1);
        } else if (instr->opc == RISCV_OPC_FMV_D_X) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FMV_D_X(rd, rs1);
        } else if (instr->opc == RISCV_OPC_FMV_X_W) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FMV_X_W(rd, rs1);
        } else if (instr->opc == RISCV_OPC_FMV_X_H) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FMV_X_H(rd, rs1);
        } else if (instr->opc == RISCV_OPC_FMV_X_D) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FMV_X_D(rd, rs1);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for FMV instruction.");
  }

  DEF_RV_OPC(FCVT) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {

        if (instr->opc == RISCV_OPC_FCVT_S_W) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_S_W(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_S_WU) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_S_WU(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_S_H) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_S_H(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_S_D) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_S_D(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_S_L) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_S_L(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_S_LU) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_S_LU(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_D_S) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_D_S(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_D_W) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_D_W(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_D_WU) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_D_WU(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_D_L) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_D_L(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_D_LU) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->FCVT_D_LU(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_D_H) {
            auto rd = GetRiscvFPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_D_H(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_W_S) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_W_S(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_W_H) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_W_H(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_W_D) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_W_D(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_WU_S) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_WU_S(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_WU_H) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_WU_H(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_WU_D) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_WU_D(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_L_S) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_W_S(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_L_H) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_W_H(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_L_D) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_W_D(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_LU_S) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_WU_S(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_LU_H) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_WU_H(rd, rs1, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FCVT_LU_D) {
            auto rd = GetRiscvGPR(rvreg0);
            auto rs1 = GetRiscvFPR(rvreg1);
            RVAssembler->FCVT_WU_D(rd, rs1, biscuit::RMode::DYN);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for FCVT instruction.");
  }

  DEF_RV_OPC(FLoad) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvFPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_MEM) {
        auto rvreg1 = GetRiscvReg(opd1->content.mem.base);
        auto rs1 = GetRiscvGPR(rvreg1);
        int32_t imm = GetRVImmMapWrapper(&opd1->content.mem.offset);

        if (instr->opc == RISCV_OPC_FLW) {
            RVAssembler->FLW(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_FLD) {
            RVAssembler->FLD(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_FSW) {
            RVAssembler->FSW(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_FSD) {
            RVAssembler->FSD(rd, imm, rs1);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for FLoad instruction.");
  }

  DEF_RV_OPC(FStore) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvFPR(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_MEM) {
        auto rvreg1 = GetRiscvReg(opd1->content.mem.base);
        auto rs1 = GetRiscvGPR(rvreg1);
        int32_t imm = GetRVImmMapWrapper(&opd1->content.mem.offset);

        if (instr->opc == RISCV_OPC_FSW) {
            RVAssembler->FSW(rd, imm, rs1);
        } else if (instr->opc == RISCV_OPC_FSD) {
            RVAssembler->FSD(rd, imm, rs1);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for FStore instruction.");
  }

  DEF_RV_OPC(FADD) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvFPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvFPR(rvreg2);

        if (instr->opc == RISCV_OPC_FADD_S) {
            RVAssembler->FADD_S(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FADD_H) {
            RVAssembler->FADD_H(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FADD_D) {
            RVAssembler->FADD_D(rd, rs1, rs2, biscuit::RMode::DYN);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for Remainder instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Remainder instruction.");
  }

  DEF_RV_OPC(FSUB) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvFPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvFPR(rvreg2);

        if (instr->opc == RISCV_OPC_FSUB_S) {
            RVAssembler->FSUB_S(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FSUB_H) {
            RVAssembler->FSUB_H(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FSUB_D) {
            RVAssembler->FSUB_D(rd, rs1, rs2, biscuit::RMode::DYN);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for Remainder instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Remainder instruction.");
  }

  DEF_RV_OPC(FMUL) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvFPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvFPR(rvreg2);

        if (instr->opc == RISCV_OPC_FMUL_S) {
            RVAssembler->FMUL_S(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FMUL_H) {
            RVAssembler->FMUL_H(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FMUL_D) {
            RVAssembler->FMUL_D(rd, rs1, rs2, biscuit::RMode::DYN);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for MUL instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for MUL instruction.");
  }

  DEF_RV_OPC(FDIV) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvFPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG && opd2->type == RISCV_OPD_TYPE_REG) {

      if (opd2->content.reg.num != RISCV_REG_INVALID) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvFPR(rvreg2);

        if (instr->opc == RISCV_OPC_FDIV_S) {
            RVAssembler->FDIV_S(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FDIV_H) {
            RVAssembler->FDIV_H(rd, rs1, rs2, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FDIV_D) {
            RVAssembler->FDIV_D(rd, rs1, rs2, biscuit::RMode::DYN);
        }

      } else
          LogMan::Msg::EFmt("[RISC-V] Unsupported reg for DIV instruction.");

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for DIV instruction.");
  }

  DEF_RV_OPC(FMulAdd) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];
    RISCVOperand *opd3   = &instr->opd[3];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvFPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG && opd3->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rvreg3 = GetRiscvReg(opd3->content.reg.num);

        auto rs2 = GetRiscvFPR(rvreg2);
        auto rs3 = GetRiscvFPR(rvreg3);

        if (instr->opc == RISCV_OPC_FMADD_S) {
            RVAssembler->FMADD_S(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FMADD_H) {
            RVAssembler->FMADD_H(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FMADD_D) {
            RVAssembler->FMADD_D(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FMSUB_S) {
            RVAssembler->FMSUB_S(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FMSUB_H) {
            RVAssembler->FMSUB_H(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FMSUB_D) {
            RVAssembler->FMSUB_D(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FNMSUB_S) {
            RVAssembler->FNMSUB_S(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FNMSUB_H) {
            RVAssembler->FNMSUB_H(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FNMSUB_D) {
            RVAssembler->FNMSUB_D(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FNMADD_S) {
            RVAssembler->FNMADD_S(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FNMADD_H) {
            RVAssembler->FNMADD_H(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        } else if (instr->opc == RISCV_OPC_FNMADD_D) {
            RVAssembler->FNMADD_D(rd, rs1, rs2, rs3, biscuit::RMode::DYN);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Mul-Add instruction.");
  }

  DEF_RV_OPC(FSignInject) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvFPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvFPR(rvreg2);

        if (instr->opc == RISCV_OPC_FSGNJ_S) {
            RVAssembler->FSGNJ_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJ_H) {
            RVAssembler->FSGNJ_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJ_D) {
            RVAssembler->FSGNJ_D(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJN_S) {
            RVAssembler->FSGNJN_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJN_H) {
            RVAssembler->FSGNJN_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJN_D) {
            RVAssembler->FSGNJN_D(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJX_S) {
            RVAssembler->FSGNJX_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJX_H) {
            RVAssembler->FSGNJX_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FSGNJX_D) {
            RVAssembler->FSGNJX_D(rd, rs1, rs2);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Sign Inject instruction.");
  }

  DEF_RV_OPC(FMINMAX) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvFPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvFPR(rvreg2);

        if (instr->opc == RISCV_OPC_FMIN_S) {
            RVAssembler->FMIN_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FMIN_H) {
            RVAssembler->FMIN_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FMIN_D) {
            RVAssembler->FMIN_D(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FMAX_S) {
            RVAssembler->FMAX_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FMAX_H) {
            RVAssembler->FMAX_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FMAX_D) {
            RVAssembler->FMAX_D(rd, rs1, rs2);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for MAX MIN instruction.");
  }

  DEF_RV_OPC(FCompare) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvFPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvFPR(rvreg2);

        if (instr->opc == RISCV_OPC_FEQ_S) {
            RVAssembler->FEQ_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FEQ_H) {
            RVAssembler->FEQ_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FEQ_D) {
            RVAssembler->FEQ_D(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FLT_S) {
            RVAssembler->FLT_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FLT_H) {
            RVAssembler->FLT_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FLT_D) {
            RVAssembler->FLT_D(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FLE_S) {
            RVAssembler->FLE_S(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FLE_H) {
            RVAssembler->FLE_H(rd, rs1, rs2);
        } else if (instr->opc == RISCV_OPC_FLE_D) {
            RVAssembler->FLE_D(rd, rs1, rs2);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Compare instruction.");
  }