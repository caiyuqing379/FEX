#ifndef X86_INSTR_H
#define X86_INSTR_H

#include "../X86Tables/X86Tables.h"

#include <cstdint>
#include <stddef.h>

#define X86_MAX_OPERAND_NUM 3 // 2 for integer and 3 for AVX

#define X86_REG_NUM 37 /* invalid, r0 - r15, CF, NF, VF, and ZF */

typedef enum {
    X86_REG_INVALID = 0,

    /* Physical registers */
    X86_REG_RAX, X86_REG_RCX, X86_REG_RDX, X86_REG_RBX,
    X86_REG_RSP, X86_REG_RBP, X86_REG_RSI, X86_REG_RDI,
    X86_REG_R8, X86_REG_R9, X86_REG_R10, X86_REG_R11,
    X86_REG_R12, X86_REG_R13, X86_REG_R14, X86_REG_R15,
    X86_REG_XMM0, X86_REG_XMM1, X86_REG_XMM2, X86_REG_XMM3,
    X86_REG_XMM4, X86_REG_XMM5, X86_REG_XMM6, X86_REG_XMM7,
    X86_REG_XMM8, X86_REG_XMM9, X86_REG_XMM10, X86_REG_XMM11,
    X86_REG_XMM12, X86_REG_XMM13, X86_REG_XMM14, X86_REG_XMM15,

    /* Eflags */
    X86_REG_OF, X86_REG_SF, X86_REG_CF, X86_REG_ZF,

    /* Symbolic registers for rule instructions, synchronized with guest */
    X86_REG_REG0, X86_REG_REG1, X86_REG_REG2, X86_REG_REG3,
    X86_REG_REG4, X86_REG_REG5, X86_REG_REG6, X86_REG_REG7,
    X86_REG_REG8, X86_REG_REG9, X86_REG_REG10, X86_REG_REG11,
    X86_REG_REG12, X86_REG_REG13, X86_REG_REG14, X86_REG_REG15,
    X86_REG_REG16, X86_REG_REG17, X86_REG_REG18, X86_REG_REG19,
    X86_REG_REG20, X86_REG_REG21, X86_REG_REG22, X86_REG_REG23,
    X86_REG_REG24, X86_REG_REG25, X86_REG_REG26, X86_REG_REG27,
    X86_REG_REG28, X86_REG_REG29, X86_REG_REG30, X86_REG_REG31,

    /* Symbolic registers for rule instructions, temporary registers */
    X86_REG_TEMP0, X86_REG_TEMP1, X86_REG_TEMP2, X86_REG_TEMP3,

    X86_REG_END
} X86Register;

typedef enum {
    X86_OPC_INVALID = 0,
    X86_OPC_NOP,
    // BASE
    X86_OPC_MOVZX,
    X86_OPC_MOVSX,
    X86_OPC_MOVSXD,
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
    X86_OPC_JL,
    X86_OPC_JLE,
    X86_OPC_JG,
    X86_OPC_JGE,
    X86_OPC_JE,
    X86_OPC_JNE,
    X86_OPC_JS,
    X86_OPC_JNS,

    X86_OPC_PUSH,
    X86_OPC_POP,

    X86_OPC_CALL,
    X86_OPC_RET,

    // SSE/AVX
    // Load/Store
    X86_OPC_MOVUPS,
    X86_OPC_MOVUPD,
    X86_OPC_MOVSS,
    X86_OPC_MOVSD,
    X86_OPC_MOVLPS,
    X86_OPC_MOVLPD,
    X86_OPC_MOVHPS,
    X86_OPC_MOVHPD,
    X86_OPC_MOVAPS,
    X86_OPC_MOVAPD,
    X86_OPC_MOVD,
    X86_OPC_MOVQ,
    X86_OPC_MOVDQA,
    X86_OPC_MOVDQU,
    X86_OPC_PMOVMSKB,

    X86_OPC_PACKUSWB,
    X86_OPC_PACKSSWB,
    X86_OPC_PACKSSDW,
    X86_OPC_PALIGNR,

    // Logical
    X86_OPC_ANDPS,
    X86_OPC_ANDPD,
    X86_OPC_ORPS,
    X86_OPC_ORPD,
    X86_OPC_XORPS,
    X86_OPC_XORPD,
    X86_OPC_PAND,
    X86_OPC_PANDN,
    X86_OPC_POR,
    X86_OPC_PXOR,

    // Shuffle
    X86_OPC_PUNPCKLBW,
    X86_OPC_PUNPCKLWD,
    X86_OPC_PUNPCKLDQ,
    X86_OPC_PUNPCKHBW,
    X86_OPC_PUNPCKHWD,
    X86_OPC_PUNPCKHDQ,
    X86_OPC_PUNPCKLQDQ,
    X86_OPC_PUNPCKHQDQ,
    X86_OPC_PSHUFD,
    X86_OPC_PSHUFLW,
    X86_OPC_PSHUFHW,

    // Comparison
    X86_OPC_PCMPGTB,
    X86_OPC_PCMPGTW,
    X86_OPC_PCMPGTD,
    X86_OPC_PCMPEQB,
    X86_OPC_PCMPEQW,
    X86_OPC_PCMPEQD,

    // Algorithm
    X86_OPC_ADDPS,
    X86_OPC_ADDPD,
    X86_OPC_ADDSS,
    X86_OPC_ADDSD,
    X86_OPC_SUBPS,
    X86_OPC_SUBPD,
    X86_OPC_SUBSS,
    X86_OPC_SUBSD,
    X86_OPC_PSUBB,
    X86_OPC_PADDD,

    X86_OPC_SET_LABEL, // fake instruction to generate label

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
    bool isRipLiteral;
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
void set_x86_instr_opd_imm(X86Instruction *instr, int opd_index, uint64_t val, bool isRipLiteral = false);
void set_x86_instr_opd_reg(X86Instruction *instr, int opd_index, int regno, bool HighBits);
void set_x86_instr_opd_mem_base(X86Instruction *instr, int opd_index, int regno);
void set_x86_instr_opd_mem_off(X86Instruction *instr, int opd_index, int32_t offset);
void set_x86_instr_opd_mem_scale(X86Instruction *instr, int opd_index, uint8_t scale);
void set_x86_instr_opd_mem_index(X86Instruction *instr, int opd_index, int regno);

void set_x86_opd_type(X86Operand *opd, X86OperandType type);
void set_x86_opd_imm_val_str(X86Operand *opd, char *imm_str, bool isRipLiteral = false, bool neg = false);
void set_x86_opd_imm_sym_str(X86Operand *opd, char *imm_str, bool isRipLiteral = false);
void set_x86_opd_reg_str(X86Operand *opd, char *reg_str, uint32_t *OpSize);

void set_x86_opd_mem_off(X86Operand *, int32_t val);
void set_x86_opd_mem_base_str(X86Operand *opd, char *reg_str);
void set_x86_opd_mem_index_str(X86Operand *, char *);
void set_x86_opd_mem_scale_str(X86Operand *, char *);
void set_x86_opd_mem_off_str(X86Operand *opd, char *off_str, bool neg);

const char *get_x86_reg_str(X86Register );
bool x86_instr_test_branch(X86Instruction *instr);

void DecodeInstToX86Inst(FEXCore::X86Tables::DecodedInst *DecodeInst, X86Instruction *instr);
#endif
