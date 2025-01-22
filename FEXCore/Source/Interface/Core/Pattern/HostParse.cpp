/**
 * @file host-parse.cpp
 * @brief host主机架构指令解析器的实现
 *
 * 本文件包含了用于解析ARM/RISC-V指令的各种函数和数据结构。
 * 主要功能包括解析ARM/RISC-V指令的操作码、操作数，以及整个ARM/RISC-V代码序列。
 * 这些功能主要用于解析转换规则中的ARM/RISC-V代码部分。
 */
#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstring>
#include <assert.h>
#include <cstdlib>

#include "arm-instr.h"
#include "RiscvInst.h"
#include "HostParse.h"

// ARM指令缓冲区的最大长度
#define RULE_ARM_INSTR_BUF_LEN 1000000
// 单条指令的最大长度
#define RULE_RISCV_INSTR_BUF_LEN 1000000

// ARM指令缓冲区与当前索引
static ARMInstruction *rule_arm_instr_buf;
static int rule_arm_instr_buf_index;

static RISCVInstruction *rule_riscv_instr_buf;
static int rule_riscv_instr_buf_index;

/**
 * @brief 初始化ARM指令缓冲区
 *
 * 分配内存并初始化ARM指令缓冲区。
 */
void RuleArmInstrBufInit(void)
{
    rule_arm_instr_buf = new ARMInstruction[RULE_ARM_INSTR_BUF_LEN];
    if (rule_arm_instr_buf == NULL)
        LogMan::Msg::IFmt("Cannot allocate memory for rule_arm_instr_buf!");

    rule_arm_instr_buf_index = 0;
}

/**
 * @brief 初始化RISC-V指令缓冲区
 *
 * 分配内存并初始化RISC-V指令缓冲区。
 */
void RuleRiscvInstrBufInit(void)
{
    rule_riscv_instr_buf = new RISCVInstruction[RULE_RISCV_INSTR_BUF_LEN];
    if (rule_riscv_instr_buf == NULL)
        LogMan::Msg::IFmt("Cannot allocate memory for rule_riscv_instr_buf!");

    rule_riscv_instr_buf_index = 0;
}

/**
 * @brief 分配一个新的ARM指令结构体
 *
 * @param pc 指令的程序计数器值
 * @return ARMInstruction* 新分配的ARM指令结构体指针
 */
static ARMInstruction *rule_arm_instr_alloc(uint64_t pc)
{
    ARMInstruction *instr = &rule_arm_instr_buf[rule_arm_instr_buf_index++];
    if (rule_arm_instr_buf_index >= RULE_ARM_INSTR_BUF_LEN)
        LogMan::Msg::IFmt("Error: rule_arm_instr_buf is not enought!");

    instr->pc = pc;
    instr->next = NULL;
    return instr;
}

/**
 * @brief 分配一个新的RISC-V指令结构体
 *
 * @param pc 指令的程序计数器值
 * @return RISCVInstruction* 新分配的RISC-V指令结构体指针
 */
static RISCVInstruction *rule_riscv_instr_alloc(uint64_t pc)
{
    RISCVInstruction *instr = &rule_riscv_instr_buf[rule_riscv_instr_buf_index++];
    if (rule_riscv_instr_buf_index >= RULE_RISCV_INSTR_BUF_LEN)
        LogMan::Msg::IFmt("Error: rule_riscv_instr_buf is not enought!");

    instr->pc = pc;
    instr->next = NULL;
    return instr;
}

/**
 * @brief 解析ARM指令的操作码
 *
 * @param line 包含指令的字符串
 * @param instr 指向ARMInstruction结构的指针
 * @return int 解析后的字符串索引
 */
static int parse_rule_arm_opcode(char *line, ARMInstruction *instr)
{
    char opc_str[20] = "\0";
    int i = 0;

    while(line[i] == ' ' || line[i] == '\t') // skip the first spaces
        i++;

    while(line[i] != ' ' && line[i] != '\n')
        strncat(opc_str, &line[i++], 1);

    set_arm_instr_opc_str(instr, opc_str);

    if (instr->opc == ARM_OPC_CSEL || instr->opc == ARM_OPC_CSET) {
        instr->cc = get_arm_cc(line);
    }

    if (line[i] == ' ')
        return i+1;
    else
        return i;
}

/**
 * @brief 解析RISC-V令的操作码
 *
 * @param line 包含指令的字符串
 * @param instr 指向RISCVInstruction结构的指针
 * @return int 解析后的字符串索引
 */
static int parse_rule_riscv_opcode(char *line, RISCVInstruction *instr)
{
    char opc_str[20] = "\0";
    int i = 0;

    while(line[i] == ' ' || line[i] == '\t') // skip the first spaces
        i++;

    while(line[i] != ' ' && line[i] != '\n')
        strncat(opc_str, &line[i++], 1);

    set_riscv_instr_opc_str(instr, opc_str);

    if (line[i] == ' ')
        return i+1;
    else
        return i;
}

/**
 * @brief 解析操作数的比例因子
 *
 * @param line 包含操作数的字符串
 * @param idx 当前解析的起始索引
 * @param pscale 指向ARMOperandScale结构的指针
 * @return int 解析后的字符串索引
 */
static int parse_scale(char *line, int idx, ARMOperandScale *pscale)
{
    char direct_str[10] = "\0";
    char scale_str[10] = "\0";
    int iix, i;

    if (line[idx] != ',')
        return idx;

    iix = idx + 2; // skip , and space
    for (i = 0; i < 3; i++) {
        if (line[iix] == '\n')
            break;
        strncat(direct_str, &line[iix++], 1);
    }

    /* Try to set the scale direct based on the string, may fail-- */
    if (set_arm_instr_opd_scale_str(pscale, direct_str))
        return idx;

    /* This is a scale, parse the following immediate or register */
    idx = iix + 1; // skip the space
    if (line[idx] == '#') {
        /* scale value is an immediate */
        idx++; // skip #
        while(line[idx] != ',' && line[idx] != ']' && line[idx] != '\n')
            strncat(scale_str, &line[idx++], 1);
        set_arm_instr_opd_scale_imm_str(pscale, scale_str);
    } else
        LogMan::Msg::EFmt( "Error to parsing operand scale value.");

    return idx;
}

/**
 * @brief 解析ARM指令的操作数
 *
 * 这个函数处理三种主要的操作数类型：立即数、寄存器和内存操作数。
 * 函数会根据操作数的类型设置相应的字段，并返回解析后的新索引位置。
 *
 * @param line 包含操作数的字符串
 * @param idx 当前解析的起始索引
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_idx 操作数在指令中的索引
 * @param index 指令在序列中的索引（用于错误报告）
 * @return int 解析后的字符串索引
 */
static int parse_rule_arm_operand(char *line, int idx, ARMInstruction *instr, int opd_idx, int index)
{
    ARMOperand *opd = &instr->opd[opd_idx];
    char fc = line[idx];

    if (fc == '#') {
        /* Immediate Operand
           1. Read immediate value, #XXX*/
        set_arm_opd_type(opd, ARM_OPD_TYPE_IMM);
        idx++; // skip '#'
        fc = line[idx];
        char imm_str[20] = "\0";

        while (line[idx] != ',' && line[idx] != '\n')
            strncat(imm_str, &line[idx++], 1);

        if (fc == 'i' || fc == 'L')
            set_arm_opd_imm_sym_str(opd, imm_str);
        else
            set_arm_opd_imm_val_str(opd, imm_str);
    } else if (fc == 'r' || fc == 'v' || fc == 'q' || fc == '{' || fc == 'w' || fc == 'x') {
        /* Register Operand
           1. Read register string, e.g., "reg0", "reg1".
           2. Check the scale type and content */
        char reg_str[20] = "\0";

        if (fc == '{')
          idx++;

        while (line[idx] != ',' && line[idx] != '\n')
            strncat(reg_str, &line[idx++], 1);

        set_arm_instr_opd_type(instr, opd_idx, ARM_OPD_TYPE_REG);
        set_arm_instr_opd_reg_str(instr, opd_idx, reg_str);

        idx = parse_scale(line, idx, &(instr->opd[opd_idx].content.reg.scale));
    } else if (fc == '[') {
        /* Memory Operand
           1. Read base register string, e.g., "reg0", "reg1".
           2. Read immediate value or index register string.
           3. Read Suffix, e.g., '!' for pre-indexing.*/
        char reg_str[20] = "\0";

        idx++; // skip '['
        while (line[idx] != ',' && line[idx] != ']' && line[idx] != '\n')
            strncat(reg_str, &line[idx++], 1);

        set_arm_instr_opd_type(instr, opd_idx, ARM_OPD_TYPE_MEM);
        set_arm_instr_opd_mem_base_str(instr, opd_idx, reg_str);

        // post-index
        if ((line[idx] == ']') && (line[idx+1] == ',')){
            set_arm_instr_opd_mem_index_type(instr, opd_idx, ARM_MEM_INDEX_TYPE_POST);
            idx++;
        }

        if (line[idx] == ',') {
            idx += 2;

            if (line[idx] == '#') { /* This is an immediate offset (#imm_xxx symbolic chars) */
                char off_str[10] = "\0";
                char tfc;

                idx++;
                tfc = line[idx];

                while (line[idx] != ',' && line[idx] != ']' && line[idx] != '\n')
                  strncat(off_str, &line[idx++], 1);

                if (tfc == 'i')
                  set_arm_opd_mem_off_str(opd, off_str);
                else
                  set_arm_opd_mem_off_val(opd, off_str);
            } else if (line[idx] == 'r') { /* This is an index register */
                char index_reg_str[20] = "\0";
                while (line[idx] != ',' && line[idx] != ']' && line[idx] != '\n')
                    strncat(index_reg_str, &line[idx++], 1);

                set_arm_instr_opd_mem_index_str(instr, opd_idx, index_reg_str);

                idx = parse_scale(line, idx, &(instr->opd[opd_idx].content.mem.scale));
            } else
                LogMan::Msg::IFmt( "Error in parsing memory operand.\n");
        }
        while (line[idx] != ']' && line[idx] != '\n')
            idx++;

        //pre-index
        if ((line[idx] == ']') && (line[idx+1] == '!')){
            set_arm_instr_opd_mem_index_type(instr, opd_idx, ARM_MEM_INDEX_TYPE_PRE);
            idx += 2;
        }
    } else {
        LogMan::Msg::EFmt("Error in NO.{} parsing {} operand: unknown operand type: {}.",
                                                index, get_arm_instr_opc(instr->opc), line[idx]);
        assert(0);
    }

    if (line[idx] == ',')
        return idx+2;
    else if (line[idx] == ']')
        return idx+1;
    else
        return idx;
}

/**
 * @brief 解析RISC-V指令的操作数
 *
 * 这个函数处理三种主要的操作数类型：立即数、寄存器和内存操作数。
 * 函数会根据操作数的类型设置相应的字段，并返回解析后的新索引位置。
 *
 * @param line 包含操作数的字符串
 * @param idx 当前解析的起始索引
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_idx 操作数在指令中的索引
 * @param index 指令在序列中的索引（用于错误报告）
 * @return int 解析后的字符串索引
 */
static int parse_rule_riscv_operand(char *line, int idx, RISCVInstruction *instr, int opd_idx, int index)
{
    RISCVOperand *opd = &instr->opd[opd_idx];
    char fc = line[idx];

    if (fc == '#' || fc == '%') {
        bool ispcrel_hi = false, ispcrel_lo = false;
        if (fc == '%') {
            idx++; // skip '%'
            char pcrel_str[20] = "\0";
            while (line[idx] != '(' && line[idx] != '\n')
                strncat(pcrel_str, &line[idx++], 1);

            if (!strcmp(pcrel_str, "pcrel_hi")) {
                ispcrel_hi = true;
            } else if (!strcmp(pcrel_str, "pcrel_lo")) {
                ispcrel_lo = true;
            }
            idx++; // skip '('
        }

        idx++; // skip '#'
        fc = line[idx];
        char imm_str[20] = "\0";

        while (line[idx] != '(' && line[idx] != ')' && line[idx] != ',' && line[idx] != '\n')
            strncat(imm_str, &line[idx++], 1);

        if (line[idx] == ',' || line[idx] == '\n'
           || (line[idx] == ')' && (line[idx+1] == ',' || line[idx+1] == '\n'))) {
            /* Immediate Operand
              1. Read immediate value, #XXX
              2. %pcrel_hi(#XXX) */
            set_riscv_opd_type(opd, RISCV_OPD_TYPE_IMM);

            if (fc == 'i' || fc == 'L')
                set_riscv_opd_imm_sym_str(opd, imm_str);
            else
                set_riscv_opd_imm_val_str(opd, imm_str);

            if (ispcrel_hi)
                set_riscv_opd_imm_pcrel_hi(opd);
            else if(ispcrel_lo)
                set_riscv_opd_imm_pcrel_lo(opd);

        } else if (line[idx] == '(' || (line[idx] == ')' && line[idx+1] == '(')) {
            /* Memory Operand
              1. Read base register string, e.g., "reg0", "reg1".
              2. Read immediate value string.
              3. %pcrel_hi(#XXX)(reg0). */
            set_riscv_instr_opd_type(instr, opd_idx, RISCV_OPD_TYPE_MEM);

            if (fc == 'i')
                set_riscv_opd_mem_off_str(opd, imm_str);
            else
                set_riscv_opd_mem_off_val(opd, imm_str);

            if (ispcrel_hi)
                set_riscv_opd_mem_off_pcrel_hi(opd);
            else if(ispcrel_lo)
                set_riscv_opd_mem_off_pcrel_lo(opd);

            if (ispcrel_hi || ispcrel_lo)
                idx++; // skip ')'

            char reg_str[20] = "\0";

            idx++; // skip '('
            while (line[idx] != ')' && line[idx] != '\n')
                strncat(reg_str, &line[idx++], 1);

            set_riscv_instr_opd_mem_base_str(instr, opd_idx, reg_str);
        }
    } else if (fc == 'r' || fc == 'x' || fc == 'a' || fc == 'f'
        || fc == 'v' || fc == 't' || fc == 's' || fc == '{') {
        /* Register Operand
           1. Read register string, e.g., "reg0", "reg1".*/
        char reg_str[20] = "\0";

        if (fc == '{')
          idx++;

        while (line[idx] != ',' && line[idx] != '\n')
            strncat(reg_str, &line[idx++], 1);

        set_riscv_instr_opd_type(instr, opd_idx, RISCV_OPD_TYPE_REG);
        set_riscv_instr_opd_reg_str(instr, opd_idx, reg_str);
    } else {
        LogMan::Msg::EFmt("Error in NO.{} parsing {} operand: unknown operand type: {}.",
                        index, get_riscv_instr_opc(instr->opc), line[idx]);
        exit(0);
    }

    if (line[idx] == ',')
        return idx+2;
    else if (line[idx] == ')')
        return idx+1;
    else
        return idx;
}

/**
 * @brief 调整特定的ARM指令(如 ASR, LSL, LSR)为等效的MOV指令
 *
 * @param instr 指向ARMInstruction结构的指针
 */
static void adjust_arm_instr(ARMInstruction *instr)
{
    if (instr->opc != ARM_OPC_ASR && instr->opc != ARM_OPC_LSL &&
        instr->opc != ARM_OPC_LSR)
        return;

    if (instr->opd[2].type == ARM_OPD_TYPE_IMM) { /* immediate shift */
        instr->opd[1].content.reg.scale.type = ARM_OPD_SCALE_TYPE_SHIFT;
        instr->opd[1].content.reg.scale.imm = instr->opd[2].content.imm;
        switch (instr->opc) {
            case ARM_OPC_ASR:
                instr->opc = ARM_OPC_MOV;
                instr->opd[1].content.reg.scale.content.direct = ARM_OPD_DIRECT_ASR;
                break;
            case ARM_OPC_LSL:
                instr->opc = ARM_OPC_MOV;
                instr->opd[1].content.reg.scale.content.direct = ARM_OPD_DIRECT_LSL;
                break;
            case ARM_OPC_LSR:
                instr->opc = ARM_OPC_MOV;
                instr->opd[1].content.reg.scale.content.direct = ARM_OPD_DIRECT_LSR;
                break;
            default:
                fprintf(stderr, "[ARM] error: unsupported opcode: %d.\n", instr->opc);
                exit(0);
        }
    } else {
        LogMan::Msg::IFmt( "[ARM] error: unsupported shift type.\n");
        exit(0);
    }
    set_arm_instr_opd_num(instr, 2);
}

/**
 * @brief 解析单条ARM指令
 *
 * @param line 包含指令的字符串
 * @param pc 指令的程序计数器值
 * @param index 指令在序列中的索引
 * @return ARMInstruction* 解析后的ARM指令结构体指针
 */
static ARMInstruction *parse_rule_arm_instruction(char *line, uint64_t pc, int index)
{
    ARMInstruction *instr = rule_arm_instr_alloc(pc);
    int opd_idx;
    int i;

    i = parse_rule_arm_opcode(line, instr);

    size_t len = strlen(line);

    opd_idx = 0;
    while (i < len && line[i] != '\n')
        i = parse_rule_arm_operand(line, i, instr, opd_idx++, index);

    set_arm_instr_opd_size(instr);
    set_arm_instr_opd_num(instr, opd_idx);

    /* adjust lsl, asr, and etc instructions to mov instructions with two operands */

    return instr;
}

/**
 * @brief 解析单条RISC-V指令
 *
 * @param line 包含指令的字符串
 * @param pc 指令的程序计数器值
 * @param index 指令在序列中的索引
 * @return RISCVInstruction* 解析后的RISC-V指令结构体指针
 */
static RISCVInstruction *parse_rule_riscv_instruction(char *line, uint64_t pc, int index)
{
    RISCVInstruction *instr = rule_riscv_instr_alloc(pc);
    int opd_idx;
    int i;

    i = parse_rule_riscv_opcode(line, instr);

    size_t len = strlen(line);

    opd_idx = 0;
    while (i < len && line[i] != '\n')
        i = parse_rule_riscv_operand(line, i, instr, opd_idx++, index);

    set_riscv_instr_opd_size(instr);
    set_riscv_instr_opd_num(instr, opd_idx);

    return instr;
}

/**
 * @brief 解析规则中的Host代码序列
 *
 * @param fp 指向规则文件的文件指针
 * @param rule 指向TranslationRule结构的指针
 * @return bool 解析是否成功
 */
bool ParseRuleHostCode(int arch, FILE *fp, TranslationRule *rule)
{
    uint64_t pc = 0;
    ARMInstruction *arm_code_head = NULL;
    ARMInstruction *arm_code_tail = NULL;
    RISCVInstruction *riscv_code_head = NULL;
    RISCVInstruction *riscv_code_tail = NULL;
    char line[500];
    bool ret = true;

    /* parse arm instructions in this rule */
    while(fgets(line, 500, fp)) {
        if (strstr(line, ".Guest:\n")) {
            fseek(fp, (0-strlen(line)), SEEK_CUR);
            break;
        }

        /* check if this line is a comment */
        char fs = line[0];
        if (fs == '#')
            continue;
        if (arch == 0) {
            ARMInstruction *cur = parse_rule_arm_instruction(line, pc, rule->index);
            if (!arm_code_head) {
                arm_code_head = arm_code_tail = cur;
            } else {
                arm_code_tail->next = cur;
                arm_code_tail = cur;
            }
        } else if (arch == 1) {
            RISCVInstruction *cur = parse_rule_riscv_instruction(line, pc, rule->index);
            if (!riscv_code_head) {
                riscv_code_head = riscv_code_tail = cur;
            } else {
                riscv_code_tail->next = cur;
                riscv_code_tail = cur;
            }
        }
        pc += 4; // 假设的值，实际应根据指令长度调整
    }

    if (arch == 0) {
        rule->arm_host = arm_code_head;
    } else if (arch == 1) {
        rule->riscv_host = riscv_code_head;
    }

    return ret;
}
