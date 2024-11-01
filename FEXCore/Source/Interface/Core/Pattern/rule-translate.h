#ifndef RULE_TRANSLATE_H
#define RULE_TRANSLATE_H

#include "arm-instr.h"
#include "x86-instr.h"
#include "RiscvInst.h"

#define PROFILE_RULE_TRANSLATION

#define X86_CC_NUM 4

#define X86_OF 0
#define X86_SF 1
#define X86_CF 2
#define X86_ZF 3

#define MAX_GUEST_LEN 500

typedef struct TranslationRule {
    int index;                   /* index of this rule */

    ARMInstruction *arm_host;    /* host code */
    RISCVInstruction *riscv_host;
    X86Instruction *x86_guest;   /* guest code */

    uint32_t guest_instr_num;       /* number of guest instructions */
    struct TranslationRule *next;   /* next rule in this hash entry */
    struct TranslationRule *prev;   /* previous rule in this hash entry */
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

    int match_counter;  /* counter the match times for each rule
                           used in rule ordering */

} TranslationRule;

typedef struct ImmMapping {
    char imm_str[20];
    uint64_t imm_val;
    struct ImmMapping *next;
} ImmMapping;

typedef struct GuestRegisterMapping {
    X86Register sym;    /* symbolic register in a rule */
    X86Register num;    /* real register in guest instruction */
    uint32_t regsize;
    bool HighBits;

    struct GuestRegisterMapping *next;
} GuestRegisterMapping;

typedef struct LabelMapping {
    char lab_str[20];
    uint64_t target;
    uint64_t fallthrough;

    struct LabelMapping *next;
} LabelMapping;

typedef struct {
    uint64_t pc;            /* Simulated guest pc */
    uint64_t target_pc;     /* Branch target pc.
                               Only valid if the last instruction of current tb is not branch
                               and covered by this rule */
    X86Instruction *last_guest;   /* last guest instr */
    TranslationRule *rule;  /* Translation rule for this instruction sequence.
                               Only valid at the first instruction */
    bool update_cc;         /* If guest instructions in this rule update condition codes */
    bool save_cc;           /* If the condition code needs to be saved */
    ImmMapping *imm_map;
    GuestRegisterMapping *g_reg_map;
    LabelMapping *l_map;
    int para_opc[20];
} RuleRecord;

int rule_hash_key(X86Instruction *, int);

TranslationRule *get_rule(void);
void ParseTranslationRules(int arch, uint64_t pid);

extern TranslationRule *rule_table[];
extern TranslationRule *cache_rule_table[];

bool is_last_access(ARMInstruction *, ARMRegister);
#endif
