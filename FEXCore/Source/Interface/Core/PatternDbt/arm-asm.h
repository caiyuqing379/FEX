#ifndef ARM_ASM_H
#define ARM_ASM_H

#include "Interface/Core/Frontend.h"
#include "Interface/Core/PatternDbt/arm-instr.h"
#include "Interface/Core/PatternDbt/rule-translate.h"

void reset_asm_buffer(void);

void assemble_arm_instruction(FEXCore::Frontend::Decoder::DecodedBlocks const *, ARMInstruction *,
                              uint32_t *, RuleRecord *);

#endif
