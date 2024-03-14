#ifndef X86_INSTR_H
#define X86_INSTR_H

#include "../X86Tables/X86Tables.h"

#include <cstdint>
#include <stddef.h>

#define X86_MAX_OPERAND_NUM 3 // 2 for integer and 3 for AVX

#define X86_REG_NUM 21 /* invalid, r0 - r15, CF, NF, VF, and ZF */

typedef enum {
    X86_REG_INVALID = 0,

    /* Physical registers */
    X86_REG_RAX, X86_REG_RCX, X86_REG_RDX, X86_REG_RBX,
    X86_REG_RSP, X86_REG_RBP, X86_REG_RSI, X86_REG_RDI,
    X86_REG_R8, X86_REG_R9, X86_REG_R10, X86_REG_R11,
    X86_REG_R12, X86_REG_R13, X86_REG_R14, X86_REG_R15,

    /* Eflags */
    X86_REG_OF, X86_REG_SF, X86_REG_CF, X86_REG_ZF,

    /* Symbolic registers for rule instructions, synchronized with guest */
    X86_REG_REG0, X86_REG_REG1, X86_REG_REG2, X86_REG_REG3,
    X86_REG_REG4, X86_REG_REG5, X86_REG_REG6, X86_REG_REG7,
    X86_REG_REG8, X86_REG_REG9, X86_REG_REG10, X86_REG_REG11,
    X86_REG_REG12, X86_REG_REG13, X86_REG_REG14, X86_REG_REG15,

    /* Symbolic registers for rule instructions, temporary registers */
    X86_REG_TEMP0, X86_REG_TEMP1, X86_REG_TEMP2, X86_REG_TEMP3,

    X86_REG_END
} X86Register;

typedef enum {
    X86_OPC_INVALID = 0,

    X86_OPC_MOVZX,
    X86_OPC_MOVSX,
    X86_OPC_MOV,
    X86_OPC_LEA,

    X86_OPC_NOT,
    X86_OPC_AND,
    X86_OPC_OR,
    X86_OPC_XOR,
    X86_OPC_NEG,

    X86_OPC_INC,
    X86_OPC_DEC,
    X86_OPC_ADD,
    X86_OPC_ADC,
    X86_OPC_SUB,
    X86_OPC_SBB,
	X86_OPC_MULL,
    X86_OPC_IMUL,

    X86_OPC_SHL,
    X86_OPC_SHR,
    X86_OPC_SAR,
    X86_OPC_SHLD,
    X86_OPC_SHRD,

    X86_OPC_BT,
    X86_OPC_TEST,
    X86_OPC_CMP,

    X86_OPC_CMOVNE,
    X86_OPC_CMOVA,
    X86_OPC_CMOVB,
    X86_OPC_CMOVL,

    X86_OPC_SETE,
    X86_OPC_CWT,

    X86_OPC_JMP,
    X86_OPC_JA,
    X86_OPC_JAE,
    X86_OPC_JB,
    X86_OPC_JBE,
    X86_OPC_JL, X86_OPC_JLE,
    X86_OPC_JG, X86_OPC_JGE,
    X86_OPC_JE, X86_OPC_JNE,
    X86_OPC_JS, X86_OPC_JNS,

    X86_OPC_PUSH,
    X86_OPC_POP,

    X86_OPC_CALL,
    X86_OPC_RET,

    X86_OPC_SET_LABEL, // fake instruction to generate label

    //parameterized opcode
    X86_OPC_OP1,
    X86_OPC_OP2,
    X86_OPC_OP3,
    X86_OPC_OP4,
    X86_OPC_OP5,
    X86_OPC_OP6,
    X86_OPC_OP7,
    X86_OPC_OP8,
    X86_OPC_OP9,
    X86_OPC_OP10,
    X86_OPC_OP11,
    X86_OPC_OP12,

    X86_OPC_END
} X86Opcode;

// x86 imm operand
typedef enum{
    X86_IMM_TYPE_NONE = 0,
    X86_IMM_TYPE_VAL,
    X86_IMM_TYPE_SYM
} X86ImmType;

typedef struct {
    X86ImmType type;
    union {
        uint64_t val;
        char sym[20]; /* this symbol might contain expression */
    } content;
} X86Imm;

typedef X86Imm X86ImmOperand;

// x86 reg operand
typedef struct {
    bool HighBits;
    X86Register num;
} X86RegOperand;

// x86 mem operand
typedef struct {
    X86Register base;
    X86Register index;
    X86Imm scale;
    X86Imm offset;
} X86MemOperand;

// x86 operand
typedef enum {
    X86_OPD_TYPE_NONE = 0,
    X86_OPD_TYPE_IMM,
    X86_OPD_TYPE_REG,
    X86_OPD_TYPE_MEM
} X86OperandType;

typedef struct {
    X86OperandType type;
    union {
        X86ImmOperand imm;
        X86RegOperand reg;
        X86MemOperand mem;
    } content;
} X86Operand;

typedef struct X86Instruction {
    uint64_t pc;    /* simulated PC of this instruction */

    X86Opcode opc;      /* Opcode of this instruction */
    X86Operand opd[X86_MAX_OPERAND_NUM];    /* Operands of this instruction */
    uint8_t opd_num;     /* number of operands of this instruction */

    uint32_t SrcSize;
    uint32_t DestSize;
    size_t InstSize; /* size of operands: 1, 2, or 4 bytes */

    struct X86Instruction *prev; /* previous instruction in this block */
    struct X86Instruction *next;    /* next instruction */

    bool reg_liveness[X86_REG_NUM]; /* liveness of each register after this instruction.
                                       True: this register will be used
                                       False: this regsiter will not be used
                                    Currently, we only maitain this for the four condition codes */
    bool save_cc;   /* If this instruction defines conditon code, save_cc indicates if it is necessary to
                        save the condition code when do rule translation. */
} X86Instruction;

extern X86Instruction *instr_buffer;
extern int instr_buffer_index;
extern int instr_block_start;

void x86_instr_buffer_init(void);

const char *get_x86_opc_str(X86Opcode opc);
X86Instruction *create_x86_instr(uint64_t pc);
void print_x86_instr(X86Instruction *instr);
void print_x86_instr_seq(X86Instruction *instr_seq);

void set_x86_instr_opc(X86Instruction *instr, X86Opcode opc);
void set_x86_instr_opc_str(X86Instruction *instr, char *opc_str);
void set_x86_instr_opd_num(X86Instruction *instr, uint8_t num);
void set_x86_instr_opd_size(X86Instruction *instr, uint32_t SrcSize, uint32_t DestSize);
void set_x86_instr_size(X86Instruction *instr, size_t size);

void set_x86_instr_opd_type(X86Instruction *instr, int opd_index, X86OperandType type);
void set_x86_instr_opd_imm(X86Instruction *instr, int opd_index, uint64_t val);
void set_x86_instr_opd_reg(X86Instruction *instr, int opd_index, int regno, bool HighBits);
void set_x86_instr_opd_mem_base(X86Instruction *instr, int opd_index, int regno);
void set_x86_instr_opd_mem_off(X86Instruction *instr, int opd_index, int32_t offset);
void set_x86_instr_opd_mem_scale(X86Instruction *instr, int opd_index, uint8_t scale);
void set_x86_instr_opd_mem_index(X86Instruction *instr, int opd_index, int regno);

void set_x86_opd_type(X86Operand *opd, X86OperandType type);
void set_x86_opd_imm_val_str(X86Operand *opd, char *imm_str);
void set_x86_opd_imm_sym_str(X86Operand *opd, char *imm_str);
void set_x86_opd_reg_str(X86Operand *opd, char *reg_str, uint32_t *OpdSize);

void set_x86_opd_mem_off(X86Operand *, int32_t val);
void set_x86_opd_mem_base_str(X86Operand *opd, char *reg_str);
void set_x86_opd_mem_index_str(X86Operand *, char *);
void set_x86_opd_mem_scale_str(X86Operand *, char *);
void set_x86_opd_mem_off_str(X86Operand *opd, char *off_str, bool neg);

const char *get_x86_reg_str(X86Register );
bool x86_instr_test_branch(X86Instruction *instr);

void DecodeInstToX86Inst(FEXCore::X86Tables::DecodedInst *DecodeInst, X86Instruction *instr);
#endif
