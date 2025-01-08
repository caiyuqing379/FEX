#ifndef RISCV_INSTR_H
#define RISCV_INSTR_H

#include <stdint.h>
#include <stddef.h>

#define RISCV_MAX_OPERAND_NUM 4
#define RISCV_REG_NUM 32 /* x0 - x31 */

/* RISC-V 64-bit registers */
typedef enum {
    RISCV_REG_INVALID = 0,

    /* Physical registers for disassembled instructions */
    RISCV_REG_X0,  RISCV_REG_X1,  RISCV_REG_X2,  RISCV_REG_X3,
    RISCV_REG_X4,  RISCV_REG_X5,  RISCV_REG_X6,  RISCV_REG_X7,
    RISCV_REG_X8,  RISCV_REG_X9,  RISCV_REG_X10, RISCV_REG_X11,
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
    RISCV_REG_V28, RISCV_REG_V29, RISCV_REG_V30, RISCV_REG_V31,

    RISCV_REG_VT0, RISCV_REG_VT1, RISCV_REG_VT2, RISCV_REG_VT3,
    RISCV_REG_VT4, RISCV_REG_VT5, RISCV_REG_VT6,

    /* Symbolic registers for rule instructions */
    RISCV_REG_REG0, RISCV_REG_REG1, RISCV_REG_REG2, RISCV_REG_REG3,
    RISCV_REG_REG4, RISCV_REG_REG5, RISCV_REG_REG6, RISCV_REG_REG7,
    RISCV_REG_REG8, RISCV_REG_REG9, RISCV_REG_REG10, RISCV_REG_REG11,
    RISCV_REG_REG12, RISCV_REG_REG13, RISCV_REG_REG14, RISCV_REG_REG15,
    RISCV_REG_REG16, RISCV_REG_REG17, RISCV_REG_REG18, RISCV_REG_REG19,
    RISCV_REG_REG20, RISCV_REG_REG21, RISCV_REG_REG22, RISCV_REG_REG23,
    RISCV_REG_REG24, RISCV_REG_REG25, RISCV_REG_REG26, RISCV_REG_REG27,
    RISCV_REG_REG28, RISCV_REG_REG29, RISCV_REG_REG30, RISCV_REG_REG31,

    RISCV_REG_END
} RISCVRegister;

typedef enum {
    RISCV_OPC_INVALID = 0,

    // Shifts
    RISCV_OPC_SLL,
    RISCV_OPC_SLLI,
    RISCV_OPC_SRL,
    RISCV_OPC_SRLI,
    RISCV_OPC_SRA,
    RISCV_OPC_SRAI,
    RISCV_OPC_SLLW,
    RISCV_OPC_SLLIW,
    RISCV_OPC_SRLW,
    RISCV_OPC_SRLIW,
    RISCV_OPC_SRAW,
    RISCV_OPC_SRAIW,

    // Arithmetic
    RISCV_OPC_ADD,
    RISCV_OPC_ADDI,
    RISCV_OPC_LI,
    RISCV_OPC_SUB,
    RISCV_OPC_LUI,
    RISCV_OPC_AUIPC,
    RISCV_OPC_ADDW,
    RISCV_OPC_ADDIW,
    RISCV_OPC_SUBW,

    // Logical
    RISCV_OPC_AND,
    RISCV_OPC_ANDI,
    RISCV_OPC_OR,
    RISCV_OPC_ORI,
    RISCV_OPC_XOR,
    RISCV_OPC_XORI,

    // Compare
    RISCV_OPC_SLT,
    RISCV_OPC_SLTI,
    RISCV_OPC_SLTU,
    RISCV_OPC_SLTIU,

    // Branch
    RISCV_OPC_BEQ,
    RISCV_OPC_BNE,
    RISCV_OPC_BNEZ,
    RISCV_OPC_BLT,
    RISCV_OPC_BLE,
    RISCV_OPC_BGT,
    RISCV_OPC_BGE,
    RISCV_OPC_BLTU,
    RISCV_OPC_BLTZ,
    RISCV_OPC_BLEZ,
    RISCV_OPC_BGEU,
    RISCV_OPC_BGEZ,
    RISCV_OPC_BGTZ,

    // Load & Store
    RISCV_OPC_LB,
    RISCV_OPC_LH,
    RISCV_OPC_LBU,
    RISCV_OPC_LHU,
    RISCV_OPC_LW,
    RISCV_OPC_LWU,
    RISCV_OPC_LD,
    RISCV_OPC_SB,
    RISCV_OPC_SH,
    RISCV_OPC_SW,
    RISCV_OPC_SD,

    // Pseudo
    RISCV_OPC_CMP,
    RISCV_OPC_CMPB,
    RISCV_OPC_CMPW,
    RISCV_OPC_CMPQ,
    RISCV_OPC_TEST,
    RISCV_OPC_TESTB,
    RISCV_OPC_BEQZ,
    RISCV_OPC_J,
    RISCV_OPC_MV,
    RISCV_OPC_RET,
    RISCV_OPC_LDAPS,
    RISCV_OPC_SAVE_FLAGS,

    // Jump & Link
    RISCV_OPC_JAL,
    RISCV_OPC_JALR,
    RISCV_OPC_CALL,

    // Multiply-Divide
    RISCV_OPC_MUL,
    RISCV_OPC_MULH,
    RISCV_OPC_MULW,
    RISCV_OPC_MULHSU,
    RISCV_OPC_MULHU,
    RISCV_OPC_DIV,
    RISCV_OPC_DIVU,
    RISCV_OPC_DIVW,
    RISCV_OPC_REM,
    RISCV_OPC_REMU,
    RISCV_OPC_REMW,
    RISCV_OPC_REMUW,

    // Floating-Point
    RISCV_OPC_FMV_W_X,    /* Move */
    RISCV_OPC_FMV_H_X,
    RISCV_OPC_FMV_D_X,
    RISCV_OPC_FMV_X_W,
    RISCV_OPC_FMV_X_H,
    RISCV_OPC_FMV_X_D,
    RISCV_OPC_FCVT_S_W,   /* Covert */
    RISCV_OPC_FCVT_S_WU,
    RISCV_OPC_FCVT_S_H,
    RISCV_OPC_FCVT_S_D,
    RISCV_OPC_FCVT_S_L,
    RISCV_OPC_FCVT_S_LU,
    RISCV_OPC_FCVT_D_S,
    RISCV_OPC_FCVT_D_W,
    RISCV_OPC_FCVT_D_WU,
    RISCV_OPC_FCVT_D_L,
    RISCV_OPC_FCVT_D_LU,
    RISCV_OPC_FCVT_D_H,
    RISCV_OPC_FCVT_W_S,
    RISCV_OPC_FCVT_W_H,
    RISCV_OPC_FCVT_W_D,
    RISCV_OPC_FCVT_WU_S,
    RISCV_OPC_FCVT_WU_H,
    RISCV_OPC_FCVT_WU_D,
    RISCV_OPC_FCVT_L_S,
    RISCV_OPC_FCVT_L_H,
    RISCV_OPC_FCVT_L_D,
    RISCV_OPC_FCVT_LU_S,
    RISCV_OPC_FCVT_LU_H,
    RISCV_OPC_FCVT_LU_D,
    RISCV_OPC_FLW,
    RISCV_OPC_FLD,
    RISCV_OPC_FSW,
    RISCV_OPC_FSD,
    RISCV_OPC_FADD_S,  /* Arithmetic */
    RISCV_OPC_FADD_H,
    RISCV_OPC_FADD_D,
    RISCV_OPC_FSUB_S,
    RISCV_OPC_FSUB_H,
    RISCV_OPC_FSUB_D,
    RISCV_OPC_FMUL_S,
    RISCV_OPC_FMUL_H,
    RISCV_OPC_FMUL_D,
    RISCV_OPC_FDIV_S,
    RISCV_OPC_FDIV_H,
    RISCV_OPC_FDIV_D,
    RISCV_OPC_FMADD_S,  /* Mul-Add */
    RISCV_OPC_FMADD_H,
    RISCV_OPC_FMADD_D,
    RISCV_OPC_FMSUB_S,
    RISCV_OPC_FMSUB_H,
    RISCV_OPC_FMSUB_D,
    RISCV_OPC_FNMSUB_S,
    RISCV_OPC_FNMSUB_H,
    RISCV_OPC_FNMSUB_D,
    RISCV_OPC_FNMADD_S,
    RISCV_OPC_FNMADD_H,
    RISCV_OPC_FNMADD_D,
    RISCV_OPC_FSGNJ_S,  /* Sign Inject */
    RISCV_OPC_FSGNJ_H,
    RISCV_OPC_FSGNJ_D,
    RISCV_OPC_FSGNJN_S,
    RISCV_OPC_FSGNJN_H,
    RISCV_OPC_FSGNJN_D,
    RISCV_OPC_FSGNJX_S,
    RISCV_OPC_FSGNJX_H,
    RISCV_OPC_FSGNJX_D,
    RISCV_OPC_FMIN_S,
    RISCV_OPC_FMIN_H,
    RISCV_OPC_FMIN_D,
    RISCV_OPC_FMAX_S,
    RISCV_OPC_FMAX_H,
    RISCV_OPC_FMAX_D,
    RISCV_OPC_FEQ_S,
    RISCV_OPC_FEQ_H,
    RISCV_OPC_FEQ_D,
    RISCV_OPC_FLT_S,
    RISCV_OPC_FLT_H,
    RISCV_OPC_FLT_D,
    RISCV_OPC_FLE_S,
    RISCV_OPC_FLE_H,
    RISCV_OPC_FLE_D,

    // Vector
    RISCV_OPC_VSETVL,
    RISCV_OPC_VSETVLI,
    RISCV_OPC_VMULH,
    RISCV_OPC_VREM,
    RISCV_OPC_VSLL,
    RISCV_OPC_VSRL,
    RISCV_OPC_VSRA,
    RISCV_OPC_VLD,
    RISCV_OPC_VLDS,   /* Load Strided */
    RISCV_OPC_VLDX,
    RISCV_OPC_VST,
    RISCV_OPC_VSTS,
    RISCV_OPC_VSTX,
    RISCV_OPC_VMV,
    RISCV_OPC_VMV_XS,
    RISCV_OPC_VCVT,
    RISCV_OPC_VADD,
    RISCV_OPC_VSUB_VV,
    RISCV_OPC_VSUB_VX,
    RISCV_OPC_VMUL,
    RISCV_OPC_VDIV,
    RISCV_OPC_VSQRT,
    RISCV_OPC_VFMADD,
    RISCV_OPC_VFMSUB,
    RISCV_OPC_VFNMSUB,
    RISCV_OPC_VFNMADD,
    RISCV_OPC_VSGNJ,
    RISCV_OPC_VMIN,
    RISCV_OPC_VMAX,
    RISCV_OPC_VXOR,
    RISCV_OPC_VOR,
    RISCV_OPC_VAND,
    RISCV_OPC_VEXTRACT, /* EXTRACT */
    RISCV_OPC_VMSBF_M,

    RISCV_OPC_END
} RISCVOpcode;

typedef enum {
    RISCV_OPD_TYPE_INVALID = 0,
    RISCV_OPD_TYPE_IMM,
    RISCV_OPD_TYPE_REG,
    RISCV_OPD_TYPE_MEM
} RISCVOperandType;

typedef enum {
    RISCV_IMM_TYPE_NONE = 0,
    RISCV_IMM_TYPE_VAL,
    RISCV_IMM_TYPE_SYM
} RISCVImmType;

typedef enum {
    RISCV_IMM_PCREL_NONE = 0,
    RISCV_IMM_PCREL_HI,
    RISCV_IMM_PCREL_LO
} RISCVImmPCRel;

typedef struct {
    RISCVImmType type;
    RISCVImmPCRel pcrel;
    union {
        int32_t val;    /* For disasmed instructions, this value is the scaled value */
        char sym[20];   /* For rule instructions, format: "imm_xxx" */
    } content;
} RISCVImm;

typedef RISCVImm RISCVImmOperand;

typedef struct {
    RISCVRegister num;
    size_t Index;
} RISCVRegOperand;

typedef struct {
    RISCVRegister base;
    RISCVImm offset;
} RISCVMemOperand;

typedef struct {
    RISCVOperandType type;

    union {
        RISCVImm imm;
        RISCVRegOperand reg;
        RISCVMemOperand mem;
    } content;
} RISCVOperand;

typedef struct RISCVInstruction {
    uint64_t pc;    /* simulated PC of this instruction */

    RISCVOpcode opc;      /* Opcode of this instruction */
    RISCVOperand opd[RISCV_MAX_OPERAND_NUM];    /* Operands of this instruction */
    size_t opd_num;       /* number of operands of this instruction */
    size_t OpSize;        /* size of operands: 1, 2, 4, 8, or 16 bytes */
    size_t ElementSize;   /* for SIMD&FD */

    struct RISCVInstruction *prev; /* previous instruction in this block */
    struct RISCVInstruction *next; /* next instruction in this block */

    bool reg_liveness[RISCV_REG_NUM]; /* liveness of each register after this instruction */
} RISCVInstruction;

void print_riscv_instr_seq(RISCVInstruction *instr_seq);
void print_riscv_instr(RISCVInstruction *instr_seq);

void set_riscv_instr_opc(RISCVInstruction *instr, RISCVOpcode opc);
void set_riscv_instr_opc_str(RISCVInstruction *instr, char *opc_str);

void set_riscv_instr_opd_num(RISCVInstruction *instr, size_t num);
void set_riscv_instr_opd_size(RISCVInstruction *instr);

void set_riscv_instr_opd_type(RISCVInstruction *instr, int opd_index, RISCVOperandType type);
void set_riscv_instr_opd_imm(RISCVInstruction *instr, int opd_index, uint32_t val);
void set_riscv_opd_imm_pcrel_hi(RISCVOperand *opd);
void set_riscv_opd_imm_pcrel_lo(RISCVOperand *opd);

void set_riscv_instr_opd_reg(RISCVInstruction *instr, int opd_index, int regno);
void set_riscv_instr_opd_reg_str(RISCVInstruction *instr, int opd_index, char *reg_str);

void set_riscv_instr_opd_mem_base(RISCVInstruction *instr, int opd_index, int regno);
void set_riscv_instr_opd_mem_base_str(RISCVInstruction *instr, int opd_index, char *reg_str);
void set_riscv_opd_mem_off_pcrel_hi(RISCVOperand *opd);
void set_riscv_opd_mem_off_pcrel_lo(RISCVOperand *opd);

void set_riscv_opd_type(RISCVOperand *opd, RISCVOperandType type);

void set_riscv_opd_imm_val_str(RISCVOperand *, char *);
void set_riscv_opd_imm_sym_str(RISCVOperand *, char *);

void set_riscv_opd_mem_off_val(RISCVOperand *opd, char *off_str);
void set_riscv_opd_mem_off_str(RISCVOperand *opd, char *off_str);

const char *get_riscv_instr_opc(RISCVOpcode);
const char *get_riscv_reg_str(RISCVRegister);
RISCVRegister get_riscv_reg(int regno);
RISCVInstruction *get_riscv_insn(RISCVInstruction *, uint64_t);

#endif
