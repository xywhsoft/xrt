# Regex

正则模块使用非回溯线性时间引擎。编译对象 `xregex` 不可变、引用计数且可以跨线程共享；`xregexmatcher` 保存可变执行缓存，只能由一个执行流使用，但可以反复匹配以避免热路径分配。

## 模块

- `XRT_MODULE_REGEX`：启用完整正则能力。
- `XRT_MODULE_REGEX_CORE`：字面量转义、编译、元数据、资源预算和结构化错误。
- `XRT_MODULE_REGEX_MATCH`：搜索、指定位置、完整匹配、遍历和捕获；依赖 Unicode 原语处理空匹配推进。
- `XRT_MODULE_REGEX_REPLACE`：模板替换、回调替换和事务式字符串构建；依赖 matcher 与字符串构建器。
- `XRT_MODULE_REGEX_SPLIT`：流式正则拆分与单块复制结果；依赖 matcher 和字符串拆分结果。
- `XRT_MODULE_REGEX_SET`：把多个编译对象合并为一个执行程序，一次返回全部命中的模式索引。

## 编译

`xrtRegexCompile` 使用默认 1 MiB 模式预算和 4096 个捕获预算。`xrtRegexCompileConfig` 允许收紧预算并设置忽略大小写、多行、点匹配换行和非贪婪标志。模式和输入都是明确长度视图，不要求零结尾，并允许嵌入零字节；命名捕获名称是例外，名称不能为空、重复或包含零字节。

编译对象通过 `xrtRegexRef` 和 `xrtRegexRelease` 管理。模块不公开底层 builder、分配器或 clone；共享编译对象不需要克隆。

动态文本不能直接拼入表达式。`xrtRegexEscapeSize` 返回转义后的精确字节数，`xrtRegexEscapeWrite` 写入调用方缓冲区并支持输入、输出同址，`xrtRegexEscape` 返回由 `xrtFree` 释放的独立字符串。转义按字节保留普通内容，只为具有语法含义的 `\\.+*?()|[]{}^$` 增加反斜杠，因此明确长度的二进制文本不会被截断。部分重叠的输入和输出会被拒绝。

```c
size_t iSize;
str sPattern = xrtRegexEscape(XRT_STR_LITERAL("file[1].txt"), &iSize);
xregex* pRegex = sPattern != NULL
	? xrtRegexCompile(xrtStrViewN(sPattern, iSize)) : NULL;

xrtFree(sPattern);
```

## 匹配

所有匹配函数返回 `xregexresult`：

| 值 | 含义 |
| --- | --- |
| `XREGEX_ERROR` | 执行失败，读取 `xrtGetError()` |
| `XREGEX_NONE` | 正常完成但未匹配 |
| `XREGEX_MATCH` | 匹配成功 |

`xrtRegexMatcherFind` 从给定字节位置搜索，`xrtRegexMatcherAt` 要求匹配从该位置开始，`xrtRegexMatcherFull` 要求覆盖完整输入。`xrtRegexMatcherNext` 遍历后续匹配；空匹配按一个合法 UTF-8 标量推进，无效序列按一个字节推进。

捕获范围统一使用半开字节区间 `[Begin, End)`。`xregexcapture.Matched` 区分“捕获未参与”与“捕获了空字符串”。捕获文本和 matcher 输入均为借用视图，在下一次匹配或 matcher 释放前有效。

## 替换

`xrtRegexReplaceTo` 是基础模板入口，把结果追加到已有 `xstrbuf`。`iLimit` 是最大替换次数，`0` 表示不替换，`SIZE_MAX` 表示全部替换。成功时可通过 `pCount` 取得实际次数；失败时，本次调用追加的内容全部撤销，进入函数前的构建器内容保持不变。输入和替换模板允许借用构建器当前有效内容，内部会在首次增长前稳定这些视图。

模板语法刻意保持精简：

| 写法 | 含义 |
| --- | --- |
| `$0` | 整体匹配 |
| `$1`、`$2` | 数字捕获 |
| `${name}` | 命名捕获 |
| `$$` | 字面量美元符号 |

未参与匹配的可选捕获展开为空文本。无效索引、未知名称和不完整美元令牌在开始匹配前报告 `XREGEX_ERROR_REPLACEMENT`，`xrtRegexErrorOffset` 返回模板字节位置。

`xrtRegexReplaceFuncTo` 让回调读取当前 matcher 的全部捕获并直接向输出尾部追加内容，适合大小写转换、编码或数据驱动重写。回调只能追加，不得清空、收缩或接管构建器。回调失败同样撤销本次替换调用的全部输出。

`xrtRegexReplace` 与 `xrtRegexReplaceFirst` 是常见路径的一行式入口，返回由调用方使用 `xrtFree` 释放的零结尾字符串。需要保留嵌入零字节后的精确长度时使用 `xrtRegexReplaceTo` 并读取 `xstrbuf.Size`。

## 拆分

`xrtRegexSplitterCreate` 创建借用输入的流式拆分器，`xrtRegexSplitterNext` 以 `XREGEX_MATCH / XREGEX_NONE / XREGEX_ERROR` 返回字段。普通字段的 `xregexsplitpart.Capture` 为 `XRT_NPOS`；启用 `XREGEX_SPLIT_CAPTURES` 后，每次分隔匹配的捕获按组号紧随字段返回。捕获项用 `Matched` 区分未参与捕获和合法空捕获。

`xregexsplitconfig.Limit` 表示最多使用多少次分隔匹配：`0` 返回完整输入，`SIZE_MAX` 不限制。默认保留首尾及相邻分隔符产生的空字段；`XREGEX_SPLIT_SKIP_EMPTY` 会同时跳过空字段和空捕获。空分隔匹配沿用 matcher 的 UTF-8 标量推进规则，不会在同一字节位置循环。

`xrtRegexSplit` 是默认配置的便捷入口。它复用同一个 splitter 执行计数和写入两遍，返回与 `xrtStrSplit` 相同布局的 `xstrlist`：视图数组、各项零结尾副本和哨兵都位于一个分配块中，使用 `xrtStrListFree` 释放。需要限制、过滤或捕获元数据时使用流式接口。

## 集合匹配

`xrtRegexSetCreate` 接受已有 `xregex`，因此同一集合中的模式可以使用不同编译标志。集合会增加每个模式的引用；创建成功后，调用方可以立即释放原引用。`xrtRegexSetCompile` 和 `xrtRegexSetCompileConfig` 是共享配置的批量编译入口。空集合合法且始终返回 `XREGEX_NONE`。

`xregexset` 与 `xregex` 一样不可变并可跨线程共享。每个执行流使用自己的 `xregexsetmatcher`。`xrtRegexSetMatcherMatch` 从明确字节位置开始搜索，并以升序返回所有能够在剩余输入中命中的模式索引。`xrtRegexSetMatcherCount`、`xrtRegexSetMatcherIndex`、`xrtRegexSetMatcherMatched` 和 `xrtRegexSetMatcherFirst` 只读取最近一轮成功执行的结果，不复制输入或匹配文本。

集合适合规则分类、路由预筛和日志扫描。需要捕获内容时，先用集合取得候选模式，再使用 `xregexmatcher` 执行对应编译对象；这样集合热路径只维护模式索引，不为每个模式保存捕获数组。

## 语法与保证

支持 RE2 风格字符类、Unicode 属性、贪婪与非贪婪量词、命名捕获、行首尾和文本首尾断言，以及表达式内标志。表达式内标志为 `i`、`m`、`s` 和 `U`；引擎始终按 Unicode 模式工作，因此不接受会暗示切换 Unicode 模式的 `u`。字符类末尾的 `-` 按字面量处理，逆序范围属于语法错误。`\b` 和 `\B` 使用 ASCII 单词字符定义。

模块有意不支持反向引用和前后向环视。这些结构无法维持线性时间保证；需要这类语义时，应把正则用于候选定位，再由调用方执行二次检查。

## 错误

模块错误域为 `xrt.regex`。语法错误使用 `XREGEX_ERROR_PATTERN`，资源预算使用 `XREGEX_ERROR_LIMIT`。`xrtRegexErrorOffset` 可读取语法错误的模式字节位置。批量编译的语法、配置和预算错误会保留原始错误作为 cause，`xrtRegexSetErrorIndex` 返回失败模式的零基索引。内存不足不再尝试分配包装错误，而是原样传播稳定的 `XERR_MEMORY`。

## 示例

普通匹配示例位于 `examples/text/regex/main.c`，替换示例位于 `examples/text/regex_replace/main.c`，拆分示例位于 `examples/text/regex_split/main.c`，集合分类示例位于 `examples/text/regex_set/main.c`。
