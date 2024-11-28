  DEF_RV_OPC(VSET) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvGPR(rvreg0);
    auto rs1 = GetRiscvGPR(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {
        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvGPR(rvreg2);

        if (instr->opc == RISCV_OPC_VSETVL) {
          RVAssembler->VSETVL(rd, rs1, rs2);
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_IMM) {
        // int32_t imm = GetRVImmMapWrapper(&opd2->content.imm);

        if (instr->opc == RISCV_OPC_VSETVLI) {
          RVAssembler->VSETVLI(rd, rs1, biscuit::SEW::E8, biscuit::LMUL::M1, biscuit::VTA::No, biscuit::VMA::No);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for SET VECTOR LEN instruction.");
  }

  DEF_RV_OPC(VMulDiv) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvVec(rvreg0);
    auto rs1 = GetRiscvVec(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);
        auto rs2 = GetRiscvVec(rvreg2);

        if (instr->opc == RISCV_OPC_VMULH) {
            RVAssembler->VMULH(rd, rs1, rs2, biscuit::VecMask::No);
        } else if (instr->opc == RISCV_OPC_VREM) {
            RVAssembler->VREM(rd, rs1, rs2, biscuit::VecMask::No);
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Multiply-Divide instruction.");
  }

  DEF_RV_OPC(VShifts) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvVec(rvreg0);
    auto rs1 = GetRiscvVec(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);

        if (rvreg2 >= RISCV_REG_X0 && rvreg2 <= RISCV_REG_X31) {
          auto rs2 = GetRiscvGPR(rvreg2);
          if (instr->opc == RISCV_OPC_VSLL) {
            RVAssembler->VSLL(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VSRL) {
            RVAssembler->VSRL(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VSRA) {
            RVAssembler->VSRA(rd, rs1, rs2, biscuit::VecMask::No);
          }
        } else if (rvreg2 >= RISCV_REG_V0 && rvreg2 <= RISCV_REG_V31) {
          auto rs2 = GetRiscvVec(rvreg2);
          if (instr->opc == RISCV_OPC_VSLL) {
            RVAssembler->VSLL(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VSRL) {
            RVAssembler->VSRL(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VSRA) {
            RVAssembler->VSRA(rd, rs1, rs2, biscuit::VecMask::No);
          }
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_IMM) {
        // I-immediate[11:0], x86 imm > riscv imm?
        int32_t imm = GetRVImmMapWrapper(&opd2->content.imm);
        if (instr->opc == RISCV_OPC_VSLL) {
          RVAssembler->VSLL(rd, rs1, imm, biscuit::VecMask::No);
        } else if (instr->opc == RISCV_OPC_VSRL) {
          RVAssembler->VSRL(rd, rs1, imm, biscuit::VecMask::No);
        } else if (instr->opc == RISCV_OPC_VSRA) {
          RVAssembler->VSRA(rd, rs1, imm, biscuit::VecMask::No);
        }
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for SET VECTOR LEN instruction.");
  }

  DEF_RV_OPC(VMV) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {
        if (instr->opc == RISCV_OPC_VMV) {
          auto rd = GetRiscvVec(rvreg0);
          auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
          if (rvreg1 >= RISCV_REG_X0 && rvreg1 <= RISCV_REG_X31) {
            auto rs1 = GetRiscvGPR(rvreg1);
            RVAssembler->VMV(rd, rs1);
          } else if (rvreg1 >= RISCV_REG_V0 && rvreg1 <= RISCV_REG_V31) {
            auto rs1 = GetRiscvVec(rvreg1);
            RVAssembler->VMV(rd, rs1);
          }
        } else if (instr->opc == RISCV_OPC_VMV_XS) {
          auto rd = GetRiscvGPR(rvreg0);
          auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
          auto rs1 = GetRiscvVec(rvreg1);
          RVAssembler->VMV_XS(rd, rs1);
        }
    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_IMM) {
        auto rd = GetRiscvVec(rvreg0);
        int32_t imm = GetRVImmMapWrapper(&opd1->content.imm);
        if (instr->opc == RISCV_OPC_VMV) {
          RVAssembler->VMV(rd, imm);
        }
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for SET VECTOR LEN instruction.");
  }

  DEF_RV_OPC(VArithmetic) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvVec(rvreg0);
    auto rs1 = GetRiscvVec(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);

        if (rvreg2 >= RISCV_REG_X0 && rvreg2 <= RISCV_REG_X31) {
          auto rs2 = GetRiscvGPR(rvreg2);
          if (instr->opc == RISCV_OPC_VADD) {
            RVAssembler->VADD(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VSUB_VX) {
            RVAssembler->VSUB(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VMUL) {
            RVAssembler->VMUL(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VDIV) {
            RVAssembler->VDIV(rd, rs1, rs2, biscuit::VecMask::No);
          }
        } else if (rvreg2 >= RISCV_REG_V0 && rvreg2 <= RISCV_REG_V31) {
          auto rs2 = GetRiscvVec(rvreg2);
          if (instr->opc == RISCV_OPC_VADD) {
            RVAssembler->VADD(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VSUB_VV) {
            RVAssembler->VSUB(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VMUL) {
            RVAssembler->VMUL(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VDIV) {
            RVAssembler->VDIV(rd, rs1, rs2, biscuit::VecMask::No);
          }
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_IMM) {
        // I-immediate[11:0], x86 imm > riscv imm?
        int32_t imm = GetRVImmMapWrapper(&opd2->content.imm);
        if (instr->opc == RISCV_OPC_VADD) {
          RVAssembler->VADD(rd, rs1, imm, biscuit::VecMask::No);
        }
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Arithmetic instruction.");
  }

  DEF_RV_OPC(VMulAdd) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg2 = GetRiscvReg(opd2->content.reg.num);

    auto rd = GetRiscvVec(rvreg0);
    auto rs2 = GetRiscvVec(rvreg2);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

        if (rvreg1 >= RISCV_REG_F0 && rvreg1 <= RISCV_REG_F31) {
          auto rs1 = GetRiscvFPR(rvreg1);
          if (instr->opc == RISCV_OPC_VFMADD) {
            RVAssembler->VFMADD(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VFMSUB) {
            RVAssembler->VFMSUB(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VFNMADD) {
            RVAssembler->VFNMADD(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VFNMSUB) {
            RVAssembler->VFNMSUB(rd, rs1, rs2, biscuit::VecMask::No);
          }
        } else if (rvreg1 >= RISCV_REG_V0 && rvreg1 <= RISCV_REG_V31) {
          auto rs1 = GetRiscvVec(rvreg1);
          if (instr->opc == RISCV_OPC_VFMADD) {
            RVAssembler->VFMADD(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VFMSUB) {
            RVAssembler->VFMSUB(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VFNMADD) {
            RVAssembler->VFNMADD(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VFNMSUB) {
            RVAssembler->VFNMSUB(rd, rs1, rs2, biscuit::VecMask::No);
          }
        }

    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Multiply ADD instruction.");
  }

  DEF_RV_OPC(VMAXMIN) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvVec(rvreg0);
    auto rs1 = GetRiscvVec(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);

        if (rvreg2 >= RISCV_REG_X0 && rvreg2 <= RISCV_REG_X31) {
          auto rs2 = GetRiscvGPR(rvreg2);
          if (instr->opc == RISCV_OPC_VMAX) {
            RVAssembler->VMAX(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VMIN) {
            RVAssembler->VMIN(rd, rs1, rs2, biscuit::VecMask::No);
          }
        } else if (rvreg2 >= RISCV_REG_V0 && rvreg2 <= RISCV_REG_V31) {
          auto rs2 = GetRiscvVec(rvreg2);
          if (instr->opc == RISCV_OPC_VMAX) {
            RVAssembler->VMAX(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VMIN) {
            RVAssembler->VMIN(rd, rs1, rs2, biscuit::VecMask::No);
          }
        }
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Vector MAX MIN instruction.");
  }

  DEF_RV_OPC(VLogical) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];
    RISCVOperand *opd2   = &instr->opd[2];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rvreg1 = GetRiscvReg(opd1->content.reg.num);

    auto rd = GetRiscvVec(rvreg0);
    auto rs1 = GetRiscvVec(rvreg1);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_REG) {

        auto rvreg2 = GetRiscvReg(opd2->content.reg.num);

        if (rvreg2 >= RISCV_REG_X0 && rvreg2 <= RISCV_REG_X31) {
          auto rs2 = GetRiscvGPR(rvreg2);
          if (instr->opc == RISCV_OPC_VXOR) {
            RVAssembler->VXOR(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VOR) {
            RVAssembler->VOR(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VAND) {
            RVAssembler->VAND(rd, rs1, rs2, biscuit::VecMask::No);
          }
        } else if (rvreg2 >= RISCV_REG_V0 && rvreg2 <= RISCV_REG_V31) {
          auto rs2 = GetRiscvVec(rvreg2);
          if (instr->opc == RISCV_OPC_VXOR) {
            RVAssembler->VXOR(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VOR) {
            RVAssembler->VOR(rd, rs1, rs2, biscuit::VecMask::No);
          } else if (instr->opc == RISCV_OPC_VAND) {
            RVAssembler->VAND(rd, rs1, rs2, biscuit::VecMask::No);
          }
        }

    } else if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG
      && opd2->type == RISCV_OPD_TYPE_IMM) {
        // I-immediate[11:0], x86 imm > riscv imm?
        int32_t imm = GetRVImmMapWrapper(&opd2->content.imm);
        if (instr->opc == RISCV_OPC_VXOR) {
          RVAssembler->VXOR(rd, rs1, imm, biscuit::VecMask::No);
        } else if (instr->opc == RISCV_OPC_VOR) {
          RVAssembler->VOR(rd, rs1, imm, biscuit::VecMask::No);
        } else if (instr->opc == RISCV_OPC_VAND) {
          RVAssembler->VAND(rd, rs1, imm, biscuit::VecMask::No);
        }
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for Logical instruction.");
  }

  DEF_RV_OPC(VMSBF) {
    RISCVOperand *opd0   = &instr->opd[0];
    RISCVOperand *opd1   = &instr->opd[1];

    auto rvreg0 = GetRiscvReg(opd0->content.reg.num);
    auto rd = GetRiscvVec(rvreg0);

    if (opd0->type == RISCV_OPD_TYPE_REG && opd1->type == RISCV_OPD_TYPE_REG) {
        auto rvreg1 = GetRiscvReg(opd1->content.reg.num);
        auto rs1 = GetRiscvVec(rvreg1);
        RVAssembler->VMSBF(rd, rs1, biscuit::VecMask::No);
    } else
        LogMan::Msg::EFmt("[RISC-V] Unsupported operand type for SET VECTOR LEN instruction.");
  }