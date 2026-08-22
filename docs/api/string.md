# 字符串 API

## 设计契约

字符串基础层只处理字节，不隐式验证、解码或修改 UTF-8。这样同一套 API 可以安全处理 UTF-8、协议字段和包含 `\0` 的二进制片段，也避免字节偏移与字符偏移混用。Unicode 验证、码点遍历、大小写折叠和规范化属于字符集模块。

`xstrview` 是 core 提供的零分配借用类型，`xstrbuf` 是字符串模块提供的可增长独占构建器，返回 `str` 的函数创建独立零结尾字符串。调用方可以按性能和使用手感选择层级，不需要先构造重量级字符串对象。

启用方式：

- `XRT_FEATURE_STRING`：视图构造、查找、变换、独立字符串和构建器；`xstrview` 类型与 `XRT_STR_LITERAL` 本身始终可用。
- `XRT_FEATURE_STRING_SPLIT`：拆分与行迭代器，依赖 `XRT_FEATURE_STRING`。
- `XRT_FEATURE_STRING_FORMAT`：`printf` 格式化，依赖 `XRT_FEATURE_STRING`。
- `XRT_FEATURE_STRING_GLOB`：严格 UTF-8 通配匹配，依赖 `XRT_FEATURE_STRING` 与 `XRT_FEATURE_UNICODE`。

## 类型与所有权

### `xstrview`

```c
typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

视图借用 `[Data, Data + Size)`，不拥有内存，也不保证 `Data[Size]` 为零。`Size == 0` 时允许 `Data == NULL`；`Size != 0` 时 `Data == NULL` 是无效视图并产生 `XERR_ARGUMENT`。只要视图仍在使用，源数据就必须保持有效且不得移动。

`XRT_STR_LITERAL("text")` 在编译期创建不含末尾零字节的视图。`XRT_NPOS` 表示未找到，也可以作为“直到结尾”的长度传给 `xrtStrSlice`。

### `xstrbuf`

```c
typedef struct xstrbuf {
	str Data;
	size_t Size;
	size_t Capacity;
} xstrbuf;
```

构建器拥有 `Data`。有效状态始终满足 `Size <= Capacity`；`Data != NULL` 时始终满足 `Data[Size] == 0`。内容仍可包含内嵌零字节，真实长度以 `Size` 为准。

构建器必须先由 `xrtStrBufInit` 初始化。增长可能移动 `Data`，外部借用视图随之失效；追加当前构建器有效内容中的子视图是受支持的。失败不会改变逻辑内容，调用方可以检查错误后重试或释放构建器。

### `xstrsplit`、`xstrlines`、`xstrfields` 与 `xstrlist`

`xstrsplit`、`xstrlines` 和 `xstrfields` 是零分配迭代器，返回的片段借用输入数据。迭代器必须通过对应的 `Init` 函数初始化，公开字段只用于栈上存储，不应由调用方修改。`xstrfields` 跳过连续 ASCII 空白且不返回空字段。`xstrlist` 是便捷结果；结构、视图数组和所有零结尾片段位于同一个分配块中，只需调用一次 `xrtStrListFree`。

## 视图函数

### `xrtStrView` 与 `xrtStrViewN`

`xrtStrView` 从零结尾字符串创建视图，`NULL` 表示空字符串。`xrtStrViewN` 保留明确字节数，可包含内嵌零字节；它只构造视图，使用视图的操作负责验证参数。

### `xrtStrEmpty` 与 `xrtStrBlank`

`xrtStrEmpty` 判断长度是否为零。`xrtStrBlank` 判断所有字节是否属于 ASCII 空白集合：空格、制表、CR、LF、垂直制表和换页；空字符串也属于 blank。

### `xrtStrCompare`、`xrtStrCaseCompare`、`xrtStrEqual` 与 `xrtStrCaseEqual`

比较按无符号字节进行，返回值只保证小于、等于或大于零。`Case` 版本只折叠 ASCII `A-Z`，非 ASCII 字节保持原样，不分配临时字符串。

### `xrtStrFind`、`xrtStrCaseFind`、`xrtStrRFind` 与 `xrtStrCaseRFind`

返回匹配起始字节偏移，未找到返回 `XRT_NPOS`。`xrtStrFind` 和 `xrtStrCaseFind` 接受起始偏移；空模式匹配该偏移。反向版本返回最右匹配，空模式匹配输入结尾。查找不分配内存，长模式使用经过旧版验证并修订的 Boyer-Moore-Horspool 跳转策略。`Case` 版本只折叠 ASCII 字母。

### `xrtStrFindByte`、`xrtStrFindAny` 与 `xrtStrContainsAny`

`xrtStrFindByte` 从指定偏移查找一个无符号字节。`xrtStrFindAny` 查找集合中的任意字节，`xrtStrContainsAny` 是对应谓词。集合按字节解释，不是子串或 Unicode 标量集合。

### `xrtStrCount`、`xrtStrContains`、`xrtStrStarts` 与 `xrtStrEnds`

`xrtStrCount` 统计不重叠匹配，空模式返回零。`xrtStrCaseCount` 提供 ASCII 大小写不敏感版本。其余函数分别判断包含、前缀和后缀，空模式是有效匹配，并各自提供 `xrtStrCaseContains`、`xrtStrCaseStarts` 和 `xrtStrCaseEnds`。

### `xrtStrCut`、`xrtStrRCut`、`xrtStrCutPrefix` 与 `xrtStrCutSuffix`

这些函数只返回借用视图，不分配内存。`Cut` 围绕第一个分隔符拆分，`RCut` 围绕最后一个分隔符拆分；未找到时返回 `false`，`Before` 仍取得完整输入，`After` 为空。`CutPrefix` 和 `CutSuffix` 仅在前缀或后缀匹配时删除它，未匹配时 `Rest` 仍取得完整输入。全部输出参数都可以为 `NULL`。

### `xrtStrSlice`

按字节返回借用子视图。起始位置和长度会钳制到输入范围，因此可以直接使用 `XRT_NPOS` 表示剩余全部内容。函数不分配内存。

### `xrtStrTrimLeft`、`xrtStrTrimRight` 与 `xrtStrTrim`

删除相应方向的 ASCII 空白并返回借用视图，不修改输入。需要独立字符串时可直接写成：

```c
str sText = xrtStrDupView(xrtStrTrim(xrtStrView("  value  ")));
```

### `xrtStrTrimLeftSet`、`xrtStrTrimRightSet` 与 `xrtStrTrimSet`

把 `Set` 解释为无符号字节集合，删除输入两端属于集合的字节。集合不是子串或 Unicode 字符集合。

## 独立字符串

### `xrtStrDup`、`xrtStrDupN` 与 `xrtStrDupView`

复制输入并追加零字节。成功结果始终由 `xrtFree` 释放，包括空字符串；`xrtStrDup(NULL)` 创建一个独立空字符串。

### `xrtStrConcat`、`xrtStrJoin` 与 `xrtStrRepeat`

分别连接两个视图、使用分隔符连接视图数组、重复一个视图。输入可以包含内嵌零字节。`iCount == 0` 仍返回独立空字符串。

### `xrtStrReplace`

替换全部不重叠匹配。空查找模式不进行插入，而是复制原字符串，避免产生隐含的 `Size + 1` 个匹配点。

### `xrtStrInsert` 与 `xrtStrRemove`

按字节位置插入或删除。超出末尾的位置钳制到末尾，删除长度钳制到剩余内容。

### `xrtStrReverseBytes`

反转字节顺序。它适用于二进制和 ASCII；对 UTF-8 文本使用字符集模块提供的标量或字素操作。`xrtStrReverseBytesTo` 写入调用方缓冲区并支持输入与输出起点相同的原地路径；目标容量必须包含末尾零，部分重叠被拒绝。

### `xrtStrLower` 与 `xrtStrUpper`

只转换 ASCII 字母并复制其他字节。`xrtStrLowerTo` 和 `xrtStrUpperTo` 写入调用方缓冲区并支持原地转换，分配型版本返回由 `xrtFree` 释放的独立字符串。完整 Unicode 大小写映射属于字符集模块。

### `xrtStrFilterTo` 与 `xrtStrFilter`

删除 `Set` 中列出的全部字节。`xrtStrFilterTo` 在输出为 `NULL`、容量为零时只查询结果长度；实际写入要求容量额外包含末尾零，并允许输入与输出起点相同。容量不足时返回所需长度且不改动目标。实现先把集合编译为 256 位栈上位图，再单次扫描输入，不会随集合长度增加每个输入字节的查找成本。`xrtStrFilter` 是一次分配的便捷层。

如果集合表达的是 Unicode 字符而不是原始字节，必须使用 `XRT_FEATURE_UNICODE_TEXT` 中的 `xrtUtf8FilterTo` 或 `xrtUtf8Filter`；把 UTF-8 多字节序列当作独立字节过滤可能破坏其他标量。

### `xrtStrPadLeft`、`xrtStrPadRight` 与 `xrtStrPadCenter`

按字节宽度重复填充视图，最后一次填充可以截断。空填充视图使用一个空格。居中时奇数个额外字节放在右侧。

## 字符串构建器

### `xrtStrBufInit`、`xrtStrBufFree` 与 `xrtStrBufClear`

初始化、释放或清空构建器。`Clear` 保留容量，`Free` 重置全部字段并允许传入 `NULL`。

### `xrtStrBufView`

返回当前内容的借用视图。任何可能增长构建器的操作之后，都应重新取得视图。

### `xrtStrBufReserve` 与 `xrtStrBufResize`

`Reserve` 保证数据容量，不把末尾零字节计入 `Capacity`。`Resize` 改变逻辑长度，扩展区域全部填零。两者都会检测整数溢出，并在分配失败时保留原构建器。

### `xrtStrBufAppend`、`xrtStrBufAppendByte` 与 `xrtStrBufAppendRepeat`

追加视图、单字节或重复视图。接口按显式长度工作，允许追加零字节，也允许源视图来自构建器当前有效内容。

### `xrtStrBufTake`

把构建器内存所有权转给调用方并将构建器重置为空。结果由 `xrtFree` 释放；从空构建器取走时也会返回独立空字符串。

## 拆分与行处理

### `xrtStrSplitInit` 与 `xrtStrSplitNext`

初始化并遍历通用拆分器。连续或位于边界的分隔符会产生空片段；空分隔符把整个输入作为唯一片段；空输入也产生一个空片段。迭代结束返回 `false`，不会设置错误。

### `xrtStrLinesInit` 与 `xrtStrLinesNext`

识别 LF、CRLF 和 CR。输入末尾的单个换行符只终止上一行，不额外产生一行；换行符之前的显式空行仍会保留。空输入没有行。

### `xrtStrFieldsInit` 与 `xrtStrFieldsNext`

按连续 ASCII 空白拆分字段，跳过前导、尾随和重复空白，不返回空字段。它与 `xrtStrSplit` 的保留空片段契约不同，适合命令行、简单记录和一般单词扫描。

### `xrtStrSplit`、`xrtStrSplitLines` 与 `xrtStrFields`

对应迭代器的单调用便捷版本。每个 `Items[i]` 都有明确长度和独立零结尾，整个结果只发生一次分配。

### `xrtStrListFree`

释放便捷拆分结果，允许传入 `NULL`。不能单独释放或长期保留 `Items[i].Data`。

## 格式化

### `xrtFormat` 与 `xrtFormatV`

使用平台 C 运行库的 `printf` 规则创建独立字符串。`xrtFormatV` 不消耗调用方传入的 `va_list`。格式串及参数类型必须匹配；不受信任的格式串不应直接传入。会写入调用方内存的 `%n` 转换（包括长度修饰和位置参数形式）始终被拒绝。空结果仍返回独立空字符串。

### `xrtStrBufAppendFormat` 与 `xrtStrBufAppendFormatV`

把格式化结果直接追加到构建器。短结果使用内部栈缓冲，长结果只使用一个临时分配；因此 `%s` 参数可以安全借用构建器当前内容，即使最终追加触发增长。空结果是成功的无操作。失败时保持原逻辑长度和零结尾，`V` 版本不消耗调用方参数列表。

## 通配匹配

### `xrtStrGlob`

匹配完整字符串，不进行子串搜索。模式支持：

- `*`：零个或多个 Unicode 标量。
- `?`：一个 Unicode 标量。
- `[abc]`、`[a-z]`：字符类和闭区间。
- `[!abc]`、`[^abc]`：反选字符类。
- `\`：转义下一个模式字符。

文本和模式都必须是严格 UTF-8，通配符按 Unicode 标量前进，不会切开多字节序列。`XSTR_GLOB_CASE_ASCII` 只折叠 ASCII 字母，不假装实现完整 Unicode 大小写折叠。算法不分配内存；非法 UTF-8 产生 `xrt.unicode` 值错误，未闭合字符类、反向范围和末尾转义产生 `xrt.string`/`XSTR_ERROR_PATTERN`。普通“不匹配”返回 `false`，不会设置新错误。

## 错误

- 无效指针、无效视图或无效公开结构产生 `XERR_ARGUMENT` 或 `XERR_STATE`。
- 长度计算或容量增长溢出产生 `XERR_RANGE`。
- 分配失败产生 `XERR_MEMORY`。
- C 运行库拒绝格式串或格式串包含 `%n` 时产生 `XERR_VALUE`，错误域为 `xrt.string`，代码为 `XSTR_ERROR_FORMAT`。

返回布尔值的迭代 `Next` 函数以 `false` 同时表示正常结束和失败。调用方只需在初始化或参数可能无效时检查当前错误；正常结束不会创建新错误。成功调用不清除旧错误，遵循 XRT 通用错误契约。

## 范例

完整范例位于：

- `examples/string/basic/main.c`：视图、裁剪和独立字符串。
- `examples/string/builder/main.c`：增量构建和所有权转移。
- `examples/string/split/main.c`：零分配行迭代。
- `examples/string/format/main.c`：直接格式化到构建器。
- `examples/string/glob/main.c`：严格 UTF-8 文件名通配。
- `examples/string/distance/main.c`：带阈值的 Unicode 标量编辑距离与相似度。

```c
xstrbuf Buffer;
xstrview Line;
xstrlines Lines;
str sResult;

xrtStrBufInit(&Buffer);
xrtStrBufAppend(&Buffer, XRT_STR_LITERAL("count="));
xrtStrBufAppendFormat(&Buffer, "%u", 3u);
sResult = xrtStrBufTake(&Buffer);

xrtStrLinesInit(&Lines, xrtStrView("alpha\r\nbeta\n"));
while ( xrtStrLinesNext(&Lines, &Line) ) {
	/* Line 借用原字符串，可按 Size 直接处理。 */
}

xrtFree(sResult);
xrtStrBufFree(&Buffer);
```
