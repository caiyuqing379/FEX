/**
 * @file x86-instr.cpp
 * @brief 实现x86指令的解码、处理和转换功能
 *
 * 本文件包含了x86指令集的操作码、寄存器和操作数的定义,
 * 以及将FEXCore解码的指令转换为自定义X86Instruction格式的函数。
 * 主要用于动态二进制翻译过程中的x86指令处理。
 */

#include "Interface/Core/PatternMatcher.h"
#include "Interface/Core/X86Tables/X86Tables.h"

#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "x86-instr.h"

using namespace FEXCore::Pattern;

#define MAX_INSTR_NUM 1000000

/**
 * @brief x86操作码的字符串表示
 *
 * 这个数组将x86操作码枚举值映射到其对应的字符串表示。
 * 用于调试输出和指令打印。
 */
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
    [X86_OPC_JA] = "ja",
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

    // Load/Store
    [X86_OPC_MOVUPS] = "movups",
    [X86_OPC_MOVUPD] = "movupd",
    [X86_OPC_MOVSS] = "movss",
    [X86_OPC_MOVSD] = "movsd",
    [X86_OPC_MOVLPS] = "movlps",
    [X86_OPC_MOVLPD] = "movlpd",
    [X86_OPC_MOVHPS] = "movhps",
    [X86_OPC_MOVHPD] = "movhpd",
    [X86_OPC_MOVAPS] = "movaps",
    [X86_OPC_MOVAPD] = "movapd",
    [X86_OPC_MOVD] = "movd",
    [X86_OPC_MOVQ] = "movq",
    [X86_OPC_MOVDQA] = "movdqa",
    [X86_OPC_MOVDQU] = "movdqu",
    [X86_OPC_PMOVMSKB] = "pmovmskb",
    [X86_OPC_PALIGNR] = "palignr",

    // Logical
    [X86_OPC_ANDPS] = "andps",
    [X86_OPC_ANDPD] = "andpd",
    [X86_OPC_ORPS] = "orps",
    [X86_OPC_ORPD] = "orpd",
    [X86_OPC_XORPS] = "xorps",
    [X86_OPC_XORPD] = "xorpd",
    [X86_OPC_PAND] = "pand",
    [X86_OPC_PANDN] = "pandn",
    [X86_OPC_POR] = "por",
    [X86_OPC_PXOR] = "pxor",

    [X86_OPC_PACKUSWB] = "packuswb",
    [X86_OPC_PACKSSWB] = "packsswb",
    [X86_OPC_PACKSSDW] = "packssdw",
    [X86_OPC_PUNPCKLBW] = "punpcklbw",
    [X86_OPC_PUNPCKLWD] = "punpcklwd",
    [X86_OPC_PUNPCKLDQ] = "punpckldq",
    [X86_OPC_PUNPCKHBW] = "punpckhbw",
    [X86_OPC_PUNPCKHWD] = "punpckhwd",
    [X86_OPC_PUNPCKHDQ] = "punpckhdq",
    [X86_OPC_PUNPCKLQDQ] = "punpcklqdq",
    [X86_OPC_PUNPCKHQDQ] = "punpckhqdq",
    // Shuffle
    [X86_OPC_SHUFPD] = "shufpd",
    [X86_OPC_PSHUFD] = "pshufd",
    [X86_OPC_PSHUFLW] = "pshuflw",
    [X86_OPC_PSHUFHW] = "pshufhw",

    // Comparison
    [X86_OPC_PCMPGTB] = "pcmpgtb",
    [X86_OPC_PCMPGTW] = "pcmpgtw",
    [X86_OPC_PCMPGTD] = "pcmpgtd",
    [X86_OPC_PCMPEQB] = "pcmpeqb",
    [X86_OPC_PCMPEQW] = "pcmpeqw",
    [X86_OPC_PCMPEQD] = "pcmpeqd",

    // Algorithm
    [X86_OPC_ADDPS] = "addps",
    [X86_OPC_ADDPD] = "addpd",
    [X86_OPC_ADDSS] = "addss",
    [X86_OPC_ADDSD] = "addsd",
    [X86_OPC_SUBPS] = "subps",
    [X86_OPC_SUBPD] = "subpd",
    [X86_OPC_SUBSS] = "subss",
    [X86_OPC_SUBSD] = "subsd",
    [X86_OPC_PSUBB] = "psubb",
    [X86_OPC_PADDD] = "paddd",

    [X86_OPC_SET_LABEL] = "set label",
};

/**
 * @brief x86寄存器枚举值表
 *
 * 这个数组定义了x86寄存器的枚举值,包括通用寄存器和XMM寄存器。
 */
static const X86Register x86_reg_table[] = {
    X86_REG_RAX,   X86_REG_RCX,   X86_REG_RDX,    X86_REG_RBX,   X86_REG_RSP,
    X86_REG_RBP,   X86_REG_RSI,   X86_REG_RDI,    X86_REG_R8,    X86_REG_R9,
    X86_REG_R10,   X86_REG_R11,   X86_REG_R12,    X86_REG_R13,   X86_REG_R14,
    X86_REG_R15,   X86_REG_XMM0,  X86_REG_XMM1,   X86_REG_XMM2,  X86_REG_XMM3,
    X86_REG_XMM4,  X86_REG_XMM5,  X86_REG_XMM6,   X86_REG_XMM7,  X86_REG_XMM8,
    X86_REG_XMM9,  X86_REG_XMM10, X86_REG_XMM11,  X86_REG_XMM12, X86_REG_XMM13,
    X86_REG_XMM14, X86_REG_XMM15, X86_REG_INVALID};

/**
 * @brief x86寄存器的字符串表示
 *
 * 这个数组将x86寄存器枚举值映射到其对应的字符串表示。
 * 用于调试输出和指令打印。
 */
static const char *x86_reg_str[] = {
    "none",  "rax",   "rcx",   "rdx",   "rbx",   "rsp",   "rbp",   "rsi",
    "rdi",   "r8",    "r9",    "r10",   "r11",   "r12",   "r13",   "r14",
    "r15",   "xmm0",  "xmm1",  "xmm2",  "xmm3",  "xmm4",  "xmm5",  "xmm6",
    "xmm7",  "xmm8",  "xmm9",  "xmm10", "xmm11", "xmm12", "xmm13", "xmm14",
    "xmm15",

    "of",    "sf",    "cf",    "zf",

    "reg0",  "reg1",  "reg2",  "reg3",  "reg4",  "reg5",  "reg6",  "reg7",
    "reg8",  "reg9",  "reg10", "reg11", "reg12", "reg13", "reg14", "reg15",
    "reg16", "reg17", "reg18", "reg19", "reg20", "reg21", "reg22", "reg23",
    "reg24", "reg25", "reg26", "reg27", "reg28", "reg29", "reg30", "reg31",

    "temp0", "temp1", "temp2", "temp3"};

/**
 * @brief 获取X86操作码的枚举值
 *
 * @param opc_str 操作码的字符串表示
 * @return X86Opcode 对应的操作码枚举值
 */
static X86Opcode get_x86_opcode(char *opc_str) {
  for (int i = X86_OPC_INVALID; i < X86_OPC_END; i++) {
    if (!strcmp(opc_str, x86_opc_str[i]))
      return static_cast<X86Opcode>(i);
  }
  LogMan::Msg::IFmt("==== [X86] unsupported opcode: {}", opc_str);
  exit(-1);
  return X86_OPC_INVALID;
}

/**
 * @brief 获取X86寄存器的枚举值
 *
 * @param str 寄存器的字符串表示
 * @return X86Register 对应的寄存器枚举值
 */
static X86Register get_x86_register(char *str) {
  int reg;
  for (reg = X86_REG_INVALID; reg < X86_REG_END; reg++) {
    if (!strcmp(str, x86_reg_str[reg]))
      return static_cast<X86Register>(reg);
  }
  return X86_REG_INVALID;
}

/**
 * @brief 获取X86操作码的字符串表示
 *
 * @param opc 操作码枚举值
 * @return const char* 对应的操作码字符串
 */
const char *get_x86_opc_str(X86Opcode opc) { return x86_opc_str[opc]; }

/**
 * @brief 打印X86指令的详细信息
 *
 * 这个函数将X86指令的各个字段（如操作码、操作数等）打印出来，
 * 主要用于调试和指令分析。
 *
 * @param instr 指向X86Instruction结构的指针
 */
void print_x86_instr(X86Instruction *instr) {
  LogMan::Msg::IFmt("0x{:x}: opcode: {} destsize:{} srcsize:{}", instr->pc,
                    x86_opc_str[instr->opc], instr->DestSize, instr->SrcSize);

  for (int i = 0; i < instr->opd_num; i++) {
    X86Operand *opd = &instr->opd[i];
    if (opd->type == X86_OPD_TYPE_IMM) {
      X86ImmOperand *imm = &opd->content.imm;
      if (imm->type == X86_IMM_TYPE_VAL)
        LogMan::Msg::IFmt("     imm: 0x{:x}", imm->content.val);
      else if (imm->type == X86_IMM_TYPE_SYM)
        LogMan::Msg::IFmt("     imm: {}", imm->content.sym);
    } else if (opd->type == X86_OPD_TYPE_REG) {
      LogMan::Msg::IFmt("     reg: {}", x86_reg_str[opd->content.reg.num]);
    } else if (opd->type == X86_OPD_TYPE_MEM) {
      fprintf(stdout, "[INFO]      mem: base(%s)",
              x86_reg_str[opd->content.mem.base]);
      if (opd->content.mem.index != X86_REG_INVALID)
        fprintf(stdout, ", index(%s)", x86_reg_str[opd->content.mem.index]);
      if (opd->content.mem.scale.type == X86_IMM_TYPE_SYM)
        fprintf(stdout, ", scale(%s)", opd->content.mem.scale.content.sym);
      else if (opd->content.mem.scale.type == X86_IMM_TYPE_VAL)
        fprintf(stdout, ", scale(0x%lx)", opd->content.mem.scale.content.val);
      if (opd->content.mem.offset.type == X86_IMM_TYPE_SYM)
        fprintf(stdout, ", offset(%s)", opd->content.mem.offset.content.sym);
      else if (opd->content.mem.offset.type == X86_IMM_TYPE_VAL)
        fprintf(stdout, ", offset(0x%lx)", opd->content.mem.offset.content.val);
      LogMan::Msg::IFmt("");
    }
  }
}

/**
 * @brief 打印X86指令序列
 *
 * 遍历并打印整个X86指令序列的详细信息。
 *
 * @param instr_seq 指向X86Instruction序列的指针
 */
void print_x86_instr_seq(X86Instruction *instr_seq) {
  X86Instruction *head = instr_seq;

  while (head) {
    print_x86_instr(head);
    head = head->next;
  }
}

/**
 * @brief 设置X86指令的操作码
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opc 要设置的操作码
 */
void set_x86_instr_opc(X86Instruction *instr, X86Opcode opc) {
  instr->opc = opc;
}

/**
 * @brief 设置X86指令的操作码（使用字符串）
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opc_str 操作码的字符串表示
 */
void set_x86_instr_opc_str(X86Instruction *instr, char *opc_str) {
  instr->opc = get_x86_opcode(opc_str);
}

/**
 * @brief 设置X86指令的操作数数量
 *
 * @param instr 指向X86Instruction结构的指针
 * @param num 操作数的数量
 */
void set_x86_instr_opd_num(X86Instruction *instr, uint8_t num) {
  instr->opd_num = num;
}

/**
 * @brief 设置X86指令的操作数大小
 *
 * @param instr 指向X86Instruction结构的指针
 * @param SrcSize 源操作数大小
 * @param DestSize 目标操作数大小
 */
void set_x86_instr_opd_size(X86Instruction *instr, uint32_t SrcSize,
                            uint32_t DestSize) {
  instr->SrcSize = SrcSize;
  instr->DestSize = DestSize;
}

/**
 * @brief 设置X86指令的大小
 *
 * @param instr 指向X86Instruction结构的指针
 * @param size 指令的大小
 */
void set_x86_instr_size(X86Instruction *instr, size_t size) {
  instr->InstSize = size;
}

/**
 * @brief 设置X86指令特定操作数的类型
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opd_index 操作数的索引
 * @param type 操作数的类型
 */
void set_x86_instr_opd_type(X86Instruction *instr, int opd_index,
                            X86OperandType type) {
  set_x86_opd_type(&instr->opd[opd_index], type);
}

/**
 * @brief 设置X86指令特定操作数为寄存器
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opd_index 操作数的索引
 * @param regno 寄存器编号
 * @param HighBits 是否使用高位
 */
void set_x86_instr_opd_reg(X86Instruction *instr, int opd_index, int regno,
                           bool HighBits) {
  X86Operand *opd = &instr->opd[opd_index];

  opd->type = X86_OPD_TYPE_REG;
  opd->content.reg.num = x86_reg_table[regno];
  opd->content.reg.HighBits = HighBits;
}

/**
 * @brief 设置X86指令特定操作数为立即数
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opd_index 操作数的索引
 * @param val 立即数的值
 * @param isRipLiteral 是否为RIP相对寻址
 */
void set_x86_instr_opd_imm(X86Instruction *instr, int opd_index, uint64_t val,
                           bool isRipLiteral) {
  X86Operand *opd = &instr->opd[opd_index];

  opd->type = X86_OPD_TYPE_IMM;
  opd->content.imm.type = X86_IMM_TYPE_VAL;
  opd->content.imm.content.val = val;
  opd->content.imm.isRipLiteral = isRipLiteral;
}

/**
 * @brief 设置X86指令特定内存操作数的基址寄存器
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opd_index 操作数的索引
 * @param regno 基址寄存器编号
 */
void set_x86_instr_opd_mem_base(X86Instruction *instr, int opd_index,
                                int regno) {
  X86MemOperand *mopd = &(instr->opd[opd_index].content.mem);

  mopd->base = x86_reg_table[regno];
}

/**
 * @brief 设置X86指令特定内存操作数的偏移量
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opd_index 操作数的索引
 * @param offset 偏移量
 */
void set_x86_instr_opd_mem_off(X86Instruction *instr, int opd_index,
                               int32_t offset) {
  set_x86_opd_mem_off(&(instr->opd[opd_index]), offset);
}

/**
 * @brief 设置X86指令特定内存操作数的比例因子
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opd_index 操作数的索引
 * @param scale 比例因子
 */
void set_x86_instr_opd_mem_scale(X86Instruction *instr, int opd_index,
                                 uint8_t scale) {
  X86MemOperand *mopd = &(instr->opd[opd_index].content.mem);

  mopd->scale.type = X86_IMM_TYPE_VAL;
  mopd->scale.content.val = scale;
}

/**
 * @brief 设置X86指令特定内存操作数的索引寄存器
 *
 * @param instr 指向X86Instruction结构的指针
 * @param opd_index 操作数的索引
 * @param regno 索引寄存器编号
 */
void set_x86_instr_opd_mem_index(X86Instruction *instr, int opd_index,
                                 int regno) {
  X86MemOperand *mopd = &(instr->opd[opd_index].content.mem);

  mopd->index = x86_reg_table[regno];
}

/**
 * @brief 设置X86操作数的类型
 *
 * @param opd 指向X86Operand结构的指针
 * @param type 操作数类型
 */
void set_x86_opd_type(X86Operand *opd, X86OperandType type) {
  switch (type) {
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

/**
 * @brief 设置X86立即数操作数的值（使用字符串）
 *
 * @param opd 指向X86Operand结构的指针
 * @param imm_str 立即数的字符串表示
 * @param isRipLiteral 是否为RIP相对寻址
 * @param neg 是否为负数
 */
void set_x86_opd_imm_val_str(X86Operand *opd, char *imm_str, bool isRipLiteral,
                             bool neg) {
  X86ImmOperand *iopd = &opd->content.imm;

  iopd->type = X86_IMM_TYPE_VAL;
  if (neg) /* negative value */
    iopd->content.val = 0 - strtol(imm_str, NULL, 16);
  else
    iopd->content.val = strtol(imm_str, NULL, 16);
  iopd->isRipLiteral = isRipLiteral;
}

/**
 * @brief 设置X86立即数操作数的符号（使用字符串）
 *
 * @param opd 指向X86Operand结构的指针
 * @param imm_str 立即数的字符串表示
 * @param isRipLiteral 是否为RIP相对寻址
 */
void set_x86_opd_imm_sym_str(X86Operand *opd, char *imm_str,
                             bool isRipLiteral) {
  X86ImmOperand *iopd = &opd->content.imm;

  iopd->type = X86_IMM_TYPE_SYM;
  strcpy(iopd->content.sym, imm_str);
  iopd->isRipLiteral = isRipLiteral;
}

/**
 * @brief 在字符串开头插入字符
 *
 * @param reg_str 原字符串
 * @param new_char 要插入的字符
 */
void insert_char_begin(char *reg_str, char new_char) {
  size_t length = strlen(reg_str);
  memmove(reg_str + 1, reg_str, length + 1);
  reg_str[0] = new_char;
}

/**
 * @brief 处理寄存器字符串，获取正确的寄存器名称和大小
 *
 * @param opd 指向X86Operand结构的指针
 * @param reg_str 寄存器的字符串表示
 * @param OpSize 操作数大小
 */
void proc_reg_str(X86Operand *opd, char *reg_str, uint32_t *OpSize) {
  int length = strlen(reg_str);
  opd->content.reg.HighBits = false;

  if (!strcmp(reg_str, "ah") || !strcmp(reg_str, "bh") ||
      !strcmp(reg_str, "ch") || !strcmp(reg_str, "dh")) {
    // byte
    *OpSize = 1;
    opd->content.reg.HighBits = true;
    insert_char_begin(reg_str, 'r');
    reg_str[length] = 'x';
  } else if (!strcmp(reg_str, "al") || !strcmp(reg_str, "bl") ||
             !strcmp(reg_str, "cl") || !strcmp(reg_str, "dl") ||
             !strcmp(reg_str, "sil") || !strcmp(reg_str, "dil") ||
             !strcmp(reg_str, "bpl") || !strcmp(reg_str, "spl") ||
             reg_str[length - 1] == 'b') {
    // byte
    *OpSize = 1;
    if (reg_str[length - 1] == 'b') {
      reg_str[length - 1] = '\0';
    } else {
      insert_char_begin(reg_str, 'r');
      if (length == 2)
        reg_str[length] = 'x';
      else
        reg_str[length] = '\0';
    }
  } else if (!strcmp(reg_str, "ax") || !strcmp(reg_str, "bx") ||
             !strcmp(reg_str, "cx") || !strcmp(reg_str, "dx") ||
             !strcmp(reg_str, "sp") || !strcmp(reg_str, "bp") ||
             !strcmp(reg_str, "si") || !strcmp(reg_str, "di") ||
             reg_str[length - 1] == 'w') {
    // word
    *OpSize = 2;
    if (reg_str[length - 1] == 'w')
      reg_str[length - 1] = '\0';
    else
      insert_char_begin(reg_str, 'r');
  } else if (!strcmp(reg_str, "eax") || !strcmp(reg_str, "ebx") ||
             !strcmp(reg_str, "ecx") || !strcmp(reg_str, "edx") ||
             !strcmp(reg_str, "esp") || !strcmp(reg_str, "ebp") ||
             !strcmp(reg_str, "esi") || !strcmp(reg_str, "edi") ||
             reg_str[length - 1] == 'd') {
    // dword
    *OpSize = 3;
    reg_str[0] = 'r';
    if (reg_str[length - 1] == 'd')
      reg_str[length - 1] = '\0';
  }
}

/**
 * @brief 设置X86寄存器操作数（使用字符串）
 *
 * @param opd 指向X86Operand结构的指针
 * @param reg_str 寄存器的字符串表示
 * @param OpSize 操作数大小的指针
 */
void set_x86_opd_reg_str(X86Operand *opd, char *reg_str, uint32_t *OpSize) {
  proc_reg_str(opd, reg_str, OpSize);
  opd->content.reg.num = get_x86_register(reg_str);
}

/**
 * @brief 设置X86内存操作数的基址寄存器（使用字符串）
 *
 * @param opd 指向X86Operand结构的指针
 * @param reg_str 寄存器的字符串表示
 */
void set_x86_opd_mem_base_str(X86Operand *opd, char *reg_str) {
  opd->content.mem.base = get_x86_register(reg_str);
}

/**
 * @brief 设置X86内存操作数的索引寄存器（使用字符串）
 *
 * @param opd 指向X86Operand结构的指针
 * @param reg_str 寄存器的字符串表示
 */
void set_x86_opd_mem_index_str(X86Operand *opd, char *reg_str) {
  opd->content.mem.index = get_x86_register(reg_str);
}

/**
 * @brief 设置X86内存操作数的比例因子（使用字符串）
 *
 * @param opd 指向X86Operand结构的指针
 * @param scale_str 比例因子的字符串表示
 */
void set_x86_opd_mem_scale_str(X86Operand *opd, char *scale_str) {
  if (strstr(scale_str, "imm")) {
    opd->content.mem.scale.type = X86_IMM_TYPE_SYM;
    strcpy(opd->content.mem.scale.content.sym, scale_str);
  } else {
    opd->content.mem.scale.type = X86_IMM_TYPE_VAL;
    opd->content.mem.scale.content.val = atoi(scale_str);
  }
}

/**
 * @brief 设置X86内存操作数的偏移量
 *
 * @param opd 指向X86Operand结构的指针
 * @param val 偏移量值
 */
void set_x86_opd_mem_off(X86Operand *opd, int32_t val) {
  opd->content.mem.offset.type = X86_IMM_TYPE_VAL;
  opd->content.mem.offset.content.val = val;
}

/**
 * @brief 设置X86内存操作数的偏移量（使用字符串）
 *
 * @param opd 指向X86Operand结构的指针
 * @param off_str 偏移量的字符串表示
 * @param neg 是否为负偏移
 */
void set_x86_opd_mem_off_str(X86Operand *opd, char *off_str, bool neg) {
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

/**
 * @brief 获取X86寄存器的字符串表示
 *
 * @param reg 寄存器枚举值
 * @return const char* 寄存器的字符串表示
 */
const char *get_x86_reg_str(X86Register reg) { return x86_reg_str[reg]; }

/**
 * @brief 测试X86指令是否为改变指令流指令
 *
 * @param instr 指向X86Instruction结构的指针
 * @return bool 如果是改变指令流指令则返回true，否则返回false
 */
bool x86_instr_test_branch(X86Instruction *instr) {
  if (instr->opc == X86_OPC_CALL || instr->opc == X86_OPC_RET ||
      instr->opc == X86_OPC_JA || instr->opc == X86_OPC_JAE ||
      instr->opc == X86_OPC_JB || instr->opc == X86_OPC_JBE ||
      instr->opc == X86_OPC_JL || instr->opc == X86_OPC_JLE ||
      instr->opc == X86_OPC_JG || instr->opc == X86_OPC_JGE ||
      instr->opc == X86_OPC_JE || instr->opc == X86_OPC_JNE ||
      instr->opc == X86_OPC_JS || instr->opc == X86_OPC_JNS ||
      instr->opc == X86_OPC_JMP)
    return true;
  return false;
}

/**
 * @brief 判断指令是否定义条件码
 *
 * @param opc 指令操作码
 * @return bool 如果指令定义条件码则返回true，否则返回false
 */
static inline bool insn_define_cc(X86Opcode opc) {
  if ((opc == X86_OPC_AND) || (opc == X86_OPC_OR) || (opc == X86_OPC_XOR) ||
      (opc == X86_OPC_SAR) || (opc == X86_OPC_NEG) || (opc == X86_OPC_INC) ||
      (opc == X86_OPC_DEC) || (opc == X86_OPC_ADD) || (opc == X86_OPC_ADC) ||
      (opc == X86_OPC_SUB) || (opc == X86_OPC_SBB) || (opc == X86_OPC_IMUL) ||
      (opc == X86_OPC_SHL) || (opc == X86_OPC_SHR) || (opc == X86_OPC_SHLD) ||
      (opc == X86_OPC_SHRD) || (opc == X86_OPC_BT) || (opc == X86_OPC_TEST) ||
      (opc == X86_OPC_CMP))
    return true;
  return false;
}

bool is_update_cc(X86Instruction *instr, int icount) {
  X86Instruction *head = instr;
  int i;

  for (i = 0; i < icount; i++) {
    if (insn_define_cc(head->opc))
      return true;
    head = head->next;
  }

  return false;
}

/**
 * @brief 决定寄存器的活跃性
 *
 * 这个函数分析指令序列，确定每条指令中各个寄存器的活跃性。
 *
 * @param succ_define_cc 后继指令是否定义条件码
 * @param insn_seq 指向X86Instruction序列的指针
 */
void decide_reg_liveness(int succ_define_cc, X86Instruction *insn_seq) {
  bool cur_liveness[X86_REG_NUM];
  X86Instruction *tail;
  X86Instruction *insn;
  bool cc_revised;
  int i;

  // 初始化所有寄存器为活跃状态
  for (i = 0; i < X86_REG_NUM; i++)
    cur_liveness[i] = true;

  // 如果后继指令定义条件码，将条件码寄存器设置为非活跃
  if (succ_define_cc == 3) {
    for (i = X86_REG_OF; i < X86_REG_NUM; i++)
      cur_liveness[i] = false;
  }

  // 找到指令序列的尾部
  tail = insn_seq;
  while (tail && tail->next)
    tail = tail->next;

  // 从尾部开始，向前决定每条指令的寄存器活跃性
  insn = tail;
  while (insn) {
    // 复制当前的活跃性状态到指令
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
    if (insn->opc == X86_OPC_ADC || insn->opc == X86_OPC_SBB ||
        insn->opc == X86_OPC_BT)
      cur_liveness[X86_REG_CF] = true;

    /* 3. Update the liveness if this instruciton defines condition code */
    if (insn_define_cc(insn->opc)) {
      for (i = X86_REG_OF; i < X86_REG_NUM; i++)
        cur_liveness[i] = false;
    }

    insn = insn->prev;
  }

  /* Decide if we need to save condition codes for instructions that define
   * condtion codes */
}

/**
 * @brief 将FEXCore解码的指令转换为自定义X86Instruction格式
 *
 * 这个函数是整个文件的核心，负责将FEXCore解码的指令转换为我们自定义的X86Instruction格式。
 * 它处理各种不同类型的指令，设置操作码、操作数等信息。
 *
 * @param DecodeInst 指向FEXCore解码的指令的指针
 * @param instr 指向要填充的X86Instruction结构的指针
 * @param pid 进程ID，用于调试日志
 */
void DecodeInstToX86Inst(FEXCore::X86Tables::DecodedInst *DecodeInst,
                        X86Instruction *instr,
                        uint64_t pid) {
  uint32_t SrcSize =
      FEXCore::X86Tables::DecodeFlags::GetSizeSrcFlags(DecodeInst->Flags);
  uint32_t DestSize =
      FEXCore::X86Tables::DecodeFlags::GetSizeDstFlags(DecodeInst->Flags);

#ifdef DEBUG_RULE_LOG
  std::string logContent =
      "[INFO] Inst at 0x" + intToHex(DecodeInst->PC) + ": 0x" +
      std::to_string(DecodeInst->OP) + " \'" + DecodeInst->TableInfo->Name +
      "\' with DS: " + std::to_string(DestSize) +
      ", SS: " + std::to_string(SrcSize) +
      ", InstSize: " + std::to_string(static_cast<int>(DecodeInst->InstSize)) +
      "\n";
  writeToLogFile(std::to_string(pid) + "fex-debug.log", logContent);
#else
  LogMan::Msg::IFmt(
      "Inst at 0x{:x}: 0x{:04x} '{}' with DS: {}, SS: {}, InstSize: {}",
      DecodeInst->PC, DecodeInst->OP, DecodeInst->TableInfo->Name ?: "UND",
      DestSize, SrcSize, DecodeInst->InstSize);
#endif

  if (DecodeInst->Flags & (FEXCore::X86Tables::DecodeFlags::FLAG_SEGMENTS |
                           FEXCore::X86Tables::DecodeFlags::FLAG_LOCK))
    return;

  bool SingleSrc = false, ThreeSrc = false, MutiplyOnce = false;

  if (!strcmp(DecodeInst->TableInfo->Name, "NOP"))
    set_x86_instr_opc(instr, X86_OPC_NOP);

  // A normal instruction is the most likely.
  if (DecodeInst->TableInfo->Type == FEXCore::X86Tables::TYPE_INST) [[likely]] {
    // 这里是一个大型的if-else结构，用于识别和设置不同类型的指令
    // 每个if语句检查指令的名称和操作码，然后设置相应的X86_OPC_*

    // 示例：处理MOV指令
    if (!strcmp(DecodeInst->TableInfo->Name, "MOV") &&
        (((DecodeInst->OP >= 0x88 &&
           DecodeInst->OP <= 0x8B)) // 0x8E Move r/m16 to segment register.
         || (DecodeInst->OP >= 0xA0 && DecodeInst->OP <= 0xA3) ||
         (DecodeInst->OP >= 0xB0 && DecodeInst->OP <= 0xBF))) {
      set_x86_instr_opc(instr, X86_OPC_MOV);
    } // ... 大量类似的if-else结构，用于处理其他指令类型 ...
    else if (!strcmp(DecodeInst->TableInfo->Name, "MOVZX") &&
               (DecodeInst->OP == 0xB6 || DecodeInst->OP == 0xB7)) {
      set_x86_instr_opc(instr, X86_OPC_MOVZX);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSX") &&
               (DecodeInst->OP == 0xBE || DecodeInst->OP == 0xBF)) {
      set_x86_instr_opc(instr, X86_OPC_MOVSX);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSXD") &&
               (DecodeInst->OP == 0x63)) {
      set_x86_instr_opc(instr, X86_OPC_MOVSXD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "LEA") &&
               (DecodeInst->OP == 0x8D)) {
      set_x86_instr_opc(instr, X86_OPC_LEA);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "AND") &&
               (DecodeInst->OP >= 0x20 && DecodeInst->OP <= 0x25)) {
      set_x86_instr_opc(instr, X86_OPC_AND);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "OR") &&
               (DecodeInst->OP >= 0x08 && DecodeInst->OP <= 0x0D)) {
      set_x86_instr_opc(instr, X86_OPC_OR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "XOR") &&
               (DecodeInst->OP >= 0x30 && DecodeInst->OP <= 0x35)) {
      set_x86_instr_opc(instr, X86_OPC_XOR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "INC") &&
               (DecodeInst->OP >= 0x40 && DecodeInst->OP <= 0x47)) {
      set_x86_instr_opc(instr, X86_OPC_INC);
      SingleSrc = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "DEC") &&
               (DecodeInst->OP == 0x48 && DecodeInst->OP <= 0x4F)) {
      set_x86_instr_opc(instr, X86_OPC_DEC);
      SingleSrc = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADD") &&
               (DecodeInst->OP >= 0x00 && DecodeInst->OP <= 0x05)) {
      set_x86_instr_opc(instr, X86_OPC_ADD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADC") &&
               (DecodeInst->OP >= 0x10 && DecodeInst->OP <= 0x15)) {
      set_x86_instr_opc(instr, X86_OPC_ADC);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUB") &&
               (DecodeInst->OP >= 0x28 && DecodeInst->OP <= 0x2D)) {
      set_x86_instr_opc(instr, X86_OPC_SUB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SBB") &&
               (DecodeInst->OP >= 0x18 && DecodeInst->OP <= 0x1D)) {
      set_x86_instr_opc(instr, X86_OPC_SBB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "IMUL") &&
               (DecodeInst->OP == 0x69 || DecodeInst->OP == 0x6B ||
                DecodeInst->OP == 0xAF)) {
      set_x86_instr_opc(instr, X86_OPC_IMUL);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "BT") &&
               (DecodeInst->OP == 0xA3)) {
      set_x86_instr_opc(instr, X86_OPC_BT);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "TEST") &&
               (DecodeInst->OP == 0x84 || DecodeInst->OP == 0x85 ||
                DecodeInst->OP == 0xA8 || DecodeInst->OP == 0xA9)) {
      set_x86_instr_opc(instr, X86_OPC_TEST);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CMP") &&
               (DecodeInst->OP >= 0x38 && DecodeInst->OP <= 0x3D)) {
      set_x86_instr_opc(instr, X86_OPC_CMP);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVNZ") &&
               (DecodeInst->OP == 0x45)) {
      set_x86_instr_opc(instr, X86_OPC_CMOVNE);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVNBE") &&
               (DecodeInst->OP == 0x47)) {
      set_x86_instr_opc(instr, X86_OPC_CMOVA);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVB") &&
               (DecodeInst->OP == 0x42)) {
      set_x86_instr_opc(instr, X86_OPC_CMOVB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CMOVL") &&
               (DecodeInst->OP == 0x4C)) {
      set_x86_instr_opc(instr, X86_OPC_CMOVL);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SETZ") &&
               (DecodeInst->OP == 0x94)) {
      set_x86_instr_opc(instr, X86_OPC_SETE);
      SingleSrc = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CQO") &&
               (DecodeInst->OP == 0x99)) {
      set_x86_instr_opc(instr, X86_OPC_CWT);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JMP") &&
               (DecodeInst->OP == 0xE9 || DecodeInst->OP == 0xEB)) {
      set_x86_instr_opc(instr, X86_OPC_JMP);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JNBE") &&
               (DecodeInst->OP == 0x77 || DecodeInst->OP == 0x87)) {
      set_x86_instr_opc(instr, X86_OPC_JA);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JNB") &&
               (DecodeInst->OP == 0x73 || DecodeInst->OP == 0x83)) {
      set_x86_instr_opc(instr, X86_OPC_JAE);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JB") &&
               (DecodeInst->OP == 0x72 || DecodeInst->OP == 0x82)) {
      set_x86_instr_opc(instr, X86_OPC_JB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JBE") &&
               (DecodeInst->OP == 0x76 || DecodeInst->OP == 0x86)) {
      set_x86_instr_opc(instr, X86_OPC_JBE);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JL") &&
               (DecodeInst->OP == 0x7C || DecodeInst->OP == 0x8C)) {
      set_x86_instr_opc(instr, X86_OPC_JL);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JLE") &&
               (DecodeInst->OP == 0x7E || DecodeInst->OP == 0x8E)) {
      set_x86_instr_opc(instr, X86_OPC_JLE);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JNLE") &&
               (DecodeInst->OP == 0x7F || DecodeInst->OP == 0x8F)) {
      set_x86_instr_opc(instr, X86_OPC_JG);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JNL") &&
               (DecodeInst->OP == 0x7D || DecodeInst->OP == 0x8D)) {
      set_x86_instr_opc(instr, X86_OPC_JGE);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JZ") &&
               (DecodeInst->OP == 0x74 || DecodeInst->OP == 0x84)) {
      set_x86_instr_opc(instr, X86_OPC_JE);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JNZ") &&
               (DecodeInst->OP == 0x75 || DecodeInst->OP == 0x85)) {
      set_x86_instr_opc(instr, X86_OPC_JNE);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JS") &&
               (DecodeInst->OP == 0x78 || DecodeInst->OP == 0x88)) {
      set_x86_instr_opc(instr, X86_OPC_JS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JNS") &&
               (DecodeInst->OP == 0x79 || DecodeInst->OP == 0x89)) {
      set_x86_instr_opc(instr, X86_OPC_JNS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUSH") &&
               ((DecodeInst->OP >= 0x50 && DecodeInst->OP <= 0x57) ||
                DecodeInst->OP == 0x68 || DecodeInst->OP == 0x6A ||
                DecodeInst->OP == 0x06 || DecodeInst->OP == 0x0E ||
                DecodeInst->OP == 0x16 || DecodeInst->OP == 0x1E ||
                DecodeInst->OP == 0xA0 || DecodeInst->OP == 0xA8)) {
      set_x86_instr_opc(instr, X86_OPC_PUSH);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "POP") &&
               ((DecodeInst->OP >= 0x58 && DecodeInst->OP <= 0x5F) ||
                DecodeInst->OP == 0x8F || DecodeInst->OP == 0x07 ||
                DecodeInst->OP == 0x17 || DecodeInst->OP == 0x1F ||
                DecodeInst->OP == 0xA1 || DecodeInst->OP == 0xA9)) {
      set_x86_instr_opc(instr, X86_OPC_POP);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CALL") &&
               (DecodeInst->OP == 0xE8)) {
      set_x86_instr_opc(instr, X86_OPC_CALL);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "RET") &&
               (DecodeInst->OP == 0xC3)) { // C2 iw	RET imm16
      set_x86_instr_opc(instr, X86_OPC_RET);
    }

    // TYPE_GROUP_1~11
#define OPD(group, prefix, Reg)                                                \
  (((group - FEXCore::X86Tables::TYPE_GROUP_1) << 6) | (prefix) << 3 | (Reg))

    // 示例：处理GROUP中的MOV指令
    if (!strcmp(DecodeInst->TableInfo->Name, "MOV") &&
        (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_11,
                               FEXCore::X86Tables::OpToIndex(0xC6), 0) ||
         DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_11,
                               FEXCore::X86Tables::OpToIndex(0xC7), 0))) {
      set_x86_instr_opc(instr, X86_OPC_MOV);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "NOT") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF7),
                                      2))) {
      set_x86_instr_opc(instr, X86_OPC_NOT);
      SingleSrc = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "NEG") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF7),
                                      3))) {
      set_x86_instr_opc(instr, X86_OPC_NEG);
      SingleSrc = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "AND") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      4))) {
      set_x86_instr_opc(instr, X86_OPC_AND);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "OR") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 1) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 1) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 1) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      1))) {
      set_x86_instr_opc(instr, X86_OPC_OR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "XOR") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      6))) {
      set_x86_instr_opc(instr, X86_OPC_XOR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "INC") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_4,
                                      FEXCore::X86Tables::OpToIndex(0xFE), 0) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5,
                                      FEXCore::X86Tables::OpToIndex(0xFF),
                                      0))) {
      set_x86_instr_opc(instr, X86_OPC_INC);
      SingleSrc = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "DEC") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_4,
                                      FEXCore::X86Tables::OpToIndex(0xFE), 1) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5,
                                      FEXCore::X86Tables::OpToIndex(0xFF),
                                      1))) {
      set_x86_instr_opc(instr, X86_OPC_DEC);
      SingleSrc = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADD") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 0) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 0) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 0) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      0))) {
      set_x86_instr_opc(instr, X86_OPC_ADD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADC") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 2) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 2) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 2) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      2))) {
      set_x86_instr_opc(instr, X86_OPC_ADC);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUB") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      5))) {
      set_x86_instr_opc(instr, X86_OPC_SUB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SBB") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 3) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 3) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 3) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      3))) {
      set_x86_instr_opc(instr, X86_OPC_SBB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "IMUL") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF7),
                                      5))) {
      set_x86_instr_opc(instr, X86_OPC_IMUL);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SHL") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC0), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC0), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD0), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD0), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD2), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD2), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC1), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC1), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD1), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD1), 6) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD3), 4) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD3),
                                      6))) {
      set_x86_instr_opc(instr, X86_OPC_SHL);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SHR") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC0), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD0), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD2), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC1), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD1), 5) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD3),
                                      5))) {
      set_x86_instr_opc(instr, X86_OPC_SHR);
      if (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                FEXCore::X86Tables::OpToIndex(0xD0), 5) ||
          DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                FEXCore::X86Tables::OpToIndex(0xD1), 5))
        MutiplyOnce = true;
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SAR") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC0), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD0), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD2), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xC1), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD1), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_2,
                                      FEXCore::X86Tables::OpToIndex(0xD3),
                                      7))) {
      set_x86_instr_opc(instr, X86_OPC_SAR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "TEST") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF6), 0) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF6), 1) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF7), 0) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF7),
                                      1))) {
      set_x86_instr_opc(instr, X86_OPC_TEST);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CMP") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x80), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x82), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x81), 7) ||
                DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_1,
                                      FEXCore::X86Tables::OpToIndex(0x83),
                                      7))) {
      set_x86_instr_opc(instr, X86_OPC_CMP);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "JMP") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5,
                                      FEXCore::X86Tables::OpToIndex(0xFF),
                                      4))) {
      set_x86_instr_opc(instr, X86_OPC_JMP);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUSH") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5,
                                      FEXCore::X86Tables::OpToIndex(0xFF),
                                      6))) {
      set_x86_instr_opc(instr, X86_OPC_PUSH);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "CALL") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_5,
                                      FEXCore::X86Tables::OpToIndex(0xFF),
                                      2))) {
      set_x86_instr_opc(instr, X86_OPC_CALL);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MUL") &&
               (DecodeInst->OP == OPD(FEXCore::X86Tables::TYPE_GROUP_3,
                                      FEXCore::X86Tables::OpToIndex(0xF7),
                                      4))) {
      set_x86_instr_opc(instr, X86_OPC_MULL);
    }
#undef OPD

    // 处理SSE指令
    if (!strcmp(DecodeInst->TableInfo->Name, "MOVUPS") &&
        ((DecodeInst->OP == 0x10) || (DecodeInst->OP == 0x11))) {
      set_x86_instr_opc(instr, X86_OPC_MOVUPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVUPD") &&
               ((DecodeInst->OP == 0x10) || (DecodeInst->OP == 0x11))) {
      set_x86_instr_opc(instr, X86_OPC_MOVUPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSS") &&
               ((DecodeInst->OP == 0x10) || (DecodeInst->OP == 0x11))) {
      set_x86_instr_opc(instr, X86_OPC_MOVSS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSD") &&
               ((DecodeInst->OP == 0x10) || (DecodeInst->OP == 0x11))) {
      set_x86_instr_opc(instr, X86_OPC_MOVSD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVLPS") &&
               ((DecodeInst->OP == 0x12) || (DecodeInst->OP == 0x13))) {
      set_x86_instr_opc(instr, X86_OPC_MOVLPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVLPD") &&
               ((DecodeInst->OP == 0x12) || (DecodeInst->OP == 0x13))) {
      set_x86_instr_opc(instr, X86_OPC_MOVLPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVHPS") &&
               (DecodeInst->OP == 0x17)) {
      set_x86_instr_opc(instr, X86_OPC_MOVHPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVHPD") &&
               ((DecodeInst->OP == 0x16) || (DecodeInst->OP == 0x17))) {
      set_x86_instr_opc(instr, X86_OPC_MOVHPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVAPS") &&
               ((DecodeInst->OP == 0x28) || (DecodeInst->OP == 0x29))) {
      set_x86_instr_opc(instr, X86_OPC_MOVAPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVAPD") &&
               ((DecodeInst->OP == 0x28) || (DecodeInst->OP == 0x29))) {
      set_x86_instr_opc(instr, X86_OPC_MOVAPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVD") &&
               ((DecodeInst->OP == 0x6E) || (DecodeInst->OP == 0x7E))) {
      set_x86_instr_opc(instr, X86_OPC_MOVD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVQ") &&
               ((DecodeInst->OP == 0x6F) || (DecodeInst->OP == 0x7F) ||
                (DecodeInst->OP == 0x7E) || (DecodeInst->OP == 0xD6))) {
      set_x86_instr_opc(instr, X86_OPC_MOVQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVDQA") &&
               ((DecodeInst->OP == 0x6F) || (DecodeInst->OP == 0x7F))) {
      set_x86_instr_opc(instr, X86_OPC_MOVDQA);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVDQU") &&
               ((DecodeInst->OP == 0x6F) || (DecodeInst->OP == 0x7F))) {
      set_x86_instr_opc(instr, X86_OPC_MOVDQU);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PMOVMSKB") &&
               ((DecodeInst->OP == 0xD7))) {
      set_x86_instr_opc(instr, X86_OPC_PMOVMSKB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PACKUSWB") &&
               ((DecodeInst->OP == 0x67))) {
      set_x86_instr_opc(instr, X86_OPC_PACKUSWB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PACKSSWB") &&
               ((DecodeInst->OP == 0x63))) {
      set_x86_instr_opc(instr, X86_OPC_PACKSSWB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PACKSSDW") &&
               ((DecodeInst->OP == 0x6B))) {
      set_x86_instr_opc(instr, X86_OPC_PACKSSDW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ANDPS") &&
               ((DecodeInst->OP == 0x54))) {
      set_x86_instr_opc(instr, X86_OPC_ANDPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ANDPD") &&
               ((DecodeInst->OP == 0x54))) {
      set_x86_instr_opc(instr, X86_OPC_ANDPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ORPS") &&
               ((DecodeInst->OP == 0x56))) {
      set_x86_instr_opc(instr, X86_OPC_ORPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ORPD") &&
               ((DecodeInst->OP == 0x56))) {
      set_x86_instr_opc(instr, X86_OPC_ORPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "XORPS") &&
               ((DecodeInst->OP == 0x57))) {
      set_x86_instr_opc(instr, X86_OPC_XORPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "XORPD") &&
               ((DecodeInst->OP == 0x57))) {
      set_x86_instr_opc(instr, X86_OPC_XORPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PAND") &&
               ((DecodeInst->OP == 0xDB))) {
      set_x86_instr_opc(instr, X86_OPC_PAND);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PANDN") &&
               ((DecodeInst->OP == 0xDF))) {
      set_x86_instr_opc(instr, X86_OPC_PANDN);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "POR") &&
               ((DecodeInst->OP == 0xEB))) {
      set_x86_instr_opc(instr, X86_OPC_POR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PXOR") &&
               ((DecodeInst->OP == 0xEF))) {
      set_x86_instr_opc(instr, X86_OPC_PXOR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLBW") &&
               ((DecodeInst->OP == 0x60))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLBW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLWD") &&
               ((DecodeInst->OP == 0x61))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLWD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLDQ") &&
               ((DecodeInst->OP == 0x62))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHBW") &&
               ((DecodeInst->OP == 0x68))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHBW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHWD") &&
               ((DecodeInst->OP == 0x69))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHWD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHDQ") &&
               ((DecodeInst->OP == 0x6A))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLQDQ") &&
               ((DecodeInst->OP == 0x6C))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLQDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHQDQ") &&
               ((DecodeInst->OP == 0x6D))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHQDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SHUFPD") &&
               ((DecodeInst->OP == 0xC6))) {
      set_x86_instr_opc(instr, X86_OPC_SHUFPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSHUFD") &&
               ((DecodeInst->OP == 0x70))) {
      set_x86_instr_opc(instr, X86_OPC_PSHUFD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSHUFLW") &&
               ((DecodeInst->OP == 0x70))) {
      set_x86_instr_opc(instr, X86_OPC_PSHUFLW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSHUFHW") &&
               ((DecodeInst->OP == 0x70))) {
      set_x86_instr_opc(instr, X86_OPC_PSHUFHW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPGTB") &&
               ((DecodeInst->OP == 0x64))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPGTB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPGTW") &&
               ((DecodeInst->OP == 0x65))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPGTW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPGTD") &&
               ((DecodeInst->OP == 0x66))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPGTD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPEQB") &&
               ((DecodeInst->OP == 0x74))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPEQB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPEQW") &&
               ((DecodeInst->OP == 0x75))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPEQW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPEQD") &&
               ((DecodeInst->OP == 0x76))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPEQD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDPS") &&
               ((DecodeInst->OP == 0x58))) {
      set_x86_instr_opc(instr, X86_OPC_ADDPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDPD") &&
               ((DecodeInst->OP == 0x58))) {
      set_x86_instr_opc(instr, X86_OPC_ADDPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDSS") &&
               ((DecodeInst->OP == 0x58))) {
      set_x86_instr_opc(instr, X86_OPC_ADDSS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDSD") &&
               ((DecodeInst->OP == 0x58))) {
      set_x86_instr_opc(instr, X86_OPC_ADDSD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBPS") &&
               ((DecodeInst->OP == 0x5C))) {
      set_x86_instr_opc(instr, X86_OPC_SUBPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBPD") &&
               ((DecodeInst->OP == 0x5C))) {
      set_x86_instr_opc(instr, X86_OPC_SUBPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBSS") &&
               ((DecodeInst->OP == 0x5C))) {
      set_x86_instr_opc(instr, X86_OPC_SUBSS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBSD") &&
               ((DecodeInst->OP == 0x5C))) {
      set_x86_instr_opc(instr, X86_OPC_SUBSD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSUBB") &&
               ((DecodeInst->OP == 0xF8))) {
      set_x86_instr_opc(instr, X86_OPC_PSUBB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PADDD") &&
               ((DecodeInst->OP == 0xFE))) {
      set_x86_instr_opc(instr, X86_OPC_PADDD);
    }

#define OPD(REX, prefix, opcode) ((REX << 9) | (prefix << 8) | opcode)
    constexpr uint16_t PF_3A_NONE = 0;
    constexpr uint16_t PF_3A_66 = 1;
    if (!strcmp(DecodeInst->TableInfo->Name, "PALIGNR") &&
        (DecodeInst->OP == OPD(0, PF_3A_NONE, 0x0F) ||
         DecodeInst->OP == OPD(0, PF_3A_66, 0x0F) ||
         DecodeInst->OP == OPD(1, PF_3A_66, 0x0F))) {
      set_x86_instr_opc(instr, X86_OPC_PALIGNR);
      ThreeSrc = true;
    }
#undef OPD

    // VEX
#define OPD(map_select, pp, opcode)                                            \
  (((map_select - 1) << 10) | (pp << 8) | (opcode))
    if (!strcmp(DecodeInst->TableInfo->Name, "SHLX") &&
        (DecodeInst->OP == OPD(2, 0b01, 0xF7))) {
      set_x86_instr_opc(instr, X86_OPC_SHLD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SHRX") &&
               (DecodeInst->OP == OPD(2, 0b11, 0xF7))) {
      set_x86_instr_opc(instr, X86_OPC_SHRD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVUPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x10)) ||
                (DecodeInst->OP == OPD(1, 0b00, 0x11)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVUPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVUPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x10)) ||
                (DecodeInst->OP == OPD(1, 0b01, 0x11)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVUPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSS") &&
               ((DecodeInst->OP == OPD(1, 0b10, 0x10)) ||
                (DecodeInst->OP == OPD(1, 0b10, 0x11)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVSS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVSD") &&
               ((DecodeInst->OP == OPD(1, 0b11, 0x10)) ||
                (DecodeInst->OP == OPD(1, 0b11, 0x11)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVSD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVLPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x12)) ||
                (DecodeInst->OP == OPD(1, 0b00, 0x13)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVLPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVLPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x12)) ||
                (DecodeInst->OP == OPD(1, 0b01, 0x13)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVLPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVHPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x16)) ||
                (DecodeInst->OP == OPD(1, 0b00, 0x17)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVHPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVHPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x16)) ||
                (DecodeInst->OP == OPD(1, 0b01, 0x17)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVHPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVAPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x28)) ||
                (DecodeInst->OP == OPD(1, 0b00, 0x29)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVAPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVAPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x28)) ||
                (DecodeInst->OP == OPD(1, 0b01, 0x29)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVAPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVQ") &&
               ((DecodeInst->OP == OPD(1, 0b10, 0x7E)) ||
                (DecodeInst->OP == OPD(1, 0b01, 0xD6)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVDQA") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x6F)) ||
                (DecodeInst->OP == OPD(1, 0b01, 0x7F)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVDQA);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "MOVDQU") &&
               ((DecodeInst->OP == OPD(1, 0b10, 0x6F)) ||
                (DecodeInst->OP == OPD(1, 0b10, 0x7F)))) {
      set_x86_instr_opc(instr, X86_OPC_MOVDQU);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PMOVMSKB") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xD7)))) {
      set_x86_instr_opc(instr, X86_OPC_PMOVMSKB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PACKUSWB") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x67)))) {
      set_x86_instr_opc(instr, X86_OPC_PACKUSWB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PACKSSWB") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x63)))) {
      set_x86_instr_opc(instr, X86_OPC_PACKSSWB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PACKSSDW") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x6B)))) {
      set_x86_instr_opc(instr, X86_OPC_PACKSSDW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PALIGNR") &&
               ((DecodeInst->OP == OPD(3, 0b01, 0x0F)))) {
      set_x86_instr_opc(instr, X86_OPC_PALIGNR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ANDPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x54)))) {
      set_x86_instr_opc(instr, X86_OPC_ANDPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ANDPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x54)))) {
      set_x86_instr_opc(instr, X86_OPC_ANDPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ORPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x56)))) {
      set_x86_instr_opc(instr, X86_OPC_ORPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ORPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x56)))) {
      set_x86_instr_opc(instr, X86_OPC_ORPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "XORPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x57)))) {
      set_x86_instr_opc(instr, X86_OPC_XORPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "XORPS") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x57)))) {
      set_x86_instr_opc(instr, X86_OPC_XORPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PAND") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xDB)))) {
      set_x86_instr_opc(instr, X86_OPC_PAND);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PANDN") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xDF)))) {
      set_x86_instr_opc(instr, X86_OPC_PANDN);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "POR") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xEB)))) {
      set_x86_instr_opc(instr, X86_OPC_POR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PXOR") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xEF)))) {
      set_x86_instr_opc(instr, X86_OPC_PXOR);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLBW") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x60)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLBW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLWD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x61)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLWD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLDQ") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x62)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHBW") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x68)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHBW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHWD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x69)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHWD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHDQ") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x6A)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKLQDQ") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x6C)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKLQDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PUNPCKHQDQ") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x6D)))) {
      set_x86_instr_opc(instr, X86_OPC_PUNPCKHQDQ);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SHUFPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xC6)))) {
      set_x86_instr_opc(instr, X86_OPC_SHUFPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSHUFD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x70)))) {
      set_x86_instr_opc(instr, X86_OPC_PSHUFD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSHUFLW") &&
               ((DecodeInst->OP == OPD(1, 0b11, 0x70)))) {
      set_x86_instr_opc(instr, X86_OPC_PSHUFLW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSHUFHW") &&
               ((DecodeInst->OP == OPD(1, 0b10, 0x70)))) {
      set_x86_instr_opc(instr, X86_OPC_PSHUFHW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPGTB") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x64)))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPGTB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPGTW") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x65)))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPGTW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPGTD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x66)))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPGTD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPEQB") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x74)))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPEQB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPEQW") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x75)))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPEQW);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PCMPEQD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x76)))) {
      set_x86_instr_opc(instr, X86_OPC_PCMPEQD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x58)))) {
      set_x86_instr_opc(instr, X86_OPC_ADDPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x58)))) {
      set_x86_instr_opc(instr, X86_OPC_ADDPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDSS") &&
               ((DecodeInst->OP == OPD(1, 0b10, 0x58)))) {
      set_x86_instr_opc(instr, X86_OPC_ADDSS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "ADDSD") &&
               ((DecodeInst->OP == OPD(1, 0b11, 0x58)))) {
      set_x86_instr_opc(instr, X86_OPC_ADDSD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBPS") &&
               ((DecodeInst->OP == OPD(1, 0b00, 0x5C)))) {
      set_x86_instr_opc(instr, X86_OPC_SUBPS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBPD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0x5C)))) {
      set_x86_instr_opc(instr, X86_OPC_SUBPD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBSS") &&
               ((DecodeInst->OP == OPD(1, 0b10, 0x5C)))) {
      set_x86_instr_opc(instr, X86_OPC_SUBSS);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "SUBSD") &&
               ((DecodeInst->OP == OPD(1, 0b11, 0x5C)))) {
      set_x86_instr_opc(instr, X86_OPC_SUBSD);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PSUBB") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xF8)))) {
      set_x86_instr_opc(instr, X86_OPC_PSUBB);
    } else if (!strcmp(DecodeInst->TableInfo->Name, "PADDD") &&
               ((DecodeInst->OP == OPD(1, 0b01, 0xFE)))) {
      set_x86_instr_opc(instr, X86_OPC_PADDD);
    }
#undef OPD
  }

  // 如果指令无效，直接返回
  if (instr->opc == X86_OPC_INVALID)
    return;

  // 设置指令的操作数大小
  set_x86_instr_opd_size(instr, SrcSize, DestSize);

  uint8_t num = 0, count = 0;
  FEXCore::X86Tables::DecodedOperand *Opd = &DecodeInst->Dest;

  // 处理指令的操作数
  while (Opd) {
    if (!Opd->IsNone()) {
      if (SingleSrc && num == 1)
        break;

#ifdef DEBUG_RULE_LOG
      writeToLogFile(std::to_string(pid) + "fex-debug.log",
                     "[INFO] ====Operand Num: " + std::to_string(num + 1) +
                         "\n");
#else
      LogMan::Msg::IFmt("====Operand Num: {:x}", num + 1);
#endif
      if (Opd->IsGPR()) {
        // 处理通用寄存器操作数
        uint8_t GPR = Opd->Data.GPR.GPR;
        bool HighBits = Opd->Data.GPR.HighBits;

#ifdef DEBUG_RULE_LOG
        writeToLogFile(std::to_string(pid) + "fex-debug.log",
                       "[INFO]      GPR: " + std::to_string(GPR) + "\n");
#else
        LogMan::Msg::IFmt("     GPR: 0x{:x}", GPR);
#endif

        if (GPR <= FEXCore::X86State::REG_XMM_15)
          set_x86_instr_opd_reg(instr, num, GPR, HighBits);
        else
          set_x86_instr_opd_reg(instr, num, 0x20, false);
      } else if (Opd->IsRIPRelative()) {
        // 处理RIP相对寻址操作数
        uint32_t Literal = Opd->Data.RIPLiteral.Value.u;

#ifdef DEBUG_RULE_LOG
        writeToLogFile(std::to_string(pid) + "fex-debug.log",
                       "[INFO]      RIPLiteral: 0x" + intToHex(Literal) + "\n");
#else
        LogMan::Msg::IFmt("     RIPLiteral: 0x{:x}", Literal);
#endif

        set_x86_instr_opd_imm(instr, num, Literal, true);
      } else if (Opd->IsLiteral()) {
        // 处理立即数操作数
        uint64_t Literal = Opd->Data.Literal.Value;

#ifdef DEBUG_RULE_LOG
        writeToLogFile(std::to_string(pid) + "fex-debug.log",
                       "[INFO]      Literal: 0x" + intToHex(Literal) + "\n");
#else
        LogMan::Msg::IFmt("     Literal: 0x{:x}", Literal);
#endif

        set_x86_instr_opd_imm(instr, num, Literal);
      } else if (Opd->IsGPRDirect()) {
        // 处理直接寄存器寻址操作数
        uint8_t GPR = Opd->Data.GPR.GPR;

#ifdef DEBUG_RULE_LOG
        writeToLogFile(std::to_string(pid) + "fex-debug.log",
                       "[INFO]      GPRDirect: " + std::to_string(GPR) + "\n");
#else
        LogMan::Msg::IFmt("     GPRDirect: 0x{:x}", GPR);
#endif

        set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
        if (GPR <= FEXCore::X86State::REG_XMM_15)
          set_x86_instr_opd_mem_base(instr, num, GPR);
        else
          set_x86_instr_opd_mem_base(instr, num, 0x20);
      } else if (Opd->IsGPRIndirect()) {
        // 处理间接寄存器寻址操作数
        uint8_t GPR = Opd->Data.GPRIndirect.GPR;
        int32_t Displacement = Opd->Data.GPRIndirect.Displacement;

#ifdef DEBUG_RULE_LOG
        writeToLogFile(std::to_string(pid) + "fex-debug.log",
                       "[INFO]      GPRIndirect - GPR: " + std::to_string(GPR) +
                           ", Displacement: " + std::to_string(Displacement) +
                           "\n");
#else
        LogMan::Msg::IFmt(
            "     GPRIndirect - GPR: 0x{:x}, Displacement: 0x{:x}", GPR,
            Displacement);
#endif

        set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
        if (GPR <= FEXCore::X86State::REG_XMM_15)
          set_x86_instr_opd_mem_base(instr, num, GPR);
        else
          set_x86_instr_opd_mem_base(instr, num, 0x20);
        set_x86_instr_opd_mem_off(instr, num, Displacement);
      } else if (Opd->IsSIB()) {
        // 处理SIB（比例-索引-基址）寻址操作数
        uint8_t Base = Opd->Data.SIB.Base;
        int32_t Offset = Opd->Data.SIB.Offset;
        uint8_t Index = Opd->Data.SIB.Index;
        uint8_t Scale = Opd->Data.SIB.Scale;

#ifdef DEBUG_RULE_LOG
        writeToLogFile(std::to_string(pid) + "fex-debug.log",
                       "[INFO]      SIB - Base: " + std::to_string(Base) +
                           ", Offset: " + std::to_string(Offset) +
                           ", Index: " + std::to_string(Index) +
                           ", Scale: " + std::to_string(Scale) + "\n");
#else
        LogMan::Msg::IFmt("     SIB - Base: 0x{:x}, Offset: 0x{:x}, Index: "
                          "0x{:x}, Scale: 0x{:x}",
                          Base, Offset, Index, Scale);
#endif

        set_x86_instr_opd_type(instr, num, X86_OPD_TYPE_MEM);
        if (Base <= FEXCore::X86State::REG_XMM_15) {
          set_x86_instr_opd_mem_base(instr, num, Base);
          set_x86_instr_opd_mem_off(instr, num, Offset);
          if (Index <= FEXCore::X86State::REG_XMM_15) {
            set_x86_instr_opd_mem_index(instr, num, Index);
            set_x86_instr_opd_mem_scale(instr, num, Scale);
          } else
            set_x86_instr_opd_mem_index(instr, num, 0x20);
        } else
          set_x86_instr_opd_mem_base(instr, num, 0x20);
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

  // 处理特殊情况
  // 对于cmp, add, or, mov, test, sub指令有三个操作数的情况，需要调整操作数的顺序或数量
  if (num == 3 && !ThreeSrc) {
    instr->opd[1] = instr->opd[2];
    num--;
  }

  if ((instr->opc == X86_OPC_JMP || instr->opc == X86_OPC_CALL ||
       instr->opc == X86_OPC_PUSH) &&
      (num == 2)) {
    instr->opd[0] = instr->opd[1];
    num--;
  }

  if ((instr->opc == X86_OPC_SHR) && MutiplyOnce) {
    set_x86_instr_opd_imm(instr, 1, 1);
  }

  // 设置操作数数量和指令大小
  set_x86_instr_opd_num(instr, num);
  set_x86_instr_size(instr, DecodeInst->InstSize);

#ifdef DEBUG_RULE_LOG
  output_x86_instr(instr, pid);
#endif
}
