#ifndef PARSE_H
#define PARSE_H

#include "arm-instr.h"
#include "x86-instr.h"

#define X86_CC_NUM 4

#define X86_OF 0
#define X86_SF 1
#define X86_CF 2
#define X86_ZF 3

#define MAX_GUEST_LEN 500

extern const X86Opcode opc_set[];

typedef struct TranslationRule {
    int index;                  /* index of this rule */

    ARMInstruction *arm_host;  /* guest code */
    X86Instruction *x86_guest;   /* host code */

    uint32_t guest_instr_num;    /* number of guest instructions */
    struct TranslationRule *next;   /* next rule in this hash entry */
    struct TranslationRule *prev;   /* previous rule in this hash entry */
    char *intermediate_regs;
    /* ARM_VF, ARM_NF, ARM_CF, ARM_ZF */
    int x86_cc_mapping[X86_CC_NUM]; /* Mapping between arm condition code and x86 condition code
                              0: arm cc is not defined
                              1: arm cc is emulated by the corresponding x86 cc
                                 ARM_VF -> x86_OF, ARM_NF -> x86_SF, ARM_CF -> x86_CF, ARM_ZF -> x86_ZF
                              2: arm cc is emulated by the negation of the corresponding x86 cc
                                 ARM_CF -> !x86_CF */

    #ifdef PROFILE_RULE_TRANSLATION
    uint64_t hit_num;   /* number of hit to this rule (static) */
    int print_flag;     /* flag used to print hit number */
    #endif

    int match_counter;  /*counter the match times for each rule
                        used in rule ordering */

} TranslationRule;

int rule_hash_key(X86Instruction *, int);

TranslationRule *get_rule(void);
void parse_translation_rules(void);

extern TranslationRule *rule_table[];
extern TranslationRule *cache_rule_table[];

#endif
