#include <FEXCore/Utils/LogManager.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "RiscvInst.h"

static const RISCVRegister riscv_reg_table[] = {
    RISCV_REG_X0, RISCV_REG_X1, RISCV_REG_X2, RISCV_REG_X3,
    RISCV_REG_X4, RISCV_REG_X5, RISCV_REG_X6, RISCV_REG_X7,
    RISCV_REG_X8, RISCV_REG_X9, RISCV_REG_X10, RISCV_REG_X11,
    RISCV_REG_X12, RISCV_REG_X13, RISCV_REG_X14, RISCV_REG_X15,
    RISCV_REG_X16, RISCV_REG_X17, RISCV_REG_X18, RISCV_REG_X19,
    RISCV_REG_X20, RISCV_REG_X21, RISCV_REG_X22, RISCV_REG_X23,
    RISCV_REG_X24, RISCV_REG_X25, RISCV_REG_X26, RISCV_REG_X27,
    RISCV_REG_X28, RISCV_REG_X29, RISCV_REG_X30, RISCV_REG_X31,

    RISCV_REG_F0, RISCV_REG_F1, RISCV_REG_F2, RISCV_REG_F3,
    RISCV_REG_F4, RISCV_REG_F5, RISCV_REG_F6, RISCV_REG_F7,
    RISCV_REG_F8, RISCV_REG_F9, RISCV_REG_F10, RISCV_REG_F11,
    RISCV_REG_F12, RISCV_REG_F13, RISCV_REG_F14, RISCV_REG_F15,
    RISCV_REG_F16, RISCV_REG_F17, RISCV_REG_F18, RISCV_REG_F19,
    RISCV_REG_F20, RISCV_REG_F21, RISCV_REG_F22, RISCV_REG_F23,
    RISCV_REG_F24, RISCV_REG_F25, RISCV_REG_F26, RISCV_REG_F27,
    RISCV_REG_F28, RISCV_REG_F29, RISCV_REG_F30, RISCV_REG_F31,

    RISCV_REG_V0, RISCV_REG_V1, RISCV_REG_V2, RISCV_REG_V3,
    RISCV_REG_V4, RISCV_REG_V5, RISCV_REG_V6, RISCV_REG_V7,
    RISCV_REG_V8, RISCV_REG_V9, RISCV_REG_V10, RISCV_REG_V11,
    RISCV_REG_V12, RISCV_REG_V13, RISCV_REG_V14, RISCV_REG_V15,
    RISCV_REG_V16, RISCV_REG_V17, RISCV_REG_V18, RISCV_REG_V19,
    RISCV_REG_V20, RISCV_REG_V21, RISCV_REG_V22, RISCV_REG_V23,
    RISCV_REG_V24, RISCV_REG_V25, RISCV_REG_V26, RISCV_REG_V27,
    RISCV_REG_V28, RISCV_REG_V29, RISCV_REG_V30, RISCV_REG_V31
};

static const char *riscv_reg_str[] = {
    "none",
    "x0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",

    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",

    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
    "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
    "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
    "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",

    "vt0", "vt1", "vt2", "vt3", "vt4", "vt5", "vt6",

    "reg0", "reg1", "reg2", "reg3", "reg4", "reg5", "reg6", "reg7",
    "reg8", "reg9", "reg10", "reg11", "reg12", "reg13", "reg14", "reg15",
    "reg16", "reg17", "reg18", "reg19", "reg20", "reg21", "reg22", "reg23",
    "reg24", "reg25", "reg26", "reg27", "reg28", "reg29", "reg30", "reg31",
};

static const char *riscv_opc_str[] = {
    [RISCV_OPC_INVALID] = "**** unsupported (riscv) ****",
    // Shifts
    [RISCV_OPC_SLL]   = "sll",
    [RISCV_OPC_SLLI]  = "slli",
    [RISCV_OPC_SRL]   = "srl",
    [RISCV_OPC_SRLI]  = "srli",
    [RISCV_OPC_SRA]   = "sra",
    [RISCV_OPC_SRAI]  = "srai",
    [RISCV_OPC_SLLW]  = "sllw",
    [RISCV_OPC_SLLIW] = "slliw",
    [RISCV_OPC_SRLW]  = "srlw",
    [RISCV_OPC_SRLIW] = "srliw",
    [RISCV_OPC_SRAW]  = "sraw",
    [RISCV_OPC_SRAIW] = "sraiw",

    // Arithmetic
    [RISCV_OPC_ADD]   = "add",
    [RISCV_OPC_ADDI]  = "addi",
    [RISCV_OPC_LI]    = "li",
    [RISCV_OPC_SUB]   = "sub",
    [RISCV_OPC_LUI]   = "lui",
    [RISCV_OPC_AUIPC] = "auipc",
    [RISCV_OPC_ADDW]  = "addw",
    [RISCV_OPC_ADDIW] = "addiw",
    [RISCV_OPC_SUBW]  = "subw",

    // Logical
    [RISCV_OPC_AND]   = "and",
    [RISCV_OPC_ANDI]  = "andi",
    [RISCV_OPC_OR]    = "or",
    [RISCV_OPC_ORI]   = "ori",
    [RISCV_OPC_XOR]   = "xor",
    [RISCV_OPC_XORI]  = "xori",

    // Compare
    [RISCV_OPC_SLT]   = "slt",
    [RISCV_OPC_SLTI]  = "slti",
    [RISCV_OPC_SLTU]  = "sltu",
    [RISCV_OPC_SLTIU] = "sltiu",

    // Branch
    [RISCV_OPC_BEQ]   = "beq",
    [RISCV_OPC_BNE]   = "bne",
    [RISCV_OPC_BNEZ]  = "bnez",
    [RISCV_OPC_BLT]   = "blt",
    [RISCV_OPC_BLE]   = "ble",
    [RISCV_OPC_BGT]   = "bgt",
    [RISCV_OPC_BGE]   = "bge",
    [RISCV_OPC_BLTU]  = "bltu",
    [RISCV_OPC_BLTZ]  = "bltz",
    [RISCV_OPC_BLEZ]  = "blez",
    [RISCV_OPC_BGEU]  = "bgeu",
    [RISCV_OPC_BGEZ]  = "bgez",
    [RISCV_OPC_BGTZ]  = "bgtz",

    // Load & Store
    [RISCV_OPC_LB]    = "lb",
    [RISCV_OPC_LH]    = "lh",
    [RISCV_OPC_LBU]   = "lbu",
    [RISCV_OPC_LHU]   = "lhu",
    [RISCV_OPC_LW]    = "lw",
    [RISCV_OPC_LWU]   = "lwu",
    [RISCV_OPC_LD]    = "ld",
    [RISCV_OPC_SB]    = "sb",
    [RISCV_OPC_SH]    = "sh",
    [RISCV_OPC_SW]    = "sw",
    [RISCV_OPC_SD]    = "sd",

    // Pseudo
    [RISCV_OPC_CMP]   = "cmp",
    [RISCV_OPC_CMPB]  = "cmpb",
    [RISCV_OPC_CMPW]  = "cmpw",
    [RISCV_OPC_CMPQ]  = "cmpq",
    [RISCV_OPC_TEST]  = "test",
    [RISCV_OPC_TESTB] = "testb",
    [RISCV_OPC_BEQZ]  = "beqz",
    [RISCV_OPC_J]     = "j",
    [RISCV_OPC_MV]    = "mv",
    [RISCV_OPC_RET]   = "ret",

    // Jump & Link
    [RISCV_OPC_JAL]   = "jal",
    [RISCV_OPC_JALR]  = "jalr",
    [RISCV_OPC_CALL]  = "call",

    // Multiply-Divide
    [RISCV_OPC_MUL]   = "mul",
    [RISCV_OPC_MULH]  = "mulh",
    [RISCV_OPC_MULW]  = "mulw",
    [RISCV_OPC_MULHSU] = "mulhsu",
    [RISCV_OPC_MULHU] = "mulhu",
    [RISCV_OPC_DIV]   = "div",
    [RISCV_OPC_DIVU]  = "divu",
    [RISCV_OPC_DIVW]  = "divw",
    [RISCV_OPC_REM]   = "rem",
    [RISCV_OPC_REMU]  = "remu",
    [RISCV_OPC_REMW]  = "remw",
    [RISCV_OPC_REMUW] = "remuw",

    // Floating-Point
    [RISCV_OPC_FMV_W_X] = "fmv.w.x",    /* Move */
    [RISCV_OPC_FMV_H_X] = "fmv.h.x",
    [RISCV_OPC_FMV_D_X] = "fmv.d.x",
    [RISCV_OPC_FMV_X_W] = "fmv.x.w",
    [RISCV_OPC_FMV_X_H] = "fmv.x.h",
    [RISCV_OPC_FMV_X_D] = "fmv.x.d",
    [RISCV_OPC_FCVT_S_W] = "fcvt.s.w",   /* Covert */
    [RISCV_OPC_FCVT_S_WU] = "fcvt.s.wu",
    [RISCV_OPC_FCVT_S_H] = "fcvt.s.h",
    [RISCV_OPC_FCVT_S_D] = "fcvt.s.d",
    [RISCV_OPC_FCVT_S_L] = "fcvt.s.l",
    [RISCV_OPC_FCVT_S_LU] = "fcvt.s.lu",
    [RISCV_OPC_FCVT_D_S] = "fcvt.d.s",
    [RISCV_OPC_FCVT_D_W]= "fcvt.d.w",
    [RISCV_OPC_FCVT_D_WU] = "fcvt.d.wu",
    [RISCV_OPC_FCVT_D_L] = "fcvt.d.l",
    [RISCV_OPC_FCVT_D_LU] = "fcvt.d.lu",
    [RISCV_OPC_FCVT_D_H] = "fcvt.d.h",
    [RISCV_OPC_FCVT_W_S] = "fcvt.w.s",
    [RISCV_OPC_FCVT_W_H] = "fcvt.w.h",
    [RISCV_OPC_FCVT_W_D] = "fcvt.w.d",
    [RISCV_OPC_FCVT_WU_S] = "fcvt.wu.s",
    [RISCV_OPC_FCVT_WU_H] = "fcvt.wu.h",
    [RISCV_OPC_FCVT_WU_D] = "fcvt.wu.d",
    [RISCV_OPC_FCVT_L_S] = "fcvt.l.s",
    [RISCV_OPC_FCVT_L_H] = "fcvt.l.h",
    [RISCV_OPC_FCVT_L_D] = "fcvt.l.d",
    [RISCV_OPC_FCVT_LU_S] = "fcvt.lu.s",
    [RISCV_OPC_FCVT_LU_H] = "fcvt.lu.h",
    [RISCV_OPC_FCVT_LU_D] = "fcvt.lu.d",
    [RISCV_OPC_FLW]    = "flw",
    [RISCV_OPC_FLD]    = "fld",
    [RISCV_OPC_FSW]    = "fsw",
    [RISCV_OPC_FSD]    = "fsd",
    [RISCV_OPC_FADD_S]  = "fadd.s",
    [RISCV_OPC_FADD_H]  = "fadd.h",
    [RISCV_OPC_FADD_D]  = "fadd.d",
    [RISCV_OPC_FSUB_S]  = "fsub.s",
    [RISCV_OPC_FSUB_H]  = "fsub.h",
    [RISCV_OPC_FSUB_D]  = "fsub.d",
    [RISCV_OPC_FMUL_S]  = "fmul.s",
    [RISCV_OPC_FMUL_H]  = "fmul.h",
    [RISCV_OPC_FMUL_D]  = "fmul.d",
    [RISCV_OPC_FDIV_S]  = "fdiv.s",
    [RISCV_OPC_FDIV_H]  = "fdiv.h",
    [RISCV_OPC_FDIV_D]  = "fdiv.d",
    [RISCV_OPC_FMADD_S]  = "fmadd.s",  /* Mul-Add */
    [RISCV_OPC_FMADD_H]  = "fmadd.h",
    [RISCV_OPC_FMADD_D]  = "fmadd.d",
    [RISCV_OPC_FMSUB_S]  = "fmsub.s",
    [RISCV_OPC_FMSUB_H]  = "fmsub.h",
    [RISCV_OPC_FMSUB_D]  = "fmsub.d",
    [RISCV_OPC_FNMSUB_S] = "fnmsub.s",
    [RISCV_OPC_FNMSUB_H] = "fnmsub.h",
    [RISCV_OPC_FNMSUB_D] = "fnmsub.d",
    [RISCV_OPC_FNMADD_S] = "fnmadd.s",
    [RISCV_OPC_FNMADD_H] = "fnmadd.d",
    [RISCV_OPC_FNMADD_D] = "fnmadd.d",
    [RISCV_OPC_FSGNJ_S] = "fsgnj.s",  /* Sign Inject */
    [RISCV_OPC_FSGNJ_H] = "fsgnj.h",
    [RISCV_OPC_FSGNJ_D] = "fsgnj.d",
    [RISCV_OPC_FSGNJN_S] = "fsgnjn.s",
    [RISCV_OPC_FSGNJN_H] = "fsgnjn.h",
    [RISCV_OPC_FSGNJN_D] = "fsgnjn.d",
    [RISCV_OPC_FSGNJX_S] = "fsgnjx.s",  /* Sign Inject */
    [RISCV_OPC_FSGNJX_H] = "fsgnjx.h",
    [RISCV_OPC_FSGNJX_D] = "fsgnjx.d",
    [RISCV_OPC_FMIN_S]  = "fmin.s",
    [RISCV_OPC_FMIN_H]  = "fmin.h",
    [RISCV_OPC_FMIN_D]  = "fmin.d",
    [RISCV_OPC_FMAX_S]  = "fmax.s",
    [RISCV_OPC_FMAX_H]  = "fmax.h",
    [RISCV_OPC_FMAX_D]  = "fmax.d",
    [RISCV_OPC_FEQ_S]   = "feq.s",
    [RISCV_OPC_FEQ_H]   = "feq.h",
    [RISCV_OPC_FEQ_D]   = "feq.d",
    [RISCV_OPC_FLT_S]   = "flt.s",
    [RISCV_OPC_FLT_H]   = "flt.h",
    [RISCV_OPC_FLT_D]   = "flt.d",
    [RISCV_OPC_FLE_S]   = "fle.s",
    [RISCV_OPC_FLE_H]   = "fle.h",
    [RISCV_OPC_FLE_D]   = "fle.d",

    // Vector
    [RISCV_OPC_VSETVL] = "vsetvl",
    [RISCV_OPC_VSETVLI] = "vsetvli",
    [RISCV_OPC_VMULH] = "vmulh",
    [RISCV_OPC_VREM]  = "vrem",
    [RISCV_OPC_VSLL]  = "vsll",
    [RISCV_OPC_VSRL]  = "vsrl",
    [RISCV_OPC_VSRA]  = "csra",
    [RISCV_OPC_VLD]   = "vld",
    [RISCV_OPC_VLDS]  = "vlds",   /* Load Strided */
    [RISCV_OPC_VLDX]  = "vldx",
    [RISCV_OPC_VST]   = "vst",
    [RISCV_OPC_VSTS]  = "vsts",
    [RISCV_OPC_VSTX]  = "vstx",
    [RISCV_OPC_VMV]  = "vmv",
    [RISCV_OPC_VMV_XS] = "vmv.x.s",
    [RISCV_OPC_VCVT]  = "vcvt",
    [RISCV_OPC_VADD]  = "vadd",
    [RISCV_OPC_VSUB_VV]  = "vsub.vv",
    [RISCV_OPC_VSUB_VX]  = "vsub.vx",
    [RISCV_OPC_VMUL]  = "vmul",
    [RISCV_OPC_VDIV]  = "vdiv",
    [RISCV_OPC_VSQRT]  = "vsqrt",
    [RISCV_OPC_VFMADD]  = "vfmadd",
    [RISCV_OPC_VFMSUB]  = "vfmsub",
    [RISCV_OPC_VFNMSUB]  = "vfnmsub",
    [RISCV_OPC_VFNMADD]  = "vfnmadd",
    [RISCV_OPC_VSGNJ]  = "vsgnj",
    [RISCV_OPC_VMIN]  = "vmin",
    [RISCV_OPC_VMAX]  = "vmax",
    [RISCV_OPC_VXOR]  = "vxor",
    [RISCV_OPC_VOR]  = "vor",
    [RISCV_OPC_VAND]  = "vand",
    [RISCV_OPC_VEXTRACT]  = "vextract", /* EXTRACT */
    [RISCV_OPC_VMSBF_M] = "vmsbf.m"
};

void print_imm_opd(RISCVImmOperand *opd)
{
    if (opd->type == RISCV_IMM_TYPE_VAL)
        fprintf(stderr,"0x%x ", opd->content.val);
    else if (opd->type == RISCV_IMM_TYPE_SYM)
        fprintf(stderr,"%s ", opd->content.sym);
}

void print_reg_opd(RISCVRegOperand *opd)
{
    fprintf(stderr,"%s ", riscv_reg_str[opd->num]);
}

void print_mem_opd(RISCVMemOperand *opd)
{
    fprintf(stderr,"[base: %s", riscv_reg_str[opd->base]);
    if (opd->offset.type == RISCV_IMM_TYPE_VAL)
        fprintf(stderr,", offset: 0x%x", opd->offset.content.val);
    else if (opd->offset.type == RISCV_IMM_TYPE_SYM)
        fprintf(stderr,", offset: %s", opd->offset.content.sym);
    fprintf(stderr,"]");
}

void print_riscv_instr(RISCVInstruction *instr)
{
    fprintf(stderr, "0x%lx: %s (%ld) ", instr->pc, riscv_opc_str[instr->opc], instr->opd_num);
    for (int i = 0; i < instr->opd_num; i++) {
        RISCVOperand *opd = &instr->opd[i];
        if (opd->type == RISCV_OPD_TYPE_IMM)
            print_imm_opd(&opd->content.imm);
        else if (opd->type == RISCV_OPD_TYPE_REG)
            print_reg_opd(&opd->content.reg);
        else if (opd->type == RISCV_OPD_TYPE_MEM)
            print_mem_opd(&opd->content.mem);
    }
    fprintf(stderr,"\n");
}

void print_riscv_instr_seq(RISCVInstruction *instr_seq)
{
    RISCVInstruction *head = instr_seq;

    while(head) {
        fprintf(stderr, "0x%lx: %s (%ld) ", head->pc, riscv_opc_str[head->opc], head->opd_num);
        for (int i = 0; i < head->opd_num; i++) {
            RISCVOperand *opd = &head->opd[i];
            if (opd->type == RISCV_OPD_TYPE_IMM)
                print_imm_opd(&opd->content.imm);
            else if (opd->type == RISCV_OPD_TYPE_REG)
                print_reg_opd(&opd->content.reg);
            else if (opd->type == RISCV_OPD_TYPE_MEM)
                print_mem_opd(&opd->content.mem);
        }
        fprintf(stderr,"\n");
        head = head->next;
    }
}

void set_riscv_instr_opc(RISCVInstruction *instr, RISCVOpcode opc)
{
    instr->opc = opc;
}

void set_riscv_instr_opd_num(RISCVInstruction *instr, size_t num)
{
    instr->opd_num = num;
}

void set_riscv_instr_opd_size(RISCVInstruction *instr)
{
    instr->OpSize = 4;
}

void set_riscv_instr_opd_type(RISCVInstruction *instr, int opd_index, RISCVOperandType type)
{
    instr->opd[opd_index].type = type;
}

void set_riscv_instr_opd_imm(RISCVInstruction *instr, int opd_index, uint32_t val)
{
    RISCVOperand *opd = &instr->opd[opd_index];
    opd->type = RISCV_OPD_TYPE_IMM;
    opd->content.imm.type = RISCV_IMM_TYPE_VAL;
    opd->content.imm.content.val = val;
}

void set_riscv_instr_opd_reg(RISCVInstruction *instr, int opd_index, int regno)
{
    RISCVOperand *opd = &instr->opd[opd_index];
    opd->type = RISCV_OPD_TYPE_REG;
    opd->content.reg.num = riscv_reg_table[regno];
}

const char *get_riscv_instr_opc(RISCVOpcode opc)
{
    return riscv_opc_str[opc];
}

static RISCVOpcode get_riscv_opcode(char *opc_str)
{
    int i;
    for (i = RISCV_OPC_INVALID; i < RISCV_OPC_END; i++) {
        if (!strcmp(opc_str, riscv_opc_str[i]))
            return static_cast<RISCVOpcode>(i);
    }
    fprintf(stderr, "[RISC-V] Error: unsupported opcode: %s\n", opc_str);
    exit(0);
    return RISCV_OPC_INVALID;
}

static RISCVRegister get_riscv_register(char *reg_str)
{
    int reg;
    for (reg = RISCV_REG_INVALID; reg < RISCV_REG_END; reg++) {
        if (!strcmp(reg_str, riscv_reg_str[reg]))
            return static_cast<RISCVRegister>(reg);
    }
    return RISCV_REG_INVALID;
}

void set_riscv_instr_opd_mem_base(RISCVInstruction *instr, int opd_index, int regno)
{
    RISCVMemOperand *mopd = &(instr->opd[opd_index].content.mem);
    mopd->base = riscv_reg_table[regno];
}

void set_riscv_instr_opd_mem_base_str(RISCVInstruction *instr, int opd_index, char *reg_str)
{
    RISCVMemOperand *mopd = &(instr->opd[opd_index].content.mem);
    mopd->base = get_riscv_register(reg_str);
}

/* set the opcode of this instruction based on the given string */
void set_riscv_instr_opc_str(RISCVInstruction *instr, char *opc_str)
{
    // FP opcode suffix
    // char *dotPos = std::strchr(opc_str, '.');
    // if (dotPos != nullptr)
    //     *dotPos = '\0';

    instr->opc = get_riscv_opcode(opc_str);
}

/* Set register type operand using register string */
void set_riscv_instr_opd_reg_str(RISCVInstruction *instr, int opd_index, char *reg_str)
{
    RISCVOperand *opd = &instr->opd[opd_index];
    size_t len = strlen(reg_str);

    if (reg_str[0] == 'r' || reg_str[len-1] == 'x') {
        instr->OpSize = 4;
    }

    opd->type = RISCV_OPD_TYPE_REG;
    opd->content.reg.num = get_riscv_register(reg_str);
}

void set_riscv_opd_type(RISCVOperand *opd, RISCVOperandType type)
{
    RISCVMemOperand *mopd;

    /* Do some intialization work here */
    switch(type) {
        case RISCV_OPD_TYPE_IMM:
            opd->content.imm.type = RISCV_IMM_TYPE_NONE;
            break;
        case RISCV_OPD_TYPE_REG:
            opd->content.reg.num = RISCV_REG_INVALID;
            break;
        case RISCV_OPD_TYPE_MEM:
            mopd = &opd->content.mem;
            mopd->base = RISCV_REG_INVALID;
            mopd->offset.type = RISCV_IMM_TYPE_NONE;
            break;
        default:
            fprintf(stderr, "Unsupported operand type in RISC-V: %d\n", type);
    }

    opd->type = type;
}

void set_riscv_opd_imm_val_str(RISCVOperand *opd, char *imm_str)
{
    RISCVImmOperand *iopd = &opd->content.imm;

    iopd->type = RISCV_IMM_TYPE_VAL;
    iopd->content.val = strtol(imm_str, NULL, 16);
}

void set_riscv_opd_imm_sym_str(RISCVOperand *opd, char *imm_str)
{
    RISCVImmOperand *iopd = &opd->content.imm;

    iopd->type = RISCV_IMM_TYPE_SYM;
    strcpy(iopd->content.sym, imm_str);
}

void set_riscv_opd_imm_pcrel_hi(RISCVOperand *opd)
{
    RISCVImmOperand *iopd = &opd->content.imm;

    iopd->pcrel = RISCV_IMM_PCREL_HI;
}

void set_riscv_opd_imm_pcrel_lo(RISCVOperand *opd)
{
    RISCVImmOperand *iopd = &opd->content.imm;

    iopd->pcrel = RISCV_IMM_PCREL_LO;
}

void set_riscv_opd_mem_off_val(RISCVOperand *opd, char *off_str)
{
    RISCVMemOperand *mopd = &opd->content.mem;

    mopd->offset.type = RISCV_IMM_TYPE_VAL;
    mopd->offset.content.val = strtol(off_str, NULL, 16);
}

void set_riscv_opd_mem_off_str(RISCVOperand *opd, char *off_str)
{
    RISCVMemOperand *mopd = &(opd->content.mem);

    mopd->offset.type = RISCV_IMM_TYPE_SYM;
    strcpy(mopd->offset.content.sym, off_str);
}

void set_riscv_opd_mem_off_pcrel_hi(RISCVOperand *opd)
{
    RISCVImmOperand *iopd = &opd->content.mem.offset;

    iopd->pcrel = RISCV_IMM_PCREL_HI;
}

void set_riscv_opd_mem_off_pcrel_lo(RISCVOperand *opd)
{
    RISCVImmOperand *iopd = &opd->content.mem.offset;

    iopd->pcrel = RISCV_IMM_PCREL_LO;
}

RISCVRegister get_riscv_reg(int regno)
{
    return riscv_reg_table[regno];
}

const char *get_riscv_reg_str(RISCVRegister reg)
{
    return riscv_reg_str[reg];
}

RISCVInstruction *get_riscv_insn(RISCVInstruction *insn_seq, uint64_t pc)
{
    RISCVInstruction *insn = insn_seq;

    while(insn) {
        if (insn->pc == pc)
            return insn;

        insn = insn->next;
    }
    return NULL;
}