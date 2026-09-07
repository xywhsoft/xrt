# XSON

`xson` 是 XRT `xvalue` 的可移植扩展文本格式。所有严格 JSON 都是语义不变的
XSON；XSON 只通过显式语法补充 bytes、time、int-map、set、非有限浮点和自定义
标签。公共协议优先使用 JSON，需要保留完整 `xvalue` 类型的内部快照、缓存和调试
文件使用 XSON。

自定义标签名称不设置固定字节上限；读取时由 `MaxInputBytes` 约束，写出时由
`MaxOutputBytes` 约束。标签视图由 parser 或调用方借用，长名称不会产生等长临时分配。

## 类型与常量

### `xxsonerror`

XSON 模块错误码在 xrt.xson 域内保持稳定。

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

| 值 | 语义 |
|---|---|
| `XXSON_ERROR_CONFIG` | 配置非法 |
| `XXSON_ERROR_SYNTAX` | 语法非法 |
| `XXSON_ERROR_LIMIT` | 超限 |
| `XXSON_ERROR_DUPLICATE` | 失败 |
| `XXSON_ERROR_NUMBER` | 失败 |
| `XXSON_ERROR_TAG` | 失败 |
| `XXSON_ERROR_STATE` | 状态非法 |
| `XXSON_ERROR_UNSUPPORTED` | 不支持 |
| `XXSON_ERROR_OUTPUT` | 输出失败 |
| `XXSON_ERROR_IO` | 写出失败 |

### `xxsonlocation`

文本位置使用零基字节偏移和一基行列。

```c
typedef struct xxsonlocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xxsonlocation;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 偏移量 |
| `Line` | `size_t` | 行号 |
| `Column` | `size_t` | 列号 |

### `xxsonreadflag`

非标准空白扩展和自定义标签默认全部关闭。

```c
typedef enum xxsonreadflag {
	XXSON_READ_COMMENTS = UINT32_C(0x00000001),
	XXSON_READ_TRAILING_COMMA = UINT32_C(0x00000002),
	XXSON_READ_CUSTOM = UINT32_C(0x00000004)
} xxsonreadflag;
```

| 值 | 语义 |
|---|---|
| `XXSON_READ_COMMENTS` | 读方向 |
| `XXSON_READ_TRAILING_COMMA` | 读方向 |
| `XXSON_READ_CUSTOM` | 自定义标签解码 |

### `xxsonduplicate`

对象和整数映射使用同一套明确的重复键策略。

```c
typedef enum xxsonduplicate {
	XXSON_DUPLICATE_REJECT = 0,
	XXSON_DUPLICATE_KEEP,
	XXSON_DUPLICATE_REPLACE
} xxsonduplicate;
```

| 值 | 语义 |
|---|---|
| `XXSON_DUPLICATE_REJECT` | REJECT |
| `XXSON_DUPLICATE_KEEP` | KEEP |
| `XXSON_DUPLICATE_REPLACE` | 重名后者覆盖 |

### `xxsonbigint`

超出 int64/uint64 的整数默认失败，可显式按 double 接收。

```c
typedef enum xxsonbigint {
	XXSON_BIGINT_REJECT = 0,
	XXSON_BIGINT_FLOAT
} xxsonbigint;
```

| 值 | 语义 |
|---|---|
| `XXSON_BIGINT_REJECT` | XXSONBIGINTREJECT |
| `XXSON_BIGINT_FLOAT` | 浮点形态（超精度可选） |

### `xxsonreadconfig`

XSON 读取配置同时约束语法、资源预算和自定义类型入口。

```c
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
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Duplicate` | `xxsonduplicate` | Duplicate |
| `BigInteger` | `xxsonbigint` | BigInteger |
| `MaxDepth` | `uint32` | MaxDepth |
| `MaxInputBytes` | `size_t` | MaxInputBytes |
| `MaxStringBytes` | `size_t` | MaxStringBytes |
| `MaxValues` | `size_t` | MaxValues |
| `MaxContainerItems` | `size_t` | MaxContainerItems |
| `MaxDecodedBytes` | `size_t` | MaxDecodedBytes |
| `Decode` | `xxsondecodeproc` | Decode |
| `DecodeData` | `ptr` | DecodeData |

### `xxsoneventtype`

访问事件直接表达全部可移植 XSON 类型。

```c
typedef enum xxsoneventtype {
	XXSON_EVENT_NULL = 0,
	XXSON_EVENT_BOOL,
	XXSON_EVENT_INT,
	XXSON_EVENT_FLOAT,
	XXSON_EVENT_STRING,
	XXSON_EVENT_BYTES,
	XXSON_EVENT_TIME,
	XXSON_EVENT_CUSTOM,
	XXSON_EVENT_ARRAY_BEGIN,
	XXSON_EVENT_ARRAY_END,
	XXSON_EVENT_INT_MAP_BEGIN,
	XXSON_EVENT_INT_MAP_END,
	XXSON_EVENT_SET_BEGIN,
	XXSON_EVENT_SET_END,
	XXSON_EVENT_OBJECT_BEGIN,
	XXSON_EVENT_OBJECT_END,
	XXSON_EVENT_UINT
} xxsoneventtype;
```

| 值 | 语义 |
|---|---|
| `XXSON_EVENT_NULL` | 空值 |
| `XXSON_EVENT_BOOL` | 布尔 |
| `XXSON_EVENT_INT` | 有符号整数 |
| `XXSON_EVENT_FLOAT` | 浮点 |
| `XXSON_EVENT_STRING` | 字符串 |
| `XXSON_EVENT_BYTES` | BYTES |
| `XXSON_EVENT_TIME` | 时间 |
| `XXSON_EVENT_CUSTOM` | CUSTOM |
| `XXSON_EVENT_ARRAY_BEGIN` | 数组形态BEGIN |
| `XXSON_EVENT_ARRAY_END` | 数组形态END |
| `XXSON_EVENT_INT_MAP_BEGIN` | 有符号整数映射形态BEGIN |
| `XXSON_EVENT_INT_MAP_END` | 有符号整数映射形态END |
| `XXSON_EVENT_SET_BEGIN` | 集合形态BEGIN |
| `XXSON_EVENT_SET_END` | 集合形态END |
| `XXSON_EVENT_OBJECT_BEGIN` | 对象形态BEGIN |
| `XXSON_EVENT_OBJECT_END` | 对象形态END |
| `XXSON_EVENT_UINT` | 无符号整数事件 |

### `xxsonvisitaction`

回调可继续、正常提前停止或报告失败。

```c
typedef enum xxsonvisitaction {
	XXSON_VISIT_NEXT = 0,
	XXSON_VISIT_STOP,
	XXSON_VISIT_FAIL
} xxsonvisitaction;
```

| 值 | 语义 |
|---|---|
| `XXSON_VISIT_NEXT` | NEXT |
| `XXSON_VISIT_STOP` | STOP |
| `XXSON_VISIT_FAIL` | 回调失败 |

### `xxsonvisitresult`

访问结果明确区分完成、调用方停止和失败。

```c
typedef enum xxsonvisitresult {
	XXSON_VISIT_ERROR = -1,
	XXSON_VISIT_DONE = 0,
	XXSON_VISIT_STOPPED = 1
} xxsonvisitresult;
```

| 值 | 语义 |
|---|---|
| `XXSON_VISIT_ERROR` | 失败 |
| `XXSON_VISIT_DONE` | 完成 |
| `XXSON_VISIT_STOPPED` | 回调请求停止 |

### `xxsontag`

自定义标签保留名称和已经完成 JSON 反转义的字符串载荷。

```c
typedef struct xxsontag {
	xstrview Name;
	xstrview Payload;
} xxsontag;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | 名称 |
| `Payload` | `xstrview` | 载荷 |

### `xxsonwriteflag`

输出标志只改变文本布局和字符串转义。

```c
typedef enum xxsonwriteflag {
	XXSON_WRITE_PRETTY = UINT32_C(0x00000001),
	XXSON_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XXSON_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XXSON_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008)
} xxsonwriteflag;
```

| 值 | 语义 |
|---|---|
| `XXSON_WRITE_PRETTY` | 写方向 |
| `XXSON_WRITE_ESCAPE_SLASH` | 写方向 |
| `XXSON_WRITE_ESCAPE_HTML` | 写方向 |
| `XXSON_WRITE_ESCAPE_NON_ASCII` | 转义非 ASCII |

### `xxsonunsupported`

不可直接表示的值默认失败，也可显式跳过容器成员。

```c
typedef enum xxsonunsupported {
	XXSON_UNSUPPORTED_REJECT = 0,
	XXSON_UNSUPPORTED_SKIP
} xxsonunsupported;
```

| 值 | 语义 |
|---|---|
| `XXSON_UNSUPPORTED_REJECT` | XXSON不支持REJECT |
| `XXSON_UNSUPPORTED_SKIP` | 跳过 |

### `xxsoncoderesult`

自定义编码回调明确区分不处理、成功和失败。

```c
typedef enum xxsoncoderesult {
	XXSON_CODE_ERROR = -1,
	XXSON_CODE_UNSUPPORTED = 0,
	XXSON_CODE_OK = 1
} xxsoncoderesult;
```

| 值 | 语义 |
|---|---|
| `XXSON_CODE_ERROR` | 失败 |
| `XXSON_CODE_UNSUPPORTED` | 不支持 |
| `XXSON_CODE_OK` | 成功 |

### `xxsonwriteconfig`

XSON 写出配置提供固定上限和唯一自定义类型入口。

```c
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
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Unsupported` | `xxsonunsupported` | Unsupported |
| `MaxDepth` | `uint32` | MaxDepth |
| `Indent` | `uint32` | Indent |
| `MaxOutputBytes` | `size_t` | MaxOutputBytes |
| `Encode` | `xxsonencodeproc` | Encode |
| `EncodeData` | `ptr` | EncodeData |

### `xxsonwriter`

增量写入器保持不透明，所有方法都拒绝回调重入。

```c
typedef struct xxsonwriter xxsonwriter;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xxsondecodeproc`

自定义标签解码器返回一个拥有引用；失败时应设置具体错误。

```c
typedef xvalue* (*xxsondecodeproc)(
	xstrview Tag,
	xstrview Payload,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xxsonvisitproc`

XSON 访问器不得保存事件中的借用视图。

```c
typedef xxsonvisitaction (*xxsonvisitproc)(
	const xxsonevent* pEvent,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xxsonencodeproc`

编码器接收仅在回调期间有效的只读快照；返回视图保持到本次调用返回。

```c
typedef xxsoncoderesult (*xxsonencodeproc)(
	const xvalue* pValue,
	xstrview* pTag,
	xstrview* pPayload,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xxsonwriteproc`

输出回调必须在返回前消费借用字节。

```c
typedef bool (*xxsonwriteproc)(xbytesview Data, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XXSON_DEPTH_DEFAULT` | `256u` | DEPTH默认值 |
| `XXSON_INPUT_DEFAULT` | `(64u * 1024u * 1024u)` | 输入默认值 |
| `XXSON_STRING_DEFAULT` | `(16u * 1024u * 1024u)` | 字符串默认值 |
| `XXSON_VALUES_DEFAULT` | `1000000u` | VALUES默认值 |
| `XXSON_CONTAINER_DEFAULT` | `1000000u` | CONTAINER默认值 |
| `XXSON_DECODED_DEFAULT` | `(64u * 1024u * 1024u)` | DECODED默认值 |

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

### `xrtXsonErrorLocation`

从 `xrt.xson` 错误的机器数据中读取文本位置。

```c
bool xrtXsonErrorLocation(const xerror* pError, xxsonlocation* pLocation)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.xson` 域 | 解析/写出错误 |
| `pLocation` | 输出 | 非空 | 接收文本位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出位置 | — |
| `false` | 错误不含位置数据 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 无位置数据返回 `false` 且不设置错误

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 错误定位

```c
			!xrtXsonErrorLocation(pError, &Location) ||
```

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

### `xrtXsonReadConfigInit`

初始化严格语法、拒绝重复键和有限资源预算。

```c
void xrtXsonReadConfigInit(xxsonreadconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[xson](../../examples/data/xson/main.c) · 读取配置

```c
	xrtXsonReadConfigInit(&ReadConfig);
```

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

### `xrtXsonParse`

使用默认严格配置解析一个完整 XSON 文本。

```c
xvalue* xrtXsonParse(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | XSON 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Value DOM 根，用后 `xrtValueFree` 释放 | — |
| `NULL` | 解析失败 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 语法或结构非法，位置可由 `xrtXsonErrorLocation` 读取
- `xrt.xson` 域错误 — 超出嵌套深度或资源预算
- `XERR_MEMORY` — DOM 或缓冲分配失败

#### 范例

[xson](../../examples/data/xson/main.c) · 默认解析

```c
	pRoot = xrtXsonParse((xstrview){ sInput, sizeof(sInput) - 1u });
```

### `xrtXsonRead`

使用高级配置解析一个完整 XSON 文本。

```c
xvalue* xrtXsonRead(xstrview Text, const xxsonreadconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | XSON 文本 |
| `pConfig` | 输入 | 允许空 | 读取配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Value DOM 根，用后 `xrtValueFree` 释放 | — |
| `NULL` | 解析失败 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 语法或结构非法，位置可由 `xrtXsonErrorLocation` 读取
- `xrt.xson` 域错误 — 超出嵌套深度或资源预算
- `XERR_MEMORY` — DOM 或缓冲分配失败

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 高级配置解析

```c
		((pDom = xrtXsonRead(XRT_STR_LITERAL(
			"{\"n\":7}"), &ReadConfig)) == NULL) ) {
```

### `xrtXsonValid`

验证默认 XSON 语法和内建标签，不构造 Value DOM。

```c
bool xrtXsonValid(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | XSON 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 语法合法 | — |
| `false` | 语法非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 语法或结构非法，位置可由 `xrtXsonErrorLocation` 读取
- `xrt.xson` 域错误 — 超出嵌套深度或资源预算

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 语法校验

```c
	if ( xrtXsonValid((xstrview) { sBad, 6u }) ||
		!xrtXsonValid(XRT_STR_LITERAL("[1,2,3]")) ||
		((pDom = xrtXsonRead(XRT_STR_LITERAL(
			"{\"n\":7}"), &ReadConfig)) == NULL) ) {
```

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

### `xrtXsonVisit`

直接访问解析事件，不构造中间 DOM。

```c
xxsonvisitresult xrtXsonVisit(xstrview Text, const xxsonreadconfig* pConfig, xxsonvisitproc pVisitor, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | XSON 文本 |
| `pConfig` | 输入 | 允许空 | 读取配置，空 = 默认 |
| `pVisitor` | 输入 | 非空 | 事件回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XXSON_VISIT_NEXT` | 已完整遍历 | — |
| `XXSON_VISIT_STOP` | 回调请求停止 | 不设错误 |
| `XXSON_VISIT_ERROR` | 失败 | `xrt.xson` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.xson` 域错误 — 语法或结构非法，位置可由 `xrtXsonErrorLocation` 读取
- `xrt.xson` 域错误 — 超出嵌套深度或资源预算

#### 范例

[xson](../../examples/data/xson/main.c) · 事件访问

```c
		xrtXsonVisit(
			(xstrview){ sInput, sizeof(sInput) - 1u },
			&ReadConfig,
			printXsonEvent,
			NULL
		) != XXSON_VISIT_DONE
```

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

### `xrtXsonWriteConfigInit`

初始化紧凑输出、严格类型和有限输出预算。

```c
void xrtXsonWriteConfigInit(xxsonwriteconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[xson](../../examples/data/xson/main.c) · 写出配置

```c
	xrtXsonWriteConfigInit(&WriteConfig);
```

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

### `xrtXsonStringify`

紧凑或美化地序列化 Value，并返回由 `xrtFree` 释放的文本。

```c
str xrtXsonStringify(const xvalue* pValue, bool bPretty, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 源 Value |
| `bPretty` | 输入 | — | 是否美化 |
| `pSize` | 输出 | 允许空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾文本，`xrtFree` 释放 | — |
| `NULL` | 失败 | `xrt.xson` 域错误等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_MEMORY` — 输出缓冲分配失败

#### 范例

[xson](../../examples/data/xson/main.c) · 序列化

```c
	sText = xrtXsonStringify(pRoot, true, &iSize);
```

### `xrtXsonWrite`

使用高级配置把 Value 同步写入调用方输出回调。

```c
bool xrtXsonWrite(const xvalue* pValue, const xxsonwriteconfig* pConfig, xxsonwriteproc pWrite, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 源 Value |
| `pConfig` | 输入 | 允许空 | 写出配置，空 = 默认 |
| `pWrite` | 输入 | 非空 | 输出回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完整写出 | — |
| `false` | 失败 | `xrt.xson` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 内部缓冲分配失败

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 回调写出

```c
	if ( !xrtXsonWrite(pDom, &WriteConfig, exampleSink,
			(ptr)&Sink) ||
		(Sink.iBytes != 7u) ) { /* {"n":7} 恰七字节。 */
```

### `xrtXsonStringifyFile`

紧凑或美化地序列化并原子替换 XSON 文件。

```c
bool xrtXsonStringifyFile(cstr sPath, const xvalue* pValue, bool bPretty)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 目标文件路径 |
| `pValue` | 输入 | 非空 | 源 Value |
| `bPretty` | 输入 | — | 是否美化 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已原子写入 | — |
| `false` | 失败 | `xrt.xson` / `xrt.file` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.file` 域错误 — 临时文件创建或替换失败
- `xrt.xson` 域错误 — 序列化失败

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 序列化到文件

```c
		!xrtXsonStringifyFile(sFile, pDom, true) ||
```

### `xrtXsonWriteFile`

使用高级配置序列化并原子替换 XSON 文件。

```c
bool xrtXsonWriteFile(cstr sPath, const xvalue* pValue, const xxsonwriteconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 目标文件路径 |
| `pValue` | 输入 | 非空 | 源 Value |
| `pConfig` | 输入 | 允许空 | 写出配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已原子写入 | — |
| `false` | 失败 | `xrt.xson` / `xrt.file` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.file` 域错误 — 临时文件创建或替换失败
- `xrt.xson` 域错误 — 序列化失败

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 高级配置写文件

```c
	if ( !xrtXsonWriteFile(sFile, pDom, &WriteConfig) ||
		!xrtXsonStringifyFile(sFile, pDom, true) ||
		(xrtXsonParseFile(sFile) == NULL) ) {
```

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

### `xrtXsonWriterCreate`

创建把增量结果保存在内存中的 XSON 写入器。

```c
xxsonwriter* xrtXsonWriterCreate(const xxsonwriteconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 写出配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 写入器 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — DOM 或缓冲分配失败

#### 范例

[xson](../../examples/data/xson/main.c) · 内存写入器

```c
	pWriter = xrtXsonWriterCreate(&WriteConfig);
```

### `xrtXsonWriterCreateSink`

创建把增量结果同步提交给回调的 XSON 写入器。

```c
xxsonwriter* xrtXsonWriterCreateSink(const xxsonwriteconfig* pConfig, xxsonwriteproc pWrite, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 写出配置，空 = 默认 |
| `pWrite` | 输入 | 非空 | 输出回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 写入器 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — DOM 或缓冲分配失败

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 回调写入器

```c
	pWriter = xrtXsonWriterCreateSink(&WriteConfig, exampleSink,
		(ptr)&Sink);
```

### `xrtXsonWriterFree`

销毁写入器和未移交的内存结果。

```c
void xrtXsonWriterFree(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[xson](../../examples/data/xson/main.c) · 销毁写入器

```c
		xrtXsonWriterFree(pWriter);
```

### `xrtXsonWriterObject`

在当前位置开始对象。

```c
bool xrtXsonWriterObject(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已开始 | — |
| `false` | 位置或预算非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson](../../examples/data/xson/main.c) · 开始对象

```c
		!xrtXsonWriterObject(pWriter) ||
```

### `xrtXsonWriterArray`

在当前位置开始数组。

```c
bool xrtXsonWriterArray(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已开始 | — |
| `false` | 位置或预算非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 开始数组

```c
		!xrtXsonWriterArray(pWriter) ||
```

### `xrtXsonWriterIntMap`

在当前位置开始整数键映射。

```c
bool xrtXsonWriterIntMap(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已开始 | — |
| `false` | 位置或预算非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 开始整数映射

```c
		!xrtXsonWriterIntMap(pWriter) ||
```

### `xrtXsonWriterSet`

在当前位置开始集合。

```c
bool xrtXsonWriterSet(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已开始 | — |
| `false` | 位置或预算非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson](../../examples/data/xson/main.c) · 开始集合

```c
		!xrtXsonWriterSet(pWriter) ||                     /* 开 set */
```

### `xrtXsonWriterEnd`

结束最近开始的容器。

```c
bool xrtXsonWriterEnd(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已结束 | — |
| `false` | 没有未结束容器 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 没有可结束的容器

#### 范例

[xson](../../examples/data/xson/main.c) · 结束容器

```c
		!xrtXsonWriterEnd(pWriter) ||                     /* 闭 set */
```

### `xrtXsonWriterName`

为对象中的下一个值写入字符串名称。

```c
bool xrtXsonWriterName(xxsonwriter* pWriter, xstrview Name)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `Name` | 输入 | 借用、严格 UTF-8 | 成员名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置或 UTF-8 非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson](../../examples/data/xson/main.c) · 写字符串键

```c
		!xrtXsonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
```

### `xrtXsonWriterKey`

为整数映射中的下一个值写入 int64 键。

```c
bool xrtXsonWriterKey(xxsonwriter* pWriter, int64 iKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `iKey` | 输入 | — | 键值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写整数键

```c
		!xrtXsonWriterKey(pWriter, 7) ||
```

### `xrtXsonWriterNull`

写入 null。

```c
bool xrtXsonWriterNull(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写 null

```c
		!xrtXsonWriterNull(pWriter) ||
```

### `xrtXsonWriterBool`

写入布尔值。

```c
bool xrtXsonWriterBool(xxsonwriter* pWriter, bool bValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `bValue` | 输入 | — | 布尔值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写布尔

```c
		!xrtXsonWriterBool(pWriter, true) ||
```

### `xrtXsonWriterInt`

写入 int64。

```c
bool xrtXsonWriterInt(xxsonwriter* pWriter, int64 iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `iValue` | 输入 | — | 整数值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson](../../examples/data/xson/main.c) · 写 int64

```c
		!xrtXsonWriterInt(pWriter, 200) ||
```

### `xrtXsonWriterUInt`

写入 uint64。

```c
bool xrtXsonWriterUInt(xxsonwriter* pWriter, uint64 iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `iValue` | 输入 | — | 无符号值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写 uint64

```c
		!xrtXsonWriterUInt(pWriter, 12u) ||
```

### `xrtXsonWriterFloat`

写入 double，非有限值使用显式 float 标签。

```c
bool xrtXsonWriterFloat(xxsonwriter* pWriter, double fValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `fValue` | 输入 | — | 浮点值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写 double

```c
		!xrtXsonWriterFloat(pWriter, 0.5) ||
```

### `xrtXsonWriterString`

写入严格 UTF-8 字符串。

```c
bool xrtXsonWriterString(xxsonwriter* pWriter, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `Text` | 输入 | 借用、严格 UTF-8 | 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | UTF-8 或位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson](../../examples/data/xson/main.c) · 写字符串

```c
		!xrtXsonWriterString(pWriter, XRT_STR_LITERAL("xrt")) ||
```

### `xrtXsonWriterBytes`

写入规范 Base64 二进制标签。

```c
bool xrtXsonWriterBytes(xxsonwriter* pWriter, xbytesview Data)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `Data` | 输入 | 借用 | 二进制数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写二进制

```c
		!xrtXsonWriterBytes(pWriter,
			(xbytesview) { (const uint8*)"xy", 2u }) ||
```

### `xrtXsonWriterTime`

写入 UTC RFC 3339 时间标签。

```c
bool xrtXsonWriterTime(xxsonwriter* pWriter, xtime Time)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `Time` | 输入 | — | UTC 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置非法 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 容器顺序非法、根值重复或输出预算耗尽
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写时间

```c
		!xrtXsonWriterTime(pWriter, (xtime)1000000) ||
```

### `xrtXsonWriterTag`

写入已经验证名称和载荷的自定义标签。

```c
bool xrtXsonWriterTag(xxsonwriter* pWriter, xstrview Tag, xstrview Payload)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `Tag` | 输入 | 借用、已验证 | 标签名 |
| `Payload` | 输入 | 借用 | 载荷文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 标签名或载荷未通过验证 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 标签名非法或载荷不是合法标签载荷

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写自定义标签

```c
		!xrtXsonWriterTag(pWriter, XRT_STR_LITERAL("base64"),
			XRT_STR_LITERAL("eHl6")) ||
```

### `xrtXsonWriterValue`

在当前位置写入完整 Value 子树。

```c
bool xrtXsonWriterValue(xxsonwriter* pWriter, const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |
| `pValue` | 输入 | 非空 | 源 Value |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 序列化失败 | `xrt.xson` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.xson` 域错误 — Value 类型不受支持或输出预算耗尽

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 写 Value 子树

```c
		!xrtXsonWriterValue(pWriter, pSub) ||
```

### `xrtXsonWriterFinish`

验证根值和容器已完整结束，并关闭写入器。

```c
bool xrtXsonWriterFinish(xxsonwriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 目标写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完成并关闭 | — |
| `false` | 根值缺失或容器未结束 | `xrt.xson` 域错误 |

#### 错误

- `xrt.xson` 域错误 — 根值缺失、容器未全部结束或根值重复

#### 范例

[xson](../../examples/data/xson/main.c) · 完成写入

```c
		!xrtXsonWriterFinish(pWriter)
```

### `xrtXsonWriterTake`

从已完成的内存写入器移交文本。

```c
str xrtXsonWriterTake(xxsonwriter* pWriter, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空、已完成 | 目标写入器 |
| `pSize` | 输出 | 允许空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾文本，`xrtFree` 释放 | — |
| `NULL` | 未完成或非内存写入器 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 写入器尚未成功 Finish 或为 sink 写入器

#### 范例

[xson](../../examples/data/xson/main.c) · 移交文本

```c
	sText = xrtXsonWriterTake(pWriter, &iSize);
```

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

### `xrtXsonParseFile`

使用默认严格配置读取并解析 XSON 文件。

```c
xvalue* xrtXsonParseFile(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | UTF-8 文件路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Value DOM 根，用后 `xrtValueFree` 释放 | — |
| `NULL` | 解析失败 | `xrt.xson` 域错误 |
（读取失败同错误节）

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.file` 域错误 — 文件读取失败
- `xrt.xson` 域错误 — 语法或结构非法，位置可由 `xrtXsonErrorLocation` 读取
- `xrt.xson` 域错误 — 超出嵌套深度或资源预算
- `XERR_MEMORY` — DOM 或缓冲分配失败

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 默认解析文件

```c
	pFileDom = xrtXsonParseFile(sFile);
```

### `xrtXsonReadFile`

使用读取配置及其输入上限解析 XSON 文件。

```c
xvalue* xrtXsonReadFile(cstr sPath, const xxsonreadconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | UTF-8 文件路径 |
| `pConfig` | 输入 | 允许空 | 读取配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Value DOM 根，用后 `xrtValueFree` 释放 | — |
| `NULL` | 解析失败 | `xrt.xson` 域错误 |
（读取失败同错误节）

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.file` 域错误 — 文件读取失败或超过输入上限
- `xrt.xson` 域错误 — 语法或结构非法，位置可由 `xrtXsonErrorLocation` 读取
- `xrt.xson` 域错误 — 超出嵌套深度或资源预算
- `XERR_MEMORY` — DOM 或缓冲分配失败

#### 范例

[xson_tour](../../examples/data/xson_tour/main.c) · 高级配置解析文件

```c
		(xrtXsonReadFile(sFile, &ReadConfig) == NULL) ) {
```
