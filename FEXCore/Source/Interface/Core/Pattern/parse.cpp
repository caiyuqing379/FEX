#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <assert.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "RuleDebug.h"
#include "HostParse.h"
#include "x86-parse.h"
#include "rule-translate.h"

#define RULE_BUF_LEN 10000

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
    rule->riscv_host = NULL;
    rule->x86_guest = NULL;
    rule->guest_instr_num = 0;
    rule->next = NULL;
    rule->prev = NULL;
    #ifdef PROFILE_RULE_TRANSLATION
    rule->hit_num = 0;
    rule->print_flag = 0;
    #endif

    for (i = 0; i < X86_CC_NUM; i++)
        rule->x86_cc_mapping[i] = 1;

    rule->match_counter = 0;

    return rule;
}

static void init_buf(void)
{
    RuleArmInstrBufInit();
    RuleRiscvInstrBufInit();
    rule_x86_instr_buf_init();

    rule_buf_init();
}

static void install_rule(TranslationRule *rule)
{
    int index = rule_hash_key(rule->x86_guest, rule->guest_instr_num);

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

int ruleindex = 0;
static void install_rule2(TranslationRule *rule)
{
    assert(ruleindex < MAX_GUEST_LEN);
    if (ruleindex) {
      rule->prev = rule_table[ruleindex-1];
      rule_table[ruleindex-1]->next = rule;
    }
    rule_table[ruleindex] = rule;
    rule->next = nullptr;
    ruleindex++;
}

int rule_hash_key(X86Instruction *x86_insn, int num)
{
    X86Instruction *p_x86_insn = x86_insn;
    int sum = 0, cnt = 0;

    while(p_x86_insn) {
        sum += p_x86_insn->opc;
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

static void flush_file(uint64_t pid)
{
    std::filesystem::path homeDir = std::filesystem::path(getenv("HOME"));
    std::filesystem::path combinedPath = homeDir / (std::to_string(pid) + "fex-asm.log");
    std::string inputFile = combinedPath.string();
    std::ofstream file(inputFile, std::ios::trunc);
    if (!file.is_open()) {
      LogMan::Msg::EFmt("Failed to open file!");
      exit(0);
    }
    file.close();

    std::filesystem::path combinedPath2 = homeDir / (std::to_string(pid) + "fex-debug.log");
    std::string inputFile2 = combinedPath2.string();
    std::ofstream file2(inputFile2, std::ios::trunc);
    if (!file2.is_open()) {
      LogMan::Msg::EFmt("Failed to open file!");
      exit(0);
    }
    file2.close();
}

void ParseTranslationRules(int arch, uint64_t pid)
{
    std::filesystem::path homeDir = std::filesystem::path(getenv("HOME"));
    std::filesystem::path combinedPath = homeDir / "rules4all";
    const char* rule_file = combinedPath.c_str();
    TranslationRule *rule = NULL;
    int counter = 0;
    int install_counter = 0;
    int i;
    char line[500];
    char *substr;
    FILE *fp;

    /* 1. init environment */
    init_buf();
    #ifdef DEBUG_RULE_LOG
      flush_file(pid);
    #endif

    LogMan::Msg::IFmt("== Loading translation rules from {}...\n", rule_file);
    /* 2. open the rule file and parse it */
    fp = fopen(rule_file, "r");
    if (fp == NULL) {
        LogMan::Msg::IFmt("== No translation rule file found.\n");
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
            const char *position = strstr(line, substr);
            int index = position - line;
            strncpy(idx, line, index);
            rule->index = atoi(idx);

            parse_rule_x86_code(fp, rule);

        } else if ((substr = strstr(line, ".Host:\n")) != NULL) {
            if (ParseRuleHostCode(arch, fp, rule)) {

                /* install this rule to the hash table*/
                install_rule(rule);

                install_counter++;
            }
        } else {
            LogMan::Msg::EFmt("Error in parsing rule file: {}.\n", line);
            assert(0);
            break;
        }
    }

    LogMan::Msg::IFmt("== Ready: {} translation rules loaded, {} installed, {} cached.\n\n",
                                                            counter, install_counter, cache_counter);
    for (i = 0; i < MAX_GUEST_LEN;i++){
        if (cache_rule_table[i]){
            TranslationRule *temp = cache_rule_table[i];
            while(temp->next) {
                temp = temp->next;
            }
            temp->next = rule_table[i];
        } else {
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

    LogMan::Msg::IFmt("Rule hit information: {} rules has zero hit.", zero_counter);
    LogMan::Msg::IFmt("Index  #Guest  #Hit");
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
            LogMan::Msg::IFmt( "  {}\t{}\t%llu",
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
    LogMan::Msg::IFmt("#Guest    #RuleCounter");
    for (i = 0; i < 5; i++)
        if (i == 4)
            LogMan::Msg::IFmt( " >4           {}", counter[i]);
        else
            LogMan::Msg::IFmt( "  {}           {}", i+1, counter[i]);
}
#endif
