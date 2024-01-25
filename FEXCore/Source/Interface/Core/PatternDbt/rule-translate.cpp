#include "Interface/Core/JIT/Arm64/JITClass.h"

#include <FEXCore/Utils/LogManager.h>

#include <cstdio>
#include <assert.h>
#include <cstring>

#include "rule-translate.h"
#include "arm-asm.h"

#define MAX_RULE_RECORD_BUF_LEN 800
#define MAX_GUEST_INSTR_LEN 800
#define MAX_HOST_RULE_LEN 800

#define MAX_MAP_BUF_LEN 1000
#define MAX_HOST_RULE_INSTR_LEN 1000

static const ARMOpcode X862ARM[] = {
    [X86_OPC_MOVB]   = ARM_OPC_INVALID,
    [X86_OPC_MOVZBL] = ARM_OPC_LDRB,
    [X86_OPC_MOVSBL] = ARM_OPC_LDRSB,
    [X86_OPC_MOVW]   = ARM_OPC_INVALID,
    [X86_OPC_MOVZWL] = ARM_OPC_LDRH,
    [X86_OPC_MOVSWL] = ARM_OPC_LDRSH,
    [X86_OPC_MOVL]   = ARM_OPC_INVALID,
    [X86_OPC_LEAL]   = ARM_OPC_MOV,
    [X86_OPC_NOTL]   = ARM_OPC_MVN,
    [X86_OPC_ANDB]   = ARM_OPC_ANDS,
    [X86_OPC_ORB]    = ARM_OPC_ORR,
    [X86_OPC_XORB]   = ARM_OPC_EOR,
    [X86_OPC_ANDW]   = ARM_OPC_ANDS,
    [X86_OPC_ORW]    = ARM_OPC_ORR,
    [X86_OPC_ANDL]   = ARM_OPC_ANDS,
    [X86_OPC_ORL]    = ARM_OPC_ORR,
    [X86_OPC_XORL]   = ARM_OPC_EOR,
    [X86_OPC_NEGL]   = ARM_OPC_INVALID,
    [X86_OPC_INCB]   = ARM_OPC_ADDS,
    [X86_OPC_INCL]   = ARM_OPC_ADDS,
    [X86_OPC_DECB]   = ARM_OPC_SUBS,
    [X86_OPC_DECW]   = ARM_OPC_SUBS,
    [X86_OPC_INCW]   = ARM_OPC_ADDS,
    [X86_OPC_DECL]   = ARM_OPC_SUBS,

    [X86_OPC_ADDB]   = ARM_OPC_ADDS,
    [X86_OPC_ADDW]   = ARM_OPC_ADDS,
    [X86_OPC_ADDL]   = ARM_OPC_ADDS,
    [X86_OPC_ADCL]   = ARM_OPC_ADCS,
    [X86_OPC_SUBL]   = ARM_OPC_SUBS,
    [X86_OPC_SBBL]   = ARM_OPC_SBCS,
    [X86_OPC_IMULL]  = ARM_OPC_MUL,
    [X86_OPC_SHLB]   = ARM_OPC_LSL,
    [X86_OPC_SHRB]   = ARM_OPC_LSR,
    [X86_OPC_SHLW]   = ARM_OPC_LSL,
    [X86_OPC_SHLL]   = ARM_OPC_LSL,
    [X86_OPC_SHRL]   = ARM_OPC_LSR,
    [X86_OPC_SARL]   = ARM_OPC_ASR,
    [X86_OPC_SHLDL]  = ARM_OPC_LSL,
    [X86_OPC_SHRDL]  = ARM_OPC_LSR,

    [X86_OPC_BTL]    = ARM_OPC_INVALID,
    [X86_OPC_TESTW]  = ARM_OPC_ANDS,
    [X86_OPC_TESTB]  = ARM_OPC_ANDS,
    [X86_OPC_CMPB]   = ARM_OPC_CMP,
    [X86_OPC_CMPW]   = ARM_OPC_CMP,
    [X86_OPC_TESTL]  = ARM_OPC_ANDS,
    [X86_OPC_CMPL]   = ARM_OPC_CMP,

    [X86_OPC_CMOVNEL]= ARM_OPC_MOV,
    [X86_OPC_CMOVAL] = ARM_OPC_MOV,
    [X86_OPC_CMOVBL] = ARM_OPC_MOV,
    [X86_OPC_CMOVLL] = ARM_OPC_MOV,
    [X86_OPC_SETE]   = ARM_OPC_INVALID,
    [X86_OPC_CWT]    = ARM_OPC_INVALID,

    [X86_OPC_JMP]    = ARM_OPC_B,
    [X86_OPC_JA]     = ARM_OPC_B,
    [X86_OPC_JAE]    = ARM_OPC_B,
    [X86_OPC_JB]     = ARM_OPC_B,
    [X86_OPC_JBE]    = ARM_OPC_B,
    [X86_OPC_JL]     = ARM_OPC_B,
    [X86_OPC_JLE]    = ARM_OPC_B,
    [X86_OPC_JG]     = ARM_OPC_B,
    [X86_OPC_JGE]    = ARM_OPC_B,
    [X86_OPC_JE]     = ARM_OPC_B,
    [X86_OPC_JNE]    = ARM_OPC_B,
    [X86_OPC_JS]     = ARM_OPC_B,
    [X86_OPC_JNS]    = ARM_OPC_B,

    [X86_OPC_PUSH]   = ARM_OPC_INVALID,
    [X86_OPC_POP]    = ARM_OPC_INVALID,
    [X86_OPC_CALL]   = ARM_OPC_BL,
    [X86_OPC_RET]    = ARM_OPC_INVALID,

    [X86_OPC_SET_LABEL] = ARM_OPC_INVALID, // fake instruction to generate label
    [X86_OPC_SYNC_M] = ARM_OPC_INVALID,
    [X86_OPC_SYNC_R] = ARM_OPC_INVALID,
	[X86_OPC_PC_IR]  = ARM_OPC_INVALID,
    [X86_OPC_PC_RR]  = ARM_OPC_INVALID,
	[X86_OPC_MULL]   = ARM_OPC_UMULL,
};


static ImmMapping imm_map_buf[MAX_MAP_BUF_LEN];
static int imm_map_buf_index = 0;

static LabelMapping label_map_buf[MAX_MAP_BUF_LEN];
static int label_map_buf_index = 0;

static GuestRegisterMapping g_reg_map_buf[MAX_MAP_BUF_LEN];
static int g_reg_map_buf_index = 0;

static RuleRecord rule_record_buf[MAX_RULE_RECORD_BUF_LEN];
static int rule_record_buf_index = 0;

static uint64_t pc_matched_buf[MAX_GUEST_INSTR_LEN];
static int pc_matched_buf_index = 0;

static int imm_map_buf_index_pre = 0;
static int g_reg_map_buf_index_pre = 0;
static int label_map_buf_index_pre = 0;

static ImmMapping *imm_map;
static GuestRegisterMapping *g_reg_map;
static LabelMapping *l_map;
static int debug = 1;
static int match_insts = 0;
static int match_counter = 10;

static TranslationRule record_host_buf[MAX_HOST_RULE_LEN];

static X86Instruction record_host_instr_buf[MAX_HOST_RULE_INSTR_LEN];
static int record_host_instr_buf_index = 0;

static uint64_t pc_para_matched_buf[MAX_GUEST_INSTR_LEN];
static int pc_para_matched_buf_index = 0;

static int reg_map_num = 0;

static inline void reset_buffer(void)
{
    imm_map_buf_index = 0;
    label_map_buf_index = 0;
    g_reg_map_buf_index = 0;

    rule_record_buf_index = 0;
    pc_matched_buf_index = 0;

    pc_para_matched_buf_index = 0;
}

static inline void save_map_buf_index(void)
{
    imm_map_buf_index_pre = imm_map_buf_index;
    g_reg_map_buf_index_pre = g_reg_map_buf_index;
    label_map_buf_index_pre = label_map_buf_index;
}

static inline void recover_map_buf_index(void)
{
    imm_map_buf_index = imm_map_buf_index_pre;
    g_reg_map_buf_index = g_reg_map_buf_index_pre;
    label_map_buf_index = label_map_buf_index_pre;
}

static inline void init_map_ptr(void)
{
    imm_map = NULL;
    g_reg_map = NULL;
    l_map = NULL;
    reg_map_num = 0;
}

static inline void add_rule_record(TranslationRule *rule, uint64_t pc, uint64_t t_pc,
                                   int icount, bool update_cc, bool save_cc, int pa_opc[20])
{
    RuleRecord *p = &rule_record_buf[rule_record_buf_index++];

    assert(rule_record_buf_index < MAX_RULE_RECORD_BUF_LEN);

    p->pc = pc;
    p->target_pc = t_pc;
    p->icount = icount;
    p->next_pc = pc + 4 * icount;
    p->rule = rule;
    p->update_cc = update_cc;
    p->save_cc = save_cc;
    p->imm_map = imm_map;
    p->g_reg_map = g_reg_map;
    p->l_map = l_map;
    int i;
    for (i = 0; i < 20; i++)
        p->para_opc[i] = pa_opc[i];
}

// not worked
static bool used_following(X86Instruction *ginstr, int idx, int skip){
    X86Instruction *p_instr = ginstr;
    X86Register reg_name = p_instr->opd[idx].content.reg.num;
    int i, j;
    // skip instructions in this rule
    for (i = 0; i <= skip; i++){
        if (p_instr->next == NULL){
            return false;
        }
        p_instr = p_instr->next;
    }

    // check if this register is used in next 10 instructions
    for (i = 0; i < 10; i++){
        for(j = 0; j < p_instr->opd_num; j++) {
            X86Operand opd = p_instr->opd[j];
            switch (opd.type){
                case X86_OPD_TYPE_REG:
                    if (opd.content.reg.num == reg_name)
                        return true;
                    break;
                case X86_OPD_TYPE_MEM:
                    if (opd.content.mem.base == reg_name)
                        return true;
                    if (opd.content.mem.index == reg_name)
                        return true;
                    break;
                default:
                    break;
            }
        }
        if (p_instr->next == NULL){
            return false;
        }
        p_instr = p_instr->next;
    }
    return false;
}

static inline void add_matched_pc(uint64_t pc)
{
    pc_matched_buf[pc_matched_buf_index++] = pc;

    assert(pc_matched_buf_index < MAX_GUEST_INSTR_LEN);
}

static inline void add_matched_para_pc(uint64_t pc)
{
    pc_para_matched_buf[pc_para_matched_buf_index++] = pc;

    assert(pc_para_matched_buf_index < MAX_GUEST_INSTR_LEN);
}


static bool match_label(char *lab_str, int64_t t, uint64_t f)
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

static bool match_register(X86Register greg, X86Register rreg)
{
    GuestRegisterMapping *gmap = g_reg_map;

    if (greg == X86_REG_INVALID &&
        rreg == X86_REG_INVALID)
        return true;

    if (greg == X86_REG_INVALID ||
        rreg == X86_REG_INVALID)
    {
        if (debug)
            LogMan::Msg::IFmt( "Unmatch reg: invalid reg\n");
        return false;
    }

    /* check if we already have this map */
    while (gmap)
    {
        if (gmap->sym != rreg)
        {
            gmap = gmap->next;
            continue;
        }
        if (debug && (gmap->num != greg))
            fprintf(stderr, "Unmatch reg: map conflict: %d %d\n", gmap->num, greg);
        return (gmap->num == greg);
    }

    /* check if this guest register has another map */
    gmap = g_reg_map;
    while (gmap)
    {
        if (gmap->num != greg)
        {
            gmap = gmap->next;
            continue;
        }

        if (debug && (gmap->sym != rreg))
            fprintf(stderr, "Unmatch reg: have anther map: %d %d %d\n", greg, gmap->sym, rreg);
        return (gmap->sym == rreg);
    }

    /* append this map to register map buffer */
    gmap = &g_reg_map_buf[g_reg_map_buf_index++];
    assert(g_reg_map_buf_index < MAX_MAP_BUF_LEN);
    gmap->sym = rreg;
    gmap->num = greg;
    ++reg_map_num;

    gmap->next = g_reg_map;
    g_reg_map = gmap;

    return true;
}

static bool match_imm(int32_t val, char *sym)
{
    ImmMapping *imap = imm_map;

    while(imap) {
        if (strcmp(sym, imap->imm_str)){
            if (debug && (val != imap->imm_val))
                LogMan::Msg::IFmt( "Unmatch imm: symbol map conflict {} {}\n", imap->imm_val, val);
            return (val == imap->imm_val);
        }

        imap = imap->next;
    }

    /* check if another immediate symbol map to the same value */
    imap = imm_map;
    while(imap) {
        if (imap->imm_val == val){
            if (debug)
                LogMan::Msg::IFmt( "Unmatch imm: another symbol map {}\n", val);
            return false;
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

static bool match_scale(X86Imm *gscale, X86Imm *rscale)
{
    if (gscale->type == X86_IMM_TYPE_NONE &&
        rscale->type == X86_IMM_TYPE_NONE)
        return true;

    if (rscale->type == X86_IMM_TYPE_VAL){
        if (debug && (gscale->content.val != rscale->content.val))
            LogMan::Msg::IFmt( "Unmatch scale value: {} {}\n", gscale->content.val, rscale->content.val);
        return gscale->content.val == rscale->content.val;
    }
    else if (rscale->type == X86_IMM_TYPE_NONE)
        return match_imm(0, rscale->content.sym);
    else
        return match_imm(gscale->content.val, rscale->content.sym);
}

static bool match_offset(X86Imm *goffset, X86Imm *roffset)
{
    int32_t off_val;
    char *sym;

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
        roffset->type == X86_IMM_TYPE_NONE){
        if (debug) {
            LogMan::Msg::IFmt( "Unmatch offset: none\n");
        }
        return false;
    }

    sym = roffset->content.sym;
    off_val = goffset->content.val;

    return match_imm(off_val, sym);
}

static bool match_opd_imm(X86ImmOperand *gopd, X86ImmOperand *ropd)
{
    if (gopd->type == X86_IMM_TYPE_NONE && ropd->type == X86_IMM_TYPE_NONE)
        return true;

    if (ropd->type == X86_IMM_TYPE_VAL)
        return (gopd->content.val == ropd->content.val);
    else if (ropd->type == X86_IMM_TYPE_SYM)
        return match_imm(gopd->content.val, ropd->content.sym);
    else{
        if (debug)
            LogMan::Msg::IFmt( "Unmatch imm: type error\n");
        return false;
    }
}

static bool match_opd_reg(X86RegOperand *gopd, X86RegOperand *ropd)
{
    return (match_register(gopd->num, ropd->num));
}

static bool match_opd_mem(X86MemOperand *gopd, X86MemOperand *ropd)
{
    return (match_register(gopd->base, ropd->base) &&
            match_register(gopd->index, ropd->index) &&
            match_offset(&gopd->offset, &ropd->offset) &&
            match_scale(&gopd->scale, &ropd->scale));
}

/* Try to match operand in guest instruction (gopd) and and the rule (ropd) */
static bool match_operand(X86Instruction *ginstr, X86Instruction *rinstr, int opd_idx)
{
    X86Operand *gopd = &ginstr->opd[opd_idx];
    X86Operand *ropd = &rinstr->opd[opd_idx];

    if (gopd->type != ropd->type)
        return false;

	if (ginstr->opc == X86_OPC_CALL) {
		unsigned int pc_val = ginstr->pc + ginstr->InstSize;
		ImmMapping * imap = &imm_map_buf[imm_map_buf_index++];
		assert(imm_map_buf_index < MAX_MAP_BUF_LEN);
   		strcpy(imap->imm_str, "imm_pc");
    	imap->imm_val = pc_val;
    	imap->next = imm_map;
    	imm_map = imap;
	}


    if (ropd->type == X86_OPD_TYPE_IMM) {
        if (x86_instr_test_branch(rinstr)) {
            assert(ropd->content.imm.type == X86_IMM_TYPE_SYM);
            return match_label(ropd->content.imm.content.sym, gopd->content.imm.content.val, ginstr->pc + ginstr->InstSize);
        } else /* match imm operand */
            return match_opd_imm(&gopd->content.imm, &ropd->content.imm);
    } else if (ropd->type == X86_OPD_TYPE_REG) {
        return match_opd_reg(&gopd->content.reg, &ropd->content.reg);
    } else if (ropd->type == X86_OPD_TYPE_MEM) {
        return match_opd_mem(&gopd->content.mem, &ropd->content.mem);
    } else
        fprintf(stderr, "Error: unsupported arm operand type: %d\n", ropd->type);

    return true;
}


static bool check_instr(X86Instruction *ginstr){
    // int i;

    //pc register can not be translated by rules now
    // for (i = 0; i < ginstr->opd_num; i++){
    //     switch (ginstr->opd[i].type){
    //         case ARM_OPD_TYPE_REG:
    //             if (ginstr->opd[i].content.reg.num == X86_REG_R15)
    //                 return false;
    //             break;
    //         case ARM_OPD_TYPE_MEM:
    //             if (ginstr->opd[i].content.mem.base == X86_REG_R15)
    //                 return false;
    //             if (ginstr->opd[i].content.mem.index == X86_REG_R15)
    //                 return false;
    //             break;
    //     }
    // }
    return true;
}

// return:
// 0: not matched
// 1: matched
// 2: matched but condition is different

static bool match_rule_internal(X86Instruction *instr, TranslationRule *rule, FEXCore::Frontend::Decoder::DecodedBlocks const *tb)
{
    X86Instruction *p_rule_instr = rule->x86_guest;
    X86Instruction *p_guest_instr = instr;
    X86Instruction *last_guest_instr = NULL;
    int i;

    int j = 0;
    /* init for this rule */
    init_map_ptr();

    while(p_rule_instr) {

        if (p_rule_instr->opc == X86_OPC_INVALID ||
            p_guest_instr->opc == X86_OPC_INVALID){
            return false;
        }

        /* check opcode and number of operands */
        if (((p_rule_instr->opc != p_guest_instr->opc) && (opc_set[p_guest_instr->opc] != p_rule_instr->opc)) || //opcode not equal
            ((p_rule_instr->opd_num != 0) && (p_rule_instr->opd_num != p_guest_instr->opd_num))) { //operand not equal

            if (debug){
                if (p_rule_instr->opd_num != p_guest_instr->opd_num)
                    LogMan::Msg::IFmt( "different operand number\n");
            }

            return false;
        }

        /*check parameterized instructions*/
        if ((p_rule_instr->opd_num == 0) && !check_instr(p_guest_instr)){
            if (debug){
                LogMan::Msg::IFmt( "parameterization check error\n");
            }
            return false;
        }

        /* match each operand */
        for(i = 0; i < p_rule_instr->opd_num; i++) {
            if (!match_operand(p_guest_instr, p_rule_instr, i)) {
                if (debug){
                    fprintf(stderr, "guest->type: %d\n", p_guest_instr->opd[i].type);
                    fprintf(stderr, "rule->type: %d\n", p_rule_instr->opd[i].type);
                    if (p_guest_instr->opd[i].type != p_rule_instr->opd[i].type){
                        LogMan::Msg::IFmt( "Unmatched operand :");
                        print_x86_instr(p_guest_instr);
                        print_x86_instr(p_rule_instr);
                    }
                }
                return false;
            }
        }

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
            (p_reg_liveness[X86_REG_ZF] && (rule->x86_cc_mapping[X86_ZF] == 0))){

                if (debug) {
                    LogMan::Msg::IFmt( "Different liveness cc!\n");
                }
                return false;
            }
    }

    return true;
}

void get_label_map(char *lab_str, int64_t *t, uint64_t *f)
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

extern int parse_str_to_int(char *s);
int32_t get_imm_map(char *sym)
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
            //LogMan::Msg::IFmt( "\t found a matched string for {}, value: {}\n", im->imm_str, im->imm_val);
            size_t len = (size_t)(p_str - t_str);
            strncpy(t_buf, t_str, len);
            sprintf(t_buf + len, "%d", im->imm_val);
            strncat(t_buf, t_str + len + strlen(im->imm_str), strlen(t_str) - len - strlen(im->imm_str));
            strcpy(t_str, t_buf);
            p_str = strstr(t_str, im->imm_str);
        }
        im = im->next;
    }
    //LogMan::Msg::IFmt( "=================val str: {}\n", t_str);
    //LogMan::Msg::IFmt( "=================val val: {} %x\n", parse_str_to_int(t_str), parse_str_to_int(t_str));
    return parse_str_to_int(t_str);
}


X86Register get_guest_reg_map(ARMRegister reg)
{
    GuestRegisterMapping *gmap = g_reg_map;

    while (gmap) {
        if (!strcmp(get_arm_reg_str(reg), get_x86_reg_str(gmap->sym))) {
            //LogMan::Msg::IFmt( "x86 reg: {}, arm reg: {}\n", reg, gmap->num);
            return static_cast<X86Register>(gmap->num);
        }

        gmap = gmap->next;
    }

    assert(0);
    return X86_REG_INVALID;
}

bool instr_is_match(uint64_t pc)
{
    int i;
    for (i = 0; i < pc_matched_buf_index; i++) {
        if (pc_matched_buf[i] == pc)
            return true;
    }
    return false;
}

bool instrs_is_match(uint64_t pc)
{
    int i;
    for (i = 0; i < pc_para_matched_buf_index; i++) {
        if (pc_para_matched_buf[i] == pc)
            return true;
    }
    return instr_is_match(pc);
}

bool tb_rule_matched(void)
{
    return (pc_matched_buf_index != 0);
}

bool check_translation_rule(uint64_t pc)
{
    int i;
    for (i = 0; i < rule_record_buf_index; i++) {
        if (rule_record_buf[i].pc == pc)
            return true;
    }
    return false;
}

RuleRecord *get_translation_rule(uint64_t pc)
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

/* For debugging */
#if defined(DEBUG_RULE_TRANSLATION)
static uint64_t skip_pc[] = {
};
static uint64_t take_pc[] = {
};
#endif

#ifdef PROFILE_RULE_TRANSLATION
uint32_t static_inst_rule_counter = 0;
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

static ARMRegister generate_matched_reg(X86Register greg){
    GuestRegisterMapping *gmap = g_reg_map;

    /* check if this guest register has a map */
    while(gmap) {
        if (gmap->num == greg) {
            // X86 reg0 is 19
            //ARM reg0 is 21
            return (ARMRegister)(gmap->sym - 2);
        }
        gmap = gmap->next;
    }

    /* append this map to register map buffer */
    gmap = &g_reg_map_buf[g_reg_map_buf_index++];
    assert(g_reg_map_buf_index < MAX_MAP_BUF_LEN);
    //ARM reg0 start from X86Instruction enum 21
    gmap->sym = (X86Register)(reg_map_num + 21);
    gmap->num = greg;

    gmap->next = g_reg_map;
    g_reg_map = gmap;
    ++reg_map_num;

    return (ARMRegister)(gmap->sym - 2);
}

/* Try to match instructions in this tb with existing rules */
void match_translation_rule(FEXCore::Frontend::Decoder::DecodedBlocks const *tb)
{
    if (match_counter <= 0)
        return;
    X86Instruction *guest_instr = tb->guest_instr;
    X86Instruction *cur_head = guest_instr;
    int guest_instr_num = 0;
    int i, j;
    uint64_t tbpc = guest_instr->pc;

    LogMan::Msg::IFmt( "=====Guest Instr Match Rule Start=====\n");

    LogMan::Msg::IFmt( "Current match pc: {:x}", tbpc);

    reset_buffer();

    #if defined(DEBUG_RULE_TRANSLATION)
    for (i = 0; i < sizeof(skip_pc)/sizeof(skip_pc[0]); i++) {
        if (skip_pc[i] == tbpc)
            return;
    }
    #if 0
    bool take = false;
    for (i = 0; i < sizeof(take_pc)/sizeof(take_pc[0]); i++) {
        if (take_pc[i] == tbpc) {
            take = true;
            break;
        }
    }
    if (!take)
        return;
    #endif
    #endif

    /* Try from the longest rule */
    while (cur_head) {
        if (cur_head->opc == X86_OPC_INVALID) {
            cur_head = cur_head->next;
            guest_instr_num--;
            continue;
        }

        bool opd_para = false;

        if (guest_instr_num <= 0){
            X86Instruction *t_head = cur_head;
            guest_instr_num = 0;
            while (t_head){
                ++guest_instr_num;
                t_head = t_head->next;
            }
        }

        for(i = guest_instr_num; i > 0; i--) {
            /* calculate hash key */
            int hindex = rule_hash_key(cur_head, i);

            if (hindex >= MAX_GUEST_LEN)
                continue;
            /* check rule with length i (number of guest instructions) */
            TranslationRule *cur_rule = cache_rule_table[hindex];

            save_map_buf_index();
            while(cur_rule) {

                if (cur_rule->guest_instr_num != i)
                    goto next;

                // LogMan::Msg::IFmt( "========starting matching internal, rule: {}=======\n", cur_rule->index);
                if (match_rule_internal(cur_head, cur_rule, tb)) {
                    #if 0
                    if (cur_rule->index == 9998) {
                        static int counter = 0;
                        counter++;
                        if (counter > 594)
                            goto next;
                    }
                    #endif
                    LogMan::Msg::IFmt( "##### match rule {} success! #####\n", cur_rule->index);
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

                // match_counter -= 1;
                match_insts += i;

                #ifdef PROFILE_RULE_TRANSLATION
                tb->num_rules_hit++;
                static_inst_rule_counter += i;
                tb->num_insts_match += i;
                cur_rule->hit_num++;
                #endif

                #ifdef DEBUG_RULE_TRANSLATION
                if (cur_rule->index == 0)
                    LogMan::Msg::IFmt( "    0x{:x},\n", tbpc);
                if (tbpc == debug_pc)
                    LogMan::Msg::IFmt( "== Hit a rule [{}] at tb: 0x{:x} instr: 0x{:x}, num_insns: {}, update_cc: {}",
                                cur_rule->index, tbpc, cur_head->pc, i, is_update_cc(cur_head, i) ? "true" : "false");
                #endif
                /* Check target_pc for this rule */
                for (j = 1; j < i; j++)
                    temp = temp->next;
                if (!temp->next && !x86_instr_test_branch(temp))
                    target_pc = temp->pc + temp->InstSize;

                X86Instruction *p_rule_instr = cur_rule->x86_guest;
                X86Instruction *p_guest_instr = cur_head;
                int pa_opc[20];
                memset(pa_opc, X86_OPC_INVALID, sizeof(int)*20);
                j = 0;

                while (p_rule_instr){
                    if ((p_rule_instr->opc >= X86_OPC_OP1) && (p_rule_instr->opc < X86_OPC_END)){
                        if (p_rule_instr->opd_num == 0){
                            opd_para = true;
                            break;
                        }
                        pa_opc[j] = p_guest_instr->opc;
                        ++j;
                    }
                    p_rule_instr = p_rule_instr->next;
                    p_guest_instr = p_guest_instr->next;
                }
                if (!opd_para){
                    add_rule_record(cur_rule , cur_head->pc, target_pc, i,
                        true, is_save_cc(cur_head, i), pa_opc);
                }

                /* We get a matched rule, keep moving forward */
                if (opd_para){
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

                break;
            }
            // para_fault:
            recover_map_buf_index();
        }

        /* No matched rule found, also keep moving forward */
        if (i == 0) {
            /* print unmatched instructions
               if not continuous, record as a new block */
            cur_head = cur_head->next;
            guest_instr_num--;
        }
    }
    LogMan::Msg::IFmt( "=====Guest Instr Match Rule End=======\n");
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
void FEXCore::CPU::Arm64JITCore::do_rule_translation(FEXCore::Frontend::Decoder::DecodedBlocks const *tb, RuleRecord *rule_r, uint32_t *reg_liveness)
{
    TranslationRule *rule;

    if (!rule_r)
        return;

    rule = rule_r->rule;
    l_map = rule_r->l_map;
    imm_map = rule_r->imm_map;
    g_reg_map = rule_r->g_reg_map;
    int i = 0;

    LogMan::Msg::IFmt( "==========Applying rule: {}\n", rule->index);

    ARMInstruction *arm_code = rule->arm_host;
    arm_host = arm_code;

    /* Assemble host instructions in the rule */
    while(arm_code) {
        if ((arm_code->opc >= ARM_OPC_OP1) && (arm_code->opc < ARM_OPC_END)){
            ARMOpcode temp = arm_code->opc;
            arm_code->opc = X862ARM[rule_r->para_opc[i]];
            ++i;
            // LogMan::Msg::IFmt( "{}\n", arm_code->opc);
            assemble_arm_instruction(tb, arm_code, reg_liveness, rule_r);
            arm_code->opc = temp;
        }
        else
            assemble_arm_instruction(tb, arm_code, reg_liveness, rule_r);
        arm_code = arm_code->next;
    }

    if (rule_r->target_pc != 0) {
        LogMan::Msg::IFmt( "--------- tb->pc: {:x}, target_pc: %lx\n", tb->guest_instr->pc, rule_r->target_pc);
        // assemble_x86_exit_tb(s, rule_r->target_pc);
    }

    /* sync and dead registers to keep consistency with TCG */
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
