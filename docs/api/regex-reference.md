# Regex 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[regex.md](regex.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `54` 个函数、`17` 个常量或宏、
`14` 个公共类型。

## `include/xrt/regex.h`

[查看带契约注释的公共头](../../include/xrt/regex.h)

### 函数 (54)

- `xrtRegexCaptureCount`
- `xrtRegexCaptureIndex`
- `xrtRegexCaptureName`
- `xrtRegexCompile`
- `xrtRegexCompileConfig`
- `xrtRegexConfigInit`
- `xrtRegexErrorOffset`
- `xrtRegexEscape`
- `xrtRegexEscapeSize`
- `xrtRegexEscapeWrite`
- `xrtRegexFlags`
- `xrtRegexFullMatch`
- `xrtRegexFullTest`
- `xrtRegexMatch`
- `xrtRegexMatcherAt`
- `xrtRegexMatcherCapture`
- `xrtRegexMatcherCaptureNamed`
- `xrtRegexMatcherCreate`
- `xrtRegexMatcherFind`
- `xrtRegexMatcherFree`
- `xrtRegexMatcherFull`
- `xrtRegexMatcherMatched`
- `xrtRegexMatcherNext`
- `xrtRegexMatcherText`
- `xrtRegexPattern`
- `xrtRegexRef`
- `xrtRegexRelease`
- `xrtRegexReplace`
- `xrtRegexReplaceFirst`
- `xrtRegexReplaceFuncTo`
- `xrtRegexReplaceTo`
- `xrtRegexSetCompile`
- `xrtRegexSetCompileConfig`
- `xrtRegexSetCount`
- `xrtRegexSetCreate`
- `xrtRegexSetErrorIndex`
- `xrtRegexSetMatcherCount`
- `xrtRegexSetMatcherCreate`
- `xrtRegexSetMatcherFirst`
- `xrtRegexSetMatcherFree`
- `xrtRegexSetMatcherIndex`
- `xrtRegexSetMatcherMatch`
- `xrtRegexSetMatcherMatched`
- `xrtRegexSetRef`
- `xrtRegexSetRegex`
- `xrtRegexSetRelease`
- `xrtRegexSetTest`
- `xrtRegexSplit`
- `xrtRegexSplitConfigInit`
- `xrtRegexSplitterCreate`
- `xrtRegexSplitterFree`
- `xrtRegexSplitterNext`
- `xrtRegexTest`
- `xrtRegexValid`

### 常量与宏 (17)

- `XREGEX_CAPTURES_DEFAULT`
- `XREGEX_DOT_ALL`
- `XREGEX_ERROR`
- `XREGEX_ERROR_CALLBACK`
- `XREGEX_ERROR_CONFIG`
- `XREGEX_ERROR_EXECUTE`
- `XREGEX_ERROR_LIMIT`
- `XREGEX_ERROR_PATTERN`
- `XREGEX_ERROR_REPLACEMENT`
- `XREGEX_IGNORE_CASE`
- `XREGEX_MATCH`
- `XREGEX_MULTILINE`
- `XREGEX_NONE`
- `XREGEX_PATTERN_DEFAULT`
- `XREGEX_SPLIT_CAPTURES`
- `XREGEX_SPLIT_SKIP_EMPTY`
- `XREGEX_UNGREEDY`

### 类型 (14)

- `xregexcapture`
- `xregexconfig`
- `xregexerror`
- `xregexflag`
- `xregexmatcher`
- `xregexreplacefn`
- `xregexresult`
- `xregexset`
- `xregexsetmatcher`
- `xregexspan`
- `xregexsplitconfig`
- `xregexsplitflag`
- `xregexsplitpart`
- `xregexsplitter`
