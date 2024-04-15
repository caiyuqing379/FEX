// SPDX-License-Identifier: MIT
/*
$info$
tags: backend|arm64
$end_info$
*/

#pragma once

#include "Interface/Core/ArchHelpers/Arm64Emitter.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Emitter.h"
#include "Interface/Core/Dispatcher/Dispatcher.h"
#include "Interface/Core/Frontend.h"
#include "Interface/Core/PatternDbt/arm-instr.h"
#include "Interface/Core/PatternDbt/rule-translate.h"

#include <aarch64/assembler-aarch64.h>
#include <aarch64/disasm-aarch64.h>

#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/CPUBackend.h>
#include <FEXCore/IR/IR.h>
#include <FEXCore/IR/IntrusiveIRList.h>
#include <FEXCore/IR/RegisterAllocationData.h>
#include <FEXCore/fextl/map.h>
#include <FEXCore/fextl/string.h>
#include <FEXCore/fextl/vector.h>

#include <array>
#include <cstdint>
#include <utility>
#include <variant>

namespace FEXCore::Core {
  struct InternalThreadState;
}

namespace FEXCore::CPU {
class Arm64JITCore final : public CPUBackend, public Arm64Emitter  {
public:
  explicit Arm64JITCore(FEXCore::Context::ContextImpl *ctx,
                        FEXCore::Core::InternalThreadState *Thread);
  ~Arm64JITCore() override;

  [[nodiscard]] fextl::string GetName() override { return "JIT"; }

  [[nodiscard]] CPUBackend::CompiledCode CompileCode(uint64_t Entry,
                                  FEXCore::IR::IRListView const *IR,
                                  FEXCore::Core::DebugData *DebugData,
                                  FEXCore::IR::RegisterAllocationData *RAData) override;

  [[nodiscard]] void *MapRegion(void* HostPtr, uint64_t, uint64_t) override { return HostPtr; }

  [[nodiscard]] bool NeedsOpDispatch() override { return true; }

  void ClearCache() override;

  void ClearRelocations() override { Relocations.clear(); }

  bool MatchTranslationRule(const void *tb) override;

private:
  FEX_CONFIG_OPT(ParanoidTSO, PARANOIDTSO);

  const bool HostSupportsSVE128{};
  const bool HostSupportsSVE256{};
  const bool HostSupportsRPRES{};
  const bool HostSupportsAFP{};

  ARMEmitter::BiDirectionalLabel *PendingTargetLabel;
  FEXCore::Context::ContextImpl *CTX;
  FEXCore::IR::IRListView const *IR;
  uint64_t Entry;
  CPUBackend::CompiledCode CodeData{};

  fextl::map<IR::NodeID, ARMEmitter::BiDirectionalLabel> JumpTargets;

  [[nodiscard]] FEXCore::ARMEmitter::Register GetReg(IR::NodeID Node) const {
    const auto Reg = GetPhys(Node);

    LOGMAN_THROW_AA_FMT(Reg.Class == IR::GPRFixedClass.Val || Reg.Class == IR::GPRClass.Val, "Unexpected Class: {}", Reg.Class);

    if (Reg.Class == IR::GPRFixedClass.Val) {
      return StaticRegisters[Reg.Reg];
    } else if (Reg.Class == IR::GPRClass.Val) {
      return GeneralRegisters[Reg.Reg];
    }

    FEX_UNREACHABLE;
  }

  [[nodiscard]] FEXCore::ARMEmitter::VRegister GetVReg(IR::NodeID Node) const {
    const auto Reg = GetPhys(Node);

    LOGMAN_THROW_AA_FMT(Reg.Class == IR::FPRFixedClass.Val || Reg.Class == IR::FPRClass.Val, "Unexpected Class: {}", Reg.Class);

    if (Reg.Class == IR::FPRFixedClass.Val) {
      return StaticFPRegisters[Reg.Reg];
    } else if (Reg.Class == IR::FPRClass.Val) {
      return GeneralFPRegisters[Reg.Reg];
    }

    FEX_UNREACHABLE;
  }

  [[nodiscard]] std::pair<FEXCore::ARMEmitter::Register, FEXCore::ARMEmitter::Register> GetRegPair(IR::NodeID Node) const {
    const auto Reg = GetPhys(Node);

    LOGMAN_THROW_AA_FMT(Reg.Class == IR::GPRPairClass.Val, "Unexpected Class: {}", Reg.Class);

    return GeneralPairRegisters[Reg.Reg];
  }

  [[nodiscard]] FEXCore::IR::RegisterClassType GetRegClass(IR::NodeID Node) const;

  [[nodiscard]] IR::PhysicalRegister GetPhys(IR::NodeID Node) const {
    auto PhyReg = RAData->GetNodeRegister(Node);

    LOGMAN_THROW_A_FMT(!PhyReg.IsInvalid(), "Couldn't Allocate register for node: ssa{}. Class: {}", Node, PhyReg.Class);

    return PhyReg;
  }

  // Converts IR-base shift type to ARMEmitter shift type.
  // Will be a no-op, only a type conversion since the two definitions match.
  [[nodiscard]] ARMEmitter::ShiftType ConvertIRShiftType(IR::ShiftType Shift) const {
    return Shift == IR::ShiftType::LSL ? ARMEmitter::ShiftType::LSL :
           Shift == IR::ShiftType::LSR ? ARMEmitter::ShiftType::LSR :
           Shift == IR::ShiftType::ASR ? ARMEmitter::ShiftType::ASR :
           ARMEmitter::ShiftType::ROR;
  }

  [[nodiscard]] bool IsFPR(IR::NodeID Node) const;
  [[nodiscard]] bool IsGPR(IR::NodeID Node) const;
  [[nodiscard]] bool IsGPRPair(IR::NodeID Node) const;

  [[nodiscard]] FEXCore::ARMEmitter::ExtendedMemOperand GenerateMemOperand(uint8_t AccessSize,
                                              FEXCore::ARMEmitter::Register Base,
                                              IR::OrderedNodeWrapper Offset,
                                              IR::MemOffsetType OffsetType,
                                              uint8_t OffsetScale);

  // NOTE: Will use TMP1 as a way to encode immediates that happen to fall outside
  //       the limits of the scalar plus immediate variant of SVE load/stores.
  //
  //       TMP1 is safe to use again once this memory operand is used with its
  //       equivalent loads or stores that this was called for.
  [[nodiscard]] FEXCore::ARMEmitter::SVEMemOperand GenerateSVEMemOperand(uint8_t AccessSize,
                                                    FEXCore::ARMEmitter::Register Base,
                                                    IR::OrderedNodeWrapper Offset,
                                                    IR::MemOffsetType OffsetType,
                                                    uint8_t OffsetScale);

  [[nodiscard]] bool IsInlineConstant(const IR::OrderedNodeWrapper& Node, uint64_t* Value = nullptr) const;
  [[nodiscard]] bool IsInlineEntrypointOffset(const IR::OrderedNodeWrapper& WNode, uint64_t* Value) const;

  struct LiveRange {
    uint32_t Begin;
    uint32_t End;
  };

  // This is purely a debugging aid for developers to see if they are in JIT code space when inspecting raw memory
  void EmitDetectionString();
  IR::RegisterAllocationPass *RAPass;
  IR::RegisterAllocationData *RAData;
  FEXCore::Core::DebugData *DebugData;

  void ResetStack();
  /**
   * @name Relocations
   * @{ */

    uint64_t GetNamedSymbolLiteral(FEXCore::CPU::RelocNamedSymbolLiteral::NamedSymbol Op);

    /**
     * @brief A literal pair relocation object for named symbol literals
     */
    struct NamedSymbolLiteralPair {
      ARMEmitter::ForwardLabel Loc;
      uint64_t Lit;
      Relocation MoveABI{};
    };

    /**
     * @brief Inserts a thunk relocation
     *
     * @param Reg - The GPR to move the thunk handler in to
     * @param Sum - The hash of the thunk
     */
    void InsertNamedThunkRelocation(ARMEmitter::Register Reg, const IR::SHA256Sum &Sum);

    /**
     * @brief Inserts a guest GPR move relocation
     *
     * @param Reg - The GPR to move the guest RIP in to
     * @param Constant - The guest RIP that will be relocated
     */
    void InsertGuestRIPMove(ARMEmitter::Register Reg, uint64_t Constant);

    /**
     * @brief Inserts a named symbol as a literal in memory
     *
     * Need to use `PlaceNamedSymbolLiteral` with the return value to place the literal in the desired location
     *
     * @param Op The named symbol to place
     *
     * @return A temporary `NamedSymbolLiteralPair`
     */
    NamedSymbolLiteralPair InsertNamedSymbolLiteral(FEXCore::CPU::RelocNamedSymbolLiteral::NamedSymbol Op);

    /**
     * @brief Place the named symbol literal relocation in memory
     *
     * @param Lit - Which literal to place
     */
    void PlaceNamedSymbolLiteral(NamedSymbolLiteralPair &Lit);

    fextl::vector<FEXCore::CPU::Relocation> Relocations;

    ///< Relocation code loading
    bool ApplyRelocations(uint64_t GuestEntry, uint64_t CodeEntry, uint64_t CursorEntry, size_t NumRelocations, const char* EntryRelocations);

  /**  @} */

  uint32_t SpillSlots{};
  using OpType = void (Arm64JITCore::*)(IR::IROp_Header const *IROp, IR::NodeID Node);

  using ScalarBinaryOpCaller = std::function<void(ARMEmitter::VRegister Dst, ARMEmitter::VRegister Src1, ARMEmitter::VRegister Src2)>;
  void VFScalarOperation(uint8_t OpSize, uint8_t ElementSize, bool ZeroUpperBits, ScalarBinaryOpCaller ScalarEmit, ARMEmitter::VRegister Dst, ARMEmitter::VRegister Vector1, ARMEmitter::VRegister Vector2);
  using ScalarUnaryOpCaller = std::function<void(ARMEmitter::VRegister Dst, std::variant<ARMEmitter::VRegister, ARMEmitter::Register> SrcVar)>;
  void VFScalarUnaryOperation(uint8_t OpSize, uint8_t ElementSize, bool ZeroUpperBits, ScalarUnaryOpCaller ScalarEmit, ARMEmitter::VRegister Dst, ARMEmitter::VRegister Vector1, std::variant<ARMEmitter::VRegister, ARMEmitter::Register> Vector2);

  // Runtime selection;
  // Load and store TSO memory style
  OpType RT_LoadMemTSO;
  OpType RT_StoreMemTSO;

#define DEF_OP(x) void Op_##x(IR::IROp_Header const *IROp, IR::NodeID Node)

  // Dynamic Dispatcher supporting operations
  DEF_OP(ParanoidLoadMemTSO);
  DEF_OP(ParanoidStoreMemTSO);

  ///< Unhandled handler
  DEF_OP(Unhandled);

  ///< No-op Handler
  DEF_OP(NoOp);

#define IROP_DISPATCH_DEFS
#include <FEXCore/IR/IRDefines_Dispatch.inc>
#undef DEF_OP


  /* Try to match instructions in this tb to existing rules */
  ImmMapping imm_map_buf[1000];
  int imm_map_buf_index;

  LabelMapping label_map_buf[1000];
  int label_map_buf_index;

  GuestRegisterMapping g_reg_map_buf[1000];
  int g_reg_map_buf_index;

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

  int reg_map_num;

  inline void reset_buffer(void);
  inline void save_map_buf_index(void);
  inline void recover_map_buf_index(void);
  inline void init_map_ptr(void);

  inline void add_rule_record(TranslationRule *rule, uint64_t pc, uint64_t t_pc,
                              int icount, bool update_cc, bool save_cc, int pa_opc[20]);
  inline void add_matched_pc(uint64_t pc);
  inline void add_matched_para_pc(uint64_t pc);
  bool match_label(char *lab_str, uint64_t t, uint64_t f);
  bool match_register(X86Register greg, X86Register rreg, uint32_t regsize = 0);
  bool match_imm(uint64_t val, char *sym);
  bool match_scale(X86Imm *gscale, X86Imm *rscale);
  bool match_offset(X86Imm *goffset, X86Imm *roffset);
  bool match_opd_imm(X86ImmOperand *gopd, X86ImmOperand *ropd);
  bool match_opd_reg(X86RegOperand *gopd, X86RegOperand *ropd, uint32_t regsize = 0);
  bool match_opd_mem(X86MemOperand *gopd, X86MemOperand *ropd);
  bool check_opd_size(X86Operand *ropd, uint32_t gsize, uint32_t rsize);
  bool match_operand(X86Instruction *ginstr, X86Instruction *rinstr, int opd_idx);
  bool match_rule_internal(X86Instruction *instr, TranslationRule *rule, FEXCore::Frontend::Decoder::DecodedBlocks const *tb);
  void get_label_map(char *lab_str, uint64_t *t, uint64_t *f);
  uint64_t get_imm_map(char *sym);
  uint64_t get_imm_map_wrapper(ARMImm *imm);
  ARMRegister get_guest_reg_map(ARMRegister& reg, uint32_t& regsize);

  bool instr_is_match(uint64_t pc);
  bool instrs_is_match(uint64_t pc);
  bool tb_rule_matched(void);
  bool check_translation_rule(uint64_t pc);
  RuleRecord* get_translation_rule(uint64_t pc);
  void do_rule_translation(RuleRecord *rule_r, uint32_t *reg_liveness);

  void FlipCF();
  IR::IROp_Header const* FindIROp(IR::IROps tIROp);
  void assemble_arm_instruction(ARMInstruction *instr, RuleRecord *rrule);
  void assemble_arm_exit_tb(uint64_t target_pc);

#define DEF_OPC(x) void Opc_##x(ARMInstruction *instr, RuleRecord *rrule)
  DEF_OPC(LDR);
  DEF_OPC(LDP);
  DEF_OPC(STR);
  DEF_OPC(STP);
  DEF_OPC(SXTW);
  DEF_OPC(MOV);
  DEF_OPC(MVN);
  DEF_OPC(AND);
  DEF_OPC(ORR);
  DEF_OPC(EOR);
  DEF_OPC(BIC);
  DEF_OPC(Shift);
  DEF_OPC(ADD);
  DEF_OPC(ADC);
  DEF_OPC(SUB);
  DEF_OPC(SBC);
  DEF_OPC(MUL);
  DEF_OPC(CLZ);
  DEF_OPC(TST);
  DEF_OPC(COMPARE);
  DEF_OPC(CSEX);
  DEF_OPC(B);
  DEF_OPC(BL);
  DEF_OPC(CBNZ);
  DEF_OPC(SET_JUMP);
  DEF_OPC(SET_CALL);
  DEF_OPC(PC_L);
  DEF_OPC(PC_S);
#undef DEF_OPC
};

} // namespace FEXCore::CPU
