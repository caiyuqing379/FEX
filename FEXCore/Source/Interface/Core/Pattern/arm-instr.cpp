/**
 * @file arm-instr.cpp
 * @brief ARM指令处理和操作的实现
 *
 * 本文件包含了用于处理ARM指令的各种函数和数据结构。
 * 主要功能包括ARM指令的解码、编码、打印和操作。
 */

#include <FEXCore/Utils/LogManager.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "arm-instr.h"

/**
 * @brief ARM寄存器枚举数组
 *
 * 定义了ARM架构中所有可用的寄存器。
 */
static const ARMRegister arm_reg_table[] = {
    ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3,
    ARM_REG_R4, ARM_REG_R5, ARM_REG_R6, ARM_REG_R7,
    ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11,
    ARM_REG_R12, ARM_REG_R13, ARM_REG_R14, ARM_REG_R15,
    ARM_REG_R16, ARM_REG_R17, ARM_REG_R18, ARM_REG_R19,
    ARM_REG_R20, ARM_REG_R21, ARM_REG_R22, ARM_REG_R23,
    ARM_REG_R24, ARM_REG_R25, ARM_REG_R26, ARM_REG_R27,
    ARM_REG_R28, ARM_REG_R29, ARM_REG_R30, ARM_REG_R31,
    ARM_REG_V0, ARM_REG_V1, ARM_REG_V2, ARM_REG_V3,
    ARM_REG_V4, ARM_REG_V5, ARM_REG_V6, ARM_REG_V7,
    ARM_REG_V8, ARM_REG_V9, ARM_REG_V10, ARM_REG_V11,
    ARM_REG_V12, ARM_REG_V13, ARM_REG_V14, ARM_REG_V15,
    ARM_REG_V16, ARM_REG_V17, ARM_REG_V18, ARM_REG_V19,
    ARM_REG_V20, ARM_REG_V21, ARM_REG_V22, ARM_REG_V23,
    ARM_REG_V24, ARM_REG_V25, ARM_REG_V26, ARM_REG_V27,
    ARM_REG_V28, ARM_REG_V29, ARM_REG_V30, ARM_REG_V31,
};

/**
 * @brief ARM条件码枚举数组
 *
 * 定义了ARM架构中所有可用的条件码。
 */
static const ARMConditionCode arm_cc_table[] = {
    ARM_CC_EQ, ARM_CC_NE, ARM_CC_CS, ARM_CC_CC, ARM_CC_MI,
    ARM_CC_PL, ARM_CC_VS, ARM_CC_VC, ARM_CC_HI, ARM_CC_LS,
    ARM_CC_GE, ARM_CC_LT, ARM_CC_GT, ARM_CC_LE, ARM_CC_AL,
    ARM_CC_XX
};

/**
 * @brief ARM寄存器的字符串表示
 *
 * 用于将寄存器枚举值转换为对应的字符串表示。
 */
static const char *arm_reg_str[] = {
    "none",
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",

    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
    "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
    "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
    "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",

    "fp", "lr", "rsp", "zr",
    "cf", "nf", "vf", "zf",

    "reg0", "reg1", "reg2", "reg3", "reg4", "reg5", "reg6", "reg7",
    "reg8", "reg9", "reg10", "reg11", "reg12", "reg13", "reg14", "reg15",
    "reg16", "reg17", "reg18", "reg19", "reg20", "reg21", "reg22", "reg23",
    "reg24", "reg25", "reg26", "reg27", "reg28", "reg29", "reg30", "reg31",
};

/**
 * @brief ARM条件码的字符串表示
 *
 * 用于将条件码枚举值转换为对应的字符串表示。
 */
static const char *arm_cc_str[] = {
    "ERROR",
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le", "al", "xx"
};

/**
 * @brief ARM内存操作的索引类型字符串表示
 */
static const char *arm_index_type_str[] = {
    "ERROR", "PRE", "POST"
};

/**
 * @brief ARM移位操作的方向字符串表示
 */
static const char *arm_direct_str[] = {
    "ERROR", "lsl", "lsr", "asr", "ror"
};

/**
 * @brief ARM扩展操作的字符串表示
 */
[[maybe_unused]] static const char *arm_extend_str[] = {
    "ERROR",
    "uxtb", "uxth", "uxtw", "uxtx",
    "sxtb", "sxth", "sxtw", "sxtx",
    "lsl"
};

/**
 * @brief ARM操作码的字符串表示
 *
 * 用于将操作码枚举值转换为对应的字符串表示。
 */
static const char *arm_opc_str[] = {
    [ARM_OPC_INVALID] = "**** unsupported (arm) ****",
    [ARM_OPC_LDRB]  = "ldrb",
    [ARM_OPC_LDRSB] = "ldrsb",
    [ARM_OPC_LDRH]  = "ldrh",
    [ARM_OPC_LDRSH] = "ldrsh",
    [ARM_OPC_LDR]   = "ldr",
    [ARM_OPC_LDAR]  = "ldar",
    [ARM_OPC_LDP]   = "ldp",
    [ARM_OPC_STRB]  = "strb",
    [ARM_OPC_STRH]  = "strh",
    [ARM_OPC_STR]   = "str",
    [ARM_OPC_STP]   = "stp",
    [ARM_OPC_SXTW]  = "sxtw",
    [ARM_OPC_MOV]   = "mov",
    [ARM_OPC_MVN]   = "mvn",
    [ARM_OPC_CSEL]  = "csel",
    [ARM_OPC_CSET]  = "cset",
    [ARM_OPC_BFXIL] = "bfxil",
    [ARM_OPC_NEG]   = "neg",
    [ARM_OPC_AND]   = "and",
    [ARM_OPC_ANDS]  = "ands",
    [ARM_OPC_ORR]   = "orr",
    [ARM_OPC_EOR]   = "eor",
    [ARM_OPC_BIC]   = "bic",
    [ARM_OPC_BICS]  = "bics",
    [ARM_OPC_LSL]   = "lsl",
    [ARM_OPC_LSR]   = "lsr",
    [ARM_OPC_ASR]   = "asr",
    [ARM_OPC_ADD]   = "add",
    [ARM_OPC_ADC]   = "adc",
    [ARM_OPC_SUB]   = "sub",
    [ARM_OPC_SBC]   = "sbc",
    [ARM_OPC_ADDS]  = "adds",
    [ARM_OPC_ADCS]  = "adcs",
    [ARM_OPC_SUBS]  = "subs",
    [ARM_OPC_SBCS]  = "sbcs",
    [ARM_OPC_MUL]   = "mul",
    [ARM_OPC_UMULL] = "umull",
    [ARM_OPC_SMULL] = "smull",
    [ARM_OPC_CLZ]   = "clz",
    [ARM_OPC_TST]   = "tst",
    [ARM_OPC_CMP]   = "cmp",
    [ARM_OPC_CMPB]  = "cmpb",
    [ARM_OPC_CMPW]  = "cmpw",
    [ARM_OPC_CMN]   = "cmn",
    [ARM_OPC_B]     = "b",
    [ARM_OPC_BL]    = "bl",
    [ARM_OPC_CBZ]   = "cbz",
    [ARM_OPC_CBNZ]  = "cbnz",

    [ARM_OPC_SET_JUMP] = "set_jump",
    [ARM_OPC_SET_CALL] = "set_call",
    [ARM_OPC_PC_L]  = "pc_l",
    [ARM_OPC_PC_LB] = "pc_lb",
    [ARM_OPC_PC_LW] = "pc_lw",
    [ARM_OPC_PC_S]  = "pc_s",
    [ARM_OPC_PC_SB] = "pc_sb",
    [ARM_OPC_PC_SW] = "pc_sw",

    // FP/NEON
    [ARM_OPC_ADDP]  = "addp",
    [ARM_OPC_CMEQ]  = "cmeq",
    [ARM_OPC_CMGT]  = "cmgt",
    [ARM_OPC_CMLT]  = "cmlt",
    [ARM_OPC_DUP]   = "dup",
    [ARM_OPC_EXT]   = "ext",
    [ARM_OPC_FMOV]  = "fmov",
    [ARM_OPC_INS]   = "ins",
    [ARM_OPC_LD1]   = "ld1",
    [ARM_OPC_SQXTUN] = "sqxtun",
    [ARM_OPC_SQXTUN2] = "sqxtun2",
    [ARM_OPC_UMOV]  = "umov",
    [ARM_OPC_ZIP1]  = "zip1",
    [ARM_OPC_ZIP2]  = "zip2"
};

/**
 * @brief 打印指令的条件码
 *
 * @param instr 指向ARMInstruction结构的指针
 */
static void print_instr_cc(ARMInstruction *instr)
{
    if (instr->cc != ARM_CC_INVALID)
        fprintf(stderr,"cc:%s \n", arm_cc_str[instr->cc]);
}

/**
 * @brief 打印立即数操作数
 *
 * @param opd 指向ARMImmOperand结构的指针
 */
void print_imm_opd(ARMImmOperand *opd)
{
    if (opd->type == ARM_IMM_TYPE_VAL)
        fprintf(stderr,"0x%x \n", opd->content.val);
    else if (opd->type == ARM_IMM_TYPE_SYM)
        fprintf(stderr,"%s \n", opd->content.sym);
}

/**
 * @brief 打印操作数的索引类型（前索引或后索引）
 *
 * @param pre_post 索引类型
 */
static void print_opd_index_type(ARMMemIndexType pre_post)
{
    if (pre_post != ARM_MEM_INDEX_TYPE_NONE)
        fprintf(stderr,", index type: %s\n", arm_index_type_str[pre_post]);
}

/**
 * @brief 打印操作数的比例因子
 *
 * @param scale 指向ARMOperandScale结构的指针
 */
void print_opd_scale(ARMOperandScale *scale)
{
    if (scale->type == ARM_OPD_SCALE_TYPE_SHIFT || scale->type == ARM_OPD_SCALE_TYPE_EXT) {
        ARMImm *imm = &scale->imm;
        if (imm->type == ARM_IMM_TYPE_VAL) {
            if (imm->content.val != 0)
                fprintf(stderr,", %s %d ", arm_direct_str[scale->content.direct], imm->content.val);
        } else
            fprintf(stderr,", %s %s ", arm_direct_str[scale->content.direct], imm->content.sym);
    }
    else
        fprintf(stderr," none scale\n");
}

/**
 * @brief 打印寄存器操作数
 *
 * @param opd 指向ARMRegOperand结构的指针
 */
void print_reg_opd(ARMRegOperand *opd)
{
    fprintf(stderr,"%s ", arm_reg_str[opd->num]);
    print_opd_scale(&opd->scale);
}

/**
 * @brief 打印内存操作数
 *
 * @param opd 指向ARMMemOperand结构的指针
 */
void print_mem_opd(ARMMemOperand *opd)
{
    fprintf(stderr,"[base: %s", arm_reg_str[opd->base]);
    if (opd->index != ARM_REG_INVALID)
        fprintf(stderr,", index: %s", arm_reg_str[opd->index]);
    if (opd->offset.type == ARM_IMM_TYPE_VAL)
        fprintf(stderr,", offset: 0x%x", opd->offset.content.val);
    else if (opd->offset.type == ARM_IMM_TYPE_SYM)
        fprintf(stderr,", offset: %s", opd->offset.content.sym);
    print_opd_scale(&opd->scale);
    print_opd_index_type(opd->pre_post);
}

/**
 * @brief 打印ARM指令序列
 *
 * @param instr_seq 指向ARMInstruction结构的指针，表示指令序列的起始
 */
void print_arm_instr_seq(ARMInstruction *instr_seq)
{
    ARMInstruction *head = instr_seq;
    int i;

    while(head) {
        fprintf(stderr,"0x%lx: %s (%ld)", head->pc,
                arm_opc_str[head->opc], head->opd_num);

        print_instr_cc(head);
        for (i = 0; i < head->opd_num; i++) {
            ARMOperand *opd = &head->opd[i];
            if (opd->type == ARM_OPD_TYPE_INVALID)
                continue;

            if (opd->type == ARM_OPD_TYPE_IMM)
                print_imm_opd(&opd->content.imm);
            else if (opd->type == ARM_OPD_TYPE_REG)
                print_reg_opd(&opd->content.reg);
            else if (opd->type == ARM_OPD_TYPE_MEM)
                print_mem_opd(&opd->content.mem);
        }
        fprintf(stderr,"\n");

        head = head->next;
    }
}

/**
 * @brief 打印单条ARM指令
 *
 * @param instr_seq 指向ARMInstruction结构的指针，表示要打印的指令
 */
void print_arm_instr(ARMInstruction *instr_seq)
{
    ARMInstruction *head = instr_seq;
    int i;

    fprintf(stderr,"%s ", arm_opc_str[head->opc]);
    print_instr_cc(head);
    for (i = 0; i < head->opd_num; i++) {
        ARMOperand *opd = &head->opd[i];
        if (opd->type == ARM_OPD_TYPE_INVALID)
            continue;

        if (opd->type == ARM_OPD_TYPE_IMM)
            print_imm_opd(&opd->content.imm);
        else if (opd->type == ARM_OPD_TYPE_REG)
            print_reg_opd(&opd->content.reg);
        else if (opd->type == ARM_OPD_TYPE_MEM)
            print_mem_opd(&opd->content.mem);
    }
    fprintf(stderr,"\n");
}

/**
 * @brief 获取ARM指令操作码的字符串表示
 *
 * @param opc ARM操作码枚举值
 * @return const char* 操作码的字符串表示
 */
const char *get_arm_instr_opc(ARMOpcode opc)
{
    return arm_opc_str[opc];
}

/**
 * @brief 从字符串获取ARM操作码
 *
 * @param opc_str 操作码的字符串表示
 * @return ARMOpcode 对应的ARM操作码枚举值
 */
static ARMOpcode get_arm_opcode(char *opc_str)
{
    int i;
    for (i = ARM_OPC_INVALID; i < ARM_OPC_END; i++) {
        if (!strcmp(opc_str, arm_opc_str[i]))
            return static_cast<ARMOpcode>(i);
    }
    LogMan::Msg::EFmt("[ARM] Error: unsupported opcode: {}", opc_str);
    exit(0);
    return ARM_OPC_INVALID;
}

/**
 * @brief 从字符串获取ARM寄存器
 *
 * @param reg_str 寄存器的字符串表示
 * @return ARMRegister 对应的ARM寄存器枚举值
 */
static ARMRegister get_arm_register(char *reg_str)
{
    int reg;
    for (reg = ARM_REG_INVALID; reg < ARM_REG_END; reg++) {
        if (!strcmp(reg_str, arm_reg_str[reg]))
            return static_cast<ARMRegister>(reg);
    }
    return ARM_REG_INVALID;
}

/**
 * @brief 从字符串获取ARM操作数的移位方向
 *
 * @param direct_str 移位方向的字符串表示
 * @return ARMOperandScaleDirect 对应的移位方向枚举值
 */
static ARMOperandScaleDirect get_arm_direct(char *direct_str)
{
    int direct;
    for (direct = ARM_OPD_DIRECT_LSL; direct < ARM_OPD_DIRECT_END; direct++) {
        if (!strcmp(direct_str, arm_direct_str[direct]))
            return static_cast<ARMOperandScaleDirect>(direct);
    }
    return ARM_OPD_DIRECT_NONE;
}

/**
 * @brief 从字符串获取ARM操作数的扩展类型
 *
 * @param extend_str 扩展类型的字符串表示
 * @return ARMOperandScaleExtend 对应的扩展类型枚举值
 */
static ARMOperandScaleExtend get_arm_extend(char *extend_str)
{
    int direct;
    for (direct = ARM_OPD_EXTEND_UXTB; direct < ARM_OPD_EXTEND_END; direct++) {
        if (!strcmp(extend_str, arm_direct_str[direct]))
            return static_cast<ARMOperandScaleExtend>(direct);
    }
    return ARM_OPD_EXTEND_NONE;
}

/**
 * @brief 从操作码字符串获取条件码
 *
 * @param opc_str 操作码字符串
 * @return ARMConditionCode 对应的条件码枚举值
 */
ARMConditionCode get_arm_cc(char *opc_str)
{
    int i;
    size_t len = strlen(opc_str);

    /* Less than 2, so guess it is always executed */
    if (len < 3)
        return ARM_CC_AL;

    for (i = ARM_CC_INVALID; i < ARM_CC_END; i++) {
        if (opc_str[len-1] == '\n') {
            char ccStr[2];
            strncpy(ccStr, opc_str + len - 3, 2);
            if (!strcmp(arm_cc_str[i], ccStr)) {
               opc_str[len-5] = '\n';
               return static_cast<ARMConditionCode>(i);
            }
        } else if (opc_str[len-3] == '.' && !strcmp(arm_cc_str[i], &opc_str[len-2])) {
            opc_str[len-3] = '\0';
            return static_cast<ARMConditionCode>(i);
        }
    }

    /* No conditional code, so always executed */
    return ARM_CC_AL;
}

/**
 * @brief 设置ARM指令的条件码
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param cond 条件码值
 */
void set_arm_instr_cc(ARMInstruction *instr, uint32_t cond)
{
    instr->cc = arm_cc_table[cond];
}

/**
 * @brief 设置ARM指令的操作码
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opc 操作码枚举值
 */
void set_arm_instr_opc(ARMInstruction *instr, ARMOpcode opc)
{
    instr->opc = opc;
}

/**
 * @brief 从字符串设置ARM指令的操作码
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opc_str 操作码的字符串表示
 */
void set_arm_instr_opc_str(ARMInstruction *instr, char *opc_str)
{
    instr->cc = get_arm_cc(opc_str);
    instr->opc = get_arm_opcode(opc_str);
}

/**
 * @brief 设置ARM指令的操作数数量
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param num 操作数的数量
 */
void set_arm_instr_opd_num(ARMInstruction *instr, size_t num)
{
    instr->opd_num = num;
}

/**
 * @brief 设置ARM指令的操作数大小
 *
 * @param instr 指向ARMInstruction结构的指针
 */
void set_arm_instr_opd_size(ARMInstruction *instr)
{
    if (instr->opc == ARM_OPC_LDRB || instr->opc == ARM_OPC_STRB)
      instr->OpSize = 1;
    else if (instr->opc == ARM_OPC_LDRH || instr->opc == ARM_OPC_STRH)
      instr->OpSize = 2;
    else if (instr->opc == ARM_OPC_SXTW)
      instr->OpSize = 4;
}

/**
 * @brief 设置ARM指令特定操作数的类型
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param type 操作数的类型
 */
void set_arm_instr_opd_type(ARMInstruction *instr, int opd_index, ARMOperandType type)
{
    set_arm_opd_type(&instr->opd[opd_index], type);
}

/**
 * @brief 设置ARM指令特定操作数为立即数
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param val 立即数值
 */
void set_arm_instr_opd_imm(ARMInstruction *instr, int opd_index, uint32_t val)
{
    ARMOperand *opd = &instr->opd[opd_index];

    opd->type = ARM_OPD_TYPE_IMM;

    opd->content.imm.type = ARM_IMM_TYPE_VAL;
    opd->content.imm.content.val = val;
}

/**
 * @brief 设置ARM指令特定操作数为寄存器
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param regno 寄存器编号
 */
void set_arm_instr_opd_reg(ARMInstruction *instr, int opd_index, int regno)
{
    ARMOperand *opd = &instr->opd[opd_index];

    opd->type = ARM_OPD_TYPE_REG;
    opd->content.reg.num = arm_reg_table[regno];
}

/**
 * @brief 从字符串设置ARM指令特定操作数为寄存器
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param reg_str 寄存器的字符串表示
 */
void set_arm_instr_opd_reg_str(ARMInstruction *instr, int opd_index, char *reg_str)
{
    ARMOperand *opd = &instr->opd[opd_index];
    size_t len = strlen(reg_str);

    // 处理不同类型的寄存器前缀和后缀，设置相应的操作数大小
    if (reg_str[0] == 'w' || (reg_str[0] == 'r' && reg_str[len-1] == 'w')) {
        if (!opd_index)
          instr->OpSize = 4;
        reg_str[0] = 'r';
    }
    else if (reg_str[0] == 'x' || (reg_str[0] == 'r' && reg_str[len-1] == 'x')) {
        if (!opd_index)
          instr->OpSize = 8;
        reg_str[0] = 'r';
    }
    else if (reg_str[0] == 'q') {
        if (!opd_index)
          instr->OpSize = 16;
        reg_str[0] = 'v';
    }
    else if (!opd_index && len >= 5) {
         // 处理向量寄存器的不同元素大小
        if (reg_str[len-3] == '1' && reg_str[len-2] == '6' && reg_str[len-1] == 'b') {
          instr->OpSize = 16;
          instr->ElementSize = 1;
        }
        else if (reg_str[len-2] == '8' && reg_str[len-1] == 'h') {
          instr->OpSize = 16;
          instr->ElementSize = 2;
        }
        else if (reg_str[len-2] == '4' && reg_str[len-1] == 's') {
          instr->OpSize = 16;
          instr->ElementSize = 4;
        }
        else if (reg_str[len-2] == '2' && reg_str[len-1] == 'd') {
          instr->OpSize = 16;
          instr->ElementSize = 8;
        }
        else if (reg_str[len-2] == '8' && reg_str[len-1] == 'b') {
          instr->OpSize = 8;
          instr->ElementSize = 1;
        }
        else if (reg_str[len-2] == '4' && reg_str[len-1] == 'h') {
          instr->OpSize = 8;
          instr->ElementSize = 2;
        }
        else if (reg_str[len-2] == '2' && reg_str[len-1] == 's') {
          instr->OpSize = 8;
          instr->ElementSize = 4;
        }
    }

    // 处理特定指令的元素索引
    if (len >= 5 && (instr->opc == ARM_OPC_UMOV || instr->opc == ARM_OPC_LD1 || instr->opc == ARM_OPC_INS)) {
        if (reg_str[len-4] == 'h' && reg_str[len-3] == '[' && reg_str[len-1] == ']') {
          instr->ElementSize = 2;
          opd->content.reg.Index = atoi(&reg_str[len-2]);
        } else if (reg_str[len-5] == 'd' && reg_str[len-3] == '[' && reg_str[len-1] == ']') {
          instr->ElementSize = 8;
          opd->content.reg.Index = atoi(&reg_str[len-2]);
        } else if (reg_str[len-4] == 'd' && reg_str[len-3] == '[' && reg_str[len-1] == ']') {
          instr->ElementSize = 8;
          opd->content.reg.Index = atoi(&reg_str[len-2]);
        }
    }

    // 移除寄存器名后的点和任何后缀
    char *dotPos = std::strchr(reg_str, '.');
    if (dotPos != nullptr)
        *dotPos = '\0';

    // 处理零寄存器的特殊情况
    if (!strcmp("rzr", reg_str))
       reg_str = "zr";

    opd->type = ARM_OPD_TYPE_REG;
    opd->content.reg.num = get_arm_register(reg_str);
}

/**
 * @brief 从字符串设置ARM指令操作数的比例因子
 *
 * @param pscale 指向ARMOperandScale结构的指针
 * @param direct_str 移位或扩展操作的字符串表示
 * @return bool 如果设置成功返回true，否则返回false
 */
bool set_arm_instr_opd_scale_str(ARMOperandScale *pscale, char *direct_str)
{
    pscale->content.direct = get_arm_direct(direct_str);
    pscale->content.extend = get_arm_extend(direct_str);
    if (pscale->content.direct != ARM_OPD_DIRECT_NONE)
        pscale->type = ARM_OPD_SCALE_TYPE_SHIFT;
    else if (pscale->content.extend != ARM_OPD_EXTEND_NONE)
        pscale->type = ARM_OPD_SCALE_TYPE_EXT;

    return (pscale->content.direct == ARM_OPD_DIRECT_NONE && pscale->content.extend == ARM_OPD_EXTEND_NONE);
}

/**
 * @brief 从字符串设置ARM指令操作数的比例因子立即数
 *
 * @param pscale 指向ARMOperandScale结构的指针
 * @param scale_str 比例因子的字符串表示
 */
void set_arm_instr_opd_scale_imm_str(ARMOperandScale *pscale, char *scale_str)
{
    if (strstr(scale_str, "imm")) { /* this is a symbol scale: imm_xxx */
        pscale->imm.type = ARM_IMM_TYPE_SYM;
        strcpy(pscale->imm.content.sym, scale_str);
    } else { /* this is a value scale */
        pscale->imm.type = ARM_IMM_TYPE_VAL;
        pscale->imm.content.val = strtol(scale_str, NULL, 10);
    }
}

/**
 * @brief 设置ARM指令内存操作数的基址寄存器
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param regno 寄存器编号
 */
void set_arm_instr_opd_mem_base(ARMInstruction *instr, int opd_index, int regno)
{
    ARMMemOperand *mopd = &(instr->opd[opd_index].content.mem);

    mopd->base = arm_reg_table[regno];
}

/**
 * @brief 从字符串设置ARM指令内存操作数的基址寄存器
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param reg_str 寄存器的字符串表示
 */
void set_arm_instr_opd_mem_base_str(ARMInstruction *instr, int opd_index, char *reg_str)
{
    ARMMemOperand *mopd = &(instr->opd[opd_index].content.mem);

    if (reg_str[0] == 'w' || reg_str[0] == 'x') {
        reg_str[0] = 'r';
    }

    mopd->base = get_arm_register(reg_str);

}

/**
 * @brief 设置ARM指令内存操作数的索引寄存器
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param regno 寄存器编号
 */
void set_arm_instr_opd_mem_index(ARMInstruction *instr, int opd_index, int regno)
{
    ARMMemOperand *mopd = &(instr->opd[opd_index].content.mem);

    mopd->index = arm_reg_table[regno];
}

/**
 * @brief 从字符串设置ARM指令内存操作数的索引寄存器
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param reg_str 寄存器的字符串表示
 */
void set_arm_instr_opd_mem_index_str(ARMInstruction *instr, int opd_index, char *reg_str)
{
    ARMMemOperand *mopd = &(instr->opd[opd_index].content.mem);

    mopd->index = get_arm_register(reg_str);
}

/**
 * @brief 设置ARM指令内存操作数的索引类型（前索引或后索引）
 *
 * @param instr 指向ARMInstruction结构的指针
 * @param opd_index 操作数的索引
 * @param type 索引类型
 */
void set_arm_instr_opd_mem_index_type(ARMInstruction *instr, int opd_index, ARMMemIndexType type)
{
    ARMMemOperand *mopd = &(instr->opd[opd_index].content.mem);

    mopd->pre_post = type;
}

/**
 * @brief 设置ARM操作数的类型
 *
 * @param opd 指向ARMOperand结构的指针
 * @param type 操作数类型
 */
void set_arm_opd_type(ARMOperand *opd, ARMOperandType type)
{
    ARMMemOperand *mopd;

    /* Do some intialization work here */
    switch(type) {
        case ARM_OPD_TYPE_IMM:
            opd->content.imm.type = ARM_IMM_TYPE_NONE;
            break;
        case ARM_OPD_TYPE_REG:
            opd->content.reg.num = ARM_REG_INVALID;
            opd->content.reg.scale.type = ARM_OPD_SCALE_TYPE_NONE;
            break;
        case ARM_OPD_TYPE_MEM:
            mopd = &opd->content.mem;
            mopd->base = ARM_REG_INVALID;
            mopd->index = ARM_REG_INVALID;
            mopd->offset.type = ARM_IMM_TYPE_NONE;
            mopd->scale.type = ARM_OPD_SCALE_TYPE_NONE;
            mopd->pre_post = ARM_MEM_INDEX_TYPE_NONE;
            break;
        default:
            fprintf(stderr, "Unsupported operand type in ARM: %d\n", type);
    }

    opd->type = type;
}

/**
 * @brief 从字符串设置ARM立即数操作数的值
 *
 * @param opd 指向ARMOperand结构的指针
 * @param imm_str 立即数的字符串表示
 */
void set_arm_opd_imm_val_str(ARMOperand *opd, char *imm_str)
{
    ARMImmOperand *iopd = &opd->content.imm;

    iopd->type = ARM_IMM_TYPE_VAL;
    iopd->content.val = strtol(imm_str, NULL, 16);
}

/**
 * @brief 从字符串设置ARM立即数操作数的符号
 *
 * @param opd 指向ARMOperand结构的指针
 * @param imm_str 立即数的字符串表示
 */
void set_arm_opd_imm_sym_str(ARMOperand *opd, char *imm_str)
{
    ARMImmOperand *iopd = &opd->content.imm;

    iopd->type = ARM_IMM_TYPE_SYM;
    strcpy(iopd->content.sym, imm_str);
}

/**
 * @brief 从字符串设置ARM内存操作数的偏移量值
 *
 * @param opd 指向ARMOperand结构的指针
 * @param off_str 偏移量的字符串表示
 */
void set_arm_opd_mem_off_val(ARMOperand *opd, char *off_str)
{
    ARMMemOperand *mopd = &opd->content.mem;

    mopd->offset.type = ARM_IMM_TYPE_VAL;
    mopd->offset.content.val = strtol(off_str, NULL, 16);
}

/**
 * @brief 从字符串设置ARM内存操作数的偏移量符号
 *
 * @param opd 指向ARMOperand结构的指针
 * @param off_str 偏移量的字符串表示
 */
void set_arm_opd_mem_off_str(ARMOperand *opd, char *off_str)
{
    ARMMemOperand *mopd = &(opd->content.mem);

    mopd->offset.type = ARM_IMM_TYPE_SYM;
    strcpy(mopd->offset.content.sym, off_str);
}

/**
 * @brief 设置ARM内存操作数的索引寄存器
 *
 * @param opd 指向ARMOperand结构的指针
 * @param regno 寄存器编号
 */
void set_arm_opd_mem_index_reg(ARMOperand *opd, int regno)
{
    ARMMemOperand *mopd = &(opd->content.mem);

    mopd->index = arm_reg_table[regno];
}

/**
 * @brief 获取ARM寄存器
 *
 * @param regno 寄存器编号
 * @return ARMRegister 对应的ARM寄存器枚举值
 */
ARMRegister get_arm_reg(int regno)
{
    return arm_reg_table[regno];
}

/**
 * @brief 获取ARM寄存器的字符串表示
 *
 * @param reg ARM寄存器枚举值
 * @return const char* 寄存器的字符串表示
 */
const char *get_arm_reg_str(ARMRegister reg)
{
    return arm_reg_str[reg];
}

/**
 * @brief 根据PC值获取ARM指令
 *
 * 在给定的指令序列中查找特定PC值对应的指令。
 *
 * @param insn_seq 指向ARMInstruction结构的指针，表示指令序列的起始
 * @param pc 要查找的PC值
 * @return ARMInstruction* 找到的指令指针，如果未找到则返回NULL
 */
ARMInstruction *get_arm_insn(ARMInstruction *insn_seq, uint64_t pc)
{
    ARMInstruction *insn = insn_seq;

    while(insn) {
        if (insn->pc == pc)
            return insn;

        insn = insn->next;
    }
    return NULL;
}
