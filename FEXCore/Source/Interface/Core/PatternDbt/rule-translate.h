#ifndef RULE_TRANSLATE_H
#define RULE_TRANSLATE_H

#include "Interface/Core/Frontend.h"

#include <FEXCore/IR/IR.h>
#include <FEXCore/IR/IntrusiveIRList.h>
#include <FEXCore/IR/RegisterAllocationData.h>

#include "parse.h"

typedef struct ImmMapping {
    char imm_str[20];
    uint64_t imm_val;
    struct ImmMapping *next;
} ImmMapping;

typedef struct GuestRegisterMapping {
    X86Register sym;    /* symbolic register in a rule */
    X86Register num;    /* real register in guest instruction */
    uint32_t regsize;

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
    TranslationRule *rule;  /* Translation rule for this instruction sequence.
                               Only valid at the first instruction */
    int icount;             /* Number of guest instructions matched by this rule */
    uint64_t next_pc;       /* Guest pc after this rule record */
    bool update_cc;         /* If guest instructions in this rule update condition codes */
    bool save_cc;           /* If the condition code needs to be saved */
    ImmMapping *imm_map;
    GuestRegisterMapping *g_reg_map;
    LabelMapping *l_map;
    int para_opc[20];
} RuleRecord;

bool instr_is_match(uint64_t);
bool instrs_is_match(uint64_t);
bool tb_rule_matched(void);
RuleRecord *get_translation_rule(uint64_t);

bool check_translation_rule(uint64_t);
void remove_guest_instruction(FEXCore::Frontend::Decoder::DecodedBlocks *, uint64_t);

/* Try to match instructions in this tb to existing rules */
bool match_translation_rule(FEXCore::Frontend::Decoder::DecodedBlocks const *);
void do_rule_translation(RuleRecord *, uint32_t *);

void get_label_map(char *, uint64_t *, uint64_t *);
uint64_t get_imm_map(char *);
int32_t get_offset_map(char *);
ARMRegister get_guest_reg_map(ARMRegister& reg, uint32_t& regsize);
bool is_last_access(ARMInstruction *, ARMRegister);
#endif
