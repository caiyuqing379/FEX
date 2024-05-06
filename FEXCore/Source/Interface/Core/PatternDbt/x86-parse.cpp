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

/* If x86 instruction has temp register, currently not supported */
static bool has_temp_register = false;

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

    // LogMan::Msg::IFmt( "**** Guest {} ****", rule->index);
    // print_x86_instr_seq(code_head);

    rule->x86_guest = code_head;
}
