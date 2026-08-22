# Unicode 与字符集 API

## 设计契约

XRT 的内部文本主线是 UTF-8。`unicode` 模块只处理 Unicode 标量值和 UTF-8/16/32 编码形式；`charset` 模块处理带明确字节序的编码方案与 BOM；`charset_detect` 提供不承诺绝对正确的启发式检测。三层可以独立裁剪：

- `XRT_FEATURE_UNICODE`：标量原语、严格校验、流式 UTF-8 校验、缓冲区转换和分配型转换。
- `XRT_FEATURE_UNICODE_TEXT`：调用方缓冲区与分配型 UTF-8 文本变换，依赖 `unicode`。
- `XRT_FEATURE_UNICODE_DISTANCE`：按 Unicode 标量计算编辑距离与相似度，依赖 `unicode`。
- `XRT_FEATURE_CHARSET`：UTF-8、UTF-16 LE/BE、UTF-32 LE/BE 字节流、BOM 和通用转码，依赖 `unicode`。
- `XRT_FEATURE_CHARSET_DETECT`：带置信度的编码猜测，依赖 `charset`。

模块遵循以下不变式：

1. UTF-8 只接受 RFC 3629 的 1 至 4 字节形式；过长形式、代理项、5/6 字节形式和大于 U+10FFFF 的值无效。
2. UTF-16 只接受配对代理项；UTF-32 码元必须直接是 Unicode 标量值。
3. `XUTF_STRICT` 在首个错误处停止并设置 `xrt.unicode` 结构化错误。
4. `XUTF_REPLACE` 按 Unicode “最大子部件”规则写入 U+FFFD，保证继续前进，同时不吞掉后面的合法序列。
5. 明确长度视图允许嵌入 U+0000；零结尾便捷函数在第一个零码元处结束。
6. 所有分配型函数，包括空结果，都返回由调用方使用 `xrtFree` 释放的独立内存。
7. BOM 只在调用方明确要求时写入；通用转码不会隐式删除输入中的 U+FEFF。

“字节”“码元”“标量”不能互换：UTF-8 视图的 `Size` 是字节数，UTF-16/32 视图的 `Size` 是码元数，`xrtUtf8Count` 和 `xrtUtf16Count` 返回 Unicode 标量数。用户可见的字素簇数量不属于本模块。

## 类型

### `xbytesview` 与 `xstrview`

两个类型由 core 提供，分别借用任意字节和文本字节。它们不拥有数据，也不要求末尾补零：

```c
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;

typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

`XRT_BYTES_LITERAL` 和 `XRT_STR_LITERAL` 在编译期创建不含末尾零字节的视图。

### `xutf16view` 与 `xutf32view`

`xutf16view.Size` 是 16 位码元数，`xutf32view.Size` 是 32 位码元数。`xrtUtf16View` 和 `xrtUtf32View` 只构造借用视图，不校验内容、不分配内存。

### `xutfpolicy`

- `XUTF_STRICT`：任何不完整或非法输入都失败。
- `XUTF_REPLACE`：每个最大子部件替换为一个 U+FFFD。

协议字段、标识符、源代码和安全边界通常应使用严格模式。展示来源不可靠的普通文本时可以明确选择替换模式。

### `xutfstatus` 与 `xutfresult`

`xutfstatus` 包含：

- `XUTF_OK`：输入已成功处理。
- `XUTF_MORE`：单标量或流式输入还缺少后续码元。
- `XUTF_INVALID`：输入或参数无效。
- `XUTF_NO_SPACE`：调用方目标缓冲区不足。
- `XUTF_OVERFLOW`：结果长度无法由 `size_t` 表示。

缓冲区转换返回：

```c
typedef struct xutfresult {
	xutfstatus Status;
	size_t Read;
	size_t Written;
	size_t Error;
} xutfresult;
```

`Read` 和 `Written` 使用各自视图的码元单位。`Error` 是源视图中的首个错误位置，成功时为 `XRT_NPOS`。目标空间不足时，`Read` 停在尚未写入的完整标量前，因此调用方可以更换缓冲区后继续。

### `xencoding`

编码方案包含 `XENCODING_UTF8`、`XENCODING_UTF16_LE`、`XENCODING_UTF16_BE`、`XENCODING_UTF32_LE`、`XENCODING_UTF32_BE` 和无法判断时使用的 `XENCODING_UNKNOWN`。

这里没有含义随平台改变的 “OEM” 编码。Windows 代码页、GBK、Shift-JIS 等传统编码属于可选的平台/外部编解码边界，不能在非 Windows 平台静默等价为 UTF-8。

## 标量原语

### `xrtUnicodeScalar`

判断数值是否在 U+0000 至 U+10FFFF 之间且不属于代理项区间 U+D800 至 U+DFFF。非字符码点仍是合法标量，本函数不会把 Unicode 属性策略混入编码合法性。

### `xrtUtf8Decode` 与 `xrtUtf16Decode`

从视图开头严格解码一个标量。成功返回 `XUTF_OK`；合法前缀被截断时返回 `XUTF_MORE`；非法形式返回 `XUTF_INVALID`。`pRead` 返回成功消费量，或容错转换应消费的最大子部件长度。

### `xrtUtf8Encode` 与 `xrtUtf16Encode`

把单个标量写入至少 4 字节或 2 个 UTF-16 码元的调用方缓冲区，返回实际写入码元数。无效标量或空目标返回零并设置错误。

## 校验与计数

### `xrtUtf8Valid`、`xrtUtf16Valid` 与 `xrtUtf32Valid`

严格校验完整视图，不分配内存。失败时可通过 `pError` 取得首个错误码元位置。校验谓词本身不把普通“内容无效”设置为执行上下文错误，便于探测输入；参数组合无效仍设置 `XERR_ARGUMENT`。

### `xrtUtf8Count` 与 `xrtUtf16Count`

返回 Unicode 标量数；输入无效时返回 `XRT_NPOS`。它们不会把组合字符或 emoji 序列折叠为字素簇。

### `xrtUtf8Offset` 与 `xrtUtf8Index`

`xrtUtf8Offset` 把标量索引转换为字节偏移，索引可以正好位于末端；越界返回 `XRT_NPOS` 和 `XERR_RANGE`。`xrtUtf8Index` 执行逆转换，字节偏移必须位于标量边界，落在多字节序列中间同样产生 `XERR_RANGE`。两者都不分配内存，只严格校验实际遍历的前缀；遇到非法 UTF-8 时返回 `XRT_NPOS` 和带字节位置的 `xrt.unicode` 值错误。

### `xrtUtf8At` 与 `xrtUtf8Slice`

`xrtUtf8At` 读取指定标量索引，末端索引没有可读标量并产生 `XERR_RANGE`。`xrtUtf8Slice` 按标量索引返回借用视图，起点与末端按切片语义钳制到输入末尾，`iCount == XRT_NPOS` 表示一直切到末尾。切片不分配、不补零，也不会把组合字符或 emoji 序列当成单个字素簇。

### `xrtUtf8Range` 与 `xrtUtf8Substr`

这组 `unicode_text` 接口承接语言运行时常用的带符号索引语义。`xrtUtf8Range` 返回借用视图，`xrtUtf8Substr` 返回由 `xrtFree` 释放的独立零结尾字符串。`iStart < 0` 从末尾按标量计数，`iCount < 0` 表示一直到末尾；超出两端的起点会钳制到边界，包括 `INT64_MIN`。空结果仍是成功结果，不与非法 UTF-8 或内存不足混淆。

```c
xstrview Range;
str sCopy;

xrtUtf8Range(XRT_STR_LITERAL("A你😀B"), -2, 1, &Range);
sCopy = xrtUtf8Substr(XRT_STR_LITERAL("A你😀B"), 1, 2);
/* Range 借用 "😀"，sCopy 保存 "你😀"。 */
xrtFree(sCopy);
```

### 标量索引搜索

`xrtUtf8Find`、`xrtUtf8CaseFind`、`xrtUtf8RFind` 和 `xrtUtf8CaseRFind` 只接受严格 UTF-8，返回 Unicode 标量索引而不是字节偏移。正向搜索的 `iStart` 同样是标量索引；空模式返回起点，反向空模式返回文本标量数。未找到返回 `XRT_NPOS`，这是正常结果，不设置错误。

`Case` 版本只折叠 ASCII 字母，行为确定且不依赖区域设置；它不会冒充完整 Unicode 大小写折叠。需要语言学大小写、规范等价或区域规则时，应在上层 Unicode 模块完成转换后再搜索。

`xrtUtf8ContainsAny` 判断文本是否包含集合中的任意标量。它与字节层 `xrtStrContainsAny` 的边界不同，不会因为多字节编码共享字节而误判。

### 标量集合裁剪

`xrtUtf8TrimLeftSet`、`xrtUtf8TrimRightSet` 和 `xrtUtf8TrimSet` 删除两端属于指定标量集合的内容并返回借用视图。三者不分配内存，严格校验文本和集合，也支持明确长度输入中的 U+0000。空集合保持原文本不变。

这些接口不隐式定义“Unicode 空白”。仅需要 ASCII 空白时直接使用字符串层 `xrtStrTrim`；需要自定义字符集合时传入明确集合；完整 Unicode 属性裁剪应由未来可选的 Unicode 属性模块提供。

### 标量位置编辑与填充

`xrtUtf8Insert` 和 `xrtUtf8Remove` 按标量位置编辑文本并返回独立字符串。位置支持负索引；`xrtUtf8Remove` 的负数量表示删除到末尾。实现先把标量范围解析为字节边界，再复用字符串层一次分配的插入和删除实现。

`xrtUtf8PadLeft`、`xrtUtf8PadRight` 和 `xrtUtf8PadCenter` 按标量数补足宽度。填充文本按标量循环，最后一次可以在完整标量边界处截断；空填充使用 ASCII 空格。中心填充在数量为奇数时把多出的一个标量放在右侧。这里的宽度不是终端显示列宽，也不是字素簇数量。

### `xrtUtf8ReverseTo` 与 `xrtUtf8Reverse`

严格校验输入后按 Unicode 标量反转 UTF-8 文本。`xrtUtf8ReverseTo` 写入调用方缓冲区，容量必须包含末尾零，并支持输入与输出起点相同的无分配原地路径；部分重叠被拒绝。`xrtUtf8Reverse` 返回由 `xrtFree` 释放的独立零结尾字符串。结果保持每个标量的 UTF-8 字节序列完整，也支持明确长度输入中的 U+0000。它不会执行字素簇分段，因此组合附加符号和由多个标量组成的 emoji 序列会分别移动；需要面向用户可见字符反转时应使用未来独立的字素模块或上层 Unicode 实现。

### `xrtUtf8FilterTo` 与 `xrtUtf8Filter`

严格校验文本和集合后，删除集合中列出的全部 Unicode 标量。`xrtUtf8FilterTo` 支持长度查询、调用方缓冲区和输入起点相同的原地过滤；容量不足或重叠非法时不改动目标。过滤集合必须在写入期间独立于目标缓冲区，避免原地压缩破坏仍需查询的集合。实现对 ASCII 使用精确位图，对非 ASCII 使用布隆预筛后再确认标量，常见小集合不发生堆分配。`xrtUtf8Filter` 是一次分配的便捷层。

该 API 按标量而不是字节删除，因此不会因为两个 UTF-8 字符共享某些编码字节而误伤内容；它不执行规范化，规范等价但编码标量序列不同的文本不会自动视为相同集合成员。

### `xrtUtf8Distance` 与 `xrtUtf8Similarity`

`xrtUtf8Distance` 按 Unicode 标量计算 Levenshtein 插入、删除和替换距离，不会把一个多字节标量误算成多个差异。`iLimit == XRT_NPOS` 计算精确距离；有限限制使用带状动态规划，长度差或最终距离超过限制时返回 `XRT_NPOS`，这是正常的阈值结果，不设置错误。实现只分配与较短字符串标量数成正比的一块内存。

`xrtUtf8Similarity` 返回 `1 - distance / max(left_scalars, right_scalars)`，范围为 `0.0` 至 `1.0`，两个空字符串为 `1.0`。非法 UTF-8 或内存不足时返回负值并保留结构化错误。该能力处理标量，不执行归一化、区域规则或字素簇分段；需要这些语义的上层可以在调用前完成相应转换。

### `xrtUtf16Len` 与 `xrtUtf32Len`

返回零结尾字符串在第一个零码元前的码元数。空指针返回零。需要保留嵌入零时必须携带明确长度，而不能再次调用长度函数。

### `xrtUtf16Dup` / `xrtUtf32Dup`

复制零结尾宽字符串并返回独立的零结尾内存。空指针按空字符串处理，返回值始终由 `xrtFree` 释放。

### `xrtUtf16DupView` / `xrtUtf32DupView`

复制明确长度视图并追加一个零码元。视图中的嵌入零会原样保留，函数只复制码元而不额外校验 Unicode 合法性；需要校验时先调用 `xrtUtf16Valid` 或 `xrtUtf32Valid`。大小计算溢出返回 `XERR_RANGE` 和 `XUTF_ERROR_OVERFLOW`。

## 流式 UTF-8

`xrtUtf8StateInit` 初始化状态；`xrtUtf8StateFeed` 接受任意分块，最后一块把 `bFinal` 设为 `true`。分块末尾最多保留 3 个合法前缀字节，因此不需要每连接固定分配大缓冲区。

返回 `XUTF_MORE` 表示当前分块结束在合法前缀中间，不是错误。最终分块仍未完成或遇到非法序列时返回 `XUTF_INVALID`；`xrtUtf8StateError` 返回从整个流开头计算的绝对字节位置。状态失败后保持失败，重新使用前必须再次初始化。

```c
xutf8state State;

xrtUtf8StateInit(&State);
if ( xrtUtf8StateFeed(&State, first, false) == XUTF_INVALID ) {
	/* 拒绝输入 */
}
if ( xrtUtf8StateFeed(&State, last, true) != XUTF_OK ) {
	/* 最终输入非法或不完整 */
}
```

## 缓冲区转换

六个函数覆盖所有 UTF 码元宽度方向：

- `xrtUtf8To16Buffer`、`xrtUtf8To32Buffer`
- `xrtUtf16To8Buffer`、`xrtUtf16To32Buffer`
- `xrtUtf32To8Buffer`、`xrtUtf32To16Buffer`

传入 `pTarget == NULL` 且 `iCapacity == 0` 时只校验并计算精确目标长度。传入目标时不写零终止码元，因为调用方缓冲区可能是大流的一部分。目标不足不会拆分一个标量，也不会修改尚未计入 `Written` 的区域。源与目标不得重叠；扩宽和收窄方向使用同一条明确契约，别名输入会以 `XERR_ARGUMENT` 拒绝且不修改缓冲区。

```c
xstrview Source = XRT_STR_LITERAL("A\xF0\x9F\x98\x80");
xutfresult Need = xrtUtf8To16Buffer(Source, NULL, 0, XUTF_STRICT);
uint16* pUnits = xrtMalloc((Need.Written + 1u) * sizeof(uint16));
xutfresult Done = xrtUtf8To16Buffer(Source, pUnits, Need.Written, XUTF_STRICT);

if ( Done.Status == XUTF_OK ) {
	pUnits[Done.Written] = 0;
}
```

分配型函数已经封装这套两遍计量流程，常见代码不需要手工重复。

## 分配型转换

### 零结尾便捷层

以下函数严格转换零结尾输入，最短名称保留给最常见的平台边界：

- `xrtUtf8To16`、`xrtUtf8To32`
- `xrtUtf16To8`、`xrtUtf16To32`
- `xrtUtf32To8`、`xrtUtf32To16`

```c
uint16* pWide = xrtUtf8To16(sPath, NULL);
if ( pWide == NULL ) {
	return false;
}
/* 调用需要 UTF-16 的平台 API。 */
xrtFree(pWide);
```

空指针按空字符串处理；返回值仍是必须释放的独立零结尾对象。`pSize` 返回目标码元数，不含末尾零码元。

### 明确长度与策略层

以下函数保留嵌入零并允许选择错误策略：

- `xrtUtf8ViewTo16`、`xrtUtf8ViewTo32`
- `xrtUtf16ViewTo8`、`xrtUtf16ViewTo32`
- `xrtUtf32ViewTo8`、`xrtUtf32ViewTo16`

返回值均由 `xrtFree` 释放，`pSize` 的单位由目标编码决定。

## BOM 与通用转码

### `xrtEncodingUnitSize`

返回编码方案的码元字节宽度：UTF-8 为 1，UTF-16 为 2，UTF-32 为 4，未知编码为 0。

### `xrtEncodingBom`

只检查输入开头。函数先检查 4 字节 UTF-32 BOM，再检查 UTF-8 和 UTF-16，避免把 `FF FE 00 00` 错认成 UTF-16 LE。没有 BOM 返回 `XENCODING_UNKNOWN`，`pSize` 为零。`pSize` 不能与输入字节重叠；参数失败时不会先改写输入或输出。

### `xrtEncodingWriteBom`

目标为空时返回 BOM 所需字节数；目标足够时写出并返回实际字节数。目标不足返回零、保持目标不变并设置 `XERR_RANGE`；编码未知返回零并设置 `XERR_ARGUMENT`。

### `xrtTranscode`

在五种 Unicode 编码方案之间直接转码，返回字节缓冲区和字节长度。实现只使用一条“解码标量 -> 编码标量”管线，不创建中间 UTF-16/32 字符串。`bWriteBom` 只控制目标前缀；返回长度包含 BOM，不含末尾补齐的目标零码元。`pSize` 是可选输出，不能与源字节重叠；参数失败时不会先改写它或源数据。

```c
size_t iSize = 0;
bytes pPacket = xrtTranscode(
	XRT_BYTES_LITERAL("hello"),
	XENCODING_UTF8,
	XENCODING_UTF16_LE,
	XUTF_STRICT,
	true,
	&iSize
);
```

如果输入带 BOM，先用 `xrtEncodingBom` 得到 `BomSize`，再把源视图切到 BOM 后。`xrtTranscode` 不擅自删除输入中的 U+FEFF，因为它也可能是调用方有意保留的文本标量。

## 编码检测

`xrtEncodingGuess` 返回：

```c
typedef struct xencodingguess {
	xencoding Encoding;
	size_t BomSize;
	uint8 Confidence;
} xencodingguess;
```

规则按可靠性排序：

1. 完整 BOM：置信度 100。
2. 严格合法且具有明显零字节分布的 UTF-16/32：中高置信度。
3. 严格合法的非 ASCII UTF-8：置信度 90。
4. 纯 ASCII：返回 UTF-8 兼容结果，但置信度仅 40，因为 ASCII、Windows 代码页和许多其他编码无法仅凭字节区分。
5. 没有可靠结论：`XENCODING_UNKNOWN`、置信度 0。

无 BOM 的单个 UTF-16/32 码元不具备足够分布信息，不会被判为宽编码；只有两个或三个码元时，置信度也会按样本量封顶。检测永远只是猜测。协议声明、文件元数据和调用方显式配置优先于启发式结果；安全敏感路径不应因为置信度较高就跳过严格校验。

## 错误

严格转换失败时，执行上下文错误具有：

- `Kind = XERR_VALUE`
- `Domain = "xrt.unicode"`
- `Code = XUTF_ERROR_INVALID`
- `Operation` 指明转换方向或 `transcode`
- `Data` 包含源视图中的 `offset`

长度溢出使用 `XERR_RANGE` 和 `XUTF_ERROR_OVERFLOW`。内存不足沿用 core 的 `XERR_MEMORY`。缓冲区容量不足由 `XUTF_NO_SPACE` 直接表达，不覆盖执行上下文错误。

## 完整示例

- `examples/charset/unicode/main.c`：严格 UTF-8/16 往返。
- `examples/charset/unicode_text/main.c`：UTF-8 标量反转与过滤。
- `examples/string/distance/main.c`：Unicode 标量编辑距离与相似度。
- `examples/charset/transcode/main.c`：直接生成带 BOM 的 UTF-16 LE 字节封包。
- `examples/charset/detect/main.c`：读取编码、BOM 长度和置信度。

## 标准依据

编码合法性、替换策略和 BOM 语义以 Unicode Standard 的 Unicode Encoding Forms 与 U+FFFD 最大子部件建议为依据：

- <https://www.unicode.org/versions/latest/core-spec/chapter-3/>
- <https://www.unicode.org/versions/latest/core-spec/chapter-5/>
- <https://www.unicode.org/faq/utf_bom.html>
