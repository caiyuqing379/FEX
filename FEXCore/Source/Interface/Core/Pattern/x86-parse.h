#ifndef X86_PARSE_H
#define X86_PARSE_H

#include "rule-translate.h"

void rule_x86_instr_buf_init(void);
void parse_rule_x86_code(FILE *fp, TranslationRule *rule);

#endif
