#pragma once
#include <cstdint>

namespace FEXCore::X86Tables {
struct DecodedInst;
}

struct X86Instruction;

struct DecodedBlocks final {
  uint64_t Entry{};
  uint32_t Size{};
  uint32_t NumInstructions{};
  uint64_t ImplicitLinkTarget{};
  FEXCore::X86Tables::DecodedInst *DecodedInstructions;
  bool HasInvalidInstruction{};
  X86Instruction *guest_instr;
};