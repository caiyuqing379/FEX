/**
 * @file x86-parse.cpp
 * @brief X86指令解析器的实现
 *
 * 本文件包含了用于解析X86指令的各种函数和数据结构。
 * 主要功能包括解析X86指令的操作码、操作数，以及整个X86代码序列。
 * 这些功能主要用于解析转换规则中的X86代码部分。
 */

#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctype.h>

#include "x86-instr.h"
#include "x86-parse.h"

// X86指令缓冲区的最大长度
#define RULE_X86_INSTR_BUF_LEN 1000000

// X86指令缓冲区和当前索引
static X86Instruction *rule_x86_instr_buf;
static int rule_x86_instr_buf_index;

/**
 * @brief 初始化X86指令缓冲区
 *
 * 分配内存并初始化X86指令缓冲区。
 */
void rule_x86_instr_buf_init(void)
{
    rule_x86_instr_buf = new X86Instruction[RULE_X86_INSTR_BUF_LEN];
    if (rule_x86_instr_buf == NULL)
        LogMan::Msg::IFmt( "Cannot allocate memory for rule_x86_instr_buf!\n");

    rule_x86_instr_buf_index = 0;
}

/**
 * @brief 分配一个新的X86指令结构体
 *
 * @param pc 指令的程序计数器值
 * @return X86Instruction* 新分配的X86指令结构体指针
 */
static X86Instruction *rule_x86_instr_alloc(uint64_t pc)
{
    X86Instruction *instr = &rule_x86_instr_buf[rule_x86_instr_buf_index++];
    if (rule_x86_instr_buf_index >= RULE_X86_INSTR_BUF_LEN)
        LogMan::Msg::IFmt( "Error: rule_x86_instr_buf is not enough!\n");

    instr->pc = pc;
    instr->next = NULL;
    return instr;
}

/**
 * @brief 解析X86指令的操作码
 *
 * @param line 包含指令的字符串
 * @param instr 指向X86Instruction结构的指针
 * @return int 解析后的字符串索引
 */
static int parse_rule_x86_opcode(char *line, X86Instruction *instr)
{
    char opc_str[20] = "\0";
    int i = 4; //skip the first 4 spaces

    if (line[i] == 'L') {// this is a lable
        set_x86_instr_opc(instr, X86_OPC_SET_LABEL);
        return i;
    }

    while(line[i] != ' ' && line[i] != '\n')
        strncat(opc_str, &line[i++], 1);

    set_x86_instr_opc_str(instr, opc_str);

    if (line[i] == ' ')
        return i+1;
    else
        return i;
}

/**
 * @brief 检查操作数是否有后缀(如 al, ah, ax 等)
 *
 * @param line 包含操作数的字符串
 * @param idx 当前检查的索引
 * @return bool 如果有后缀返回true，否则返回false
 */
static bool HasSuffix(char *line, int idx) {
    if (line == NULL) {
        LogMan::Msg::EFmt( "line is NULL!");
        return false;
    }

    if((line[idx] == 'a' || line[idx] == 'b' || line[idx] == 'c' || line[idx] == 'd' || line[idx] == 's')
      && (line[idx+1] == 'l' || line[idx+1] == 'h' || line[idx+1] == 'x' || line[idx+1] == 'i' || line[idx+1] == 'p'))
        return true;
    else
        return false;
}

// 如果X86指令有临时寄存器，当前不支持
static bool has_temp_register = false;

/**
 * @brief 解析X86指令的操作数
 *
 * 这个函数负责解析X86指令的单个操作数，包括立即数、寄存器和内存操作数。
 * 函数首先检查操作数的第一个字符来确定其类型
 * 1）'$' 表示立即数，函数解析其值或符号
 * 2）'r', 'e' 或带有特定后缀的字符表示寄存器，函数解析寄存器名称
 * 3）'b', 'w', 'd', 'q', 'x' 或 '[' 表示内存操作数，
 *    函数解析基址寄存器、索引寄存器、比例因子和偏移量
 *
 * @param line 包含操作数的字符串
 * @param idx 当前解析的起始索引
 * @param instr 指向X86Instruction结构的指针
 * @param opd_idx 操作数在指令中的索引
 * @return int 解析后的字符串索引
 */
static int parse_rule_x86_operand(char *line, int idx, X86Instruction *instr, int opd_idx)
{
    X86Operand *opd = &instr->opd[opd_idx];
    char fc = line[idx];
    uint32_t OpSize = 0;

    if (fc == '$') {
        /* Immediate Operand */
        char imm_str[20] = "\0";

        idx++; // skip '$'
        fc = line[idx];
        if (fc == '(') // the immediate is an expression
            idx++; // skip '('

        while (line[idx] != ',' && line[idx] != ':' && line[idx] != ')' && line[idx] != '\n')
            strncat(imm_str, &line[idx++], 1);

        if (line[idx] == ')')
            idx++;

        set_x86_opd_type(opd, X86_OPD_TYPE_IMM);
        if (fc == 'i' || fc == '(' || fc == 'L')
            set_x86_opd_imm_sym_str(opd, imm_str, false);
        else
            set_x86_opd_imm_val_str(opd, imm_str, false, false);

        if (line[idx] == ':')
            idx++; // skip ':'
    } else if (fc == 'r' || fc == 'e' || HasSuffix(line, idx)) {
        /* Register operand */
        char reg_str[20] = "\0";

        if (fc == 't')
            has_temp_register = true;

        while (line[idx] != ',' && line[idx] != '\n')
            strncat(reg_str, &line[idx++], 1);

        set_x86_opd_type(opd, X86_OPD_TYPE_REG);
        set_x86_opd_reg_str(opd, reg_str, &OpSize);
    } else if (fc == 'b' || fc == 'w' || fc == 'd' || fc == 'q' || fc == 'x' || fc == '[') {
        /* Memory operand with or without offset (imm_XXX) */
        char reg_str[10] = "\0";

        set_x86_opd_type(opd, X86_OPD_TYPE_MEM);
        if (fc == 'b') {
            idx += 4;
            OpSize = 1;
        } else if (fc == 'w') {
            idx += 4;
            OpSize = 2;
        } else if (fc == 'd' && line[idx+1] == 'w') {
            idx += 5;
            OpSize = 3;
        } else if (fc == 'q' && line[idx+1] == 'w') {
            idx += 5;
            OpSize = 4;
        } else if (fc == 'x') {
            idx += 7;
            OpSize = 5;
        }

        if (line[idx] == ' ') {
          idx++; // skip ' '
          fc = line[idx];
        }

        if (fc == '[') {
            idx++; // skip '['
            while (line[idx] != ' '&&line[idx] != ']')
                strncat(reg_str, &line[idx++], 1);

            // rip literal
            if (!strcmp(reg_str,"rip")) {
                idx++; // skip ' '
                fc = line[idx];
                if (fc == '+' || fc == '-') {
                    char off_str[20] = "\0"; // parse offset
                    bool neg = line[idx] == '-' ? true : false;
                    idx+=2;
                    fc = line[idx];

                    while(line[idx] != ']')
                        strncat(off_str, &line[idx++], 1);

                    set_x86_opd_type(opd, X86_OPD_TYPE_IMM);
                    if (fc == 'i')
                       set_x86_opd_imm_sym_str(opd, off_str, true);
                    else
                       set_x86_opd_imm_val_str(opd, off_str, true, neg);
                }
                goto next;
            }

            set_x86_opd_mem_base_str(opd, reg_str);

            idx++; // skip ' '
            fc = line[idx];
            if (fc == '+' || fc == '-') {
                if (fc == '+' && line[idx+2] == 'r') { // have index register
                    char index_str[10] = "\0";
                    idx+=2;
                    while(line[idx] != ' ' && line[idx] != ']')
                      strncat(index_str, &line[idx++], 1);
                    idx++;
                    set_x86_opd_mem_index_str(opd, index_str);

                    if (line[idx] == '*') { // have scale value
                      char scale_str[20] = "\0";
                      idx+=2;
                      while(line[idx] != ' ' && line[idx] != ']')
                        strncat(scale_str, &line[idx++], 1);
                      set_x86_opd_mem_scale_str(opd, scale_str);
                    }

                    if (line[idx] == ' ') idx++;
                }
                if (line[idx] == '+' || line[idx] == '-') {
                    char off_str[20] = "\0"; // parse offset
                    bool neg = line[idx] == '-' ? true : false;
                    idx+=2; // skip '+ '

                    while(line[idx] != ']')
                        strncat(off_str, &line[idx++], 1);

                    set_x86_opd_mem_off_str(opd, off_str, neg);
                }
            }
        }

        next:
        while (line[idx] == ']')
          idx++;
    } else
        fprintf(stderr, "Error in parsing x86 operand: unknown operand type at idx %d char %c in line: %s", idx, line[idx], line);

    if (!opd_idx)
        instr->DestSize = OpSize;
    else
        instr->SrcSize = OpSize;

    if (line[idx] == ',')
        return idx+2;
    else
        return idx;
}

/**
 * @brief 解析单条X86指令
 *
 * @param line 包含指令的字符串
 * @param pc 指令的程序计数器值
 * @return X86Instruction* 解析后的X86指令结构体指针
 */
static X86Instruction *parse_rule_x86_instruction(char *line, uint64_t pc)
{
    X86Instruction *instr = rule_x86_instr_alloc(pc);
    int opd_idx;
    int i;

    i = parse_rule_x86_opcode(line, instr);

    if (instr->opc == X86_OPC_RET)
      set_x86_instr_opd_size(instr, 4, 4);

    opd_idx = 0;
    while(line[i] != '\n')
        i = parse_rule_x86_operand(line, i, instr, opd_idx++);

    set_x86_instr_opd_num(instr, opd_idx);

    return instr;
}

/**
 * @brief 解析规则中的X86代码序列
 *
 * @param fp 指向规则文件的文件指针
 * @param rule 指向TranslationRule结构的指针
 */
void parse_rule_x86_code(FILE *fp, TranslationRule *rule)
{
    uint64_t pc = 0;
    X86Instruction *code_head = NULL;
    X86Instruction *code_tail = NULL;
    char line[500];

    has_temp_register = false;

    /* parse x86 (host) instruction in this rule */
    while(fgets(line, 500, fp)) {
        if (strstr(line, ".Host:\n")) {
            fseek(fp, (0-strlen(line)), SEEK_CUR);
            break;
        }

        /* check if this line is a comment */
        char fs = line[0];
        if (fs == '#')
            continue;
        X86Instruction *cur = parse_rule_x86_instruction(line, pc);
        if (!code_head) {
            code_head = code_tail = cur;
        } else {
            code_tail->next = cur;
            code_tail = cur;
        }
        pc += 4;    // 假设的值，实际应根据指令长度调整
        rule->guest_instr_num++;
    }

    // 以下注释掉的代码用于调试
    // LogMan::Msg::IFmt( "**** Guest {} ****", rule->index);
    // print_x86_instr_seq(code_head);

    rule->x86_guest = code_head;
}
