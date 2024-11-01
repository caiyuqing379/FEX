#ifndef RULE_DEBUG_LOG_H
#define RULE_DEBUG_LOG_H

#include <string.h>
#include "rule-translate.h"

// #define DEBUG_RULE_LOG

void ofstream_x86_instr(X86Instruction *instr_seq, uint64_t pid);
void ofstream_rule_arm_instr(X86Instruction *instr_seq, uint64_t pid);

void ofstream_x86_instr2(X86Instruction *instr_seq, uint64_t pid);
void ofstream_rule_arm_instr2(X86Instruction *instr_seq, uint64_t pid);

void output_x86_instr(X86Instruction *instr, uint64_t pid);
std::string intToHex(uint64_t value);
void writeToLogFile(const std::string& filename, const std::string& logContent);

#endif
