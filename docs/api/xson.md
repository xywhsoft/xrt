# XSON

`xson` 是 XRT `xvalue` 的可移植扩展文本格式。所有严格 JSON 都是语义不变的
XSON；XSON 只通过显式语法补充 bytes、time、int-map、set、非有限浮点和自定义
标签。公共协议优先使用 JSON，需要保留完整 `xvalue` 类型的内部快照、缓存和调试
文件使用 XSON。

自定义标签名称不设置固定字节上限；读取时由 `MaxInputBytes` 约束，写出时由
`MaxOutputBytes` 约束。标签视图由 parser 或调用方借用，长名称不会产生等长临时分配。

## 裁剪与依赖

| 能力 | 公开选择宏 | 实现宏 | 主要依赖 |
|---|---|---|---|
| 公共错误与位置 | `XRT_MODULE_XSON_CORE` | `XRT_FEATURE_XSON_CORE` | `core` |
| 校验、事件访问、DOM 解析 | `XRT_MODULE_XSON_READ` | `XRT_FEATURE_XSON_READ` | `xson_core`, `buffer`, `base64`, `time_text`, `number`, `unicode`, `value_container` |
| DOM 与增量写出 | `XRT_MODULE_XSON_WRITE` | `XRT_FEATURE_XSON_WRITE` | `xson_core`, `buffer`, `base64`, `time_text`, `number`, `unicode`, `value_container` |
| XSON 文件读写 | `XRT_MODULE_XSON_FILE` | `XRT_FEATURE_XSON_FILE` | `xson_read`, `xson_write`, `file_whole` |
| 完整 XSON | `XRT_MODULE_XSON` | `XRT_FEATURE_XSON` | `xson_file` |

读、写和文件层可独立裁剪。只访问事件时不需要文件层，只生成内部快照时不需要
读取层。

## 语法

标准 JSON 类型保持原义：

```xson
null
true
123
3.14
"text"
[1, 2, 3]
{"name": "xrt"}
```

扩展类型只有以下显式形式：

```xson
bytes("AAEC/w==")
time("2026-07-31T08:00:00+08:00")
float("nan")
float("inf")
float("-inf")
intmap{-5: "left", 7: true}
set["read", "write"]
app.id("42")
```

- `bytes` 使用标准、带规范填充的 Base64。
- `time` 使用严格 RFC 3339；写出时统一规范化为 UTC，并使用 `Z`。
- 有限浮点继续使用 JSON 数字；只有 `nan`、`inf`、`-inf` 使用 `float` 标签。
- `intmap` 键必须是 `int64` 整数字面量。
- `set` 元素必须满足 `xvalue` 集合的可哈希标量约束。
- 自定义标签名最长 128 字节，首字节是 ASCII 字母或下划线，后续还可使用数字、
  `.` 和 `-`；载荷始终是完成 JSON 反转义的 UTF-8 字符串。
- `bytes`、`time`、`float`、`intmap` 和 `set` 是保留名称。

格式不支持根据第一个元素猜测容器类型，也不支持旧版 `class(Base64)` 原始 ABI
内存快照。空数组、空对象、空集合和空整数映射分别固定写作 `[]`、`{}`、
`set[]`、`intmap{}`。

## 稳定契约

- 默认语法严格；注释、尾随逗号和自定义标签必须分别开启。
- 输入和字符串都使用显式长度，解码后的字符串允许包含 `U+0000`。
- 所有文本和标签载荷必须是合法 UTF-8，字符串转义遵守 JSON 规则。
- `xrtXsonParse("null")` 返回 `xrtValueNull()`；失败返回 C `NULL`。
- 整数优先无损保存为 `int64`，非负且超过 `INT64_MAX` 时保存为 `uint64`；超出 `uint64` 或低于 `int64` 范围默认失败。
- 读取和写出默认都有有限资源预算；配置必须先调用对应 `Init`。
- 事件中的字符串、字节、键、原始 token 和标签视图只在回调期间有效。
- sink 回调必须在返回前消费字节。回调设置的具体错误会保留为当前错误。
- 增量 writer 不可重入；任一失败都会进入不可恢复的失败终态。
- 文件写出先完成序列化，再原子替换目标。序列化失败不会改变已有文件。
- 模块不带隐式锁。不同解析或写入对象可并行使用，同一 writer 由调用方串行访问。

## 默认预算

```c
#define XXSON_DEPTH_DEFAULT     256u
#define XXSON_INPUT_DEFAULT     (64u * 1024u * 1024u)
#define XXSON_STRING_DEFAULT    (16u * 1024u * 1024u)
#define XXSON_VALUES_DEFAULT    1000000u
#define XXSON_CONTAINER_DEFAULT 1000000u
#define XXSON_DECODED_DEFAULT   (64u * 1024u * 1024u)
```

`MaxDecodedBytes` 约束单个 `bytes` 标签的解码结果。服务端和不可信文件读取应按
实际接口进一步收紧全部预算。

## 错误与位置

XSON 模块使用 `xrt.xson` 错误域：

```c
typedef enum xxsonerror {
	XXSON_ERROR_CONFIG = 1401,
	XXSON_ERROR_SYNTAX,
	XXSON_ERROR_LIMIT,
	XXSON_ERROR_DUPLICATE,
	XXSON_ERROR_NUMBER,
	XXSON_ERROR_TAG,
	XXSON_ERROR_STATE,
	XXSON_ERROR_UNSUPPORTED,
	XXSON_ERROR_OUTPUT,
	XXSON_ERROR_IO
} xxsonerror;
```

语法、限制、数值、重复键和标签错误尽量携带位置：

```c
typedef struct xxsonlocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xxsonlocation;

bool xrtXsonErrorLocation(
	const xerror* pError,
	xxsonlocation* pLocation
);
```

`Offset` 是零基 UTF-8 字节偏移，行列从 1 开始。函数只读取机器数据，不修改
当前错误；错误域不匹配或错误没有位置时返回 `false`。

## 读取配置

```c
typedef enum xxsonreadflag {
	XXSON_READ_COMMENTS = UINT32_C(0x00000001),
	XXSON_READ_TRAILING_COMMA = UINT32_C(0x00000002),
	XXSON_READ_CUSTOM = UINT32_C(0x00000004)
} xxsonreadflag;

typedef enum xxsonduplicate {
	XXSON_DUPLICATE_REJECT = 0,
	XXSON_DUPLICATE_KEEP,
	XXSON_DUPLICATE_REPLACE
} xxsonduplicate;

typedef enum xxsonbigint {
	XXSON_BIGINT_REJECT = 0,
	XXSON_BIGINT_FLOAT
} xxsonbigint;
```

对象和 `intmap` 默认拒绝重复键。`KEEP` 保留首值并继续完整校验后续值，
`REPLACE` 保留末值。`BIGINT_FLOAT` 允许把超出 `uint64` 或低于 `int64` 范围的整数有损读为 `double`。

```c
typedef xvalue* (*xxsondecodeproc)(
	xstrview Tag,
	xstrview Payload,
	ptr pUserData
);

typedef struct xxsonreadconfig {
	uint32 Flags;
	xxsonduplicate Duplicate;
	xxsonbigint BigInteger;
	uint32 MaxDepth;
	size_t MaxInputBytes;
	size_t MaxStringBytes;
	size_t MaxValues;
	size_t MaxContainerItems;
	size_t MaxDecodedBytes;
	xxsondecodeproc Decode;
	ptr DecodeData;
	uint32 Reserved[4];
} xxsonreadconfig;

void xrtXsonReadConfigInit(xxsonreadconfig* pConfig);
```

自定义标签需要同时开启 `XXSON_READ_CUSTOM`。事件访问会直接报告
`XXSON_EVENT_CUSTOM`；DOM 解析还要求 `Decode` 返回一个拥有引用，解析器成功挂入
后接管该引用。解码器失败返回 `NULL`，并应设置业务错误。`Reserved` 必须保持零。

## DOM 读取

```c
xvalue* xrtXsonParse(xstrview Text);

xvalue* xrtXsonRead(
	xstrview Text,
	const xxsonreadconfig* pConfig
);

bool xrtXsonValid(xstrview Text);
```

`Parse` 使用默认严格配置，`Read` 用于语法选项、重复键、自定义标签和资源预算。
返回值由 `xrtValueRelease` 释放。`Valid` 不构造 DOM，但为字符串反转义和 bytes
语义校验可能使用可复用临时缓冲，因此不承诺零分配。

映射关系如下：

| XSON | `xvaluetype` |
|---|---|
| `null`, `bool`, signed integer, unsigned integer, finite/nonfinite float, string | 对应 `XVALUE_NULL`、`XVALUE_BOOL`、`XVALUE_INT`、`XVALUE_UINT`、`XVALUE_FLOAT`、`XVALUE_STRING` |
| `bytes(...)` | `XVALUE_BYTES` |
| `time(...)` | `XVALUE_TIME` |
| `[...]` | `XVALUE_ARRAY` |
| `intmap{...}` | `XVALUE_INT_MAP` |
| `set[...]` | `XVALUE_SET` |
| `{...}` | `XVALUE_OBJECT` |
| custom tag | 由 `Decode` 决定 |

## 事件访问

```c
typedef enum xxsonvisitaction {
	XXSON_VISIT_NEXT = 0,
	XXSON_VISIT_STOP,
	XXSON_VISIT_FAIL
} xxsonvisitaction;

typedef enum xxsonvisitresult {
	XXSON_VISIT_ERROR = -1,
	XXSON_VISIT_DONE = 0,
	XXSON_VISIT_STOPPED = 1
} xxsonvisitresult;
```

`xxsoneventtype` 明确区分标量事件和容器边界：

| 事件 | 含义 |
|---|---|
| `XXSON_EVENT_NULL` | 空值，`Value` 无效 |
| `XXSON_EVENT_BOOL` | 布尔值，读取 `Value.Boolean` |
| `XXSON_EVENT_INT` | 64 位有符号整数，读取 `Value.Integer` |
| `XXSON_EVENT_UINT` | 64 位无符号整数，读取 `Value.Unsigned` |
| `XXSON_EVENT_FLOAT` | 浮点数，包括显式非有限值，读取 `Value.Float` |
| `XXSON_EVENT_STRING` | UTF-8 字符串，读取 `Value.String` |
| `XXSON_EVENT_BYTES` | 已解码字节串，读取 `Value.Bytes` |
| `XXSON_EVENT_TIME` | 已解析时间，读取 `Value.Time` |
| `XXSON_EVENT_CUSTOM` | 自定义标签，读取 `Value.Tag` |
| `XXSON_EVENT_ARRAY_BEGIN` / `XXSON_EVENT_ARRAY_END` | 数组开始与结束 |
| `XXSON_EVENT_INT_MAP_BEGIN` / `XXSON_EVENT_INT_MAP_END` | 整数键映射开始与结束 |
| `XXSON_EVENT_SET_BEGIN` / `XXSON_EVENT_SET_END` | 集合开始与结束 |
| `XXSON_EVENT_OBJECT_BEGIN` / `XXSON_EVENT_OBJECT_END` | 字符串键对象开始与结束 |

事件结构为：

```c
typedef struct xxsontag {
	xstrview Name;
	xstrview Payload;
} xxsontag;

typedef struct xxsonevent {
	xxsoneventtype Type;
	xxsonlocation Location;
	size_t Depth;
	xvaluekey Key;
	xstrview Raw;
	union {
		bool Boolean;
		int64 Integer;
		uint64 Unsigned;
		double Float;
		xstrview String;
		xbytesview Bytes;
		xtime Time;
		xxsontag Tag;
	} Value;
} xxsonevent;
```

`Key.Type` 在对象、数组、int-map 和 set 中分别是 `XVALUE_KEY_STRING`、
`XVALUE_KEY_INDEX`、`XVALUE_KEY_INT` 和 `XVALUE_KEY_NONE`。数字事件的 `Raw`
保留原 token，便于上层接入 BigInt 或 Decimal。

```c
typedef xxsonvisitaction (*xxsonvisitproc)(
	const xxsonevent* pEvent,
	ptr pUserData
);

xxsonvisitresult xrtXsonVisit(
	xstrview Text,
	const xxsonreadconfig* pConfig,
	xxsonvisitproc pVisitor,
	ptr pUserData
);
```

`STOP` 是正常提前完成。`FAIL` 应先设置具体业务错误；没有新错误时 XSON 层建立
`XXSON_ERROR_STATE`。

## 写出配置

```c
typedef enum xxsonwriteflag {
	XXSON_WRITE_PRETTY = UINT32_C(0x00000001),
	XXSON_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XXSON_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XXSON_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008)
} xxsonwriteflag;

typedef enum xxsonunsupported {
	XXSON_UNSUPPORTED_REJECT = 0,
	XXSON_UNSUPPORTED_SKIP
} xxsonunsupported;
```

`ESCAPE_HTML` 转义 `<`、`>`、`&`，但不能代替完整 HTML 上下文转义。
`ESCAPE_NON_ASCII` 使用 `\uXXXX`，补充平面字符输出代理对。

```c
typedef enum xxsoncoderesult {
	XXSON_CODE_ERROR = -1,
	XXSON_CODE_UNSUPPORTED = 0,
	XXSON_CODE_OK = 1
} xxsoncoderesult;

typedef xxsoncoderesult (*xxsonencodeproc)(
	const xvalue* pValue,
	xstrview* pTag,
	xstrview* pPayload,
	ptr pUserData
);

typedef struct xxsonwriteconfig {
	uint32 Flags;
	xxsonunsupported Unsupported;
	uint32 MaxDepth;
	uint32 Indent;
	size_t MaxOutputBytes;
	xxsonencodeproc Encode;
	ptr EncodeData;
	uint32 Reserved[4];
} xxsonwriteconfig;

void xrtXsonWriteConfigInit(xxsonwriteconfig* pConfig);
```

内建 XSON 可以无损写出除 Pointer、Handle 外的可移植 `xvalue` 类型。Pointer 和
Handle 默认失败，`Encode` 可把它们映射为非保留自定义标签。编码器接收只在回调
期间有效的只读快照，可以使用 Value getter；不能保存该外壳。返回的 Tag/Payload
视图必须保持到当前 `WriterValue` 返回。

`XXSON_UNSUPPORTED_SKIP` 只跳过 DOM 容器中的 Pointer/Handle 成员；根值和直接
`WriterValue` 仍失败。选择 SKIP 时这些成员不会再进入编码器。`Indent` 范围为
0 到 16，`Reserved` 必须保持零。

## DOM 与 sink 写出

```c
str xrtXsonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
);

typedef bool (*xxsonwriteproc)(xbytesview Data, ptr pUserData);

bool xrtXsonWrite(
	const xvalue* pValue,
	const xxsonwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
);
```

`Stringify` 返回由 `xrtFree` 释放的零结尾文本；`pSize` 不包含末尾零，失败时保持
不变。`Write` 同步提交若干借用字节块，不保留完整结果。sink 已接受的块无法撤回，
因此 sink 路径不提供事务输出。

Bytes 使用固定小块流式 Base64 编码，不建立第二份等大的 Base64 临时字符串。

## 增量 writer

```c
xxsonwriter* xrtXsonWriterCreate(const xxsonwriteconfig* pConfig);
xxsonwriter* xrtXsonWriterCreateSink(
	const xxsonwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
);
```

两种 writer 共享以下操作：

```c
bool xrtXsonWriterObject(xxsonwriter* pWriter);
bool xrtXsonWriterArray(xxsonwriter* pWriter);
bool xrtXsonWriterIntMap(xxsonwriter* pWriter);
bool xrtXsonWriterSet(xxsonwriter* pWriter);
bool xrtXsonWriterEnd(xxsonwriter* pWriter);
bool xrtXsonWriterName(xxsonwriter* pWriter, xstrview Name);
bool xrtXsonWriterKey(xxsonwriter* pWriter, int64 iKey);
bool xrtXsonWriterNull(xxsonwriter* pWriter);
bool xrtXsonWriterBool(xxsonwriter* pWriter, bool bValue);
bool xrtXsonWriterInt(xxsonwriter* pWriter, int64 iValue);
bool xrtXsonWriterUInt(xxsonwriter* pWriter, uint64 iValue);
bool xrtXsonWriterFloat(xxsonwriter* pWriter, double fValue);
bool xrtXsonWriterString(xxsonwriter* pWriter, xstrview Text);
bool xrtXsonWriterBytes(xxsonwriter* pWriter, xbytesview Data);
bool xrtXsonWriterTime(xxsonwriter* pWriter, xtime Time);
bool xrtXsonWriterTag(
	xxsonwriter* pWriter,
	xstrview Tag,
	xstrview Payload
);
bool xrtXsonWriterValue(xxsonwriter* pWriter, const xvalue* pValue);
bool xrtXsonWriterFinish(xxsonwriter* pWriter);
str xrtXsonWriterTake(xxsonwriter* pWriter, size_t* pSize);
void xrtXsonWriterFree(xxsonwriter* pWriter);
```

Object 必须按 `Name -> Value` 交替写入；IntMap 按 `Key -> Value` 交替；Array 和 Set
直接写值。`End` 结束最近容器。每个 writer 只接受一个根值，全部容器关闭后使用
`Finish` 封闭结果。`Value` 在当前位置写入已有 DOM 子树。

`Take` 只适用于已完成的内存 writer，只能成功一次，结果由 `xrtFree` 释放。
sink writer 不支持 `Take`。`Free` 可接收空指针。

## 文件

```c
xvalue* xrtXsonParseFile(cstr sPath);
xvalue* xrtXsonReadFile(
	cstr sPath,
	const xxsonreadconfig* pConfig
);
bool xrtXsonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xxsonwriteconfig* pConfig
);
bool xrtXsonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
);
```

`ReadFile` 在读取阶段执行 `MaxInputBytes`，然后使用同一配置解析。文件打开、读取、
临时文件、刷新或替换失败建立 `XXSON_ERROR_IO`，底层文件错误保留在原因链。

完整可运行示例位于 `examples/data/xson/main.c`。
