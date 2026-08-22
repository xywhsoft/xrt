# HTTP 验证器、条件请求与范围

`http_semantics.h` 提供与传输、客户端和服务器都无关的 HTTP 语义底座。
它不创建响应对象，也不要求网络模块；调用方可以把结果用于原始封包、高级
服务器、缓存、文件服务或自定义协议网关。

## 裁剪边界

| 宏 | 能力 | 依赖 |
|---|---|---|
| `XHTTP_FEATURE_HTTP_ETAG` | entity-tag 解析、列表、比较、写出 | `HTTP` |
| `XHTTP_FEATURE_HTTP_PRECONDITION` | 条件请求顺序、HTTP-date、If-Range | `HTTP_ETAG`、`TIME_TEXT` |
| `XHTTP_FEATURE_HTTP_RANGE` | Range、字节范围、Content-Range | `HTTP` |
| `XHTTP_FEATURE_HTTP_RANGE_MULTIPART` | multipart/byteranges 长度与分段写出 | `HTTP_RANGE` |

四个能力独立成实现单元。只处理 Range 的客户端不需要带入日期解析，只使用
ETag 的程序也不需要带入条件请求；单范围应用不需要带入 multipart 编码器。

## 公共分类

ETag 列表项区分普通值和独占星号：

```c
typedef enum xhttpetagkind {
	XHTTP_ETAG_VALUE = 1,
	XHTTP_ETAG_ANY
} xhttpetagkind;

typedef struct xhttpetagitem {
	xhttpetagkind Kind;
	xhttpetag Tag;
} xhttpetagitem;
```

条件请求和 Range 的结果可以直接映射到 HTTP 响应决策：

```c
typedef enum xhttpprecondition {
	XHTTP_PRECONDITION_ERROR = -1,
	XHTTP_PRECONDITION_PROCEED = 0,
	XHTTP_PRECONDITION_NOT_MODIFIED = 304,
	XHTTP_PRECONDITION_FAILED = 412
} xhttpprecondition;

typedef enum xhttprangespecform {
	XHTTP_RANGE_SPEC_CLOSED = 1,
	XHTTP_RANGE_SPEC_OPEN,
	XHTTP_RANGE_SPEC_SUFFIX
} xhttprangespecform;

typedef enum xhttprangeresult {
	XHTTP_RANGE_ERROR = -1,
	XHTTP_RANGE_UNSATISFIED = 0,
	XHTTP_RANGE_SATISFIED = 1,
	XHTTP_RANGE_EMPTY = 2
} xhttprangeresult;
```

线路范围和 Content-Range 使用以下无所有权描述符：

```c
typedef struct xhttprangespec {
	xhttprangespecform Form;
	uint64 First;
	uint64 Last;
} xhttprangespec;

typedef struct xhttpcontentrange {
	bool Satisfied;
	bool HasLength;
	uint64 First;
	uint64 Last;
	uint64 Length;
} xhttpcontentrange;
```

## 实体标签

`xhttpetag` 借用 opaque-tag 正文，不包含线路双引号：

```c
xhttpetag Tag;

if ( !xrtHttpETagParse(XRT_STR_LITERAL("W/\"revision-7\""), &Tag) ) {
	return false;
}
```

`xrtHttpETagParse` 严格接收一个完整 `entity-tag`。`W/` 区分大小写，正文允许
空值、可见 ASCII 中除双引号以外的字节以及 obs-text，不接受 OWS、控制字符
或额外尾部。返回的 `Opaque` 借用输入。

`xrtHttpETagNext` 迭代 `If-Match` 与 `If-None-Match` 使用的列表；`Offset`
初始为零。接收方会忽略逗号产生的空列表元素，但星号只能作为整个字段值
出现。`xrtHttpETagListStrongHas` 与 `xrtHttpETagListWeakHas` 即使命中也会
验证余下文本，避免恶意尾部绕过语法检查。

强比较要求两个标签都不是弱标签且正文逐字节相等。弱比较忽略 `Weak` 标记，
但正文仍区分大小写。`xrtHttpETagWrite` 是无分配路径；传入空输出可查询精确
长度。`xrtHttpETagBuild` 返回由 `xrtFree` 释放的零结尾字符串。

```c
bool xrtHttpETagStrongEqual(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
);
bool xrtHttpETagWeakEqual(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
);
```

Tag、列表迭代游标、迭代结果和长度输出都可以使用未对齐存储。比较、列表和
写出接口会先快照完整 Tag，再访问其中的借用正文；固定结构、标量和正文范围
都必须完整且地址不回绕。写出成功后才发布最终长度。

## 条件请求

`xhttprepresentation` 描述服务器已经选择的当前表示：

```c
xhttprepresentation Current = { 0 };

Current.Exists = true;
Current.HasETag = true;
Current.ETag.Opaque = XRT_STR_LITERAL("revision-7");
Current.HasLastModified = true;
Current.LastModifiedStrong = true;
Current.LastModified = Modified;
```

`HasETag` 与 `HasLastModified` 只表示验证器可用，不会由库猜测生成。
`LastModifiedStrong` 只影响 If-Range；调用方必须根据验证器生成条件决定日期
是否足够强。不存在的表示不得携带验证器。

`xrtHttpPreconditionsEvaluate` 读取 `xhttpfield` 数组，因此既可接受
`xhttpheaders` 的连续数据，也能用于栈上字段：

```c
xhttpprecondition Result = xrtHttpPreconditionsEvaluate(
	XRT_STR_LITERAL("GET"),
	xrtHttpHeadersData(pHeaders),
	xrtHttpHeadersCount(pHeaders),
	&Current
);
```

字段描述符数组与 `xhttprepresentation` 可以放在未对齐存储中；库会先验证
完整、非回绕的结构范围，再逐项快照。字段名、字段值与 ETag 的借用字节仍须在
调用期间有效。`xrtHttpIfRangeMatch` 对 Current 使用同一规则。

评估顺序固定为：

1. If-Match，使用强 ETag 比较。
2. 仅在没有 If-Match 时评估 If-Unmodified-Since。
3. If-None-Match，使用弱 ETag 比较。
4. 仅对 GET/HEAD 且没有 If-None-Match 时评估 If-Modified-Since。

结果为 `PROCEED`、`NOT_MODIFIED`、`FAILED` 或 `ERROR`，分别对应继续、304、
412 和字段/调用错误。HTTP 方法名区分大小写。CONNECT、OPTIONS 与 TRACE
直接忽略这些前置条件；自定义方法是否选择或修改表示由服务器在调用前决定。
日期比较降到整秒，因为 HTTP-date 没有亚秒精度；无效、重复或列表化日期按
协议被忽略且不会污染线程错误。畸形 ETag 列表返回 `ERROR`，不会静默绕过
写入前置条件。服务器必须先完成普通路由、认证和状态检查，并且只在无条件
响应原本会是 2xx 或 412 时调用评估器。

`xrtHttpIfRangeMatch` 独立判断 Range 是否仍可使用。实体标签必须强匹配；
日期必须和当前强 Last-Modified 验证器在整秒上精确相等。无效值、弱标签和
不匹配都返回 false，调用方应忽略 Range 并发送完整表示，而不是返回 412。

## Range

`xrtHttpRangeParse` 先把字段拆成大小写不敏感的范围单位和单位专用集合。
这保留扩展单位，不会把未知单位误当成畸形 bytes：

```c
xstrview Unit;
xstrview Set;

if ( !xrtHttpRangeParse(Value, &Unit, &Set) ) {
	return false;
}
if ( !xrtHttpTokenEqual(Unit, XRT_STR_LITERAL("bytes")) ) {
	/* 由应用决定忽略、扩展处理或返回 416。 */
}
```

`xrtHttpByteRangeNext` 无分配迭代闭区间 `0-99`、开放区间 `200-` 和后缀
`-50`。所有线路整数使用 `uint64` 并检查上溢。`xrtHttpByteRangeResolve`
再把一项解析到完整表示长度内，返回：

- `SATISFIED`：`xhttpbyterange` 是已经裁剪的闭区间。
- `UNSATISFIED`：整数范围起点越界或后缀长度为零。
- `EMPTY`：零长度表示上的非零 suffix-range；规范称其可满足，但没有可写入
  Content-Range 的字节。服务器应利用“空表示可忽略 Range”的规则发送完整
  200 空响应。
- `ERROR`：项本身无效，例如结束位置小于起点。

`xrtHttpByteRangesResolve` 把完整 byte-range-set 一次解析到调用方数组，忽略
其中不可满足的项，对可满足项排序，并合并重叠、相邻或间距不超过 `MergeGap`
的区间：

```c
xhttpbyterange ranges[16];
size_t rangeCount;
uint64 selectedLength;

xhttprangeresult result = xrtHttpByteRangesResolve(
	Set,
	completeLength,
	ranges,
	16,
	0,
	&rangeCount,
	&selectedLength
);
```

输入项数超过容量会在修改输出前返回 `ERROR`，因此容量同时是明确的工作上限。
只要存在可满足项，结果就是 `SATISFIED`；全部越界或 `-0` 返回
`UNSATISFIED`；零长度表示只有非零后缀项时返回 `EMPTY`。成功时前
`rangeCount` 项是规范化闭区间，`selectedLength` 不重复计算被合并的字节。
应用仍应在进入解析前限制重复 Range 字段和字段总长度。

Range 的解析输出、迭代游标、范围项、范围数组及计数/长度输出均支持未对齐
存储。每个固定结构和数组必须覆盖完整且地址不回绕的范围；函数通过局部快照
读取，并只在成功后发布结果。`xrtHttpRangeMultipartLength` 对范围数组与长度
输出采用相同契约。

`xrtHttpRangeWrite` / `Build` 生成 `bytes=` 请求值。`xrtHttpContentRangeParse`
接收满足形式 `bytes 0-99/200`、未知完整长度 `bytes 0-99/*` 和不满足形式
`bytes */200`；它会验证闭区间及完整长度的一致性。对应的 `Write` / `Build`
用于 206 分段和 416 响应。

```c
bool xrtHttpByteRangeCount(xstrview Set, size_t* pCount);
str xrtHttpRangeBuild(
	const xhttprangespec* pSpecs,
	size_t iCount,
	size_t* pSize
);
bool xrtHttpContentRangeWrite(
	const xhttpcontentrange* pRange,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
str xrtHttpContentRangeBuild(
	const xhttpcontentrange* pRange,
	size_t* pSize
);
```

## 多范围线缆

`XHTTP_FEATURE_HTTP_RANGE_MULTIPART` 提供与文件、Body、客户端和服务器都无关的
canonical `multipart/byteranges` 编码器：

```c
uint64 bodyLength;
size_t headLength;

if ( !xrtHttpRangeMultipartLength(
	ranges,
	rangeCount,
	completeLength,
	contentType,
	boundary,
	&bodyLength
) || !xrtHttpRangeMultipartHeadWrite(
	&ranges[0],
	completeLength,
	contentType,
	boundary,
	head,
	headCapacity,
	&headLength
) ) {
	return false;
}
```

线缆格式固定为：

```text
--boundary\r\n
Content-Type: media/type\r\n
Content-Range: bytes first-last/complete\r\n
\r\n
range bytes\r\n
...
--boundary--\r\n
```

`xrtHttpRangeMultipartLength` 计算包含全部 Part 头、范围负载、段尾 CRLF 和关闭
边界的精确正文长度。范围必须位于完整表示内、按起点严格递增且互不重叠；
边界必须是 1 到 70 字节的 HTTP token，空媒体类型采用
`application/octet-stream`。

`xrtHttpRangeMultipartHeadWrite` 写出一个 Part 头，`EndWrite` 写出负载后的
CRLF，`CloseWrite` 写出最终关闭边界。三个写函数都支持空输出测量，容量不足
不会留下部分数据，所有 `size_t` 与 `uint64` 累加都检查溢出。完整示例位于
`examples/http/range_multipart/main.c`。

```c
bool xrtHttpRangeMultipartEndWrite(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
bool xrtHttpRangeMultipartCloseWrite(
	xstrview Boundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

Range/Content-Range 写入数组、单项描述符、multipart Part 范围和所有长度
输出均支持未对齐存储。实际写入前会验证输出的精确范围；地址回绕、输入输出
重叠或容量不足都不会留下部分线缆数据。

## 所有权与错误

所有解析结果都借用输入。`Write` 函数不分配，不附加零字符，容量不足时返回
所需长度并设置范围错误。`Build` 是唯一分配路径，返回值由 `xrtFree` 释放。
解析结构、迭代游标和计数输出不得覆盖输入字节或彼此重叠；别名错误在修改
输入和输出前返回。写出目标也不得覆盖仍需读取的描述结构或借用字节。
调用参数错误使用 `XERR_ARGUMENT`，线路或语义值错误使用 `XERR_VALUE`，
大小计算上溢使用 `XERR_RANGE`。

## 历史资产

本模块继承旧 `xweb` 静态文件服务已经验证的 ETag、固定范围、开放范围、
后缀范围、304 和 416 场景，但没有照搬其内部函数。旧实现只比较文本、只
支持单范围，且没有完整前置条件顺序；新模块把这些能力提升为公共协议 API，
并补齐强弱验证器、多字段、64 位边界和扩展范围单位。
