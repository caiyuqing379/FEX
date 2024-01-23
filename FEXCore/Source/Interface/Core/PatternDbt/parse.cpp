#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <assert.h>
#include <cstdlib>

#include "arm-parse.h"
#include "x86-parse.h"

#include "parse.h"

#define RULE_BUF_LEN 10000

const X86Opcode opc_set[] = {
    [X86_OPC_MOVB]   = X86_OPC_OP4,
    [X86_OPC_MOVZBL] = X86_OPC_OP3,
    [X86_OPC_MOVSBL] = X86_OPC_OP3,
    [X86_OPC_MOVW]   = X86_OPC_OP4,
    [X86_OPC_MOVZWL] = X86_OPC_OP3,
    [X86_OPC_MOVSWL] = X86_OPC_OP3,
    [X86_OPC_MOVL]   = X86_OPC_OP4,
    [X86_OPC_LEAL]   = X86_OPC_OP4,
    [X86_OPC_NOTL]   = X86_OPC_INVALID,
    [X86_OPC_ANDB]   = X86_OPC_INVALID,
    [X86_OPC_ORB]    = X86_OPC_INVALID,
    [X86_OPC_XORB]   = X86_OPC_INVALID,
    [X86_OPC_ANDW]   = X86_OPC_INVALID,
    [X86_OPC_ORW]    = X86_OPC_INVALID,
    [X86_OPC_ANDL]   = X86_OPC_OP5,
    [X86_OPC_ORL]    = X86_OPC_OP5,
    [X86_OPC_XORL]   = X86_OPC_OP5,
    [X86_OPC_NEGL]   = X86_OPC_INVALID,
    [X86_OPC_INCB]   = X86_OPC_INVALID,
    [X86_OPC_INCL]   = X86_OPC_INVALID,
    [X86_OPC_DECB]   = X86_OPC_INVALID,
    [X86_OPC_DECW]   = X86_OPC_INVALID,
    [X86_OPC_INCW]   = X86_OPC_INVALID,
    [X86_OPC_DECL]   = X86_OPC_INVALID,

    [X86_OPC_ADDB]   = X86_OPC_INVALID,
    [X86_OPC_ADDW]   = X86_OPC_INVALID,
    [X86_OPC_ADDL]   = X86_OPC_OP5,
    [X86_OPC_ADCL]   = X86_OPC_OP8,
    [X86_OPC_SUBL]   = X86_OPC_OP7,
    [X86_OPC_SBBL]   = X86_OPC_OP9,

    [X86_OPC_IMULL]  = X86_OPC_OP1,

    [X86_OPC_SHLB]   = X86_OPC_INVALID,
    [X86_OPC_SHRB]   = X86_OPC_INVALID,
    [X86_OPC_SHLW]   = X86_OPC_INVALID,
    [X86_OPC_SHLL]   = X86_OPC_INVALID,
    [X86_OPC_SHRL]   = X86_OPC_INVALID,
    [X86_OPC_SARL]   = X86_OPC_INVALID,
    [X86_OPC_SHLDL]  = X86_OPC_INVALID,
    [X86_OPC_SHRDL]  = X86_OPC_INVALID,

    [X86_OPC_BTL]    = X86_OPC_INVALID,
    [X86_OPC_TESTW]  = X86_OPC_INVALID,
    [X86_OPC_TESTB]  = X86_OPC_INVALID,
    [X86_OPC_CMPB]   = X86_OPC_INVALID,
    [X86_OPC_CMPW]   = X86_OPC_INVALID,
    [X86_OPC_TESTL]  = X86_OPC_INVALID,
    [X86_OPC_CMPL]   = X86_OPC_INVALID,

    [X86_OPC_CMOVNEL]= X86_OPC_INVALID,
    [X86_OPC_CMOVAL] = X86_OPC_INVALID,
    [X86_OPC_CMOVBL] = X86_OPC_INVALID,
    [X86_OPC_CMOVLL] = X86_OPC_INVALID,

    [X86_OPC_SETE]   = X86_OPC_INVALID,
    [X86_OPC_CWT]    = X86_OPC_INVALID,

    [X86_OPC_JMP]    = X86_OPC_INVALID,
    [X86_OPC_JA]     = X86_OPC_INVALID,
    [X86_OPC_JAE]    = X86_OPC_INVALID,
    [X86_OPC_JB]     = X86_OPC_INVALID,
    [X86_OPC_JBE]    = X86_OPC_INVALID,
    [X86_OPC_JL]     = X86_OPC_INVALID,
    [X86_OPC_JLE]    = X86_OPC_INVALID,
    [X86_OPC_JG]     = X86_OPC_INVALID,
    [X86_OPC_JGE]    = X86_OPC_INVALID,
    [X86_OPC_JE]     = X86_OPC_INVALID,
    [X86_OPC_JNE]    = X86_OPC_INVALID,
    [X86_OPC_JS]     = X86_OPC_INVALID,
    [X86_OPC_JNS]    = X86_OPC_INVALID,

    [X86_OPC_PUSH]   = X86_OPC_INVALID,
    [X86_OPC_POP]    = X86_OPC_INVALID,
    [X86_OPC_CALL]   = X86_OPC_INVALID,
    [X86_OPC_RET]    = X86_OPC_INVALID,
    [X86_OPC_SET_LABEL] = X86_OPC_INVALID, // fake instruction to generate label
    [X86_OPC_SYNC_M] = X86_OPC_INVALID,
    [X86_OPC_SYNC_R] = X86_OPC_INVALID,
	[X86_OPC_PC_IR]  = X86_OPC_INVALID,
    [X86_OPC_PC_RR]  = X86_OPC_INVALID,
	[X86_OPC_MULL]   = X86_OPC_INVALID,

    //parameterized opcode
    [X86_OPC_OP1]    = X86_OPC_OP1,
    [X86_OPC_OP2]    = X86_OPC_OP2,
    [X86_OPC_OP3]    = X86_OPC_OP3,
    [X86_OPC_OP4]    = X86_OPC_OP4,
    [X86_OPC_OP5]    = X86_OPC_OP5,
    [X86_OPC_OP6]    = X86_OPC_OP6,
    [X86_OPC_OP7]    = X86_OPC_OP7,
    [X86_OPC_OP8]    = X86_OPC_OP8,
    [X86_OPC_OP9]    = X86_OPC_OP9,
    [X86_OPC_OP10]   = X86_OPC_OP10,
    [X86_OPC_OP11]   = X86_OPC_OP11,
    [X86_OPC_OP12]   = X86_OPC_OP12,
};

static const int cache_index[] = {2483,
896,
2,
7,
121,
252,
2484,
37,
2482,
138,
446,
101,
2485,
176,
111,
46,
79,
23,
876,
189,
44,
88,
5,
212,
437,
339,
51,
1873,
218,
58,
299,
39,
675,
1026,
2349,
59,
753,
2216,
611,
64,
820,
2492,
300,
317,
1659,
794,
1237,
440,
206,
720,
1647,
9,
549,
2079,
1089,
33,
940,
167,
78,
2488,
328,
2490,
22,
170,
186,
1950,
11,
585,
24,
1401,
2295,
12,
191,
1239,
183,
482,
201,
655,
2486,
2375,
2491,
226,
2449,
840,
102,
2487,
844,
1336,
68,
53,
1875,
462,
2204};

static TranslationRule *rule_buf;
static int rule_buf_index;

int cache_counter = 0;

TranslationRule *rule_table[MAX_GUEST_LEN] = {NULL};
TranslationRule *cache_rule_table[MAX_GUEST_LEN] = {NULL};

static void rule_buf_init(void)
{
    rule_buf = new TranslationRule[RULE_BUF_LEN];
    if (rule_buf == NULL)
        LogMan::Msg::IFmt( "Cannot allocate memory for rule_buf!\n");

    rule_buf_index = 0;
}

static TranslationRule *rule_alloc(void)
{
    TranslationRule *rule = &rule_buf[rule_buf_index++];
    int i;

    if (rule_buf_index >= RULE_BUF_LEN)
        LogMan::Msg::IFmt( "Error: rule_buf is not enough!\n");

    rule->index = 0;
    rule->arm_host = NULL;
    rule->x86_guest = NULL;
    rule->guest_instr_num = 0;
    rule->next = NULL;
    rule->prev = NULL;
    rule->intermediate_regs=NULL;
    #ifdef PROFILE_RULE_TRANSLATION
    rule->hit_num = 0;
    rule->print_flag = 0;
    #endif

    for (i = 0; i < X86_CC_NUM; i++)
        rule->x86_cc_mapping[i] = 0;

    rule->match_counter = 0;

    return rule;
}

static void init_buf(void)
{
    rule_arm_instr_buf_init();
    rule_x86_instr_buf_init();

    rule_buf_init();
}

static void install_rule(TranslationRule *rule)
{
    int index = rule_hash_key(rule->x86_guest, rule->guest_instr_num);


    // LogMan::Msg::IFmt( "{}\n", index);
    assert(index < MAX_GUEST_LEN);

    int i;
    for (i = 0; i < 93; i++){
        if (cache_index[i] == rule->index){
            ++cache_counter;
            rule->next = cache_rule_table[index];
            cache_rule_table[index] = rule;
            return;
        }
    }

    rule->next = rule_table[index];
    if (rule_table[index])
        rule_table[index]->prev = rule;
    rule_table[index] = rule;

}

int rule_hash_key(X86Instruction *x86_insn, int num)
{
    X86Instruction *p_x86_insn = x86_insn;
    int sum = 0, cnt = 0;

    while(p_x86_insn) {
        sum += opc_set[p_x86_insn->opc];
        p_x86_insn = p_x86_insn->next;
        cnt++;
    }

    if(cnt < num)
      LogMan::Msg::IFmt( "num: {} < cnt:{}, X86 inst num error!\n", num, cnt);

    return (sum/num);
}

TranslationRule *get_rule(void)
{
    return &rule_buf[0];
}

void parse_translation_rules(void)
{
    const char rule_file[] = "/home/ubuntu/BINTRANS/rules4all";
    TranslationRule *rule = NULL;
    int counter = 0;
    int install_counter = 0;
    int i;
    char line[500];
    char *substr;
    FILE *fp;

    /* 1. init environment */
    init_buf();

    LogMan::Msg::IFmt( "== Loading translation rules from {}...\n", rule_file);
    /* 2. open the rule file and parse it */
    fp = fopen(rule_file, "r");
    if (fp == NULL) {
        LogMan::Msg::IFmt( "== No translation rule file found.\n");
        return;
    }

    while(!feof(fp)) {
        if(fgets(line, 500, fp) == NULL)
            break;

        /* check if this line is a comment */
        char fs = line[0];
        if (fs == '#' || fs == '\n')
            continue;

        if ((substr = strstr(line, ".Guest:\n")) != NULL) {
            char idx[20] = "\0";

            rule = rule_alloc();
            counter++;

            /* get the index of this rule */
            strncpy(idx, line, strlen(line) - strlen(substr));
            rule->index = atoi(idx);

            // LogMan::Msg::IFmt( "===== Parsing guest with index: {}, idx: {}=====\n", rule->index, idx);
            parse_rule_x86_code(fp, rule);

        } else if (strstr(line, ".Host:\n")) {
            // LogMan::Msg::IFmt( "===== Parsing host =====\n");
            if (parse_rule_arm_code(fp, rule)) {

                /* install this rule to the hash table*/
                install_rule(rule);

                install_counter++;
            }
            //LogMan::Msg::IFmt( "===== Finish parsing host =====\n");
        } else
            LogMan::Msg::IFmt( "Error in parsing rule file: {}.\n", line);
    }

    LogMan::Msg::IFmt( "== Ready: {} translation rules loaded, {} installed, {} cached.\n\n", counter, install_counter, cache_counter);
    for (i = 0; i < MAX_GUEST_LEN;i++){
        if (cache_rule_table[i]){
            TranslationRule *temp = cache_rule_table[i];
            while(temp->next){
                temp = temp->next;
            }
            temp->next = rule_table[i];
        }
        else
        {
            cache_rule_table[i] = rule_table[i];
        }

    }
}

#ifdef PROFILE_RULE_TRANSLATION
void print_rule_hit_num(void );
void print_rule_hit_num(void)
{
    TranslationRule *cur_max;
    int zero_counter = 0;
    int counter[5] = {0};
    int i;

    for (i = 0; i < MAX_GUEST_LEN; i++) {
        TranslationRule *cur_rule = rule_table[i];
        while(cur_rule) {
            if (cur_rule->hit_num == 0)
                zero_counter++;
            cur_rule = cur_rule->next;
        }
    }

    LogMan::Msg::IFmt( "Rule hit information: {} rules has zero hit.\n", zero_counter);
    LogMan::Msg::IFmt( "Index  #Guest  #Hit\n");
    while(1) {
        cur_max = NULL;
        for (i = 0; i < MAX_GUEST_LEN; i++) {
            TranslationRule *cur_rule = rule_table[i];
            while(cur_rule) {
                if (cur_rule->print_flag == 0 &&
                    ((cur_max != NULL && cur_rule->hit_num > cur_max->hit_num) ||
                     (cur_max == NULL && cur_rule->hit_num > 0)))
                    cur_max = cur_rule;
                cur_rule = cur_rule->next;
            }
        }
        if (cur_max) {
            LogMan::Msg::IFmt( "  {}\t{}\t%llu\n",
                    cur_max->index, cur_max->guest_instr_num, cur_max->hit_num);
            cur_max->print_flag = 1;
            if (cur_max->guest_instr_num > 4)
                counter[4]++;
            else
                counter[cur_max->guest_instr_num-1]++;
        }
        else
            break;
    }
    LogMan::Msg::IFmt( "\n#Guest    #RuleCounter\n");
    for (i = 0; i < 5; i++)
        if (i == 4)
            LogMan::Msg::IFmt( " >4           {}\n", counter[i]);
        else
            LogMan::Msg::IFmt( "  {}           {}\n", i+1, counter[i]);
}
#endif
