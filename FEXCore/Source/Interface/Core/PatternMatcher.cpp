#include "Interface/Core/PatternMatcher.h"
#include "Interface/Core/Pattern/arm-instr.h"

using namespace FEXCore::Pattern;

PatternMatcher::PatternMatcher(ARCH Arch, FEXCore::Context::ContextImpl *Ctx,
                               FEXCore::Core::InternalThreadState *Thread,
                               const std::vector<int> &GPRMappedIdx,
                               const std::vector<int> &GPRTempIdx,
                               const std::vector<int> &XMMMappedIdx,
                               const std::vector<int> &XMMTempIdx)
    : Arch(Arch), Ctx(Ctx), Thread(Thread), GPRMappedIdx(GPRMappedIdx),
      GPRTempIdx(GPRTempIdx), XMMMappedIdx(XMMMappedIdx),
      XMMTempIdx(XMMTempIdx) {}

void PatternMatcher::Prepare(ARCH Arch) {
  // 准备工作，例如加载规则或初始化状态
  int arch = (Arch == ARM64) ? 0 : 1;
  ParseTranslationRules(arch, 0);
}

int PatternMatcher::GetRuleIndex(uint64_t pc) {
  // 查询匹配时的规则index
  // 可以使用pc查找对应的规则索引
  return -1; // 默认返回-1，表示未找到
}

void PatternMatcher::SetCodeBuffer(uint8_t *Buffer, size_t Size) {
  // 设置代码缓冲区的位置
  if (Arch == ARM64) {

  } else { // rv64
    RVAssembler->SwapCodeBuffer(biscuit::CodeBuffer(Buffer, Size));
  }
}

void PatternMatcher::SetPrologue(uint8_t *Code, size_t Size) {
  // 设置序言代码
}

void PatternMatcher::SetEpilogue(uint8_t *Code, size_t Size) {
  // 设置尾声代码
}

std::pair<uint8_t *, size_t> PatternMatcher::EmitCode() {
  // 根据规则生成Host指令
  // 1. 在CodeBuffer中写入指令
  // 2. 写入RIP
  // 3. 添加ret指令
RuleRecord *rule_r = GetTranslationRule(BlockPC);;
  TranslationRule *rule;

  if (!rule_r)
    return {NULL, 0};

  rule = rule_r->rule;
  l_map = rule_r->l_map;
  imm_map = rule_r->imm_map;
  g_reg_map = rule_r->g_reg_map;

  auto BlockStartHostCode = RVAssembler->GetCursorPointer();

  RISCVInstruction *riscv_code = rule->riscv_host;

  /* Assemble host instructions in the rule */
  while (riscv_code) {
    switch (riscv_code->opc) {
#define REGISTER_OP(op, x)                                                     \
  case RISCV_OPC_##op:                                                         \
    Opc_##x(riscv_code, rule_r);                                               \
    break

#define RISCV_DISPATCH
#include "Interface/Core/Pattern/CodeEmitter/RiscvDispatch.inc"
#undef REGISTER_OP
    default:
      LogMan::Msg::EFmt(
          "Unsupported riscv instruction in the assembler: {}, rule index: {}.",
          get_riscv_instr_opc(riscv_code->opc), rule_r->rule->index);
    }

    riscv_code = riscv_code->next;
  }

  return {BlockStartHostCode,
          static_cast<uint32_t>(RVAssembler->GetCursorPointer() -
                                BlockStartHostCode)}; // 返回生成的代码和大小
}

RISCVRegister PatternMatcher::GuestMapRiscvReg(X86Register &reg) {
  switch (reg) {
  case X86_REG_RAX:
    return get_riscv_reg(GPRMappedIdx[0]);
  case X86_REG_RCX:
    return get_riscv_reg(GPRMappedIdx[1]);
  case X86_REG_RDX:
    return get_riscv_reg(GPRMappedIdx[2]);
  case X86_REG_RBX:
    return get_riscv_reg(GPRMappedIdx[3]);
  case X86_REG_RSP:
    return get_riscv_reg(GPRMappedIdx[4]);
  case X86_REG_RBP:
    return get_riscv_reg(GPRMappedIdx[5]);
  case X86_REG_RSI:
    return get_riscv_reg(GPRMappedIdx[6]);
  case X86_REG_RDI:
    return get_riscv_reg(GPRMappedIdx[7]);
  case X86_REG_R8:
    return get_riscv_reg(GPRMappedIdx[8]);
  case X86_REG_R9:
    return get_riscv_reg(GPRMappedIdx[9]);
  case X86_REG_R10:
    return get_riscv_reg(GPRMappedIdx[10]);
  case X86_REG_R11:
    return get_riscv_reg(GPRMappedIdx[11]);
  case X86_REG_R12:
    return get_riscv_reg(GPRMappedIdx[12]);
  case X86_REG_R13:
    return get_riscv_reg(GPRMappedIdx[13]);
  case X86_REG_R14:
    return get_riscv_reg(GPRMappedIdx[14]);
  case X86_REG_R15:
    return get_riscv_reg(GPRMappedIdx[15]);
  case X86_REG_XMM0:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[0]);
  case X86_REG_XMM1:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[1]);
  case X86_REG_XMM2:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[2]);
  case X86_REG_XMM3:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[3]);
  case X86_REG_XMM4:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[4]);
  case X86_REG_XMM5:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[5]);
  case X86_REG_XMM6:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[6]);
  case X86_REG_XMM7:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[7]);
  case X86_REG_XMM8:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[8]);
  case X86_REG_XMM9:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[9]);
  case X86_REG_XMM10:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[10]);
  case X86_REG_XMM11:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[11]);
  case X86_REG_XMM12:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[12]);
  case X86_REG_XMM13:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[13]);
  case X86_REG_XMM14:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[14]);
  case X86_REG_XMM15:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMMappedIdx[15]);
  default:
    LOGMAN_MSG_A_FMT("Unsupported guest symbol reg num");
    return RISCV_REG_INVALID;
  }
}

RISCVRegister PatternMatcher::GetRiscvTmpReg(RISCVRegister &reg) {
  switch (reg) {
  case RISCV_REG_T0:
    return get_riscv_reg(GPRTempIdx[0]);
  case RISCV_REG_T1:
    return get_riscv_reg(GPRTempIdx[1]);
  case RISCV_REG_T2:
    return get_riscv_reg(GPRTempIdx[2]);
  case RISCV_REG_T3:
    return get_riscv_reg(GPRTempIdx[3]);
  case RISCV_REG_T4:
    return get_riscv_reg(GPRTempIdx[4]);
  case RISCV_REG_T5:
    return get_riscv_reg(GPRTempIdx[5]);
  case RISCV_REG_T6:
    return get_riscv_reg(GPRTempIdx[6]);
  case RISCV_REG_VT0:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMTempIdx[0]);
  case RISCV_REG_VT1:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMTempIdx[1]);
  case RISCV_REG_VT2:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMTempIdx[2]);
  case RISCV_REG_VT3:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMTempIdx[3]);
  case RISCV_REG_VT4:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMTempIdx[4]);
  case RISCV_REG_VT5:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMTempIdx[5]);
  case RISCV_REG_VT6:
    return get_riscv_reg(2 * RISCV_REG_NUM + XMMTempIdx[6]);
  default:
    LOGMAN_MSG_A_FMT("Unsupported guest symbol reg num");
    return RISCV_REG_INVALID;
  }
}

RISCVRegister PatternMatcher::GetRiscvReg(RISCVRegister &reg) {
  if (reg == RISCV_REG_INVALID)
    LOGMAN_MSG_A_FMT("RISC-V Reg is Invalid!");

  if (RISCV_REG_X0 <= reg && reg <= RISCV_REG_V31) {
    return reg;
  }

  if (RISCV_REG_T0 <= reg && reg <= RISCV_REG_VT6) {
    return GetRiscvTmpReg(reg);
  }

  GuestRegisterMapping *gmap = g_reg_map;

  while (gmap) {
    if (!strcmp(get_riscv_reg_str(reg), get_x86_reg_str(gmap->sym))) {
      RISCVRegister riscvreg = GuestMapRiscvReg(gmap->num);
      if (riscvreg == RISCV_REG_INVALID) {
        LogMan::Msg::EFmt("Unsupported reg num - RISCV: {}, x86: {}",
                          get_riscv_reg_str(reg), get_x86_reg_str(gmap->num));
        exit(0);
      }
      return riscvreg;
    }

    gmap = gmap->next;
  }

  assert(0);
  return RISCV_REG_INVALID;
}

biscuit::GPR PatternMatcher::GetRiscvGPR(RISCVRegister &reg) {
  biscuit::GPR reg_invalid{255};
  switch (reg) {
  case RISCV_REG_X0:
    return biscuit::x0;
  case RISCV_REG_X1:
    return biscuit::x1;
  case RISCV_REG_X2:
    return biscuit::x2;
  case RISCV_REG_X3:
    return biscuit::x3;
  case RISCV_REG_X4:
    return biscuit::x4;
  case RISCV_REG_X5:
    return biscuit::x5;
  case RISCV_REG_X6:
    return biscuit::x6;
  case RISCV_REG_X7:
    return biscuit::x7;
  case RISCV_REG_X8:
    return biscuit::x8;
  case RISCV_REG_X9:
    return biscuit::x9;
  case RISCV_REG_X10:
    return biscuit::x10;
  case RISCV_REG_X11:
    return biscuit::x11;
  case RISCV_REG_X12:
    return biscuit::x12;
  case RISCV_REG_X13:
    return biscuit::x13;
  case RISCV_REG_X14:
    return biscuit::x14;
  case RISCV_REG_X15:
    return biscuit::x15;
  case RISCV_REG_X16:
    return biscuit::x16;
  case RISCV_REG_X17:
    return biscuit::x17;
  case RISCV_REG_X18:
    return biscuit::x18;
  case RISCV_REG_X19:
    return biscuit::x19;
  case RISCV_REG_X20:
    return biscuit::x20;
  case RISCV_REG_X21:
    return biscuit::x21;
  case RISCV_REG_X22:
    return biscuit::x22;
  case RISCV_REG_X23:
    return biscuit::x23;
  case RISCV_REG_X24:
    return biscuit::x24;
  case RISCV_REG_X25:
    return biscuit::x25;
  case RISCV_REG_X26:
    return biscuit::x26;
  case RISCV_REG_X27:
    return biscuit::x27;
  case RISCV_REG_X28:
    return biscuit::x28;
  case RISCV_REG_X29:
    return biscuit::x29;
  case RISCV_REG_X30:
    return biscuit::x30;
  case RISCV_REG_X31:
    return biscuit::x31;
  default:
    LOGMAN_MSG_A_FMT("Unsupported host reg num");
  }
  return reg_invalid;
}

biscuit::FPR PatternMatcher::GetRiscvFPR(RISCVRegister &reg) {
  biscuit::FPR reg_invalid(255);
  switch (reg) {
  case RISCV_REG_F0:
    return biscuit::f0;
  case RISCV_REG_F1:
    return biscuit::f1;
  case RISCV_REG_F2:
    return biscuit::f2;
  case RISCV_REG_F3:
    return biscuit::f3;
  case RISCV_REG_F4:
    return biscuit::f4;
  case RISCV_REG_F5:
    return biscuit::f5;
  case RISCV_REG_F6:
    return biscuit::f6;
  case RISCV_REG_F7:
    return biscuit::f7;
  case RISCV_REG_F8:
    return biscuit::f8;
  case RISCV_REG_F9:
    return biscuit::f9;
  case RISCV_REG_F10:
    return biscuit::f10;
  case RISCV_REG_F11:
    return biscuit::f11;
  case RISCV_REG_F12:
    return biscuit::f12;
  case RISCV_REG_F13:
    return biscuit::f13;
  case RISCV_REG_F14:
    return biscuit::f14;
  case RISCV_REG_F15:
    return biscuit::f15;
  case RISCV_REG_F16:
    return biscuit::f16;
  case RISCV_REG_F17:
    return biscuit::f17;
  case RISCV_REG_F18:
    return biscuit::f18;
  case RISCV_REG_F19:
    return biscuit::f19;
  case RISCV_REG_F20:
    return biscuit::f20;
  case RISCV_REG_F21:
    return biscuit::f21;
  case RISCV_REG_F22:
    return biscuit::f22;
  case RISCV_REG_F23:
    return biscuit::f23;
  case RISCV_REG_F24:
    return biscuit::f24;
  case RISCV_REG_F25:
    return biscuit::f25;
  case RISCV_REG_F26:
    return biscuit::f26;
  case RISCV_REG_F27:
    return biscuit::f27;
  case RISCV_REG_F28:
    return biscuit::f28;
  case RISCV_REG_F29:
    return biscuit::f29;
  case RISCV_REG_F30:
    return biscuit::f30;
  case RISCV_REG_F31:
    return biscuit::f31;
  default:
    LOGMAN_MSG_A_FMT("Unsupported host freg num");
  }
  return reg_invalid;
}

biscuit::Vec PatternMatcher::GetRiscvVec(RISCVRegister &reg) {
  biscuit::Vec reg_invalid(255);
  switch (reg) {
  case RISCV_REG_V0:
    return biscuit::v0;
  case RISCV_REG_V1:
    return biscuit::v1;
  case RISCV_REG_V2:
    return biscuit::v2;
  case RISCV_REG_V3:
    return biscuit::v3;
  case RISCV_REG_V4:
    return biscuit::v4;
  case RISCV_REG_V5:
    return biscuit::v5;
  case RISCV_REG_V6:
    return biscuit::v6;
  case RISCV_REG_V7:
    return biscuit::v7;
  case RISCV_REG_V8:
    return biscuit::v8;
  case RISCV_REG_V9:
    return biscuit::v9;
  case RISCV_REG_V10:
    return biscuit::v10;
  case RISCV_REG_V11:
    return biscuit::v11;
  case RISCV_REG_V12:
    return biscuit::v12;
  case RISCV_REG_V13:
    return biscuit::v13;
  case RISCV_REG_V14:
    return biscuit::v14;
  case RISCV_REG_V15:
    return biscuit::v15;
  case RISCV_REG_V16:
    return biscuit::v16;
  case RISCV_REG_V17:
    return biscuit::v17;
  case RISCV_REG_V18:
    return biscuit::v18;
  case RISCV_REG_V19:
    return biscuit::v19;
  case RISCV_REG_V20:
    return biscuit::v20;
  case RISCV_REG_V21:
    return biscuit::v21;
  case RISCV_REG_V22:
    return biscuit::v22;
  case RISCV_REG_V23:
    return biscuit::v23;
  case RISCV_REG_V24:
    return biscuit::v24;
  case RISCV_REG_V25:
    return biscuit::v25;
  case RISCV_REG_V26:
    return biscuit::v26;
  case RISCV_REG_V27:
    return biscuit::v27;
  case RISCV_REG_V28:
    return biscuit::v28;
  case RISCV_REG_V29:
    return biscuit::v29;
  case RISCV_REG_V30:
    return biscuit::v30;
  case RISCV_REG_V31:
    return biscuit::v31;
  default:
    LOGMAN_MSG_A_FMT("Unsupported host vreg num");
  }
  return reg_invalid;
}

uint64_t PatternMatcher::GetImmMap(char *sym) {
  ImmMapping *im = imm_map;
  char t_str[50]; /* replaced string */
  char t_buf[50]; /* buffer string */

  /* Due to the expression in host imm_str, We replace all imm_xxx in host
     imm_str with the corresponding guest values, and parse it to get the value
     of the expression */
  strcpy(t_str, sym);

  while (im) {
    char *p_str = strstr(t_str, im->imm_str);
    while (p_str) {
      size_t len = (size_t)(p_str - t_str);
      strncpy(t_buf, t_str, len);
      sprintf(t_buf + len, "%lu", im->imm_val);
      strncat(t_buf, t_str + len + strlen(im->imm_str),
              strlen(t_str) - len - strlen(im->imm_str));
      strcpy(t_str, t_buf);
      p_str = strstr(t_str, im->imm_str);
    }
    im = im->next;
  }

  return std::stoull(t_str);
}

uint64_t PatternMatcher::GetARMImmMapWrapper(ARMImm *imm) {
  if (imm->type == ARM_IMM_TYPE_NONE)
    return 0;

  if (imm->type == ARM_IMM_TYPE_VAL)
    return imm->content.val;

  return GetImmMap(imm->content.sym);
}

uint64_t PatternMatcher::GetRVImmMapWrapper(RISCVImm *imm) {
  if (imm->type == RISCV_IMM_TYPE_NONE)
    return 0;

  if (imm->type == RISCV_IMM_TYPE_VAL)
    return imm->content.val;

  return GetImmMap(imm->content.sym);
}

void PatternMatcher::GetLabelMap(char *lab_str, uint64_t *t, uint64_t *f) {
  LabelMapping *lmap = l_map;

  while (lmap) {
    if (!strcmp(lmap->lab_str, lab_str)) {
      *t = lmap->target;
      *f = lmap->fallthrough;
      return;
    }
    lmap = lmap->next;
  }

  assert(0);
}