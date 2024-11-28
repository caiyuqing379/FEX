#pragma once
#include "FEXCore/fextl/memory.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Emitter.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Registers.h"
#include "Interface/Core/Pattern/HostParse.h"
#include "Interface/Core/Pattern/arm-instr.h"
#include <FEXCore/Utils/LogManager.h>

#include <biscuit/assembler.hpp>
#include <cstdint>
#include <utility>
#include <vector>

namespace FEXCore::Context {
class ContextImpl;
}

namespace FEXCore::Core {
struct InternalThreadState;
}

namespace FEXCore::X86Tables {
struct DecodedInst;
}

namespace FEXCore::Pattern {
enum ARCH {
  ARM64 = 0,
  RV64 = 1,
};

enum X86_GPR : uint8_t {
  RAX = 0,
  RCX = 1,
  RDX = 2,
  RBX = 3,
  RSP = 4,
  RBP = 5,
  RSI = 6,
  RDI = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  R13 = 13,
  R14 = 14,
  R15 = 15,
  RIP = 16,
  STATE = 17,
};

enum X86_XMM : uint8_t {
  XMM0 = 0,
  XMM1 = 1,
  XMM2 = 2,
  XMM3 = 3,
  XMM4 = 4,
  XMM5 = 5,
  XMM6 = 6,
  XMM7 = 7,
  XMM8 = 8,
  XMM9 = 9,
  XMM10 = 10,
  XMM11 = 11,
  XMM12 = 12,
  XMM13 = 13,
  XMM14 = 14,
  XMM15 = 15,
};

class PatternMatcher {
public:
  // Arch: 指定Host架构
  // GPRMappedIdx： 指定RAX~R15,RIP 映射到Host的寄存器
  // GPRTempIdx: 指定生成Host指令时可以使用的临时通用寄存器
  // XMMMappedIdx: 指定XMM0～XMM15 映射到Host的向量寄存器
  // XMMTempIdx: 指定生成Host指令时可以使用的临时向量寄存器
  PatternMatcher(ARCH Arch, FEXCore::Context::ContextImpl *Ctx,
                 FEXCore::Core::InternalThreadState *Thread,
                 const std::vector<int> &GPRMappedIdx,
                 const std::vector<int> &GPRTempIdx,
                 const std::vector<int> &XMMMappedIdx,
                 const std::vector<int> &XMMTempIdx);

  // 准备工作，用作进程初始化时进行规则的解析
  static void Prepare(ARCH arch);

  // 基本块匹配函数，Block为传入的待匹配的X86指令基本块
  // 返回匹配成功与否
  /* 传入的基本块结构定义如下，X86Instrunction为PatternDBT自定义，其结构需要由头文件导出
    struct DecodedBlocks final {
      uint64_t Entry{};
      uint32_t Size{};
      uint32_t NumInstructions{};
      uint64_t ImplicitLinkTarget{};
      FEXCore::X86Tables::DecodedInst *DecodedInstructions;
      bool HasInvalidInstruction{};
      X86Instruction *guest_instr;
  };
*/
  [[nodiscard]] bool MatchBlock(const void *tb);

  // 指定EmiCode释放代码的位置
  void SetCodeBuffer(uint8_t *Buffer, size_t Size);

  // 设置规则批生成指令的序言代码，添加到开头
  void SetPrologue(uint8_t *Code, size_t Size);
  // 设置规则批生成指令的尾声代码，添加到结尾ret之前
  void SetEpilogue(uint8_t *Code, size_t Size);

  // 根据规则，生成匹配的基本块对应的Host指令
  // 另外：每个块结束时，将Guest对应的目标地址（跳转或者函数调用）写入RIP对应的Host的寄存器，
  // 不尝试链接其他块，因为外部不会传入其他翻译的块
  // 并以ret指令返回到Dispatcher，作为结束指令
  size_t EmitCode();
  // 查询基本块匹配时的规则index
  int GetRuleIndex(uint64_t pc);

  void SetBlockCnt(size_t Cnt) {}
  void SetInstCnt(size_t Cnt) {}

  friend class biscuit::Assembler;
  friend class FEXCore::ARMEmitter::Emitter;

private:
  inline void reset_buffer(void);
  inline void save_map_buf_index(void);
  inline void recover_map_buf_index(void);
  inline void init_map_ptr(void);

  inline void add_rule_record(TranslationRule *rule, uint64_t pc, uint64_t t_pc, size_t blocksize,
                              X86Instruction *last_guest, bool update_cc,
                              bool save_cc, int pa_opc[20]);
  inline void add_matched_pc(uint64_t pc);
  inline void add_matched_para_pc(uint64_t pc);
  bool match_label(char *lab_str, uint64_t t, size_t s,uint64_t f);
  bool match_register(X86Register greg, X86Register rreg, uint32_t regsize = 0,
                      bool HighBits = false);
  bool match_imm(uint64_t val, char *sym);
  bool match_scale(X86Imm *gscale, X86Imm *rscale);
  bool match_offset(X86Imm *goffset, X86Imm *roffset);
  bool match_opd_imm(X86ImmOperand *gopd, X86ImmOperand *ropd);
  bool match_opd_reg(X86RegOperand *gopd, X86RegOperand *ropd,
                     uint32_t regsize = 0);
  bool match_opd_mem(X86MemOperand *gopd, X86MemOperand *ropd);
  bool check_opd_size(X86Operand *ropd, uint32_t gsize, uint32_t rsize);
  bool match_operand(X86Instruction *ginstr, X86Instruction *rinstr,
                     int opd_idx);
  bool match_rule_internal(
      X86Instruction *instr, TranslationRule *rule,
      const void /*FEXCore::Frontend::Decoder::DecodedBlocks*/ *tb);

private:
  bool InstIsMatch(uint64_t pc);
  bool InstParaIsMatch(uint64_t pc);
  bool TBRuleMatched(void);
  bool CheckTranslationRule(uint64_t pc);
  RuleRecord *GetTranslationRule(uint64_t pc);
  void GenArm64Code(RuleRecord *rule_r);

private:
  void GetLabelMap(char *lab_str, uint64_t *t, uint64_t *f, size_t *s = nullptr);
  uint64_t GetImmMap(char *sym);
  uint64_t GetARMImmMapWrapper(ARMImm *imm);
  uint64_t GetRVImmMapWrapper(RISCVImm *imm);
  ARMRegister GetGuestRegMap(ARMRegister &reg, uint32_t &regsize);
  ARMRegister GetGuestRegMap(ARMRegister &reg, uint32_t &regsize,
                             bool &&HighBits);

  void FlipCF();
  void assemble_arm_instr(ARMInstruction *instr, RuleRecord *rrule);
  void assemble_arm_exit(uint64_t target_pc);

  void StoreNZCV() {}
  void LoadNZCV() {}

private:
  RISCVRegister GuestMapRiscvReg(X86Register &reg);
  RISCVRegister GetRiscvTmpReg(RISCVRegister &reg);
  RISCVRegister GetRiscvReg(RISCVRegister &reg);
  biscuit::GPR GetRiscvGPR(RISCVRegister &reg);
  biscuit::FPR GetRiscvFPR(RISCVRegister &reg);
  biscuit::Vec GetRiscvVec(RISCVRegister &reg);

private:
#define DEF_RV_OPC(x) void Opc_##x(RISCVInstruction *instr, RuleRecord *rrule)
#include "Interface/Core/Pattern/CodeEmitter/BaseIntegerOps.inl"
#include "Interface/Core/Pattern/CodeEmitter/FPOps.inl"
#include "Interface/Core/Pattern/CodeEmitter/VectorOps.inl"
#undef DEF_RV_OPC

#define DEF_OPC(x) void Opc_##x(ARMInstruction *instr, RuleRecord *rrule)
#define ARM_ASM_DEFS
#include "Interface/Core/Pattern/arm-asm.inc"
#undef DEF_OPC
private:
  void LoadConstant(FEXCore::ARMEmitter::Size s,
                    FEXCore::ARMEmitter::Register Reg, uint64_t Constant,
                    bool NOPPad = false);

private:
  /* Try to match instructions in this tb to existing rules */
  ImmMapping imm_map_buf[1000];
  int imm_map_buf_index;

  LabelMapping label_map_buf[1000];
  int label_map_buf_index;

  GuestRegisterMapping g_reg_map_buf[1000];
  int g_reg_map_buf_index;
  int reg_map_num;

  RuleRecord rule_record_buf[800];
  int rule_record_buf_index;

  uint64_t pc_matched_buf[800];
  int pc_matched_buf_index;

  int imm_map_buf_index_pre;
  int g_reg_map_buf_index_pre;
  int label_map_buf_index_pre;

  ImmMapping *imm_map;
  GuestRegisterMapping *g_reg_map;
  LabelMapping *l_map;

  uint64_t pc_para_matched_buf[800];
  int pc_para_matched_buf_index;

  uint32_t num_rules_match;

private:
  ARCH Arch;
  FEXCore::Context::ContextImpl *Ctx;
  FEXCore::Core::InternalThreadState *Thread;
  const std::vector<int> &GPRMappedIdx;
  const std::vector<int> &GPRTempIdx;
  const std::vector<int> &XMMMappedIdx;
  const std::vector<int> &XMMTempIdx;

  std::unique_ptr<biscuit::Assembler> RVAssembler;
  std::unique_ptr<FEXCore::ARMEmitter::Emitter> ARMAssembler;

  uint64_t BlockPC;
};

} // namespace FEXCore::Pattern
