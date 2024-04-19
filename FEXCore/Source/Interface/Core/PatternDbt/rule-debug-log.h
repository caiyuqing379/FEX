#ifndef RULE_DEBUG_LOG_H
#define RULE_DEBUG_LOG_H

#include <string.h>
#include "parse.h"

// #define DEBUG_RULE_LOG

void ofstream_x86_instr(X86Instruction *instr_seq);
void ofstream_rule_arm_instr(X86Instruction *instr_seq);

void ofstream_x86_instr2(X86Instruction *instr_seq);
void ofstream_rule_arm_instr2(X86Instruction *instr_seq);

void output_x86_instr(X86Instruction *instr);
std::string intToHex(uint64_t value);
void writeToLogFile(const std::string& filename, const std::string& logContent);

#endif
