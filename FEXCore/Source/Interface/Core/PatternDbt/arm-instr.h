#ifndef ARM_INSTR_H
#define ARM_INSTR_H

#include <stdint.h>
#include <stddef.h>

#define ARM_MAX_OPERAND_NUM 4

#define ARM_REG_NUM 21 /* invalid, r0 - r15, CF, NF, VF, and ZF */

typedef enum {
    ARM_REG_INVALID = 0,

    /* Physical registers for disasmed instrucutions */
    ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3,
    ARM_REG_R4, ARM_REG_R5, ARM_REG_R6, ARM_REG_R7,
    ARM_REG_R8, ARM_REG_R9, ARM_REG_R10,
    ARM_REG_R11,
    ARM_REG_R12,
    ARM_REG_R13,
    ARM_REG_R14,
    ARM_REG_R15,
    ARM_REG_R16, ARM_REG_R17, ARM_REG_R18, ARM_REG_R19,
    ARM_REG_R20, ARM_REG_R21, ARM_REG_R22, ARM_REG_R23,
    ARM_REG_R24, ARM_REG_R25, ARM_REG_R26, ARM_REG_R27,
    ARM_REG_R28, ARM_REG_R29, ARM_REG_R30, ARM_REG_R31,

    ARM_REG_FP, ARM_REG_LR, ARM_REG_RSP, ARM_REG_ZR,

    /* Conditional Code */
    ARM_REG_CF, ARM_REG_NF, ARM_REG_VF, ARM_REG_ZF,

    /* Symbolic registers for rule instructions */
    ARM_REG_REG0, ARM_REG_REG1, ARM_REG_REG2, ARM_REG_REG3,
    ARM_REG_REG4, ARM_REG_REG5, ARM_REG_REG6, ARM_REG_REG7,
    ARM_REG_REG8, ARM_REG_REG9, ARM_REG_REG10, ARM_REG_REG11,
    ARM_REG_REG12, ARM_REG_REG13, ARM_REG_REG14, ARM_REG_REG15,

    ARM_REG_END
} ARMRegister;

typedef enum {
    ARM_CC_INVALID,
    ARM_CC_EQ,  // 0000 Equal
    ARM_CC_NE,  // 0001 Not equal
    ARM_CC_CS,  // 0010 Carry set/unsigned higher or same
    ARM_CC_CC,  // 0011 Carry clear/unsigned lower
    ARM_CC_MI,  // 0100 Minus/negative
    ARM_CC_PL,  // 0101 Plus/positive or zero
    ARM_CC_VS,  // 0110 Overflow
    ARM_CC_VC,  // 0111 No overflow
    ARM_CC_HI,  // 1000 Unsigned higher
    ARM_CC_LS,  // 1001 Unsigned lower or same
    ARM_CC_GE,  // 1010 Signed greater than or equal
    ARM_CC_LT,  // 1011 Signed less than
    ARM_CC_GT,  // 1100 Signed greater than
    ARM_CC_LE,  // 1101 Signed less than or equal
    ARM_CC_AL,  // 1110 Always (unconditional)
    ARM_CC_XX,  // 1111 Not used currently
    ARM_CC_END
} ARMConditionCode;

typedef enum {
    ARM_OPC_INVALID = 0,

    ARM_OPC_LDRB,
    ARM_OPC_LDRSB,
    ARM_OPC_LDRH,
    ARM_OPC_LDRSH,
    ARM_OPC_LDR,
    ARM_OPC_LDAR,
    ARM_OPC_LDP,
    ARM_OPC_STRB,
    ARM_OPC_STRH,
    ARM_OPC_STR,
    ARM_OPC_STP,

    ARM_OPC_SXTW,

    ARM_OPC_MOV,
    ARM_OPC_MVN,
    ARM_OPC_CSEL,

    ARM_OPC_AND,
    ARM_OPC_ANDS,
    ARM_OPC_ORR,
    ARM_OPC_EOR,
    ARM_OPC_BIC,
    ARM_OPC_BICS,

    ARM_OPC_LSL,
    ARM_OPC_LSR,
    ARM_OPC_ASR,

    ARM_OPC_ADD,
    ARM_OPC_ADC,
    ARM_OPC_SUB,
    ARM_OPC_SBC,
    ARM_OPC_ADDS,
    ARM_OPC_ADCS,
    ARM_OPC_SUBS,
    ARM_OPC_SBCS,
    ARM_OPC_MUL,
    ARM_OPC_UMULL,
    ARM_OPC_SMULL,

    ARM_OPC_CLZ,

    ARM_OPC_TST,
    ARM_OPC_CMP,
    ARM_OPC_CMPB,
    ARM_OPC_CMN,

    ARM_OPC_B,
    ARM_OPC_BL,
    ARM_OPC_CBZ,
    ARM_OPC_CBNZ,

    ARM_OPC_SET_JUMP,
    ARM_OPC_SET_CALL,
    ARM_OPC_PC_L,
    ARM_OPC_PC_LB,
    ARM_OPC_PC_S,
    ARM_OPC_PC_SB,

// parameterized opcode
    ARM_OPC_OP1,
    ARM_OPC_OP2,
    ARM_OPC_OP3,
    ARM_OPC_OP4,
    ARM_OPC_OP5,
    ARM_OPC_OP6,
    ARM_OPC_OP7,
    ARM_OPC_OP8,
    ARM_OPC_OP9,
    ARM_OPC_OP10,
    ARM_OPC_OP11,
    ARM_OPC_OP12,

    ARM_OPC_END
} ARMOpcode;

typedef enum {
    ARM_OPD_TYPE_INVALID = 0,
    ARM_OPD_TYPE_IMM,
    ARM_OPD_TYPE_REG,
    ARM_OPD_TYPE_MEM
} ARMOperandType;

typedef enum {
    ARM_OPD_SCALE_TYPE_NONE = 0,
    ARM_OPD_SCALE_TYPE_SHIFT,
    ARM_OPD_SCALE_TYPE_EXT
} ARMOperandScaleType;

typedef enum {
    ARM_OPD_DIRECT_NONE = 0,
    ARM_OPD_DIRECT_LSL,
    ARM_OPD_DIRECT_LSR,
    ARM_OPD_DIRECT_ASR,
    ARM_OPD_DIRECT_ROR,
    ARM_OPD_DIRECT_END
} ARMOperandScaleDirect;

typedef enum {
    ARM_OPD_EXTEND_NONE = 0,
    ARM_OPD_EXTEND_UXTB,
    ARM_OPD_EXTEND_UXTH,
    ARM_OPD_EXTEND_UXTW,
    ARM_OPD_EXTEND_UXTX,
    ARM_OPD_EXTEND_SXTB,
    ARM_OPD_EXTEND_SXTH,
    ARM_OPD_EXTEND_SXTW,
    ARM_OPD_EXTEND_SXTX,
    ARM_OPD_EXTEND_LSL_32 = ARM_OPD_EXTEND_UXTW,
    ARM_OPD_EXTEND_LSL_64 = ARM_OPD_EXTEND_UXTX,
    ARM_OPD_EXTEND_END
} ARMOperandScaleExtend;

typedef enum {
    ARM_IMM_TYPE_NONE = 0,
    ARM_IMM_TYPE_VAL,
    ARM_IMM_TYPE_SYM
} ARMImmType;

typedef struct {
    ARMImmType type;
    union {
        int32_t val;    /* For disasmed instructions, this value is the scaled value */
        char sym[20];   /* For rule instructions, format: "imm_xxx" */
    } content;
} ARMImm;

typedef ARMImm ARMImmOperand;

typedef struct {
    ARMOperandScaleType type;
    ARMImm imm;
    union {
        ARMOperandScaleDirect direct;
        ARMOperandScaleExtend extend;
    } content;
} ARMOperandScale;

typedef struct {
    ARMRegister num;
    ARMOperandScale scale;
} ARMRegOperand;

typedef enum {
    ARM_MEM_INDEX_TYPE_NONE = 0,
    ARM_MEM_INDEX_TYPE_PRE,
    ARM_MEM_INDEX_TYPE_POST
} ARMMemIndexType;

typedef struct {
    ARMRegister base;
    ARMRegister index;
    ARMImm offset;

    ARMOperandScale scale;
    ARMMemIndexType pre_post;
} ARMMemOperand;

typedef struct {
    ARMOperandType type;

    union {
        ARMImmOperand imm;
        ARMRegOperand reg;
        ARMMemOperand mem;
    } content;
} ARMOperand;

typedef struct ARMInstruction {
    uint64_t pc;    /* simulated PC of this instruction */

    ARMConditionCode cc;    /* Condition code of this instruction */

    ARMOpcode opc;      /* Opcode of this instruction */
    ARMOperand opd[ARM_MAX_OPERAND_NUM];    /* Operands of this instruction */
    size_t opd_num;     /* number of operands of this instruction */
    size_t OpdSize;     /* size of operands: 1, 2, or 4 bytes */

    struct ARMInstruction *prev; /* previous instruction in this block */
    struct ARMInstruction *next; /* next instruction in this block */

    bool reg_liveness[ARM_REG_NUM]; /* liveness of each register after this instruction.
                                       True: this register will be used
                                       False: this regsiter will not be used
                                   Currently, we only maitain this for the four condition codes */

    bool save_cc;   /* If this instruction defines conditon code, save_cc indicates if it is necessary to
                        save the condition code when do rule translation. */
    uint32_t raw_binary; /* raw binary code of this instruction */
} ARMInstruction;

void print_arm_instr_seq(ARMInstruction *instr_seq);
void print_arm_instr(ARMInstruction *instr_seq);

void set_arm_instr_cc(ARMInstruction *instr, uint32_t cond);
void set_arm_instr_opc(ARMInstruction *instr, ARMOpcode opc);
void set_arm_instr_opc_str(ARMInstruction *instr, char *opc_str);

void set_arm_instr_opd_num(ARMInstruction *instr, size_t num);
void set_arm_instr_opd_size(ARMInstruction *instr);

void set_arm_instr_opd_type(ARMInstruction *instr, int opd_index, ARMOperandType type);
void set_arm_instr_opd_imm(ARMInstruction *instr, int opd_index, uint32_t val);

void set_arm_instr_opd_reg(ARMInstruction *instr, int opd_index, int regno);
void set_arm_instr_opd_reg_str(ARMInstruction *instr, int opd_index, char *reg_str);
void set_arm_instr_opd_reg_scale_imm(ARMInstruction *instr, int opd_index, uint32_t shift_op, uint32_t shift);

bool set_arm_instr_opd_scale_str(ARMOperandScale *, char *);
void set_arm_instr_opd_scale_imm_str(ARMOperandScale *, char *);
void set_arm_instr_opd_scale_reg_str(ARMOperandScale *, char *);

void set_arm_instr_opd_mem_base(ARMInstruction *instr, int opd_index, int regno);
void set_arm_instr_opd_mem_base_str(ARMInstruction *instr, int opd_index, char *reg_str);
void set_arm_instr_opd_mem_index(ARMInstruction *instr, int opd_index, int regno);
void set_arm_instr_opd_mem_index_str(ARMInstruction *instr, int opd_index, char *reg_str);

void set_arm_instr_opd_mem_index_type(ARMInstruction *, int, ARMMemIndexType);

void set_arm_opd_type(ARMOperand *opd, ARMOperandType type);

void set_arm_opd_imm_val_str(ARMOperand *, char *);
void set_arm_opd_imm_sym_str(ARMOperand *, char *);

void set_arm_opd_mem_off_val(ARMOperand *opd, char *off_str);
void set_arm_opd_mem_off_str(ARMOperand *opd, char *off_str);

void set_arm_opd_mem_index_reg(ARMOperand *, int);
void set_arm_opd_mem_scale_imm(ARMOperand *, uint32_t, uint32_t);

const char *get_arm_instr_opc(ARMOpcode);
const char *get_arm_reg_str(ARMRegister);
ARMRegister get_arm_reg(int regno);
ARMInstruction *get_arm_insn(ARMInstruction *, uint64_t);
#endif
