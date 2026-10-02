# Pattern 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[pattern.md](pattern.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `47` 个函数、`14` 个常量或宏、
`8` 个公共类型。

## `include/xrt/pattern.h`

[查看带契约注释的公共头](../../include/xrt/pattern.h)

### 函数 (47)

- `xrtPatternBuilderAdd`
- `xrtPatternBuilderAddMany`
- `xrtPatternBuilderClear`
- `xrtPatternBuilderCompile`
- `xrtPatternBuilderContains`
- `xrtPatternBuilderCount`
- `xrtPatternBuilderCreate`
- `xrtPatternBuilderCreateConfig`
- `xrtPatternBuilderDirty`
- `xrtPatternBuilderFree`
- `xrtPatternBuilderPrepareAdd`
- `xrtPatternBuilderPrepareClear`
- `xrtPatternBuilderPrepareRemove`
- `xrtPatternBuilderPrepareSet`
- `xrtPatternBuilderRemove`
- `xrtPatternBuilderReserve`
- `xrtPatternBuilderSet`
- `xrtPatternBuilderVersion`
- `xrtPatternCaptureCount`
- `xrtPatternCaptureIndex`
- `xrtPatternCaptureName`
- `xrtPatternCompile`
- `xrtPatternCompileConfig`
- `xrtPatternCompileMany`
- `xrtPatternCompileManyConfig`
- `xrtPatternCompiledBytes`
- `xrtPatternConfigInit`
- `xrtPatternCount`
- `xrtPatternEditCommit`
- `xrtPatternEditCount`
- `xrtPatternEditFree`
- `xrtPatternEditId`
- `xrtPatternEditReady`
- `xrtPatternErrorOffset`
- `xrtPatternErrorPattern`
- `xrtPatternExtract`
- `xrtPatternExtractConfig`
- `xrtPatternId`
- `xrtPatternLookup`
- `xrtPatternMatch`
- `xrtPatternMaxCaptureCount`
- `xrtPatternRef`
- `xrtPatternRelease`
- `xrtPatternSeparators`
- `xrtPatternSource`
- `xrtPatternTest`
- `xrtPatternValue`

### 常量与宏 (14)

- `XPATTERN_CAPTURES_DEFAULT`
- `XPATTERN_COMPILED_DEFAULT`
- `XPATTERN_ERROR`
- `XPATTERN_ERROR_CAPACITY`
- `XPATTERN_ERROR_CONFIG`
- `XPATTERN_ERROR_CONFLICT`
- `XPATTERN_ERROR_LIMIT`
- `XPATTERN_ERROR_PATTERN`
- `XPATTERN_ID_INVALID`
- `XPATTERN_MATCH`
- `XPATTERN_NONE`
- `XPATTERN_PATTERNS_DEFAULT`
- `XPATTERN_PATTERN_DEFAULT`
- `XPATTERN_STATES_DEFAULT`

### 类型 (8)

- `xpatternbuilder`
- `xpatternconfig`
- `xpatternedit`
- `xpatternerror`
- `xpatternid`
- `xpatternmatch`
- `xpatternresult`
- `xpatternspec`
