#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "x86-instr.h"

#define MAX_INSTR_NUM 1000000

X86Instruction *instr_buffer;
int instr_buffer_index;
int instr_block_start;

static const char *x86_opc_str[] = {
    [X86_OPC_INVALID] = "**** unsupported (x86) ****",
    [X86_OPC_MOVB] = "movb",
    [X86_OPC_MOVZBL] = "movzbl",
    [X86_OPC_MOVSBL] = "movsbl",
    [X86_OPC_MOVW] = "movw",
    [X86_OPC_MOVZWL] = "movzwl",
    [X86_OPC_MOVSWL] = "movswl",
    [X86_OPC_MOVL] = "movl",
    [X86_OPC_LEAL] = "leal",
    [X86_OPC_NOTL] = "notl",
    [X86_OPC_ANDB] = "andb",
    [X86_OPC_ANDW] = "andw",
    [X86_OPC_ANDL] = "andl",
    [X86_OPC_ORB] = "orb",
    [X86_OPC_XORB] = "xorb",
    [X86_OPC_ORW] = "orw",
    [X86_OPC_ORL] = "orl",
    [X86_OPC_XORL] = "xorl",
    [X86_OPC_NEGL] = "negl",
    [X86_OPC_INCB] = "incb",
    [X86_OPC_INCW] = "incw",
    [X86_OPC_INCL] = "incl",
    [X86_OPC_DECB] = "decb",
    [X86_OPC_DECW] = "decw",
    [X86_OPC_DECL] = "decl",
    [X86_OPC_ADDB] = "addb",
    [X86_OPC_ADDW] = "addw",
    [X86_OPC_ADDL] = "addl",
    [X86_OPC_ADCL] = "adcl",
    [X86_OPC_SUBL] = "subl",
    [X86_OPC_SBBL] = "sbbl",
    [X86_OPC_MULL] = "mull",
    [X86_OPC_IMULL] = "imull",
    [X86_OPC_SHLB] = "shlb",
    [X86_OPC_SHRB] = "shrb",
    [X86_OPC_SHLW] = "shlw",
    [X86_OPC_SHLL] = "shll",
    [X86_OPC_SHRL] = "shrl",
    [X86_OPC_SARL] = "sarl",
    [X86_OPC_SHLDL] = "shldl",
    [X86_OPC_SHRDL] = "shrdl",
    [X86_OPC_BTL] = "btl",
    [X86_OPC_TESTB] = "testb",
    [X86_OPC_TESTW] = "testw",
    [X86_OPC_TESTL] = "testl",
    [X86_OPC_CMPB] = "cmpb",
    [X86_OPC_CMPW] = "cmpw",
    [X86_OPC_CMPL] = "cmpl",
    [X86_OPC_CMOVNEL] = "cmovnel",
    [X86_OPC_CMOVAL] = "cmoval",
    [X86_OPC_CMOVBL] = "cmovbl",
    [X86_OPC_CMOVLL] = "cmovll",
    [X86_OPC_SETE] = "sete",
    [X86_OPC_CWT] = "cwt",
    [X86_OPC_JMP] = "jmp",
    [X86_OPC_JA]  = "ja",
    [X86_OPC_JAE] = "jae",
    [X86_OPC_JB] = "jb",
    [X86_OPC_JBE] = "jbe",
    [X86_OPC_JL] = "jl",
    [X86_OPC_JLE] = "jle",
    [X86_OPC_JG] = "jg",
    [X86_OPC_JGE] = "jge",
    [X86_OPC_JE] = "je",
    [X86_OPC_JNE] = "jne",
    [X86_OPC_JS] = "js",
    [X86_OPC_JNS] = "jns",
    [X86_OPC_PUSH] = "push",
    [X86_OPC_POP] = "pop",
    [X86_OPC_CALL] = "call",
    [X86_OPC_RET] = "ret",
    [X86_OPC_SET_LABEL] = "set label",

    //parameterized opcode
    [X86_OPC_OP1] = "op1",
    [X86_OPC_OP2] = "op2",
    [X86_OPC_OP3] = "op3",
    [X86_OPC_OP4] = "op4",
    [X86_OPC_OP5] = "op5",
    [X86_OPC_OP6] = "op6",
    [X86_OPC_OP7] = "op7",
    [X86_OPC_OP8] = "op8",
    [X86_OPC_OP9] = "op9",
    [X86_OPC_OP10] = "op10",
    [X86_OPC_OP11] = "op11",
    [X86_OPC_OP12] = "op12",

    [X86_OPC_SYNC_M] = "sync_m",
    [X86_OPC_SYNC_R] = "sync_r",
	  [X86_OPC_PC_IR] = "pc_ir",
	  [X86_OPC_PC_RR] = "pc_rr",
};

static const X86Register x86_reg_table[] = {
    X86_REG_RAX, X86_REG_RCX, X86_REG_RDX, X86_REG_RBX,
    X86_REG_RSP, X86_REG_RBP, X86_REG_RSI, X86_REG_RDI,
    X86_REG_R8, X86_REG_R9, X86_REG_R10, X86_REG_R11,
    X86_REG_R12, X86_REG_R13, X86_REG_R14, X86_REG_R15,
    X86_REG_INVALID
};

static const char *x86_reg_str[] = {
    "none",
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",

    "of", "sf", "cf", "zf",

    "reg0", "reg1", "reg2", "reg3", "reg4", "reg5", "reg6", "reg7",
    "reg8", "reg9", "reg10", "reg11", "reg12", "reg13", "reg14", "reg15",

    "temp0", "temp1", "temp2", "temp3"
};

void x86_instr_buffer_init(void)
{
    instr_buffer = new X86Instruction[MAX_INSTR_NUM];
    if (instr_buffer == NULL)
        LogMan::Msg::IFmt( "Cannot allocate memory for instruction buffer!");

    instr_buffer_index = 0;
    instr_block_start = 0;
}

static X86Opcode get_x86_opcode(char *opc_str)
{
    int i;
    for (i = X86_OPC_INVALID; i < X86_OPC_END; i++) {
        if (!strcmp(opc_str, x86_opc_str[i]))
            return static_cast<X86Opcode>(i);
    }
    LogMan::Msg::IFmt( "==== [X86] unsupported opcode: {}", opc_str);
    exit(-1);
    return X86_OPC_INVALID;
}

static X86Register get_x86_register(char *str)
{
    int reg;
    for (reg = X86_REG_INVALID; reg < X86_REG_END; reg++) {
        if(!strcmp(str, x86_reg_str[reg]))
            return static_cast<X86Register>(reg);
    }
    return X86_REG_INVALID;
}

const char *get_x86_opc_str(X86Opcode opc)
{
    return x86_opc_str[opc];
}

/* create an X86 instruction and insert it to tb */
X86Instruction *create_x86_instr(uint64_t pc)
{
    X86Instruction *instr = &instr_buffer[instr_buffer_index];
    if (instr_buffer_index >= MAX_INSTR_NUM)
        LogMan::Msg::IFmt( "Instruction buffer is not enough!\n");

    instr->pc = pc;
    instr->next = nullptr;

    if (instr_block_start == instr_buffer_index)
        instr->prev = nullptr;
    else {
        instr->prev = &instr_buffer[instr_buffer_index-1];
        instr->prev->next = instr;
    }

    for (int i = 0; i < X86_MAX_OPERAND_NUM; i++)
        instr->opd[i].type = X86_OPD_TYPE_NONE;
    instr->opc = X86_OPC_INVALID;
    for (int i = 0; i < X86_REG_NUM; i++)
        instr->reg_liveness[i] = true;

    instr_buffer_index++;

    return instr;
}

void print_x86_instr(X86Instruction *instr)
{
    int i;
    LogMan::Msg::IFmt("0x{:x}: {}", instr->pc, x86_opc_str[instr->opc]);
    for (i = 0; i < instr->opd_num; i++) {
      X86Operand *opd = &instr->opd[i];
      if (opd->type == X86_OPD_TYPE_IMM) {
        X86ImmOperand *imm = &opd->content.imm;
        if (imm->type == X86_IMM_TYPE_VAL)
          LogMan::Msg::IFmt("     imm: 0x{:x}", imm->content.val);
        else if (imm->type == X86_IMM_TYPE_SYM)
          LogMan::Msg::IFmt("     imm: {}", imm->content.sym);
      }
      else if (opd->type == X86_OPD_TYPE_REG)
        LogMan::Msg::IFmt("     reg: {}", x86_reg_str[opd->content.reg.num]);
      else if (opd->type == X86_OPD_TYPE_MEM) {
        fprintf(stderr,"[INFO]      mem: base(%s)", x86_reg_str[opd->content.mem.base]);
        if (opd->content.mem.index != X86_REG_INVALID)
          fprintf(stderr,", index(%s)", x86_reg_str[opd->content.mem.index]);
        if (opd->content.mem.scale.type == X86_IMM_TYPE_SYM)
          fprintf(stderr,", scale(%s)", opd->content.mem.scale.content.sym);
        else if (opd->content.mem.scale.type == X86_IMM_TYPE_VAL)
          fprintf(stderr,", scale(0x%lx)", opd->content.mem.scale.content.val);
        if (opd->content.mem.offset.type == X86_IMM_TYPE_SYM)
          fprintf(stderr,", offset(%s)", opd->content.mem.offset.content.sym);
        else if (opd->content.mem.offset.type == X86_IMM_TYPE_VAL)
          fprintf(stderr,", offset(0x%lx)", opd->content.mem.offset.content.val);
        fprintf(stderr,"\n");
      }
    }
}

void print_x86_instr_seq(X86Instruction *instr_seq)
{
    X86Instruction *head = instr_seq;

    while(head) {
        print_x86_instr(head);
        head = head->next;
    }
}

void set_x86_instr_opc(X86Instruction *instr, X86Opcode opc)
{
    instr->opc = opc;
}

/* set the opcode of this insturction */
void set_x86_instr_opc_str(X86Instruction *instr, char *opc_str)
{
    instr->opc = get_x86_opcode(opc_str);
}

/* set the number of operands of this instruction */
void set_x86_instr_opd_num(X86Instruction *instr, uint32_t num)
{
    instr->opd_num = num;
}

void set_x86_instr_size(X86Instruction *instr, size_t size)
{
    instr->InstSize = size;
}

void set_x86_instr_opd_type(X86Instruction *instr, int opd_index, X86OperandType type)
{
    set_x86_opd_type(&instr->opd[opd_index], type);
}

void set_x86_instr_opd_reg(X86Instruction *instr, int opd_index, int regno)
{
    X86Operand *opd = &instr->opd[opd_index];

    opd->type = X86_OPD_TYPE_REG;
    opd->content.reg.num = x86_reg_table[regno];
}

void set_x86_instr_opd_imm(X86Instruction *instr, int opd_index, uint64_t val)
{
    X86Operand *opd = &instr->opd[opd_index];

    opd->type = X86_OPD_TYPE_IMM;
    opd->content.imm.type = X86_IMM_TYPE_VAL;
    opd->content.imm.content.val = val;
}

void set_x86_instr_opd_mem_base(X86Instruction *instr, int opd_index, int regno)
{
    X86MemOperand *mopd = &(instr->opd[opd_index].content.mem);

    mopd->base = x86_reg_table[regno];
}

void set_x86_instr_opd_mem_off(X86Instruction *instr, int opd_index, int32_t offset)
{
    set_x86_opd_mem_off(&(instr->opd[opd_index]), offset);
}

void set_x86_instr_opd_mem_scale(X86Instruction *instr, int opd_index, uint8_t scale)
{
    X86MemOperand *mopd = &(instr->opd[opd_index].content.mem);

    mopd->scale.type = X86_IMM_TYPE_VAL;
    mopd->scale.content.val = scale;
}

void set_x86_instr_opd_mem_index(X86Instruction *instr, int opd_index, int regno)
{
    X86MemOperand *mopd = &(instr->opd[opd_index].content.mem);

    mopd->index = x86_reg_table[regno];
}

/* set the type of this operand */
void set_x86_opd_type(X86Operand *opd, X86OperandType type)
{
    switch(type) {
        case X86_OPD_TYPE_IMM:
            opd->content.imm.type = X86_IMM_TYPE_NONE;
            break;
        case X86_OPD_TYPE_REG:
            opd->content.reg.num = X86_REG_INVALID;
            break;
        case X86_OPD_TYPE_MEM:
            opd->content.mem.base = X86_REG_INVALID;
            opd->content.mem.index = X86_REG_INVALID;
            opd->content.mem.scale.type = X86_IMM_TYPE_NONE;
            opd->content.mem.offset.type = X86_IMM_TYPE_NONE;
            break;
        default:
            fprintf(stderr, "Unsupported operand type in X86: %d\n", type);
    }

    opd->type = type;
}

/* set immediate operand using given string */
void set_x86_opd_imm_val_str(X86Operand *opd, char *imm_str)
{
    X86ImmOperand *iopd = &opd->content.imm;

    iopd->type = X86_IMM_TYPE_VAL;
    iopd->content.val = strtol(imm_str, NULL, 0);
}

void set_x86_opd_imm_sym_str(X86Operand *opd, char *imm_str)
{
    X86ImmOperand *iopd = &opd->content.imm;

    iopd->type = X86_IMM_TYPE_SYM;
    strcpy(iopd->content.sym, imm_str);
}

/* set register operand using given string */
void set_x86_opd_reg_str(X86Operand *opd, char *reg_str)
{
    opd->content.reg.num = get_x86_register(reg_str);
}

void set_x86_opd_mem_base_str(X86Operand *opd, char *reg_str)
{
    opd->content.mem.base = get_x86_register(reg_str);
}

void set_x86_opd_mem_index_str(X86Operand *opd, char *reg_str)
{
    opd->content.mem.index = get_x86_register(reg_str);
}

void set_x86_opd_mem_scale_str(X86Operand *opd, char *scale_str)
{
    if (strstr(scale_str, "imm")) {
        opd->content.mem.scale.type = X86_IMM_TYPE_SYM;
        strcpy(opd->content.mem.scale.content.sym, scale_str);
    } else {
        opd->content.mem.scale.type = X86_IMM_TYPE_VAL;
        opd->content.mem.scale.content.val = atoi(scale_str);
    }
}

void set_x86_opd_mem_off(X86Operand *opd, int32_t val)
{
    opd->content.mem.offset.type = X86_IMM_TYPE_VAL;
    opd->content.mem.offset.content.val = val;
}

void set_x86_opd_mem_off_str(X86Operand *opd, char *off_str)
{
    if (strstr(off_str, "imm")) { /* offset is a symbol */
        opd->content.mem.offset.type = X86_IMM_TYPE_SYM;
        strcpy(opd->content.mem.offset.content.sym, off_str);
    } else { /* offset is a constant integer */
        opd->content.mem.offset.type = X86_IMM_TYPE_VAL;
        if(off_str[0] == '-') /* negative value */
            opd->content.mem.offset.content.val = 0 - strtol(&off_str[1], NULL, 0);
        else
            opd->content.mem.offset.content.val = strtol(off_str, NULL, 0);
    }
}

const char *get_x86_reg_str(X86Register reg)
{
    return x86_reg_str[reg];
}

bool x86_instr_test_branch(X86Instruction *instr)
{
    if (instr->opc == X86_OPC_JMP
      || instr->opc == X86_OPC_JA || instr->opc == X86_OPC_JAE
      || instr->opc == X86_OPC_JB || instr->opc == X86_OPC_JBE
      || instr->opc == X86_OPC_JL || instr->opc == X86_OPC_JLE
      || instr->opc == X86_OPC_JG || instr->opc == X86_OPC_JGE
      || instr->opc == X86_OPC_JE || instr->opc == X86_OPC_JNE
      || instr->opc == X86_OPC_JS || instr->opc == X86_OPC_JNS)
      return true;
    return false;
}

static inline bool insn_define_cc(X86Opcode opc)
{
    if ((opc == X86_OPC_ANDB) || (opc == X86_OPC_ORB) ||
       (opc == X86_OPC_XORB) || (opc == X86_OPC_ANDW) ||
       (opc == X86_OPC_ORW)  || (opc == X86_OPC_ANDL) ||
       (opc == X86_OPC_ORL)  || (opc == X86_OPC_XORL) ||
       (opc == X86_OPC_NEGL) || (opc == X86_OPC_INCB) ||
       (opc == X86_OPC_INCL) || (opc == X86_OPC_DECB) ||
       (opc == X86_OPC_DECW) || (opc == X86_OPC_INCW) ||
       (opc == X86_OPC_DECL) || (opc == X86_OPC_ADDB) ||
       (opc == X86_OPC_ADDW) || (opc == X86_OPC_ADDL) ||
       (opc == X86_OPC_ADCL) || (opc == X86_OPC_SUBL) ||
       (opc == X86_OPC_SBBL) || (opc == X86_OPC_IMULL) ||
       (opc == X86_OPC_SHLB) || (opc == X86_OPC_SHRB) ||
       (opc == X86_OPC_SHLW) || (opc == X86_OPC_SHLL) ||
       (opc == X86_OPC_SHRL) || (opc == X86_OPC_SARL) ||
       (opc == X86_OPC_SHLDL)|| (opc == X86_OPC_SHRDL)||
       (opc == X86_OPC_BTL)  || (opc == X86_OPC_TESTW)||
       (opc == X86_OPC_TESTB)|| (opc == X86_OPC_CMPB) ||
       (opc == X86_OPC_CMPW) || (opc == X86_OPC_TESTL)||
       (opc == X86_OPC_CMPL))
      return true;
    return false;
}

void decide_reg_liveness(int succ_define_cc, X86Instruction *insn_seq)
{
    bool cur_liveness[X86_REG_NUM];
    X86Instruction *tail;
    X86Instruction *insn;
    bool cc_revised;
    int i;

    for (i = 0; i < X86_REG_NUM; i++)
        cur_liveness[i] = true;

    if (succ_define_cc == 3) {
        for (i = X86_REG_OF; i < X86_REG_NUM; i++)
            cur_liveness[i] = false;
    }

    /* Find out the tail */
    tail = insn_seq;
    while (tail && tail->next)
        tail = tail->next;

    /* Decide register liveness */
    insn = tail;
    while(insn) {
        for (i = 0; i < X86_REG_NUM; i++)
            insn->reg_liveness[i] = cur_liveness[i];

        /* Check if this instruciton uses any condition code */
        /* 1. Conditional execution */
        switch (insn->opc) {
            case X86_OPC_CMOVNEL:
            case X86_OPC_SETE:
            case X86_OPC_JE:
            case X86_OPC_JNE:
                cur_liveness[X86_REG_ZF] = true;
                break;
            case X86_OPC_CMOVBL:
            case X86_OPC_JAE:
            case X86_OPC_JB:
                cur_liveness[X86_REG_CF] = true;
                break;
            case X86_OPC_JS:
            case X86_OPC_JNS:
                cur_liveness[X86_REG_SF] = true;
                break;
            case X86_OPC_CMOVAL:
            case X86_OPC_JA:
            case X86_OPC_JBE:
                cur_liveness[X86_REG_CF] = true;
                cur_liveness[X86_REG_ZF] = true;
                break;
            case X86_OPC_CMOVLL:
            case X86_OPC_JL:
            case X86_OPC_JGE:
                cur_liveness[X86_REG_SF] = true;
                cur_liveness[X86_REG_OF] = true;
                break;
            case X86_OPC_JLE:
            case X86_OPC_JG:
                cur_liveness[X86_REG_ZF] = true;
                cur_liveness[X86_REG_SF] = true;
                cur_liveness[X86_REG_OF] = true;
                break;
            default:
                fprintf(stderr, "Error: unexpected condition code: %d\n", insn->opc);
        }
        /* 2. Condition code as an operand */
        if (insn->opc == X86_OPC_ADCL || insn->opc == X86_OPC_SBBL || insn->opc == X86_OPC_BTL)
            cur_liveness[X86_REG_CF] = true;

        /* 3. Update the liveness if this instruciton defines condition code */
        if (insn_define_cc(insn->opc)) {
            for (i = X86_REG_OF; i < X86_REG_NUM; i++)
                cur_liveness[i] = false;
        }

        insn = insn->prev;
    }

    /* Decide if we need to save condition codes for instructions that define condtion codes */
}

void DecodeInstToX86Inst(FEXCore::X86Tables::DecodedInst *DecodeInst, X86Instruction *instr)
{
    bool SingleSrc = false;

    // A normal instruction is the most likely.
    if (DecodeInst->TableInfo->Type == FEXCore::X86Tables::TYPE_INST) [[likely]] {
        if (!strcmp(DecodeInst->TableInfo->Name, "MOV")
          && ((DecodeInst->OP == 0x88 || DecodeInst->OP == 0x8A)
          || (DecodeInst->OP >= 0xB0 && DecodeInst->OP <= 0xB7))) {
            set_x86_instr_opc(instr, X86_OPC_MOVB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOV")
          && ((DecodeInst->OP == 0x89 || DecodeInst->OP == 0x8B)
          || (DecodeInst->OP >= 0xB8 && DecodeInst->OP <= 0xBF))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_MOVW : X86_OPC_MOVL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOVZX") && (DecodeInst->OP == 0xB6)) {
            set_x86_instr_opc(instr, X86_OPC_MOVZBL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOVZX") && (DecodeInst->OP == 0xB7)) {
            set_x86_instr_opc(instr, X86_OPC_MOVZWL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSX") && (DecodeInst->OP == 0xBE)) {
            set_x86_instr_opc(instr, X86_OPC_MOVSBL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSX") && (DecodeInst->OP == 0xBF)) {
            set_x86_instr_opc(instr, X86_OPC_MOVSWL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "LEA") && (DecodeInst->OP == 0x8D)) {
            set_x86_instr_opc(instr, X86_OPC_LEAL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "AND")
          && (DecodeInst->OP == 0x20 || DecodeInst->OP == 0x22 || DecodeInst->OP == 0x24)) {
            set_x86_instr_opc(instr, X86_OPC_ANDB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "AND")
          && (DecodeInst->OP == 0x21 || DecodeInst->OP == 0x23 || DecodeInst->OP == 0x25)) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_ANDW : X86_OPC_ANDL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "OR")
          && (DecodeInst->OP == 0x20 || DecodeInst->OP == 0x22 || DecodeInst->OP == 0x24)) {
            set_x86_instr_opc(instr, X86_OPC_ORB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "OR")
          && (DecodeInst->OP == 0x21 || DecodeInst->OP == 0x23 || DecodeInst->OP == 0x25)) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_ORW : X86_OPC_ORL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "XOR")
          && (DecodeInst->OP == 0x30 || DecodeInst->OP == 0x32 || DecodeInst->OP == 0x34)) {
            set_x86_instr_opc(instr, X86_OPC_XORB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "XOR")
          && (DecodeInst->OP == 0x31 || DecodeInst->OP == 0x33 || DecodeInst->OP == 0x35)) {
            set_x86_instr_opc(instr, X86_OPC_XORL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "INC")
          && (DecodeInst->OP == 0x40)) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_INCW : X86_OPC_INCL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "DEC")
          && (DecodeInst->OP == 0x48)) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_DECW : X86_OPC_DECL);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADD")
          && (DecodeInst->OP == 0x00 || DecodeInst->OP == 0x02 || DecodeInst->OP == 0x04)) {
            set_x86_instr_opc(instr, X86_OPC_ADDB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADD")
          && (DecodeInst->OP == 0x01 || DecodeInst->OP == 0x03 || DecodeInst->OP == 0x05)) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_ADDW : X86_OPC_ADDL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADC")
          && (DecodeInst->OP == 0x11 || DecodeInst->OP == 0x13 || DecodeInst->OP == 0x15)) {
            set_x86_instr_opc(instr, X86_OPC_ADCL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SUB")
          && (DecodeInst->OP == 0x29 || DecodeInst->OP == 0x2B || DecodeInst->OP == 0x2D)) {
            set_x86_instr_opc(instr, X86_OPC_SUBL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SBB")
          && (DecodeInst->OP == 0x19 || DecodeInst->OP == 0x1B || DecodeInst->OP == 0x1D)) {
            set_x86_instr_opc(instr, X86_OPC_SBBL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "IMUL")
          && (DecodeInst->OP == 0x69 || DecodeInst->OP == 0x6B || DecodeInst->OP == 0xAF)) {
            set_x86_instr_opc(instr, X86_OPC_IMULL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "BT")
          && (DecodeInst->OP == 0xA3)) {
            set_x86_instr_opc(instr, X86_OPC_BTL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "TEST")
          && (DecodeInst->OP == 0x84)) {
            set_x86_instr_opc(instr, X86_OPC_TESTB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "TEST")
          && (DecodeInst->OP == 0x85)) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_TESTW : X86_OPC_TESTL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMP")
          && (DecodeInst->OP == 0x38 || DecodeInst->OP == 0x3A || DecodeInst->OP == 0x3C)) {
            set_x86_instr_opc(instr, X86_OPC_CMPB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMP")
          && (DecodeInst->OP == 0x39 || DecodeInst->OP == 0x3B || DecodeInst->OP == 0x3D)) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_CMPW : X86_OPC_CMPL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVNZ")
          && (DecodeInst->OP == 0x45)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVNEL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVNBE")
          && (DecodeInst->OP == 0x47)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVAL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVB")
          && (DecodeInst->OP == 0x42)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVBL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVL")
          && (DecodeInst->OP == 0x4C)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVLL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SETZ")
          && (DecodeInst->OP == 0x94)) {
            set_x86_instr_opc(instr, X86_OPC_SETE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CQO")
          && (DecodeInst->OP == 0x99)) {
            set_x86_instr_opc(instr, X86_OPC_CWT);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JMP")
          && (DecodeInst->OP == 0xE9 || DecodeInst->OP == 0xEB)) {
            set_x86_instr_opc(instr, X86_OPC_JMP);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JNBE")
          && (DecodeInst->OP == 0x77 || DecodeInst->OP == 0x87)) {
            set_x86_instr_opc(instr, X86_OPC_JA);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JNB")
          && (DecodeInst->OP == 0x73 || DecodeInst->OP == 0x83)) {
            set_x86_instr_opc(instr, X86_OPC_JAE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JB")
          && (DecodeInst->OP == 0x72 || DecodeInst->OP == 0x82)) {
            set_x86_instr_opc(instr, X86_OPC_JB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JBE")
          && (DecodeInst->OP == 0x76 || DecodeInst->OP == 0x86)) {
            set_x86_instr_opc(instr, X86_OPC_JBE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JL")
          && (DecodeInst->OP == 0x7C || DecodeInst->OP == 0x8C)) {
            set_x86_instr_opc(instr, X86_OPC_JL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JLE")
          && (DecodeInst->OP == 0x7E || DecodeInst->OP == 0x8E)) {
            set_x86_instr_opc(instr, X86_OPC_JLE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JNLE")
          && (DecodeInst->OP == 0x7F || DecodeInst->OP == 0x8F)) {
            set_x86_instr_opc(instr, X86_OPC_JG);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JNL")
          && (DecodeInst->OP == 0x7D || DecodeInst->OP == 0x8D)) {
            set_x86_instr_opc(instr, X86_OPC_JGE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JZ")
          && (DecodeInst->OP == 0x74 || DecodeInst->OP == 0x84)) {
            set_x86_instr_opc(instr, X86_OPC_JE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JNZ")
          && (DecodeInst->OP == 0x75 || DecodeInst->OP == 0x85)) {
            set_x86_instr_opc(instr, X86_OPC_JNE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JS")
          && (DecodeInst->OP == 0x78 || DecodeInst->OP == 0x88)) {
            set_x86_instr_opc(instr, X86_OPC_JS);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JNS")
          && (DecodeInst->OP == 0x79 || DecodeInst->OP == 0x89)) {
            set_x86_instr_opc(instr, X86_OPC_JNS);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "PUSH")
          && ((DecodeInst->OP >= 0x50 && DecodeInst->OP <= 0x57) || DecodeInst->OP == 0x68 || DecodeInst->OP == 0x6A
          || DecodeInst->OP == 0x06 || DecodeInst->OP == 0x0E || DecodeInst->OP == 0x16
          || DecodeInst->OP == 0x1E || DecodeInst->OP == 0xA0 || DecodeInst->OP == 0xA8)) {
            set_x86_instr_opc(instr, X86_OPC_PUSH);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "POP")
          && ((DecodeInst->OP >= 0x58 && DecodeInst->OP <= 0x5F) || DecodeInst->OP == 0x8F
          || DecodeInst->OP == 0x07 || DecodeInst->OP == 0x17 || DecodeInst->OP == 0x1F
          || DecodeInst->OP == 0xA1 || DecodeInst->OP == 0xA9)) {
            set_x86_instr_opc(instr, X86_OPC_POP);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CALL")
          && (DecodeInst->OP == 0xE8)) {
            set_x86_instr_opc(instr, X86_OPC_CALL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "RET")
          && (DecodeInst->OP == 0xC2 || DecodeInst->OP == 0xC3)) {
            set_x86_instr_opc(instr, X86_OPC_RET);
        }

        // TYPE_GROUP_1~11
#define OPD(group, prefix, Reg) (((group - FEXCore::X86Tables::TYPE_GROUP_1) << 6) | (prefix) << 3 | (Reg))

        if (!strcmp(DecodeInst->TableInfo->Name, "MOV")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_11, FEXCore::X86Tables::OpToIndex(0xC6), 0))) {
            set_x86_instr_opc(instr, X86_OPC_MOVB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOV")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_11, FEXCore::X86Tables::OpToIndex(0xC7), 0))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_MOVW : X86_OPC_MOVL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "NOT")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 2))) {
            set_x86_instr_opc(instr, X86_OPC_NOTL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "NEG")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 3))) {
            set_x86_instr_opc(instr, X86_OPC_NEGL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "AND")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 4))) {
            set_x86_instr_opc(instr, X86_OPC_ANDB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "AND")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 4))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_ANDW : X86_OPC_ANDL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "OR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 1)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 1))) {
            set_x86_instr_opc(instr, X86_OPC_ORB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "OR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 1)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 1))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_ORW : X86_OPC_ORL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "XOR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 6))) {
            set_x86_instr_opc(instr, X86_OPC_XORB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "XOR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 6))) {
            set_x86_instr_opc(instr, X86_OPC_XORL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "INC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_4, FEXCore::X86Tables::OpToIndex(0xFE), 0))) {
            set_x86_instr_opc(instr, X86_OPC_INCB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "INC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5, FEXCore::X86Tables::OpToIndex(0xFF), 0))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_INCW : X86_OPC_INCL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "DEC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_4, FEXCore::X86Tables::OpToIndex(0xFE), 1))) {
            set_x86_instr_opc(instr, X86_OPC_DECB);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "DEC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5, FEXCore::X86Tables::OpToIndex(0xFF), 1))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_DECW : X86_OPC_DECL);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADD")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 0))) {
            set_x86_instr_opc(instr, X86_OPC_ANDB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADD")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 0))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_ANDW : X86_OPC_ANDL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 2)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 2))) {
            set_x86_instr_opc(instr, X86_OPC_ADCL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SUB")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 5))) {
            set_x86_instr_opc(instr, X86_OPC_SUBL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SBB")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 3)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 3))) {
            set_x86_instr_opc(instr, X86_OPC_SUBL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "IMULL")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 5))) {
            set_x86_instr_opc(instr, X86_OPC_IMULL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHL")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC0), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC0), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD2), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD2), 6))) {
            set_x86_instr_opc(instr, X86_OPC_SHLB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHL")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 6))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_SHLW : X86_OPC_SHLL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC0), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD2), 5))) {
            set_x86_instr_opc(instr, X86_OPC_SHRB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 5))) {
            set_x86_instr_opc(instr, X86_OPC_SHRL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SAR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 7))) {
            set_x86_instr_opc(instr, X86_OPC_SARL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "TEST")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF6), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF6), 1))) {
            set_x86_instr_opc(instr, X86_OPC_TESTB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "TEST")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 1))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_TESTW : X86_OPC_TESTL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMP")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 7))) {
            set_x86_instr_opc(instr, X86_OPC_CMPB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMP")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 7))) {
            set_x86_instr_opc(instr, DecodeInst->LastEscapePrefix == 0x66 ? X86_OPC_CMPW : X86_OPC_CMPL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "JMP")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5, FEXCore::X86Tables::OpToIndex(0xFF), 4))) {
            set_x86_instr_opc(instr, X86_OPC_JMP);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "PUSH")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5, FEXCore::X86Tables::OpToIndex(0xFF), 6))) {
            set_x86_instr_opc(instr, X86_OPC_PUSH);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CALL")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5, FEXCore::X86Tables::OpToIndex(0xFF), 2))) {
            set_x86_instr_opc(instr, X86_OPC_CALL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MUL")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 4))) {
            set_x86_instr_opc(instr, X86_OPC_MULL);
        }
#undef OPD

        // TYPE_VEX_TABLE_PREFIX
#define OPD(map_select, pp, opcode) (((map_select - 1) << 10) | (pp << 8) | (opcode))
        if (!strcmp(DecodeInst->TableInfo->Name, "SHLX") && (DecodeInst->OP == OPD(2, 0b01, 0xF7))) {
            set_x86_instr_opc(instr, X86_OPC_SHLDL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHRX") && (DecodeInst->OP == OPD(2, 0b11, 0xF7))) {
            set_x86_instr_opc(instr, X86_OPC_SHRDL);
        }
#undef OPD
    }

    int num = 0, count = 0;
    FEXCore::X86Tables::DecodedOperand *Opd = &DecodeInst->Dest;

    while(Opd) {
        if(!Opd->IsNone()) {
          if (SingleSrc && num == 1)
            break;

          LogMan::Msg::IFmt( "====Operand Num: 0x{:x}", num+1);
          if (Opd->IsGPR()){
              uint8_t GPR = Opd->Data.GPR.GPR;

              LogMan::Msg::IFmt( "     GPR: 0x{:x}", GPR);

              if(GPR <= FEXCore::X86State::REG_R15)
                set_x86_instr_opd_reg(instr, num, GPR);
              else
                set_x86_instr_opd_reg(instr, num, 0x10);
          }
          else if(Opd->IsRIPRelative()){
              uint32_t Literal = Opd->Data.RIPLiteral.Value.u;
              LogMan::Msg::IFmt( "     RIPLiteral: 0x{:x}", Literal);
              set_x86_instr_opd_imm(instr, num, Literal);
          }
          else if(Opd->IsLiteral()){
              uint64_t Literal = Opd->Data.Literal.Value;
              LogMan::Msg::IFmt( "     Literal: 0x{:x}", Literal);
              set_x86_instr_opd_imm(instr, num, Literal);
          }
          else if(Opd->IsGPRDirect()) {
              uint8_t GPR = Opd->Data.GPR.GPR;
              LogMan::Msg::IFmt( "     GPRDirect: 0x{:x}", GPR);
              set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
              if(GPR <= FEXCore::X86State::REG_R15)
                set_x86_instr_opd_mem_base(instr, num, GPR);
              else
                set_x86_instr_opd_mem_base(instr, num, 0x10);
          }
          else if(Opd->IsGPRIndirect()){
              uint8_t GPR = Opd->Data.GPRIndirect.GPR;
              int32_t Displacement = Opd->Data.GPRIndirect.Displacement;

              LogMan::Msg::IFmt( "     GPRIndirect - GPR: 0x{:x}, Displacement: 0x{:x}", GPR, Displacement);

              set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
              if(GPR <= FEXCore::X86State::REG_R15)
                set_x86_instr_opd_mem_base(instr, num, GPR);
              else
                set_x86_instr_opd_mem_base(instr, num, 0x10);
              set_x86_instr_opd_mem_off(instr, num, Displacement);
          }
          else if(Opd->IsSIB()){
              uint8_t Base = Opd->Data.SIB.Base;
              int32_t Offset = Opd->Data.SIB.Offset;
              uint8_t Index = Opd->Data.SIB.Index;
              uint8_t Scale = Opd->Data.SIB.Scale;

              LogMan::Msg::IFmt( "     SIB - Base: 0x{:x}, Offset: 0x{:x}, Index: 0x{:x}, Scale: 0x{:x}", Base, Offset, Index, Scale);

              set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
              if(Base <= FEXCore::X86State::REG_R15)
                set_x86_instr_opd_mem_base(instr, num, Base);
              else
                set_x86_instr_opd_mem_base(instr, num, 0x10);
              set_x86_instr_opd_mem_off(instr, num, Offset);
              if(Index <= FEXCore::X86State::REG_R15)
                set_x86_instr_opd_mem_index(instr, num, Index);
              else
                set_x86_instr_opd_mem_index(instr, num, 0x10);
              set_x86_instr_opd_mem_scale(instr, num, Scale);
          }
          num++;
        }

      if (count >= 3) {
        Opd = nullptr;
        break;
      } else {
        Opd = &DecodeInst->Src[count++];
      }
    }

    if (instr->opc == X86_OPC_CMPB || instr->opc == X86_OPC_CMPW || instr->opc == X86_OPC_CMPL) {
      if(num == 3) {
        instr->opd[1] = instr->opd[2];
        num--;
      }
    }

    set_x86_instr_opd_num(instr, num);

    print_x86_instr(instr);
}
