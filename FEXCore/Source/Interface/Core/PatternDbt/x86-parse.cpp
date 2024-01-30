#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctype.h>

#include "x86-instr.h"
#include "x86-parse.h"

#define RULE_X86_INSTR_BUF_LEN 1000000

static X86Instruction *rule_x86_instr_buf;
static int rule_x86_instr_buf_index;

void rule_x86_instr_buf_init(void)
{
    rule_x86_instr_buf = new X86Instruction[RULE_X86_INSTR_BUF_LEN];
    if (rule_x86_instr_buf == NULL)
        LogMan::Msg::IFmt( "Cannot allocate memory for rule_x86_instr_buf!\n");

    rule_x86_instr_buf_index = 0;
}

static X86Instruction *rule_x86_instr_alloc(uint64_t pc)
{
    X86Instruction *instr = &rule_x86_instr_buf[rule_x86_instr_buf_index++];
    if (rule_x86_instr_buf_index >= RULE_X86_INSTR_BUF_LEN)
        LogMan::Msg::IFmt( "Error: rule_x86_instr_buf is not enough!\n");

    instr->pc = pc;
    instr->next = NULL;
    return instr;
}

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

/* If x86 instruction has temp register, currently not supported */
static bool has_temp_register = false;

static int parse_rule_x86_operand(char *line, int idx, X86Instruction *instr, int opd_idx)
{
    X86Operand *opd = &instr->opd[opd_idx];
    char fc = line[idx];

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
            set_x86_opd_imm_sym_str(opd, imm_str);
        else
            set_x86_opd_imm_val_str(opd, imm_str);

        if (line[idx] == ':')
            idx++; // skip ':'
    } else if (fc == 'r' || fc == 't') {
        /* Register operand */
        char reg_str[20] = "\0";

        if (fc == 't')
            has_temp_register = true;

        while (line[idx] != ',' && line[idx] != '\n')
            strncat(reg_str, &line[idx++], 1);

        set_x86_opd_type(opd, X86_OPD_TYPE_REG);
        set_x86_opd_reg_str(opd, reg_str);
    } else if (fc == 'i' || fc == '(' || fc == '-' || isdigit(fc)) {
        /* Memory operand with or without offset (imm_XXX) */
        char reg_str[10] = "\0";

        set_x86_opd_type(opd, X86_OPD_TYPE_MEM);
        if (fc == 'i' || fc == '-' || isdigit(fc) ||
            (fc == '(' && line[idx+1] != 'r' && line[idx+1] != 't')) { // parse offset
            char off_str[20] = "\0";
            if (fc == '(')
                idx++;
            while(line[idx] != '(' && line[idx] != ')')
                strncat(off_str, &line[idx++], 1);
            if (line[idx] == ')')
                idx++;
            set_x86_opd_mem_off_str(opd, off_str);
        }
        idx++; // skip '('
        if (line[idx] == 't')
            has_temp_register = true;
        while (line[idx] != ',' && line[idx] != ')' && line[idx] != '\n')
            strncat(reg_str, &line[idx++], 1);
        set_x86_opd_mem_base_str(opd, reg_str);
        if (line[idx] == ',') {// have index register
            char index_str[10] = "\0";
            idx += 2; // skip , and space
            if (line[idx] == 't')
                has_temp_register = true;
            while(line[idx] != ',' && line[idx] != ')' && line[idx] != '\n')
                strncat(index_str, &line[idx++], 1);
            set_x86_opd_mem_index_str(opd, index_str);
        }
        if (line[idx] == ',') { // have scale value
            char scale_str[20] = "\0";
            idx += 2; // skip , and space
            if (line[idx] == '(') // this is an expression
                idx ++;
            while(line[idx] != ',' && line[idx] != ')' && line[idx] != '\n')
                strncat(scale_str, &line[idx++], 1);
            set_x86_opd_mem_scale_str(opd, scale_str);
        }
        while (line[idx] == ')')
            idx++;
    } else
        fprintf(stderr, "Error in parsing x86 operand: unknown operand type at idx %d char %c in line: %s", idx, line[idx], line);

    if (line[idx] == ',')
        return idx+2;
    else
        return idx;
}

static X86Instruction *parse_rule_x86_instruction(char *line, uint64_t pc)
{
    X86Instruction *instr = rule_x86_instr_alloc(pc);
    int opd_idx;
    int i;

    // LogMan::Msg::IFmt( "============== parsing x86 line: {}\n", line);
    i = parse_rule_x86_opcode(line, instr);

    opd_idx = 0;
    while(line[i] != '\n')
        i = parse_rule_x86_operand(line, i, instr, opd_idx++);

    set_x86_instr_opd_num(instr, opd_idx);

    return instr;
}

void parse_rule_x86_code(FILE *fp, TranslationRule *rule)
{
    uint64_t pc = 0;
    X86Instruction *code_head = NULL;
    X86Instruction *code_tail = NULL;
    char line[500];
    bool ret = false;

    has_temp_register = false;

    /* parse x86 (host) instruction in this rule */
    while(fgets(line, 500, fp)) {
        if (strstr(line, ".Host:\n")) {
            fseek(fp, (0-strlen(line)), SEEK_CUR);
            break;
        }
        // if (strstr(line, "Context")) {
        //     if(fgets(line, 500, fp)) {
        //         char inter_regs[50] = "";
        //         rule->intermediate_regs = strcpy(inter_regs, line);
        //         ret = true;
        //         // LogMan::Msg::IFmt( "{}\n", rule->intermediate_regs);
        //         continue;
        //     }
        // }

        // if (!strcmp(line, "\n"))
        //     break;

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
        pc += 4;    // fake value
        rule->guest_instr_num++;
    }

    if (has_temp_register)
        ret = false;


    LogMan::Msg::IFmt( "\n**** Guest {} ****\n", rule->index);
    print_x86_instr_seq(code_head);

    rule->x86_guest = code_head;
}
