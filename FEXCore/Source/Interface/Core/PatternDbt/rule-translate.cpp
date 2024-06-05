#include "Interface/Core/JIT/Arm64/JITClass.h"

#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <assert.h>
#include <cstring>

#include "rule-translate.h"
#include "rule-debug-log.h"

#define MAX_RULE_RECORD_BUF_LEN 800
#define MAX_GUEST_INSTR_LEN 800
#define MAX_HOST_RULE_LEN 800

#define MAX_MAP_BUF_LEN 1000
#define MAX_HOST_RULE_INSTR_LEN 1000

static int debug = 0;
static int match_insts = 0;
static int match_counter = 10;

inline void FEXCore::CPU::Arm64JITCore::reset_buffer(void)
{
    imm_map_buf_index = 0;
    label_map_buf_index = 0;
    g_reg_map_buf_index = 0;

    rule_record_buf_index = 0;
    pc_matched_buf_index = 0;

    pc_para_matched_buf_index = 0;
}

inline void FEXCore::CPU::Arm64JITCore::save_map_buf_index(void)
{
    imm_map_buf_index_pre = imm_map_buf_index;
    g_reg_map_buf_index_pre = g_reg_map_buf_index;
    label_map_buf_index_pre = label_map_buf_index;
}

inline void FEXCore::CPU::Arm64JITCore::recover_map_buf_index(void)
{
    imm_map_buf_index = imm_map_buf_index_pre;
    g_reg_map_buf_index = g_reg_map_buf_index_pre;
    label_map_buf_index = label_map_buf_index_pre;
}

inline void FEXCore::CPU::Arm64JITCore::init_map_ptr(void)
{
    imm_map = NULL;
    g_reg_map = NULL;
    l_map = NULL;
    reg_map_num = 0;
}

inline void FEXCore::CPU::Arm64JITCore::add_rule_record(TranslationRule *rule, uint64_t pc, uint64_t t_pc,
                                   X86Instruction *last_guest, bool update_cc, bool save_cc, int pa_opc[20])
{
    RuleRecord *p = &rule_record_buf[rule_record_buf_index++];

    assert(rule_record_buf_index < MAX_RULE_RECORD_BUF_LEN);

    p->pc = pc;
    p->target_pc = t_pc;
    p->last_guest = last_guest;
    p->rule = rule;
    p->update_cc = update_cc;
    p->save_cc = save_cc;
    p->imm_map = imm_map;
    p->g_reg_map = g_reg_map;
    p->l_map = l_map;
    for (int i = 0; i < 20; i++)
        p->para_opc[i] = pa_opc[i];
}


inline void FEXCore::CPU::Arm64JITCore::add_matched_pc(uint64_t pc)
{
    pc_matched_buf[pc_matched_buf_index++] = pc;

    assert(pc_matched_buf_index < MAX_GUEST_INSTR_LEN);
}

inline void FEXCore::CPU::Arm64JITCore::add_matched_para_pc(uint64_t pc)
{
    pc_para_matched_buf[pc_para_matched_buf_index++] = pc;

    assert(pc_para_matched_buf_index < MAX_GUEST_INSTR_LEN);
}


bool FEXCore::CPU::Arm64JITCore::match_label(char *lab_str, uint64_t t, uint64_t f)
{
    LabelMapping *lmap = l_map;

    while(lmap) {
        if (strcmp(lmap->lab_str, lab_str)) {
            lmap = lmap->next;
            continue;
        }

        return (lmap->target == t && lmap->fallthrough == f);
    }

    /* append this map to label map buffer */
    lmap = &label_map_buf[label_map_buf_index++];
    assert(label_map_buf_index < MAX_MAP_BUF_LEN);
    strcpy(lmap->lab_str, lab_str);
    lmap->target = t;
    lmap->fallthrough = f;

    lmap->next = l_map;
    l_map = lmap;

    return true;
}

bool FEXCore::CPU::Arm64JITCore::match_register(X86Register greg, X86Register rreg, uint32_t regsize, bool HighBits)
{
    GuestRegisterMapping *gmap = g_reg_map;

    if (greg == X86_REG_INVALID && rreg == X86_REG_INVALID)
        return true;

    if (greg == X86_REG_INVALID || rreg == X86_REG_INVALID) {
        if(debug)
            LogMan::Msg::IFmt("Unmatch reg: one invalid reg!");
        return false;
    }

    /* use physical register */
    if ((X86_REG_RAX <= rreg && rreg <= X86_REG_XMM15) && greg == rreg)
        return true;

    if (!(X86_REG_REG0 <= rreg && rreg <= X86_REG_REG31)) {
        if(debug)
            LogMan::Msg::IFmt("Unmatch reg: not reg sym!");
        return false;
    }

    /* check if we already have this map */
    while (gmap) {
        if (gmap->sym != rreg) {
            gmap = gmap->next;
            continue;
        }
        if (debug && (gmap->num != greg))
            fprintf(stderr, "Unmatch reg: map conflict: %d %d\n", gmap->num, greg);
        return (gmap->num == greg);
    }

    /* append this map to register map buffer */
    gmap = &g_reg_map_buf[g_reg_map_buf_index++];
    assert(g_reg_map_buf_index < MAX_MAP_BUF_LEN);
    gmap->sym = rreg;
    gmap->num = greg;

    if (regsize)
      gmap->regsize = regsize;
    else
      gmap->regsize = 0;

    if (HighBits)
      gmap->HighBits = true;
    else
      gmap->HighBits = false;
    ++reg_map_num;

    gmap->next = g_reg_map;
    g_reg_map = gmap;

    return true;
}

bool FEXCore::CPU::Arm64JITCore::match_imm(uint64_t val, char *sym)
{
    ImmMapping *imap = imm_map;

    while(imap) {
        if (!strcmp(sym, imap->imm_str)) {
            if (debug && (val != imap->imm_val))
                LogMan::Msg::IFmt( "Unmatch imm: symbol map conflict {} {}", imap->imm_val, val);
            return (val == imap->imm_val);
        }

        imap = imap->next;
    }

    /* add this map to immediate map buffer */
    imap = &imm_map_buf[imm_map_buf_index++];
    assert(imm_map_buf_index < MAX_MAP_BUF_LEN);
    strcpy(imap->imm_str, sym);
    imap->imm_val = val;

    imap->next = imm_map;
    imm_map = imap;

    return true;
}

bool FEXCore::CPU::Arm64JITCore::match_scale(X86Imm *gscale, X86Imm *rscale)
{
    if (gscale->type == X86_IMM_TYPE_NONE &&
        rscale->type == X86_IMM_TYPE_NONE)
        return true;

    if (rscale->type == X86_IMM_TYPE_VAL){
        if (debug && (gscale->content.val != rscale->content.val))
            LogMan::Msg::IFmt("Unmatch scale value: {} {}", gscale->content.val, rscale->content.val);
        return gscale->content.val == rscale->content.val;
    }
    else if (rscale->type == X86_IMM_TYPE_NONE)
        return match_imm(0, rscale->content.sym);
    else
        return match_imm(gscale->content.val, rscale->content.sym);
}

bool FEXCore::CPU::Arm64JITCore::match_offset(X86Imm *goffset, X86Imm *roffset)
{
    char *sym;
    int32_t off_val;

    if (roffset->type != X86_IMM_TYPE_NONE &&
        goffset->type == X86_IMM_TYPE_NONE)
        return match_imm(0, roffset->content.sym);

    if (goffset->type == X86_IMM_TYPE_NONE &&
        roffset->type == X86_IMM_TYPE_NONE)
        return true;

    if (roffset->type == X86_IMM_TYPE_NONE &&
        goffset->type == X86_IMM_TYPE_VAL && goffset->content.val == 0)
        return true;

    if (goffset->type == X86_IMM_TYPE_NONE ||
        roffset->type == X86_IMM_TYPE_NONE) {
        if (debug) {
            LogMan::Msg::IFmt("Unmatch offset: none");
        }
        return false;
    }

    sym = roffset->content.sym;
    off_val = goffset->content.val;

    return match_imm(off_val, sym);
}

bool FEXCore::CPU::Arm64JITCore::match_opd_imm(X86ImmOperand *gopd, X86ImmOperand *ropd)
{
    if (gopd->type == X86_IMM_TYPE_NONE && ropd->type == X86_IMM_TYPE_NONE)
        return true;

    if (ropd->type == X86_IMM_TYPE_VAL)
        return (gopd->content.val == ropd->content.val);
    else if (ropd->type == X86_IMM_TYPE_SYM)
        return match_imm(gopd->content.val, ropd->content.sym);
    else {
        if (debug)
            LogMan::Msg::IFmt("Unmatch imm: type error");
        return false;
    }
}

bool FEXCore::CPU::Arm64JITCore::match_opd_reg(X86RegOperand *gopd, X86RegOperand *ropd, uint32_t regsize)
{
    /* physical reg, but high bit not match */
    if ((X86_REG_RAX <= ropd->num && ropd->num <= X86_REG_XMM15) && gopd->HighBits != ropd->HighBits) {
        if (debug)
            LogMan::Msg::IFmt("Unmatch reg: phy reg, but high bit error.");
        return false;
    }
    return match_register(gopd->num, ropd->num, regsize, gopd->HighBits);
}

bool FEXCore::CPU::Arm64JITCore::match_opd_mem(X86MemOperand *gopd, X86MemOperand *ropd)
{
    return (match_register(gopd->base, ropd->base) &&
            match_register(gopd->index, ropd->index) &&
            match_offset(&gopd->offset, &ropd->offset) &&
            match_scale(&gopd->scale, &ropd->scale));
}

bool FEXCore::CPU::Arm64JITCore::check_opd_size(X86Operand *ropd, uint32_t gsize, uint32_t rsize)
{
    if ((ropd->type == X86_OPD_TYPE_REG && X86_REG_RAX <= ropd->content.reg.num && ropd->content.reg.num <= X86_REG_XMM15)
      || (ropd->type == X86_OPD_TYPE_IMM && ropd->content.imm.isRipLiteral) || ropd->type == X86_OPD_TYPE_MEM) {
            return gsize == rsize;
    }
    return true;
}

/* Try to match operand in guest instruction (gopd) and and the rule (ropd) */
bool FEXCore::CPU::Arm64JITCore::match_operand(X86Instruction *ginstr, X86Instruction *rinstr, int opd_idx)
{
    X86Operand *gopd = &ginstr->opd[opd_idx];
    X86Operand *ropd = &rinstr->opd[opd_idx];
    uint32_t regsize = opd_idx == 0 ? ginstr->DestSize : ginstr->SrcSize;

    if (gopd->type != ropd->type) {
        #ifdef DEBUG_RULE_LOG
            writeToLogFile(std::to_string(ThreadState->ThreadManager.PID) + "fex-debug.log", "[INFO] Different operand type\n");
        #else
            LogMan::Msg::IFmt("Different operand {} type", opd_idx);
        #endif
        return false;
    }

    if (!opd_idx && rinstr->DestSize && !check_opd_size(ropd, ginstr->DestSize, rinstr->DestSize)) {
        if (debug) {
            #ifdef DEBUG_RULE_LOG
                writeToLogFile(std::to_string(ThreadState->ThreadManager.PID) + "fex-debug.log", "[INFO] Different dest size - RULE: " + std::to_string(rinstr->DestSize)
                                + ", GUEST: " + std::to_string(ginstr->DestSize) + "\n");
            #else
                LogMan::Msg::IFmt("Different dest size - RULE: {}, GUEST: {}", rinstr->DestSize, ginstr->DestSize);
            #endif
        }
        return false;
    }

    if (opd_idx && rinstr->SrcSize && !check_opd_size(ropd, ginstr->SrcSize, rinstr->SrcSize)) {
        if (debug)
            LogMan::Msg::IFmt("Different opd src size.");
        return false;
    }

    if (ropd->type == X86_OPD_TYPE_IMM) {
        if (gopd->content.imm.isRipLiteral != ropd->content.imm.isRipLiteral)
            return false;
        if (x86_instr_test_branch(rinstr) || ropd->content.imm.isRipLiteral) {
            assert(ropd->content.imm.type == X86_IMM_TYPE_SYM);
            return match_label(ropd->content.imm.content.sym, gopd->content.imm.content.val, ginstr->pc + ginstr->InstSize);
        } else /* match imm operand */
            return match_opd_imm(&gopd->content.imm, &ropd->content.imm);
    } else if (ropd->type == X86_OPD_TYPE_REG) {
        return match_opd_reg(&gopd->content.reg, &ropd->content.reg, regsize);
    } else if (ropd->type == X86_OPD_TYPE_MEM) {
        return match_opd_mem(&gopd->content.mem, &ropd->content.mem);
    } else
        fprintf(stderr,"Error: unsupported arm operand type: %d\n", ropd->type);

    return true;
}

// not used
static bool check_instr(X86Instruction *ginstr){
    return true;
}

// return:
// 0: not matched
// 1: matched
// 2: matched but condition is different
bool FEXCore::CPU::Arm64JITCore::match_rule_internal(X86Instruction *instr, TranslationRule *rule,
                    FEXCore::Frontend::Decoder::DecodedBlocks const *tb)
{
    X86Instruction *p_rule_instr = rule->x86_guest;
    X86Instruction *p_guest_instr = instr;
    X86Instruction *last_guest_instr = NULL;
    int i;

    int j = 0;
    /* init for this rule */
    init_map_ptr();

    while(p_rule_instr) {

        if (p_rule_instr->opc == X86_OPC_INVALID || p_guest_instr->opc == X86_OPC_INVALID) {
            return false;
        }

        if (p_rule_instr->opc == X86_OPC_NOP && p_guest_instr->opc == X86_OPC_NOP) {
            goto next_check;
        }

        /* check opcode and number of operands */
        if ((p_rule_instr->opc != p_guest_instr->opc) ||  //opcode not equal
            ((p_rule_instr->opd_num != 0) && (p_rule_instr->opd_num != p_guest_instr->opd_num))) {  //operand not equal

            if (debug) {
                if (p_rule_instr->opd_num != p_guest_instr->opd_num)
                    LogMan::Msg::IFmt("Different operand number, rule index {}", rule->index);
            }

            return false;
        }

        /*check parameterized instructions*/
        if ((p_rule_instr->opd_num == 0) && !check_instr(p_guest_instr)) {
            if (debug) {
                LogMan::Msg::IFmt("parameterization check error!");
            }
            return false;
        }

        /* match each operand */
        for(i = 0; i < p_rule_instr->opd_num; i++) {
            if (!match_operand(p_guest_instr, p_rule_instr, i)) {
                if (debug) {
                    #ifdef DEBUG_RULE_LOG
                        writeToLogFile(std::to_string(ThreadState->ThreadManager.PID) + "fex-debug.log", "[INFO] Rule index " + std::to_string(rule->index)
                                                + ", unmatched operand index: " + std::to_string(i) + "\n");
                        output_x86_instr(p_guest_instr, ThreadState->ThreadManager.PID);
                        output_x86_instr(p_rule_instr, ThreadState->ThreadManager.PID);
                    #else
                        LogMan::Msg::IFmt("Rule index {}, unmatched operand index: {}", rule->index, i);
                        print_x86_instr(p_guest_instr);
                        print_x86_instr(p_rule_instr);
                    #endif
                }
                return false;
            }
        }

        next_check:
        last_guest_instr = p_guest_instr;

        /* check next instruction */
        p_rule_instr = p_rule_instr->next;
        p_guest_instr = p_guest_instr->next;
        j++;
    }

    if (last_guest_instr) {
        bool *p_reg_liveness = last_guest_instr->reg_liveness;
        if ((p_reg_liveness[X86_REG_CF] && (rule->x86_cc_mapping[X86_CF] == 0)) ||
            (p_reg_liveness[X86_REG_SF] && (rule->x86_cc_mapping[X86_SF] == 0)) ||
            (p_reg_liveness[X86_REG_OF] && (rule->x86_cc_mapping[X86_OF] == 0)) ||
            (p_reg_liveness[X86_REG_ZF] && (rule->x86_cc_mapping[X86_ZF] == 0))) {

                if (debug) {
                    LogMan::Msg::IFmt( "Different liveness cc!");
                }
                return false;
            }
    }

    return true;
}

void FEXCore::CPU::Arm64JITCore::get_label_map(char *lab_str, uint64_t *t, uint64_t *f)
{
    LabelMapping *lmap = l_map;

    while(lmap) {
        if (!strcmp(lmap->lab_str, lab_str)) {
            *t = lmap->target;
            *f = lmap->fallthrough;
            return;
        }
        lmap = lmap->next;
    }

    assert (0);
}


uint64_t FEXCore::CPU::Arm64JITCore::get_imm_map(char *sym)
{
    ImmMapping *im = imm_map;
    char t_str[50]; /* replaced string */
    char t_buf[50]; /* buffer string */

    /* Due to the expression in host imm_str, We replace all imm_xxx in host imm_str
       with the corresponding guest values, and parse it to get the value of the expression */
    strcpy(t_str, sym);

    while(im) {
        char *p_str = strstr(t_str, im->imm_str);
        while (p_str) {
            size_t len = (size_t)(p_str - t_str);
            strncpy(t_buf, t_str, len);
            sprintf(t_buf + len, "%lu", im->imm_val);
            strncat(t_buf, t_str + len + strlen(im->imm_str), strlen(t_str) - len - strlen(im->imm_str));
            strcpy(t_str, t_buf);
            p_str = strstr(t_str, im->imm_str);
        }
        im = im->next;
    }
    if (debug)
        LogMan::Msg::IFmt("get imm val: {}", t_str);
    return std::stoull(t_str);
}

uint64_t FEXCore::CPU::Arm64JITCore::GetImmMapWrapper(ARMImm *imm)
{
    if (imm->type == ARM_IMM_TYPE_NONE)
        return 0;

    if (imm->type == ARM_IMM_TYPE_VAL)
        return imm->content.val;

    return get_imm_map(imm->content.sym);
}

static ARMRegister guest_host_reg_map(X86Register& reg)
{
  switch (reg) {
    case X86_REG_RAX:   return ARM_REG_R4;
    case X86_REG_RCX:   return ARM_REG_R5;
    case X86_REG_RDX:   return ARM_REG_R6;
    case X86_REG_RBX:   return ARM_REG_R7;
    case X86_REG_RSP:   return ARM_REG_R8;
    case X86_REG_RBP:   return ARM_REG_R9;
    case X86_REG_RSI:   return ARM_REG_R10;
    case X86_REG_RDI:   return ARM_REG_R11;
    case X86_REG_R8:    return ARM_REG_R12;
    case X86_REG_R9:    return ARM_REG_R13;
    case X86_REG_R10:   return ARM_REG_R14;
    case X86_REG_R11:   return ARM_REG_R15;
    case X86_REG_R12:   return ARM_REG_R16;
    case X86_REG_R13:   return ARM_REG_R17;
    case X86_REG_R14:   return ARM_REG_R19;
    case X86_REG_R15:   return ARM_REG_R29;
    case X86_REG_XMM0:  return ARM_REG_V16;
    case X86_REG_XMM1:  return ARM_REG_V17;
    case X86_REG_XMM2:  return ARM_REG_V18;
    case X86_REG_XMM3:  return ARM_REG_V19;
    case X86_REG_XMM4:  return ARM_REG_V20;
    case X86_REG_XMM5:  return ARM_REG_V21;
    case X86_REG_XMM6:  return ARM_REG_V22;
    case X86_REG_XMM7:  return ARM_REG_V23;
    case X86_REG_XMM8:  return ARM_REG_V24;
    case X86_REG_XMM9:  return ARM_REG_V25;
    case X86_REG_XMM10: return ARM_REG_V26;
    case X86_REG_XMM11: return ARM_REG_V27;
    case X86_REG_XMM12: return ARM_REG_V28;
    case X86_REG_XMM13: return ARM_REG_V29;
    case X86_REG_XMM14: return ARM_REG_V30;
    case X86_REG_XMM15: return ARM_REG_V31;
    default:
      LOGMAN_MSG_A_FMT("Unsupported guest reg num");
      return ARM_REG_INVALID;
  }
}

ARMRegister FEXCore::CPU::Arm64JITCore::GetGuestRegMap(ARMRegister& reg, uint32_t& regsize)
{
    return GetGuestRegMap(reg, regsize, false);
}

ARMRegister FEXCore::CPU::Arm64JITCore::GetGuestRegMap(ARMRegister& reg, uint32_t& regsize, bool&& HighBits)
{
    if (reg == ARM_REG_INVALID)
        LOGMAN_MSG_A_FMT("ArmReg is Invalid!");

    if (ARM_REG_R0 <= reg && reg <= ARM_REG_ZR) {
        regsize = 0;
        HighBits = false;
        return reg;
    }

    GuestRegisterMapping *gmap = g_reg_map;

    while (gmap) {
        if (!strcmp(get_arm_reg_str(reg), get_x86_reg_str(gmap->sym))) {
            regsize = gmap->regsize;
            HighBits = gmap->HighBits;
            ARMRegister armreg = guest_host_reg_map(gmap->num);
            if (armreg == ARM_REG_INVALID) {
                LogMan::Msg::EFmt("Unsupported reg num - arm: {}, x86: {}", get_arm_reg_str(reg), get_x86_reg_str(gmap->num));
                exit(0);
            }
            return armreg;
        }

        gmap = gmap->next;
    }

    assert(0);
    return ARM_REG_INVALID;
}

bool FEXCore::CPU::Arm64JITCore::instr_is_match(uint64_t pc)
{
    int i;
    for (i = 0; i < pc_matched_buf_index; i++) {
        if (pc_matched_buf[i] == pc)
            return true;
    }
    return false;
}

bool FEXCore::CPU::Arm64JITCore::instrs_is_match(uint64_t pc)
{
    int i;
    for (i = 0; i < pc_para_matched_buf_index; i++) {
        if (pc_para_matched_buf[i] == pc)
            return true;
    }
    return instr_is_match(pc);
}

bool FEXCore::CPU::Arm64JITCore::tb_rule_matched(void)
{
    return (pc_matched_buf_index != 0);
}

bool FEXCore::CPU::Arm64JITCore::check_translation_rule(uint64_t pc)
{
    int i;
    for (i = 0; i < rule_record_buf_index; i++) {
        if (rule_record_buf[i].pc == pc)
            return true;
    }
    return false;
}

RuleRecord* FEXCore::CPU::Arm64JITCore::get_translation_rule(uint64_t pc)
{
    int i;
    for (i = 0; i < rule_record_buf_index; i++) {
        if (rule_record_buf[i].pc == pc) {
            rule_record_buf[i].pc = 0xffffffff; /* disable it after translation */
            return &rule_record_buf[i];
        }
    }
    return NULL;
}


#ifdef PROFILE_RULE_TRANSLATION
uint64_t rule_guest_pc = 0;
uint32_t num_rules_hit = 0;
uint32_t num_rules_replace = 0;
#endif

static bool is_save_cc(X86Instruction *pins, int icount)
{
    X86Instruction *head = pins;
    int i;

    for (i = 0; i < icount; i++) {
        if (head->save_cc)
            return true;
        head = head->next;
    }

    return false;
}


/* Try to match instructions in this tb with existing rules */
bool FEXCore::CPU::Arm64JITCore::MatchTranslationRule(const void *tb)
{
    auto transblock = static_cast<const FEXCore::Frontend::Decoder::DecodedBlocks*>(tb);
    if (match_counter <= 0)
        return false;

    X86Instruction *guest_instr = transblock->guest_instr;
    X86Instruction *cur_head = guest_instr;
    int guest_instr_num = 0;
    int i, j;
    bool ismatch = false;

    #ifdef DEBUG_RULE_LOG
      ofstream_x86_instr2(guest_instr, ThreadState->ThreadManager.PID);
      ofstream_rule_arm_instr2(guest_instr, ThreadState->ThreadManager.PID);
    #endif

    LogMan::Msg::IFmt("=====Guest Instr Match Rule Start, Guest PC: 0x{:x}=====\n", guest_instr->pc);

    reset_buffer();

    /* Try from the longest rule */
    while (cur_head) {

        bool opd_para = false;

        if (guest_instr_num <= 0) {
            X86Instruction *t_head = cur_head;
            guest_instr_num = 0;
            while (t_head){
                ++guest_instr_num;
                t_head = t_head->next;
            }
        }

        for (i = guest_instr_num; i > 0; i--) {
            /* calculate hash key */
            int hindex = rule_hash_key(cur_head, i);

            if (hindex >= MAX_GUEST_LEN)
                continue;
            /* check rule with length i (number of guest instructions) */
            TranslationRule *cur_rule = cache_rule_table[hindex];

            save_map_buf_index();
            uint32_t num_rules_match = 0;
            while (cur_rule) {

                if (cur_rule->guest_instr_num != i)
                    goto next;

                num_rules_match++;

                if (match_rule_internal(cur_head, cur_rule, transblock)) {
                    #if defined(PROFILE_RULE_TRANSLATION) && defined(DEBUG_RULE_LOG)
                        writeToLogFile(std::to_string(ThreadState->ThreadManager.PID) + "fex-debug.log", "[INFO] #####  Rule index " +
                            std::to_string(cur_rule->index) + ", match num:" +
                            std::to_string(num_rules_match) + "#####\n\n");
                    #endif
                    break;
                }

                next:
                cur_rule = cur_rule->next;
                recover_map_buf_index();
            }
            /* We find a matched rule, save it */
            if (cur_rule) {
                X86Instruction *temp = cur_head;
                uint64_t target_pc = 0;

                match_insts += i;

                /* Check target_pc for this rule */
                for (j = 1; j < i; j++)
                    temp = temp->next;
                if (!temp->next) // last instr
                    target_pc = temp->pc + temp->InstSize;

                int pa_opc[20];
                if (!opd_para) {
                    add_rule_record(cur_rule , cur_head->pc, target_pc, temp,
                        true, is_save_cc(cur_head, i), pa_opc);
                }

                /* We get a matched rule, keep moving forward */
                if (opd_para) {
                  for (j = 0; j < i; j++) {
                    add_matched_para_pc(cur_head->pc);
                    cur_head = cur_head->next;
                    guest_instr_num--;
                  }
                } else {
                  for (j = 0; j < i; j++) {
                    add_matched_pc(cur_head->pc);
                    cur_head = cur_head->next;
                    guest_instr_num--;
                  }
                }

                ismatch = true;
                break;
            }

            recover_map_buf_index();

            if(1) goto final;
        }

        /* No matched rule found, also keep moving forward */
        if (i == 0) {
            /* print unmatched instructions
               if not continuous, record as a new block */
            cur_head = cur_head->next;
            guest_instr_num--;
        }
    }
    final:
    return ismatch;
}

void remove_guest_instruction(FEXCore::Frontend::Decoder::DecodedBlocks *tb, uint64_t pc)
{
    X86Instruction *head = tb->guest_instr;

    if (!head)
        return;

    if (head->pc == pc) {
        tb->guest_instr = head->next;
        tb->NumInstructions--;
        return;
    }

    while(head->next) {
        if (head->next->pc == pc) {
            head->next = head->next->next;
            tb->NumInstructions--;
            return;
        }
        head = head->next;
    }
}

static ARMInstruction *arm_host;
void FEXCore::CPU::Arm64JITCore::do_rule_translation(RuleRecord *rule_r, uint32_t *reg_liveness)
{
    TranslationRule *rule;

    if (!rule_r)
        return;

    rule = rule_r->rule;
    l_map = rule_r->l_map;
    imm_map = rule_r->imm_map;
    g_reg_map = rule_r->g_reg_map;

    #ifdef PROFILE_RULE_TRANSLATION
        num_rules_replace++;
        #ifdef DEBUG_RULE_LOG
            writeToLogFile(std::to_string(ThreadState->ThreadManager.PID) + "fex-debug.log", "[INFO] ##### PC: 0x" + intToHex(rule_guest_pc) + ", Rule index " +
                                   std::to_string(rule->index) + ", Total replace num:" +
                                   std::to_string(num_rules_replace) + "#####\n\n");
        #else
            LogMan::Msg::IFmt( "##### PC: 0x{:x}, Rule index {}, Total replace num: {} #####\n",
                rule_guest_pc, rule->index, num_rules_replace);
        #endif
    #endif

    ARMInstruction *arm_code = rule->arm_host;
    arm_host = arm_code;

    /* Assemble host instructions in the rule */
    while(arm_code) {
        assemble_arm_instr(arm_code, rule_r);
        arm_code = arm_code->next;
    }

    if (rule_r->target_pc != 0) {
        if (debug) {
            LogMan::Msg::IFmt("Current TB target_pc: 0x{:x}\n", rule_r->target_pc);
        }

        X86Instruction* last_x86 = rule_r->last_guest;

        if (!x86_instr_test_branch(last_x86)) {
            assemble_arm_exit(rule_r->target_pc);
        } else if (last_x86->opc == X86_OPC_CALL) {
            assemble_arm_exit(0);
        } else if (last_x86->opc == X86_OPC_RET) {
            this->RipReg = ARM_REG_R20;
            assemble_arm_exit(0);
        } else if (last_x86->opc == X86_OPC_JMP) {
            if (last_x86->opd_num && last_x86->opd[0].type == X86_OPD_TYPE_IMM
                    && last_x86->opd[0].content.imm.isRipLiteral) {
                this->RipReg = ARM_REG_R20;
            }
            assemble_arm_exit(0);
        } else {
            // False Block
            const auto IsTarget1 = JumpTargets2.try_emplace(this->FalseNewRip).first;
            Bind(&IsTarget1->second);
            assemble_arm_exit(this->FalseNewRip);
            // True Block
            const auto IsTarget2 = JumpTargets2.try_emplace(this->TrueNewRip).first;
            Bind(&IsTarget2->second);
            assemble_arm_exit(this->TrueNewRip);
        }
    }
}

bool is_last_access(ARMInstruction *insn, ARMRegister reg)
{
    ARMInstruction *head = arm_host;
    int i;

    while(head && head != insn)
        head = head->next;
    if (!head)
        return true;

    head = head->next;

    while(head) {
        for (i = 0; i < head->opd_num; i++) {
            ARMOperand *opd = &head->opd[i];

            if (opd->type == ARM_OPD_TYPE_REG) {
                if (opd->content.reg.num == reg)
                    return false;
            } else if (opd->type == ARM_OPD_TYPE_MEM) {
                if (opd->content.mem.base == reg)
                    return false;
                if (opd->content.mem.index == reg)
                    return false;
            }
        }
        head = head->next;
    }

    return true;
}
