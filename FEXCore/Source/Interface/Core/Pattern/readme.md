# 阶段1: 初始化和配置
- **x86-instr.cpp**:
  - 初始化 `instr_buffer` 和 `instr_buffer_index`，为存储 x86 指令做准备。
- **arm-instr.cpp**:
  - 同理
- **parse.cpp**:
  - 调用 `ParseTranslationRules` 函数，从文件中读取翻译规则，并存储到 `rule_table` 中。
# 阶段2: 源指令解码
- **x86-parse.cpp**:
  - 定义 `FEXCore::Frontend::Decoder::DecodedBlocks` 类型，用于表示源 x86 指令的解码结果。
- **arm-parse.cpp**:
  - 同理
- **Fronted.cpp**:
  - `DecodeInstruction` 函数读取源 x86 指令，调用 `DecodeInst` 进行解码，然后调用 `DecodeInstToX86Inst` 将解码结果转换为 `x86_instr`。
# 阶段3: 规则匹配和应用
- **Core.cpp**:
  - 调用 `MatchTranslationRule` 函数，尝试将当前翻译块与预定义的翻译规则进行匹配。
    - 如果匹配成功，设置 `IsRuleTrans` 为 true，然后返回，表示不需要继续 IR 翻译。
    - 如果匹配失败，继续进行 IR 翻译。
- **rule-translate.cpp**:
  - `MatchTranslationRule` 函数的定义，它遍历 `rule_table` 中的规则，尝试与当前翻译块进行匹配。
    - 如果匹配成功，调用 `do_rule_translation` 函数应用规则，直接生成目标代码。
    - 如果匹配失败，返回 false，表示需要继续 IR 翻译。
# 阶段4: IR 翻译和优化
- **JIT.cpp**:
  - 如果 IR 为 nullptr，说明 IR 翻译被跳过，直接返回。
# 阶段5: 目标代码生成
- **arm-asm.inc**:
  - 定义 `DEF_OPC` 宏，用于生成 ARM 指令编码函数的声明。
- **arm-asm.cpp**:
  - 实现 ARM 指令编码函数，如 `Opc_ADD`、`Opc_SUB` 等。
    - 如果使用了规则翻译，这些函数会在 `do_rule_translation` 中被调用，直接生成 ARM 代码。
    - 如果使用了 IR 翻译，这些函数会在 IR 转换为 ARM 代码的过程中被调用。