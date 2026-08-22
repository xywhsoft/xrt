# Codec

## HEX Codec

`codec_hex` 是任意字节与十六进制文本之间的最小编码层，只依赖 `core`。启用：

```c
#define XRT_FEATURE_CODEC_HEX
```

基础 API `xrtHexEncode` / `xrtHexDecode` 采用统一的查询与写入契约：`output == NULL && capacity == 0` 只校验并返回所需长度；编码目标容量必须额外包含末尾零，解码目标是任意二进制，不要求哨兵。两者都不分配内存，允许输出与输入从同一地址开始，其他部分重叠被拒绝。所有显式指针范围会在读取前检查地址回绕；容量不足时，别名检查只覆盖调用方声明的输出容量，因此缓冲区之后的独立长度字段仍能接收精确需求值。

编码默认使用小写 `a-f`，`XHEX_UPPER` 改用大写。解码同时接受大小写，默认严格拒绝任何非 HEX 字符和奇数个数字；`XHEX_IGNORE_SPACE` 可显式忽略 SP、HT、VT、FF、CR、LF，适合读取分行或分组后的展示文本。编码器拒绝解码专用标志，解码器也拒绝编码专用标志。

便捷函数 `xrtHexEncodeNew` 返回零结尾文本，`xrtHexDecodeNew` 返回带额外零哨兵的字节块；两者都由 `xrtFree` 释放。空输入仍返回独立的可释放结果。格式错误使用 `xrt.codec` 域的 `XCODEC_ERROR_HEX_FORMAT`，标志错误使用 `XCODEC_ERROR_HEX_CONFIG`。

旧版 `lib/string.h` 的 HEX 资产已迁移到该层：显式零长度不再触发 `strlen`，编码长度先检查溢出，解码先完整验证再写入，失败不会发布半个结果。

- `examples/codec/hex/main.c`
- `tests/codec/test_hex.c`
- `tests/codec/test_hex_oom.c`
- `tests/single/test_single_codec_hex.c`



## Base64 Codec

`codec_base64` 是字符串、PEM、WebSocket、HTTP 认证、XID 与序列化模块共用的基础编码层。它只依赖 `core`，不与字符串对象或网络对象绑定。

## 裁剪

```c
#define XRT_FEATURE_CODEC_BASE64
```

该宏启用 `include/xrt/codec.h` 与 `src/codec/base64.c`。未启用时不发布 Base64 类型和函数。

## 配置

`xbase64config` 的零初始化值表示 RFC 4648 标准字母表、规范 `=` 填充和严格输入：

```c
xbase64config Config = { 0 };
```

- `XBASE64_URL`：使用 URL-safe 的 `-`、`_` 字母表。
- `XBASE64_NO_PADDING`：编码不写 `=`，解码严格拒绝 `=`。
- `XBASE64_IGNORE_SPACE`：解码时忽略 RFC 7468 定义的 SP、HT、VT、FF、CR 和 LF，供 PEM/MIME 使用；编码器拒绝该标志。
- `XBASE64_OPTIONAL_PADDING`：编码仍写规范填充，解码同时接受完整、部分或缺失的末尾填充；不能与 `XBASE64_NO_PADDING` 同时使用。该模式用于 RFC 9651 Byte Sequence 等明确要求接收端补齐填充的协议。
- `Alphabet`：可选的 64 字符自定义可见 ASCII 字母表。字符必须唯一且不能包含 `=`；自定义字母表不能与 `XBASE64_URL` 同时使用。

## 基础 API

```c
bool xrtBase64Encode(
	const void* data, size_t size,
	char* output, size_t capacity, size_t* outputSize,
	const xbase64config* config
);

bool xrtBase64Decode(
	const char* text, size_t textSize,
	void* output, size_t capacity, size_t* outputSize,
	const xbase64config* config
);
```

把输出传为 `NULL`、容量传为零即可验证参数并查询所需长度。编码长度不包含末尾零字节，实际编码缓冲必须再多一个字节。解码长度就是二进制字节数。

编码与解码允许输出和输入从同一地址开始，因此可执行原地扩张或收缩；其他部分重叠会被拒绝。参数、格式和容量失败不会修改输出。容量不足时 `outputSize` 返回所需长度，其他失败保持调用前的长度值。输入、输出、长度字段、配置结构和自定义字母表的显式范围都会在读取前检查地址回绕；短缓冲区的别名检查以实际声明容量为界，不会把相邻的独立长度字段误判为输出的一部分。

解码器严格拒绝：

- 非末尾填充、超过两个填充或不匹配的填充数量；
- 非规范的末尾残余位；
- 不属于字母表的字符；
- 默认模式下的空白或无填充文本；
- 无填充模式下长度余一的文本。

## 便捷 API

```c
char* xrtBase64EncodeNew(
	const void* data, size_t size,
	const xbase64config* config
);

unsigned char* xrtBase64DecodeNew(
	const char* text, size_t textSize, size_t* outputSize,
	const xbase64config* config
);
```

两个结果都由 `xrtFree` 释放。解码结果额外保留一个不计入长度的零字节，但二进制数据仍必须使用返回长度，不能假定其中没有零。

## 错误

Base64 格式和配置错误使用 `xrt.codec` 域：

- `XCODEC_ERROR_BASE64_CONFIG`
- `XCODEC_ERROR_BASE64_FORMAT`

空指针、非法重叠、容量不足、长度溢出和 OOM 继续使用统一的 `XERR_ARGUMENT`、`XERR_RANGE` 与 `XERR_MEMORY` 类别。

## 示例与测试

- `examples/codec/base64/main.c`
- `tests/codec/test_base64.c`
- `tests/codec/test_base64_oom.c`
- `tests/single/test_single_codec_base64.c`

测试覆盖 RFC 4648 向量、URL-safe、自定义字母表、PEM 空白、无填充、原地编解码、格式原子性、长度溢出和 OOM。旧版 `lib/string.h` 的核心位拼装逻辑被保留并收敛到这里；旧版宽松填充、无长度返回、空值哨兵和 WebSocket 私有副本不再保留。



# Percent Codec

`codec_percent` 是 URI 字节转义的纯编解码层。它不解析 URL，不拆 Query，也不实现 `application/x-www-form-urlencoded` 的加号规则，因此这些上层协议可以独立裁剪并共用同一个基础实现。

## 流式解码

```c
typedef enum xpercentnext {
	XPERCENT_NEXT_ERROR = -1,
	XPERCENT_NEXT_END = 0,
	XPERCENT_NEXT_BYTE = 1
} xpercentnext;

xpercentnext xrtPercentNext(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pOffset,
	uint8* pValue
);
```

`xrtPercentNext` 是零分配逐字节路径。`Offset` 从零开始，成功读取时才推进；
非法转义、回绕或输出别名返回 `ERROR`，保持输出和线程错误不变。URI 路径应把
`bPlusAsSpace` 设为 `false`；只有表单语义才把加号解码为空格。

## 批量与高性能 API

```c
typedef struct xpercentmap {
	uint64 Bits[2];
} xpercentmap;

bool xrtPercentMapInit(
	xpercentmap* map, xstrview safe, bool includeUnreserved
);
bool xrtPercentMeasure(
	const void* data, size_t size, const xpercentmap* map,
	bool spaceAsPlus, size_t* outputSize
);
size_t xrtPercentWriteMeasured(
	const void* data, size_t size, const xpercentmap* map,
	bool spaceAsPlus, char* output
);
void xrtPercentEncodeMeasured(
	const void* data, size_t size, const xpercentmap* map,
	bool spaceAsPlus, char* output, size_t outputSize, bool terminate
);
bool xrtPercentDecodeMeasure(
	xstrview text, bool plusAsSpace, size_t* outputSize
);
size_t xrtPercentDecodeMeasured(
	xstrview text, bool plusAsSpace, void* output
);
```

`xpercentmap` 面向 Query、表单和其他多字段协议实现。字符集合只构建一次，随后
可以重复测量和写出任意数量的字段，避免每个字段重复解析安全字符配置。
`includeUnreserved` 为真时自动加入 RFC 3986 unreserved 字符；`safe` 只接受可见
ASCII。表单层使用精确的 `ALPHA / DIGIT / "*" / "-" / "." / "_"` 集合，并把
`spaceAsPlus` / `plusAsSpace` 设为真。

`Measure` 和 `DecodeMeasure` 是安全预检边界。两个 `Measured` 写入函数不再重复
容量和格式检查，调用方必须使用同一输入、位图和模式，并提供不少于测量结果的
空间；`WriteMeasured` 用于互不重叠的协议片段，`EncodeMeasured` 还支持输入输出
同址的向后扩张。这个分层让普通调用保持完整安全检查，同时让协议实现获得无
分配、无重复扫描的稳定快速路径。

## 裁剪

```c
#define XRT_FEATURE_CODEC_PERCENT
```

该宏启用 `include/xrt/codec.h` 中的 percent API 和 `src/codec/percent.c`，只依赖 `core`。它可以在不启用 URL、HTTP、字符串对象或网络时单独使用。

## 基础 API

```c
bool xrtPercentEncode(
	const void* data, size_t size, xstrview extraSafe,
	char* output, size_t capacity, size_t* outputSize
);

bool xrtPercentWrite(
	const void* data, size_t size, xstrview extraSafe,
	char* output, size_t capacity, size_t* outputSize
);

bool xrtPercentDecode(
	xstrview text,
	void* output, size_t capacity, size_t* outputSize
);
```

编码器始终直接保留 RFC 3986 unreserved 字节 `ALPHA / DIGIT / "-" / "." / "_" / "~"`，其他字节使用大写 `%HH`。`extraSafe` 可按 URL 组件额外保留 RFC 3986 reserved 字符，例如：

- path segment 使用空集合，斜杠会编码为 `%2F`；
- 完整 path 可以传 `/`；
- 已知结构安全的 authority 子层可以传 `:@`；
- 调用方也可以组合其他 reserved 字符，但必须负责目标组件的结构安全。

`extraSafe` 中重复出现的 unreserved 或 reserved 字符没有副作用。`%`、空白、控制字符、反斜杠和非 ASCII 字节不能声明为安全字符，避免产生含伪造转义或不合法字节的 URI 文本。

把输出传为 `NULL`、容量传为零，可以完整验证参数和格式并查询所需长度。`xrtPercentEncode` 的长度不包含零结尾，实际编码缓冲区必须额外保留一个字节；`xrtPercentWrite` 不写零结尾，容量恰好等于返回长度即可直接写入协议片段。解码输出是任意二进制，可以包含零。

编码和解码都支持输出与输入从同一地址开始。编码器从后向前扩张，解码器从前向后收缩；其他部分重叠会被拒绝。格式或配置失败保持输出和 `outputSize` 不变，容量不足时保持输出不变并通过 `outputSize` 返回精确所需长度。

所有显式指针范围都会在扫描前检查地址加长度是否回绕。容量不足时，别名检查只使用调用方声明的实际输出容量，而不是理论所需长度；因此结构体中紧随小缓冲区的独立长度字段仍能可靠接收精确需求值。

解码器接受大小写十六进制数字，并严格要求每个 `%` 后紧跟两个十六进制数字。它只负责 percent 语法，不替 URL 解析器判断组件是否合法，也不会把 `+` 转换为空格；后者只属于 form-urlencoded。

## 便捷 API

```c
char* xrtPercentEncodeNew(
	const void* data, size_t size,
	xstrview extraSafe, size_t* outputSize
);

unsigned char* xrtPercentDecodeNew(
	xstrview text, size_t* outputSize
);
```

两个结果都由 `xrtFree` 释放。编码长度输出可以为空；二进制解码要求长度输出非空，并额外保留一个不计入长度的末尾零哨兵。

## 错误

percent 配置与格式错误使用 `xrt.codec` 域：

- `XCODEC_ERROR_PERCENT_CONFIG`：`extraSafe` 包含不能直接写入 URI 的字符；
- `XCODEC_ERROR_PERCENT_FORMAT`：输入包含不完整或非法的 `%HH`。

空指针、非法重叠、容量不足、长度溢出和 OOM 使用统一错误类别。

## 旧资产与验证

旧版 `lib/xhttp_util.h` 的直接缓冲与分配便捷路径被保留，`lib/xurl.h` 中重复的解码实现被合并。普通 URI API 不暴露容易误用的表单模式；布尔模式只保留在明确要求调用方先测量的协议实现层，`xrtForm*` 仍是应用代码的无歧义入口。

- `examples/codec/percent/main.c`
- `tests/codec/test_percent.c`
- `tests/codec/test_percent_mutation.c`
- `tests/codec/test_percent_noalloc.c`
- `tests/codec/test_percent_oom.c`
- `tests/single/test_single_codec_percent.c`

测试覆盖旧向量、全部 256 个字节、reserved 上下文、嵌入零、加号边界、自定义精确字符位图、已测量写入、原地路径、部分重叠、短缓冲原子性、非法转义、配置错误、6000 组确定性变异、无分配路径、OOM 与单头文件。
