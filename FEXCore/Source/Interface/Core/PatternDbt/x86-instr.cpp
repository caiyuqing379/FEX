#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "x86-instr.h"
#include "rule-debug-log.h"

#define MAX_INSTR_NUM 1000000

static const char *x86_opc_str[] = {
    [X86_OPC_INVALID] = "**** unsupported (x86) ****",
    [X86_OPC_NOP] = "nop",
    [X86_OPC_MOVZX] = "movzx",
    [X86_OPC_MOVSX] = "movsx",
    [X86_OPC_MOVSXD] = "movsxd",
    [X86_OPC_MOV] = "mov",
    [X86_OPC_LEA] = "lea",
    [X86_OPC_NOT] = "not",
    [X86_OPC_AND] = "and",
    [X86_OPC_OR] = "or",
    [X86_OPC_XOR] = "xor",
    [X86_OPC_NEG] = "neg",
    [X86_OPC_INC] = "inc",
    [X86_OPC_DEC] = "dec",
    [X86_OPC_ADD] = "add",
    [X86_OPC_ADC] = "adc",
    [X86_OPC_SUB] = "sub",
    [X86_OPC_SBB] = "sbb",
    [X86_OPC_MULL] = "mul",
    [X86_OPC_IMUL] = "imul",
    [X86_OPC_SHL] = "shl",
    [X86_OPC_SHR] = "shr",
    [X86_OPC_SAR] = "sar",
    [X86_OPC_SHLD] = "shld",
    [X86_OPC_SHRD] = "shrd",
    [X86_OPC_BT] = "bt",
    [X86_OPC_TEST] = "test",
    [X86_OPC_CMP] = "cmp",
    [X86_OPC_CMOVNE] = "cmovne",
    [X86_OPC_CMOVA] = "cmova",
    [X86_OPC_CMOVB] = "cmovb",
    [X86_OPC_CMOVL] = "cmovl",
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
    "reg16", "reg17", "reg18", "reg19", "reg20", "reg21", "reg22", "reg23",
    "reg24", "reg25", "reg26", "reg27", "reg28", "reg29", "reg30", "reg31",

    "temp0", "temp1", "temp2", "temp3"
};

static X86Opcode get_x86_opcode(char *opc_str)
{
    for (int i = X86_OPC_INVALID; i < X86_OPC_END; i++) {
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

void print_x86_instr(X86Instruction *instr)
{
    LogMan::Msg::IFmt("0x{:x}: opcode: {} destsize:{} srcsize:{}",
        instr->pc, x86_opc_str[instr->opc], instr->DestSize, instr->SrcSize);

    for (int i = 0; i < instr->opd_num; i++) {
      X86Operand *opd = &instr->opd[i];
      if (opd->type == X86_OPD_TYPE_IMM) {
        X86ImmOperand *imm = &opd->content.imm;
        if (imm->type == X86_IMM_TYPE_VAL)
          LogMan::Msg::IFmt("     imm: 0x{:x}", imm->content.val);
        else if (imm->type == X86_IMM_TYPE_SYM)
          LogMan::Msg::IFmt("     imm: {}", imm->content.sym);
      }
      else if (opd->type == X86_OPD_TYPE_REG) {
        LogMan::Msg::IFmt("     reg: {}", x86_reg_str[opd->content.reg.num]);
      }
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
        LogMan::Msg::EFmt("");
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
void set_x86_instr_opd_num(X86Instruction *instr, uint8_t num)
{
    instr->opd_num = num;
}

void set_x86_instr_opd_size(X86Instruction *instr, uint32_t SrcSize, uint32_t DestSize)
{
    instr->SrcSize = SrcSize;
    instr->DestSize = DestSize;
}

void set_x86_instr_size(X86Instruction *instr, size_t size)
{
    instr->InstSize = size;
}

void set_x86_instr_opd_type(X86Instruction *instr, int opd_index, X86OperandType type)
{
    set_x86_opd_type(&instr->opd[opd_index], type);
}

void set_x86_instr_opd_reg(X86Instruction *instr, int opd_index, int regno, bool HighBits)
{
    X86Operand *opd = &instr->opd[opd_index];

    opd->type = X86_OPD_TYPE_REG;
    opd->content.reg.num = x86_reg_table[regno];
    opd->content.reg.HighBits = HighBits;
}

void set_x86_instr_opd_imm(X86Instruction *instr, int opd_index, uint64_t val, bool isRipLiteral)
{
    X86Operand *opd = &instr->opd[opd_index];

    opd->type = X86_OPD_TYPE_IMM;
    opd->content.imm.type = X86_IMM_TYPE_VAL;
    opd->content.imm.content.val = val;
    opd->content.imm.isRipLiteral = isRipLiteral;
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
void set_x86_opd_imm_val_str(X86Operand *opd, char *imm_str, bool isRipLiteral, bool neg)
{
    X86ImmOperand *iopd = &opd->content.imm;

    iopd->type = X86_IMM_TYPE_VAL;
    if (neg) /* negative value */
      iopd->content.val = 0 - strtol(imm_str, NULL, 16);
    else
      iopd->content.val = strtol(imm_str, NULL, 16);
    iopd->isRipLiteral = isRipLiteral;
}

void set_x86_opd_imm_sym_str(X86Operand *opd, char *imm_str, bool isRipLiteral)
{
    X86ImmOperand *iopd = &opd->content.imm;

    iopd->type = X86_IMM_TYPE_SYM;
    strcpy(iopd->content.sym, imm_str);
    iopd->isRipLiteral = isRipLiteral;
}

void insert_char_begin(char *reg_str, char new_char) {
    size_t length = strlen(reg_str);
    memmove(reg_str + 1, reg_str, length + 1);
    reg_str[0] = new_char;
}

void proc_reg_str(X86Operand *opd, char *reg_str, uint32_t *OpdSize)
{
    int length = strlen(reg_str);
    opd->content.reg.HighBits = false;

    if (!strcmp(reg_str, "ah") || !strcmp(reg_str, "bh")|| !strcmp(reg_str, "ch") || !strcmp(reg_str, "dh")) {
        // byte
        *OpdSize = 1;
        opd->content.reg.HighBits = true;
        insert_char_begin(reg_str, 'r');
        reg_str[length] = 'x';
    } else if (!strcmp(reg_str, "al") || !strcmp(reg_str, "bl") || !strcmp(reg_str, "cl")
        || !strcmp(reg_str, "dl") || !strcmp(reg_str, "sil") || !strcmp(reg_str, "dil")
        || !strcmp(reg_str, "bpl") || !strcmp(reg_str, "spl") || reg_str[length-1] == 'b') {
        // byte
        *OpdSize = 1;
        if (reg_str[length-1] == 'b') {
          reg_str[length-1] = '\0';
        } else {
          insert_char_begin(reg_str, 'r');
          if (length == 2)
            reg_str[length] = 'x';
          else
            reg_str[length] = '\0';
        }
    } else if (!strcmp(reg_str, "ax") || !strcmp(reg_str, "bx") || !strcmp(reg_str, "cx")
        || !strcmp(reg_str, "dx") || !strcmp(reg_str, "sp") || !strcmp(reg_str, "bp")
        || !strcmp(reg_str, "si") || !strcmp(reg_str, "di") || reg_str[length-1] == 'w') {
        // word
        *OpdSize = 2;
        if (reg_str[length-1] == 'w')
          reg_str[length-1] = '\0';
        else
          insert_char_begin(reg_str, 'r');
    } else if (!strcmp(reg_str, "eax") || !strcmp(reg_str, "ebx") || !strcmp(reg_str, "ecx")
        || !strcmp(reg_str, "edx") || !strcmp(reg_str, "esp") || !strcmp(reg_str, "ebp")
        || !strcmp(reg_str, "esi") || !strcmp(reg_str, "edi") || reg_str[length-1] == 'd') {
        // dword
        *OpdSize = 3;
        reg_str[0] = 'r';
        if (reg_str[length-1] == 'd')
          reg_str[length-1] = '\0';
    } else
        *OpdSize = 4;
}

/* set register operand using given string */
void set_x86_opd_reg_str(X86Operand *opd, char *reg_str, uint32_t *OpdSize)
{
    proc_reg_str(opd, reg_str, OpdSize);
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

void set_x86_opd_mem_off_str(X86Operand *opd, char *off_str, bool neg)
{
    if (strstr(off_str, "imm")) { /* offset is a symbol */
        opd->content.mem.offset.type = X86_IMM_TYPE_SYM;
        strcpy(opd->content.mem.offset.content.sym, off_str);
    } else { /* offset is a constant integer */
        opd->content.mem.offset.type = X86_IMM_TYPE_VAL;
        if (neg) /* negative value */
            opd->content.mem.offset.content.val = 0 - strtol(off_str, NULL, 16);
        else
            opd->content.mem.offset.content.val = strtol(off_str, NULL, 16);
    }
}

const char *get_x86_reg_str(X86Register reg)
{
    return x86_reg_str[reg];
}

bool x86_instr_test_branch(X86Instruction *instr)
{
    if (instr->opc == X86_OPC_CALL || instr->opc == X86_OPC_RET
      || instr->opc == X86_OPC_JA || instr->opc == X86_OPC_JAE
      || instr->opc == X86_OPC_JB || instr->opc == X86_OPC_JBE
      || instr->opc == X86_OPC_JL || instr->opc == X86_OPC_JLE
      || instr->opc == X86_OPC_JG || instr->opc == X86_OPC_JGE
      || instr->opc == X86_OPC_JE || instr->opc == X86_OPC_JNE
      || instr->opc == X86_OPC_JS || instr->opc == X86_OPC_JNS
      || instr->opc == X86_OPC_JMP)
      return true;
    return false;
}

static inline bool insn_define_cc(X86Opcode opc)
{
    if ((opc == X86_OPC_AND) || (opc == X86_OPC_OR) ||
       (opc == X86_OPC_XOR) || (opc == X86_OPC_SAR) ||
       (opc == X86_OPC_NEG) || (opc == X86_OPC_INC) ||
       (opc == X86_OPC_DEC) || (opc == X86_OPC_ADD) ||
       (opc == X86_OPC_ADC) || (opc == X86_OPC_SUB) ||
       (opc == X86_OPC_SBB) || (opc == X86_OPC_IMUL) ||
       (opc == X86_OPC_SHL) || (opc == X86_OPC_SHR) ||
       (opc == X86_OPC_SHLD)|| (opc == X86_OPC_SHRD)||
       (opc == X86_OPC_BT)  || (opc == X86_OPC_TEST)||
       (opc == X86_OPC_CMP))
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
            case X86_OPC_CMOVNE:
            case X86_OPC_SETE:
            case X86_OPC_JE:
            case X86_OPC_JNE:
                cur_liveness[X86_REG_ZF] = true;
                break;
            case X86_OPC_CMOVB:
            case X86_OPC_JAE:
            case X86_OPC_JB:
                cur_liveness[X86_REG_CF] = true;
                break;
            case X86_OPC_JS:
            case X86_OPC_JNS:
                cur_liveness[X86_REG_SF] = true;
                break;
            case X86_OPC_CMOVA:
            case X86_OPC_JA:
            case X86_OPC_JBE:
                cur_liveness[X86_REG_CF] = true;
                cur_liveness[X86_REG_ZF] = true;
                break;
            case X86_OPC_CMOVL:
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
        if (insn->opc == X86_OPC_ADC || insn->opc == X86_OPC_SBB || insn->opc == X86_OPC_BT)
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
    uint32_t SrcSize = FEXCore::X86Tables::DecodeFlags::GetSizeSrcFlags(DecodeInst->Flags);
    uint32_t DestSize = FEXCore::X86Tables::DecodeFlags::GetSizeDstFlags(DecodeInst->Flags);

    #ifdef DEBUG_RULE_LOG
      std::string logContent = "[INFO] Inst at 0x" + intToHex(DecodeInst->PC) +
                             ": 0x" + std::to_string(DecodeInst->OP) +
                             " \'" + DecodeInst->TableInfo->Name + "\' with DS: " +
                             std::to_string(DestSize) + ", SS: " + std::to_string(SrcSize) +
                             ", InstSize: " + std::to_string(static_cast<int>(DecodeInst->InstSize)) + "\n";
      writeToLogFile("fex-debug-log", logContent);
    #else
      LogMan::Msg::IFmt("Inst at 0x{:x}: 0x{:04x} '{}' with DS: {}, SS: {}, InstSize: {}",
              DecodeInst->PC, DecodeInst->OP, DecodeInst->TableInfo->Name ?: "UND", DestSize, SrcSize, DecodeInst->InstSize);
    #endif

    bool SingleSrc = false, MutiplyOnce = false;

    if (DecodeInst->Flags & (FEXCore::X86Tables::DecodeFlags::FLAG_SEGMENTS))
      return;

    if (!strcmp(DecodeInst->TableInfo->Name, "NOP"))
      set_x86_instr_opc(instr, X86_OPC_NOP);

    // A normal instruction is the most likely.
    if (DecodeInst->TableInfo->Type == FEXCore::X86Tables::TYPE_INST) [[likely]] {
        if (!strcmp(DecodeInst->TableInfo->Name, "MOV")
          && (((DecodeInst->OP >= 0x88 && DecodeInst->OP <= 0x8B) || DecodeInst->OP == 0x8E)
          || (DecodeInst->OP >= 0xA0 && DecodeInst->OP <= 0xA3)
          || (DecodeInst->OP >= 0xB0 && DecodeInst->OP <= 0xBF))) {
            set_x86_instr_opc(instr, X86_OPC_MOV);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOVZX")
          && (DecodeInst->OP == 0xB6 || DecodeInst->OP == 0xB7)) {
            set_x86_instr_opc(instr, X86_OPC_MOVZX);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSX")
          && (DecodeInst->OP == 0xBE || DecodeInst->OP == 0xBF)) {
            set_x86_instr_opc(instr, X86_OPC_MOVSX);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSXD")
          && (DecodeInst->OP == 0x63)) {
            set_x86_instr_opc(instr, X86_OPC_MOVSXD);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "LEA") && (DecodeInst->OP == 0x8D)) {
            set_x86_instr_opc(instr, X86_OPC_LEA);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "AND")
          && (DecodeInst->OP >= 0x20 && DecodeInst->OP <= 0x25)) {
            set_x86_instr_opc(instr, X86_OPC_AND);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "OR")
          && (DecodeInst->OP >= 0x08 && DecodeInst->OP <= 0x0D)) {
            set_x86_instr_opc(instr, X86_OPC_OR);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "XOR")
          && (DecodeInst->OP >= 0x30 && DecodeInst->OP <= 0x35)) {
            set_x86_instr_opc(instr, X86_OPC_XOR);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "INC")
          && (DecodeInst->OP >= 0x40 && DecodeInst->OP <= 0x47)) {
            set_x86_instr_opc(instr, X86_OPC_INC);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "DEC")
          && (DecodeInst->OP == 0x48 && DecodeInst->OP <= 0x4F)) {
            set_x86_instr_opc(instr, X86_OPC_DEC);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADD")
          && (DecodeInst->OP >= 0x00 && DecodeInst->OP <= 0x05)) {
            set_x86_instr_opc(instr, X86_OPC_ADD);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADC")
          && (DecodeInst->OP >= 0x10 && DecodeInst->OP <= 0x15)) {
            set_x86_instr_opc(instr, X86_OPC_ADC);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SUB")
          && (DecodeInst->OP >= 0x28 && DecodeInst->OP <= 0x2D)) {
            set_x86_instr_opc(instr, X86_OPC_SUB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SBB")
          && (DecodeInst->OP >= 0x18 && DecodeInst->OP <= 0x1D)) {
            set_x86_instr_opc(instr, X86_OPC_SBB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "IMUL")
          && (DecodeInst->OP == 0x69 || DecodeInst->OP == 0x6B || DecodeInst->OP == 0xAF)) {
            set_x86_instr_opc(instr, X86_OPC_IMUL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "BT")
          && (DecodeInst->OP == 0xA3)) {
            set_x86_instr_opc(instr, X86_OPC_BT);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "TEST")
          && (DecodeInst->OP == 0x84 || DecodeInst->OP == 0x85
          || DecodeInst->OP == 0xA8 || DecodeInst->OP == 0xA9)) {
            set_x86_instr_opc(instr, X86_OPC_TEST);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMP")
          && (DecodeInst->OP >= 0x38 && DecodeInst->OP <= 0x3D)) {
            set_x86_instr_opc(instr, X86_OPC_CMP);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVNZ")
          && (DecodeInst->OP == 0x45)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVNE);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVNBE")
          && (DecodeInst->OP == 0x47)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVA);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVB")
          && (DecodeInst->OP == 0x42)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVL")
          && (DecodeInst->OP == 0x4C)) {
            set_x86_instr_opc(instr, X86_OPC_CMOVL);
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
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_11, FEXCore::X86Tables::OpToIndex(0xC6), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_11, FEXCore::X86Tables::OpToIndex(0xC7), 0))) {
            set_x86_instr_opc(instr, X86_OPC_MOV);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "NOT")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 2))) {
            set_x86_instr_opc(instr, X86_OPC_NOT);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "NEG")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 3))) {
            set_x86_instr_opc(instr, X86_OPC_NEG);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "AND")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 4))) {
            set_x86_instr_opc(instr, X86_OPC_AND);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "OR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 1)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 1)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 1)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 1))) {
            set_x86_instr_opc(instr, X86_OPC_OR);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "XOR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 6)
          ||DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 6))) {
            set_x86_instr_opc(instr, X86_OPC_XOR);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "INC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_4, FEXCore::X86Tables::OpToIndex(0xFE), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5, FEXCore::X86Tables::OpToIndex(0xFF), 0))) {
            set_x86_instr_opc(instr, X86_OPC_INC);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "DEC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_4, FEXCore::X86Tables::OpToIndex(0xFE), 1)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5, FEXCore::X86Tables::OpToIndex(0xFF), 1))) {
            set_x86_instr_opc(instr, X86_OPC_DEC);
            SingleSrc = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADD")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 0))) {
            set_x86_instr_opc(instr, X86_OPC_ADD);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "ADC")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 2)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 2)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 2)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 2))) {
            set_x86_instr_opc(instr, X86_OPC_ADC);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SUB")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 5))) {
            set_x86_instr_opc(instr, X86_OPC_SUB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SBB")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 3)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 3)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 3)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 3))) {
            set_x86_instr_opc(instr, X86_OPC_SBB);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "IMUL")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 5))) {
            set_x86_instr_opc(instr, X86_OPC_IMUL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHL")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC0), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC0), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD2), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD2), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 6)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 4)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 6))) {
            set_x86_instr_opc(instr, X86_OPC_SHL);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC0), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD2), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 5)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 5))) {
            set_x86_instr_opc(instr, X86_OPC_SHR);
            if (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 5)
            || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 5))
                MutiplyOnce = true;
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SAR")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC0), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD0), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD2), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xC1), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD1), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2, FEXCore::X86Tables::OpToIndex(0xD3), 7))) {
            set_x86_instr_opc(instr, X86_OPC_SAR);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "TEST")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF6), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF6), 1)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 0)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3, FEXCore::X86Tables::OpToIndex(0xF7), 1))) {
            set_x86_instr_opc(instr, X86_OPC_TEST);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "CMP")
          && (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x80), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x82), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x81), 7)
          || DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1, FEXCore::X86Tables::OpToIndex(0x83), 7))) {
            set_x86_instr_opc(instr, X86_OPC_CMP);
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
            set_x86_instr_opc(instr, X86_OPC_SHLD);
        }
        else if (!strcmp(DecodeInst->TableInfo->Name, "SHRX") && (DecodeInst->OP == OPD(2, 0b11, 0xF7))) {
            set_x86_instr_opc(instr, X86_OPC_SHRD);
        }
#undef OPD
    }

    if (instr->opc == X86_OPC_INVALID) return;

    set_x86_instr_opd_size(instr, SrcSize, DestSize);

    uint8_t num = 0, count = 0;
    FEXCore::X86Tables::DecodedOperand *Opd = &DecodeInst->Dest;

    while (Opd) {
        if (!Opd->IsNone()) {
          if (SingleSrc && num == 1)
            break;

          #ifdef DEBUG_RULE_LOG
            writeToLogFile("fex-debug-log", "[INFO] ====Operand Num: " + std::to_string(num+1) + "\n");
          #else
            LogMan::Msg::IFmt("====Operand Num: {:x}", num+1);
          #endif
          if (Opd->IsGPR()){
              uint8_t GPR = Opd->Data.GPR.GPR;
              bool HighBits = Opd->Data.GPR.HighBits;

              #ifdef DEBUG_RULE_LOG
                writeToLogFile("fex-debug-log", "[INFO]      GPR: 0x" + std::to_string(GPR) + "\n");
              #else
                LogMan::Msg::IFmt("     GPR: 0x{:x}", GPR);
              #endif

              if(GPR <= FEXCore::X86State::REG_R15)
                set_x86_instr_opd_reg(instr, num, GPR, HighBits);
              else
                set_x86_instr_opd_reg(instr, num, 0x10, false);
          }
          else if(Opd->IsRIPRelative()){
              uint32_t Literal = Opd->Data.RIPLiteral.Value.u;

              #ifdef DEBUG_RULE_LOG
                writeToLogFile("fex-debug-log", "[INFO]      RIPLiteral: 0x" + intToHex(Literal) + "\n");
              #else
                LogMan::Msg::IFmt( "     RIPLiteral: 0x{:x}", Literal);
              #endif

              set_x86_instr_opd_imm(instr, num, Literal, true);
          }
          else if(Opd->IsLiteral()){
              uint64_t Literal = Opd->Data.Literal.Value;

              #ifdef DEBUG_RULE_LOG
                writeToLogFile("fex-debug-log", "[INFO]      Literal: 0x" + intToHex(Literal) + "\n");
              #else
                LogMan::Msg::IFmt( "     Literal: 0x{:x}", Literal);
              #endif

              set_x86_instr_opd_imm(instr, num, Literal);
          }
          else if(Opd->IsGPRDirect()) {
              uint8_t GPR = Opd->Data.GPR.GPR;

              #ifdef DEBUG_RULE_LOG
                writeToLogFile("fex-debug-log", "[INFO]      GPRDirect: 0x" + std::to_string(GPR) + "\n");
              #else
                LogMan::Msg::IFmt( "     GPRDirect: 0x{:x}", GPR);
              #endif

              set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
              if(GPR <= FEXCore::X86State::REG_R15)
                set_x86_instr_opd_mem_base(instr, num, GPR);
              else
                set_x86_instr_opd_mem_base(instr, num, 0x10);
          }
          else if(Opd->IsGPRIndirect()){
              uint8_t GPR = Opd->Data.GPRIndirect.GPR;
              int32_t Displacement = Opd->Data.GPRIndirect.Displacement;

              #ifdef DEBUG_RULE_LOG
                writeToLogFile("fex-debug-log", "[INFO]      GPRIndirect - GPR: 0x" + std::to_string(GPR)
                                             + ", Displacement: 0x" + std::to_string(Displacement) + "\n");
              #else
                LogMan::Msg::IFmt( "     GPRIndirect - GPR: 0x{:x}, Displacement: 0x{:x}", GPR, Displacement);
              #endif

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

              #ifdef DEBUG_RULE_LOG
                writeToLogFile("fex-debug-log", "[INFO]      SIB - Base: 0x" + std::to_string(Base)
                                              + ", Offset: 0x" + std::to_string(Offset)
                                              + ", Index: 0x" + std::to_string(Index)
                                              + ", Scale: 0x" + std::to_string(Scale) + "\n");
              #else
                LogMan::Msg::IFmt( "     SIB - Base: 0x{:x}, Offset: 0x{:x}, Index: 0x{:x}, Scale: 0x{:x}",
                                Base, Offset, Index, Scale);
              #endif

              set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
              if (Base <= FEXCore::X86State::REG_R15) {
                set_x86_instr_opd_mem_base(instr, num, Base);
                set_x86_instr_opd_mem_off(instr, num, Offset);
                if (Index <= FEXCore::X86State::REG_R15) {
                  set_x86_instr_opd_mem_index(instr, num, Index);
                  set_x86_instr_opd_mem_scale(instr, num, Scale);
                } else
                  set_x86_instr_opd_mem_index(instr, num, 0x10);
              } else
                set_x86_instr_opd_mem_base(instr, num, 0x10);
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

    // cmp, add, or, mov, test, sub
    if (num == 3) {
      instr->opd[1] = instr->opd[2];
      num--;
    }

    if ((instr->opc == X86_OPC_JMP || instr->opc == X86_OPC_CALL || instr->opc == X86_OPC_PUSH) && (num == 2)) {
      instr->opd[0] = instr->opd[1];
      num--;
    }

    if ((instr->opc == X86_OPC_SHR) && MutiplyOnce) {
      set_x86_instr_opd_imm(instr, 1, 1);
    }

    set_x86_instr_opd_num(instr, num);
    set_x86_instr_size(instr, DecodeInst->InstSize);

    #ifdef DEBUG_RULE_LOG
      output_x86_instr(instr);
    #endif
    // print_x86_instr(instr);
}
