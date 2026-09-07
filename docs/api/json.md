# JSON

JSON 提供严格 RFC 8259 解析、事件流访问与序列化；不构造中间 DOM 时可用 Visit 直达事件。

## 类型与常量

### `xjsonerror`

JSON 模块错误码在 xrt.json 域内保持稳定。

```c
typedef enum xjsonerror {
	XJSON_ERROR_CONFIG = 1301,
	XJSON_ERROR_SYNTAX,
	XJSON_ERROR_LIMIT,
	XJSON_ERROR_DUPLICATE,
	XJSON_ERROR_NUMBER,
	XJSON_ERROR_STATE,
	XJSON_ERROR_UNSUPPORTED,
	XJSON_ERROR_OUTPUT,
	XJSON_ERROR_IO
} xjsonerror;
```

| 值 | 语义 |
|---|---|
| `XJSON_ERROR_CONFIG` | 配置非法 |
| `XJSON_ERROR_SYNTAX` | 语法非法 |
| `XJSON_ERROR_LIMIT` | 超限 |
| `XJSON_ERROR_DUPLICATE` | 失败 |
| `XJSON_ERROR_NUMBER` | 失败 |
| `XJSON_ERROR_STATE` | 状态非法 |
| `XJSON_ERROR_UNSUPPORTED` | 不支持 |
| `XJSON_ERROR_OUTPUT` | 输出失败 |
| `XJSON_ERROR_IO` | 写出失败 |

### `xjsonlocation`

文本位置使用零基字节偏移和一基行列；列按 UTF-8 字节计算。

```c
typedef struct xjsonlocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xjsonlocation;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 偏移量 |
| `Line` | `size_t` | 行号 |
| `Column` | `size_t` | 列号 |

### `xjsonreadflag`

非标准读取能力默认全部关闭，只能由调用方逐项开启。

```c
typedef enum xjsonreadflag {
	XJSON_READ_COMMENTS = UINT32_C(0x00000001),
	XJSON_READ_TRAILING_COMMA = UINT32_C(0x00000002)
} xjsonreadflag;
```

| 值 | 语义 |
|---|---|
| `XJSON_READ_COMMENTS` | XJSON读方向COMMENTS |
| `XJSON_READ_TRAILING_COMMA` | 允许尾随逗号 |

### `xjsonduplicate`

对象重复键必须由 DOM 调用方明确选择处理口径。

```c
typedef enum xjsonduplicate {
	XJSON_DUPLICATE_REJECT = 0,
	XJSON_DUPLICATE_KEEP,
	XJSON_DUPLICATE_REPLACE
} xjsonduplicate;
```

| 值 | 语义 |
|---|---|
| `XJSON_DUPLICATE_REJECT` | REJECT |
| `XJSON_DUPLICATE_KEEP` | KEEP |
| `XJSON_DUPLICATE_REPLACE` | 重名成员后者覆盖 |

### `xjsonbigint`

超出 int64/uint64 的整数字面量默认失败，显式浮点策略允许有损接收。

```c
typedef enum xjsonbigint {
	XJSON_BIGINT_REJECT = 0,
	XJSON_BIGINT_FLOAT
} xjsonbigint;
```

| 值 | 语义 |
|---|---|
| `XJSON_BIGINT_REJECT` | XJSONBIGINTREJECT |
| `XJSON_BIGINT_FLOAT` | 浮点形态（超精度可选） |

### `xjsonreadconfig`

JSON 读取配置同时约束资源消耗和少量显式兼容语法。

```c
typedef struct xjsonreadconfig {
	uint32 Flags;
	xjsonduplicate Duplicate;
	xjsonbigint BigInteger;
	uint32 MaxDepth;
	size_t MaxInputBytes;
	size_t MaxStringBytes;
	size_t MaxValues;
	size_t MaxContainerItems;
	uint32 Reserved[4];
} xjsonreadconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Duplicate` | `xjsonduplicate` | Duplicate |
| `BigInteger` | `xjsonbigint` | BigInteger |
| `MaxDepth` | `uint32` | MaxDepth |
| `MaxInputBytes` | `size_t` | MaxInputBytes |
| `MaxStringBytes` | `size_t` | MaxStringBytes |
| `MaxValues` | `size_t` | MaxValues |
| `MaxContainerItems` | `size_t` | MaxContainerItems |

### `xjsoneventtype`

访问事件在回调返回后失效；字符串与名称已经完成反转义。

```c
typedef enum xjsoneventtype {
	XJSON_EVENT_NULL = 0,
	XJSON_EVENT_BOOL,
	XJSON_EVENT_INT,
	XJSON_EVENT_FLOAT,
	XJSON_EVENT_STRING,
	XJSON_EVENT_ARRAY_BEGIN,
	XJSON_EVENT_ARRAY_END,
	XJSON_EVENT_OBJECT_BEGIN,
	XJSON_EVENT_OBJECT_END,
	XJSON_EVENT_UINT
} xjsoneventtype;
```

| 值 | 语义 |
|---|---|
| `XJSON_EVENT_NULL` | 空值 |
| `XJSON_EVENT_BOOL` | 布尔 |
| `XJSON_EVENT_INT` | 有符号整数 |
| `XJSON_EVENT_FLOAT` | 浮点 |
| `XJSON_EVENT_STRING` | 字符串 |
| `XJSON_EVENT_ARRAY_BEGIN` | 数组形态BEGIN |
| `XJSON_EVENT_ARRAY_END` | 数组形态END |
| `XJSON_EVENT_OBJECT_BEGIN` | 对象形态BEGIN |
| `XJSON_EVENT_OBJECT_END` | 对象形态END |
| `XJSON_EVENT_UINT` | 无符号整数事件 |

### `xjsonvisitaction`

回调可继续、正常提前停止或报告失败。

```c
typedef enum xjsonvisitaction {
	XJSON_VISIT_NEXT = 0,
	XJSON_VISIT_STOP,
	XJSON_VISIT_FAIL
} xjsonvisitaction;
```

| 值 | 语义 |
|---|---|
| `XJSON_VISIT_NEXT` | NEXT |
| `XJSON_VISIT_STOP` | STOP |
| `XJSON_VISIT_FAIL` | 回调失败 |

### `xjsonvisitresult`

访问结果明确区分完整完成、调用方停止和解析失败。

```c
typedef enum xjsonvisitresult {
	XJSON_VISIT_ERROR = -1,
	XJSON_VISIT_DONE = 0,
	XJSON_VISIT_STOPPED = 1
} xjsonvisitresult;
```

| 值 | 语义 |
|---|---|
| `XJSON_VISIT_ERROR` | 失败 |
| `XJSON_VISIT_DONE` | 完成 |
| `XJSON_VISIT_STOPPED` | 回调请求停止 |

### `xjsonwriteflag`

输出标志只改变文本表示，不改变 Value 数据。

```c
typedef enum xjsonwriteflag {
	XJSON_WRITE_PRETTY = UINT32_C(0x00000001),
	XJSON_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XJSON_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XJSON_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008),
	XJSON_WRITE_CONTAINER_COMPAT = UINT32_C(0x00000010)
} xjsonwriteflag;
```

| 值 | 语义 |
|---|---|
| `XJSON_WRITE_PRETTY` | 写方向 |
| `XJSON_WRITE_ESCAPE_SLASH` | 写方向 |
| `XJSON_WRITE_ESCAPE_HTML` | 写方向 |
| `XJSON_WRITE_ESCAPE_NON_ASCII` | 写方向 |
| `XJSON_WRITE_CONTAINER_COMPAT` | 容器兼容模式 |

### `xjsonnonfinite`

非有限浮点默认失败，也可显式写成 null 或字符串。

```c
typedef enum xjsonnonfinite {
	XJSON_NONFINITE_REJECT = 0,
	XJSON_NONFINITE_NULL,
	XJSON_NONFINITE_STRING
} xjsonnonfinite;
```

| 值 | 语义 |
|---|---|
| `XJSON_NONFINITE_REJECT` | REJECT |
| `XJSON_NONFINITE_NULL` | 空值 |
| `XJSON_NONFINITE_STRING` | 字符串表示 |

### `xjsonunsupported`

不受 JSON 表达的 Value 默认失败，也可显式写 null 或跳过成员。

```c
typedef enum xjsonunsupported {
	XJSON_UNSUPPORTED_REJECT = 0,
	XJSON_UNSUPPORTED_NULL,
	XJSON_UNSUPPORTED_SKIP
} xjsonunsupported;
```

| 值 | 语义 |
|---|---|
| `XJSON_UNSUPPORTED_REJECT` | 不支持 |
| `XJSON_UNSUPPORTED_NULL` | 空值 |
| `XJSON_UNSUPPORTED_SKIP` | 跳过 |

### `xjsonwriteconfig`

JSON 写出配置提供固定上限；Indent 只在美化输出时生效。

```c
typedef struct xjsonwriteconfig {
	uint32 Flags;
	xjsonnonfinite NonFinite;
	xjsonunsupported Unsupported;
	uint32 MaxDepth;
	uint32 Indent;
	size_t MaxOutputBytes;
	uint32 Reserved[4];
} xjsonwriteconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `NonFinite` | `xjsonnonfinite` | NonFinite |
| `Unsupported` | `xjsonunsupported` | Unsupported |
| `MaxDepth` | `uint32` | MaxDepth |
| `Indent` | `uint32` | Indent |
| `MaxOutputBytes` | `size_t` | MaxOutputBytes |

### `xjsonwriter`

增量写入器保持不透明，写入方法不可从输出回调重入。

```c
typedef struct xjsonwriter xjsonwriter;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xjsonvisitproc`

JSON 访问器不得保存事件中的借用视图，失败时应设置更具体的错误。

```c
typedef xjsonvisitaction (*xjsonvisitproc)(
	const xjsonevent* pEvent,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xjsonwriteproc`

输出回调必须在返回前消费借用字节，失败时应设置具体错误。

```c
typedef bool (*xjsonwriteproc)(xbytesview Data, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XJSON_DEPTH_DEFAULT` | `256u` | DEPTH默认值 |
| `XJSON_INPUT_DEFAULT` | `(64u * 1024u * 1024u)` | 输入默认值 |
| `XJSON_STRING_DEFAULT` | `(16u * 1024u * 1024u)` | 字符串默认值 |
| `XJSON_VALUES_DEFAULT` | `1000000u` | VALUES默认值 |
| `XJSON_CONTAINER_DEFAULT` | `1000000u` | CONTAINER默认值 |

## 字符串 token

`json_escape` 是不依赖 DOM、Buffer 和完整 Writer 的底层能力。`xrtJsonQuoteWrite` 严格校验 UTF-8，并把包含首尾双引号的 JSON 字符串 token 分段写入同步回调。可独立选择斜杠、HTML 字节和非 ASCII 转义；失败时返回 `xrt.json` 错误及 UTF-8 字节位置。

完整 JSON/XSON Writer 与 Logger JSONL 共用这一实现，不再分别维护字符串转义规则。

`json` 提供严格 JSON 校验、事件访问、`xvalue` DOM 转换、增量写出和文件读写。程序内部长期持有的数据继续使用 `xvalue`；JSON 是文件、HTTP 和其他外部边界上的标准交换格式。

## 裁剪与依赖

| 能力 | 公开选择宏 | 实现宏 | 主要依赖 |
|---|---|---|---|
| 公共错误与位置 | `XRT_MODULE_JSON_CORE` | `XRT_FEATURE_JSON_CORE` | `core` |
| 校验、事件访问、DOM 解析 | `XRT_MODULE_JSON_READ` | `XRT_FEATURE_JSON_READ` | `json_core`, `buffer`, `number`, `unicode`, `value_container` |
| DOM 与增量写出 | `XRT_MODULE_JSON_WRITE` | `XRT_FEATURE_JSON_WRITE` | `json_core`, `buffer`, `number`, `unicode`, `value_container` |
| JSON 文件读写 | `XRT_MODULE_JSON_FILE` | `XRT_FEATURE_JSON_FILE` | `json_read`, `json_write`, `file_whole` |
| 完整 JSON | `XRT_MODULE_JSON` | `XRT_FEATURE_JSON` | `json_file` |

只需要验证或访问事件时选择 `XRT_MODULE_JSON_READ`，只需要生成响应正文时选择 `XRT_MODULE_JSON_WRITE`。完整模块不会成为 HTTP 协议层的强制依赖；HTTP 调用方可以直接发送已有 JSON 字节，也可以按需使用这里的构建器。

## 稳定契约

- 默认只接受 RFC 8259 JSON。注释和尾随逗号必须逐项显式开启。
- 所有文本都使用显式长度，可包含末尾无零字节的输入；JSON 字符串值也可包含解码后的 `U+0000`。
- 文本必须是合法 UTF-8，`\uXXXX` 使用 UTF-16 代理对规则转换为 UTF-8。
- `xrtJsonParse("null")` 返回 `xrtValueNull()`；失败返回 C `NULL`，两者没有歧义。
- 整数字面量优先保存为 `int64`，非负且超过 `INT64_MAX` 时保存为 `uint64`。超出 `uint64` 或低于 `int64` 范围默认失败，也可显式按 `double` 有损接收。
- 读取和写出均有深度、字节与项目预算；默认预算有限，不信任输入不会无限消耗资源。
- `xrtJsonValid` 对合法输入不分配动态内存。`xrtJsonVisit` 不构造 DOM，但含转义字符串会使用可复用的临时缓冲。
- 事件中的名称、字符串和数字视图只借用到回调返回，调用方保存时必须复制。
- sink 回调必须在返回前消费字节。事件和 sink 回调新设置的错误会原样传播；未设置错误的失败会由 JSON 层补充稳定错误。
- 增量写入器不可重入。任一写入失败后进入失败终态，只能释放。
- 文件写出先完整序列化，再原子替换目标；序列化失败不会损坏已有文件。
- 模块不带隐式锁。不同解析器或写入器可并行使用，同一写入器需要由调用方串行访问。

## 默认预算

```c
#define XJSON_DEPTH_DEFAULT     256u
#define XJSON_INPUT_DEFAULT     (64u * 1024u * 1024u)
#define XJSON_STRING_DEFAULT    (16u * 1024u * 1024u)
#define XJSON_VALUES_DEFAULT    1000000u
#define XJSON_CONTAINER_DEFAULT 1000000u
```

这些值是默认上限，不是格式能力上限。服务端应按接口实际需要进一步收紧。

## 错误

所有模块错误使用 `xrt.json` 域：

```c
typedef enum xjsonerror {
	XJSON_ERROR_CONFIG = 1301,
	XJSON_ERROR_SYNTAX,
	XJSON_ERROR_LIMIT,
	XJSON_ERROR_DUPLICATE,
	XJSON_ERROR_NUMBER,
	XJSON_ERROR_STATE,
	XJSON_ERROR_UNSUPPORTED,
	XJSON_ERROR_OUTPUT,
	XJSON_ERROR_IO
} xjsonerror;
```

语法、限制、重复键和数值错误尽量携带零基字节偏移与一基行列：

```c
typedef struct xjsonlocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xjsonlocation;

bool xrtJsonErrorLocation(
	const xerror* pError,
	xjsonlocation* pLocation
);
```

`xrtJsonErrorLocation` 只读取位置数据，不修改错误。非 JSON 错误或不带位置的 JSON 错误返回 `false`。

## 读取配置

```c
typedef enum xjsonreadflag {
	XJSON_READ_COMMENTS = UINT32_C(0x00000001),
	XJSON_READ_TRAILING_COMMA = UINT32_C(0x00000002)
} xjsonreadflag;

typedef enum xjsonduplicate {
	XJSON_DUPLICATE_REJECT = 0,
	XJSON_DUPLICATE_KEEP,
	XJSON_DUPLICATE_REPLACE
} xjsonduplicate;

typedef enum xjsonbigint {
	XJSON_BIGINT_REJECT = 0,
	XJSON_BIGINT_FLOAT
} xjsonbigint;

typedef struct xjsonreadconfig {
	uint32 Flags;
	xjsonduplicate Duplicate;
	xjsonbigint BigInteger;
	uint32 MaxDepth;
	size_t MaxInputBytes;
	size_t MaxStringBytes;
	size_t MaxValues;
	size_t MaxContainerItems;
	uint32 Reserved[4];
} xjsonreadconfig;

void xrtJsonReadConfigInit(xjsonreadconfig* pConfig);
```

配置必须先由 `Init` 初始化。`Reserved` 必须保持零。重复键默认拒绝；`KEEP` 保留首值并完整校验后续值，`REPLACE` 保留末值。兼容标志只放宽指定语法，不开启单引号、十六进制、`NaN` 或其他非标准字面量。

## DOM 读取

```c
xvalue* xrtJsonParse(xstrview Text);

xvalue* xrtJsonRead(
	xstrview Text,
	const xjsonreadconfig* pConfig
);

bool xrtJsonValid(xstrview Text);
```

`Parse` 使用默认严格配置。`Read` 用于重复键策略、兼容语法和资源预算。返回的 `xvalue` 由调用方使用 `xrtValueRelease` 释放；单例 `null` 同样允许释放。

标准 JSON 类型映射为 `XVALUE_NULL`、`XVALUE_BOOL`、`XVALUE_INT`、`XVALUE_UINT`、`XVALUE_FLOAT`、`XVALUE_STRING`、`XVALUE_ARRAY` 和 `XVALUE_OBJECT`。JSON 不表达 bytes、time、set、int-map、handle 或自定义类型。

## 事件访问

```c
typedef enum xjsoneventtype {
	XJSON_EVENT_NULL = 0,
	XJSON_EVENT_BOOL,
	XJSON_EVENT_INT,
	XJSON_EVENT_FLOAT,
	XJSON_EVENT_STRING,
	XJSON_EVENT_ARRAY_BEGIN,
	XJSON_EVENT_ARRAY_END,
	XJSON_EVENT_OBJECT_BEGIN,
	XJSON_EVENT_OBJECT_END,
	XJSON_EVENT_UINT
} xjsoneventtype;

typedef enum xjsonvisitaction {
	XJSON_VISIT_NEXT = 0,
	XJSON_VISIT_STOP,
	XJSON_VISIT_FAIL
} xjsonvisitaction;

typedef enum xjsonvisitresult {
	XJSON_VISIT_ERROR = -1,
	XJSON_VISIT_DONE = 0,
	XJSON_VISIT_STOPPED = 1
} xjsonvisitresult;
```

事件结构提供 token 起点、父容器定位和解析值：

```c
typedef struct xjsonevent {
	xjsoneventtype Type;
	xjsonlocation Location;
	size_t Depth;
	bool HasName;
	xstrview Name;
	size_t Index;
	xstrview Raw;
	union {
		bool Boolean;
		int64 Integer;
		uint64 Unsigned;
		double Float;
		xstrview String;
	} Value;
} xjsonevent;
```

对象成员通过 `HasName` 和已解码 `Name` 定位，数组成员通过 `Index` 定位。`Raw` 只在有符号整数、无符号整数和浮点事件中保存原始数字 token。容器开始和结束分别产生事件，根深度为零。

```c
typedef xjsonvisitaction (*xjsonvisitproc)(
	const xjsonevent* pEvent,
	ptr pUserData
);

xjsonvisitresult xrtJsonVisit(
	xstrview Text,
	const xjsonreadconfig* pConfig,
	xjsonvisitproc pVisitor,
	ptr pUserData
);
```

`STOP` 是正常提前完成，返回 `XJSON_VISIT_STOPPED`，不会伪装为错误。`FAIL` 应先设置业务错误；未设置时 JSON 层建立 `XJSON_ERROR_STATE`。

## 写出配置

```c
typedef enum xjsonwriteflag {
	XJSON_WRITE_PRETTY = UINT32_C(0x00000001),
	XJSON_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XJSON_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XJSON_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008),
	XJSON_WRITE_CONTAINER_COMPAT = UINT32_C(0x00000010)
} xjsonwriteflag;

typedef enum xjsonnonfinite {
	XJSON_NONFINITE_REJECT = 0,
	XJSON_NONFINITE_NULL,
	XJSON_NONFINITE_STRING
} xjsonnonfinite;

typedef enum xjsonunsupported {
	XJSON_UNSUPPORTED_REJECT = 0,
	XJSON_UNSUPPORTED_NULL,
	XJSON_UNSUPPORTED_SKIP
} xjsonunsupported;
```

`ESCAPE_HTML` 转义 `<`、`>`、`&`，适合嵌入 HTML 的受控场景；它不能替代完整的 HTML 上下文转义。`ESCAPE_NON_ASCII` 使用 `\uXXXX`，补充平面字符输出代理对。`CONTAINER_COMPAT` 把 set 写成数组、int-map 写成字符串键对象，因此属于显式有损兼容。Set 按 Value 的首次插入顺序稳定写出，IntMap 按整数键升序稳定写出；这些顺序只保证编码结果可重复，不赋予集合顺序语义，也不改变 JSON 对象成员无序的协议语义。

非有限浮点默认失败，也可写成 `null` 或字符串。非 JSON `xvalue` 默认失败，也可写成 `null`；`SKIP` 只在遍历 DOM 对象成员或序列项时跳过值，根值和直接写入的单值不能跳过。

```c
typedef struct xjsonwriteconfig {
	uint32 Flags;
	xjsonnonfinite NonFinite;
	xjsonunsupported Unsupported;
	uint32 MaxDepth;
	uint32 Indent;
	size_t MaxOutputBytes;
	uint32 Reserved[4];
} xjsonwriteconfig;

void xrtJsonWriteConfigInit(xjsonwriteconfig* pConfig);
```

`Indent` 范围为 0 到 16，只在 `PRETTY` 时生效。配置必须由 `Init` 初始化，`Reserved` 必须保持零。

## DOM 写出

```c
str xrtJsonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
);

typedef bool (*xjsonwriteproc)(xbytesview Data, ptr pUserData);

bool xrtJsonWrite(
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
);
```

`Stringify` 是常用入口，返回零结尾文本并通过 `pSize` 返回不含末尾零的字节数，结果由 `xrtFree` 释放。失败不修改 `pSize`。

`Write` 直接把若干字节块同步提交给 sink，不保留完整结果，适合 HTTP 正文、文件抽象或哈希管线。此前已经提交的块无法撤回，因此 sink 模式只保证内部状态正确，不提供事务性输出。

## 增量写入器

```c
typedef struct xjsonwriter xjsonwriter;

xjsonwriter* xrtJsonWriterCreate(const xjsonwriteconfig* pConfig);
xjsonwriter* xrtJsonWriterCreateSink(
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
);
```

内存写入器保存结果；sink 写入器边生成边提交。两者共享以下状态化操作：

```c
bool xrtJsonWriterObject(xjsonwriter* pWriter);
bool xrtJsonWriterArray(xjsonwriter* pWriter);
bool xrtJsonWriterEnd(xjsonwriter* pWriter);
bool xrtJsonWriterName(xjsonwriter* pWriter, xstrview Name);
bool xrtJsonWriterNull(xjsonwriter* pWriter);
bool xrtJsonWriterBool(xjsonwriter* pWriter, bool bValue);
bool xrtJsonWriterInt(xjsonwriter* pWriter, int64 iValue);
bool xrtJsonWriterUInt(xjsonwriter* pWriter, uint64 iValue);
bool xrtJsonWriterFloat(xjsonwriter* pWriter, double fValue);
bool xrtJsonWriterString(xjsonwriter* pWriter, xstrview Text);
bool xrtJsonWriterValue(xjsonwriter* pWriter, const xvalue* pValue);
bool xrtJsonWriterFinish(xjsonwriter* pWriter);
```

对象中必须按 `Name -> Value` 交替写入；数组中直接写值。`End` 结束最近容器。一个写入器只接受一个根值，所有容器结束后调用 `Finish` 封闭结果。`Value` 可以在任意值位置写入现有 `xvalue` 子树，并检测循环引用。

```c
str xrtJsonWriterTake(xjsonwriter* pWriter, size_t* pSize);
void xrtJsonWriterFree(xjsonwriter* pWriter);
```

`Take` 只适用于已经 `Finish` 的内存写入器，只能成功一次，返回值由 `xrtFree` 释放。`Free` 释放写入器及尚未移交的结果。

## 文件

```c
xvalue* xrtJsonParseFile(cstr sPath);

xvalue* xrtJsonReadFile(
	cstr sPath,
	const xjsonreadconfig* pConfig
);

bool xrtJsonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig
);

bool xrtJsonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
);
```

`ReadFile` 在读取阶段执行 `MaxInputBytes`，再按同一配置解析。文件打开、读取、临时文件或替换失败会建立 `XJSON_ERROR_IO`，底层文件错误保留在原因链中。

## 示例

解析、读取字段并重新输出：

```c
xvalue* pRoot;
xvalue* pName;
xstrview Name;
str sJson;
size_t iSize;

pRoot = xrtJsonParse(XRT_STR_LITERAL(
	"{\"name\":\"xrt\",\"features\":[\"json\",\"http\"]}"
));
if ( pRoot == NULL ) {
	return false;
}
pName = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name"));
if ( !xrtValueGetString(pName, &Name) ) {
	xrtValueRelease(pRoot);
	return false;
}
printf("name = %.*s\n", (int)Name.Size, Name.Data);

sJson = xrtJsonStringify(pRoot, true, &iSize);
xrtValueRelease(pRoot);
if ( sJson == NULL ) {
	return false;
}
printf("%.*s\n", (int)iSize, sJson);
xrtFree(sJson);
```

不构造 DOM，直接生成固定形状的 HTTP JSON 正文：

```c
xjsonwriteconfig Config;
xjsonwriter* pWriter;
str sBody;
size_t iSize;

xrtJsonWriteConfigInit(&Config);
pWriter = xrtJsonWriterCreate(&Config);
if (
	(pWriter == NULL) ||
	!xrtJsonWriterObject(pWriter) ||
	!xrtJsonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
	!xrtJsonWriterInt(pWriter, 200) ||
	!xrtJsonWriterName(pWriter, XRT_STR_LITERAL("message")) ||
	!xrtJsonWriterString(pWriter, XRT_STR_LITERAL("OK")) ||
	!xrtJsonWriterEnd(pWriter) ||
	!xrtJsonWriterFinish(pWriter)
) {
	xrtJsonWriterFree(pWriter);
	return false;
}
sBody = xrtJsonWriterTake(pWriter, &iSize);
xrtJsonWriterFree(pWriter);
```

完整可运行示例位于 `examples/data/json/main.c`。

## JSON 与 XSON

JSON 保持标准、严格和可互操作。需要无损保存 bytes、time、set、int-map 等 XRT 扩展值时使用 XSON；不要通过非标准 JSON 字面量偷偷扩展 JSON 语义。两者共享底层文本、安全预算和错误设计，但保持独立裁剪入口与格式契约。

## API

### `xrtJsonReadConfigInit`

初始化严格 JSON、重复键拒绝和有限资源预算的读取配置。

```c
void xrtJsonReadConfigInit(xjsonreadconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[data/json · data/json_tour · 读取](../../examples/data/json/main.c) · 观察

```c
	xrtJsonReadConfigInit(&ReadConfig);
```


### `xrtJsonParse`

使用默认严格配置解析一个完整 JSON 文本。

```c
xvalue* xrtJsonParse(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用、严格 UTF-8 | 完整 JSON 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有的 Value DOM（`xvalue` 族操作/释放） | — |
| `NULL` | 语法错误、超限或 OOM | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）
- `XERR_MEMORY`

#### 范例

[data/json · data/json · 解析](../../examples/data/json/main.c) · 观察

```c
	pRoot = xrtJsonParse(XRT_STR_LITERAL(
		"{\"name\":\"xrt\",\"features\":[\"json\",\"http\"]}"
	));
```


### `xrtJsonRead`

使用高级配置解析一个完整 JSON 文本。

```c
xvalue* xrtJsonRead(
	xstrview Text,
	const xjsonreadconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 完整 JSON 文本 |
| `pConfig` | 输入 | 允许空 | 空 = 默认严格配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有的 Value DOM | — |
| `NULL` | 失败 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）
- `XERR_MEMORY`

#### 范例

[data/json_tour · data/json_tour · 读取](../../examples/data/json_tour/main.c) · 观察

```c
	pDom = xrtJsonRead(SV(sText), &ReadConfig);
```


### `xrtJsonValid`

使用默认严格配置验证一个完整 JSON 文本，不构造 Value DOM。

```c
bool xrtJsonValid(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待验证文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 JSON | — |
| `false` | 非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 验证](../../examples/data/json_tour/main.c) · 观察

```c
		!xrtJsonValid(SV(sText)) ||
```


### `xrtJsonVisit`

直接访问解析事件，不构造中间 DOM。

```c
xjsonvisitresult xrtJsonVisit(
	xstrview Text,
	const xjsonreadconfig* pConfig,
	xjsonvisitproc pVisitor,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 完整 JSON 文本 |
| `pConfig` | 输入 | 允许空 | 读取配置 |
| `pVisitor` | 输入 | 非空 | 事件回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XJSON_VISIT_OK/STOP/ERROR` | 完成/回调请求停止/错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）
- `XERR_CANCELLED` — 回调请求停止（STOP 映射）

#### 范例

[data/json · data/json_tour · 事件](../../examples/data/json/main.c) · 观察

```c
		xrtJsonVisit(
			XRT_STR_LITERAL("{\"code\":200,\"ok\":true}"),
			&ReadConfig,
			printJsonEvent,
			NULL
		) != XJSON_VISIT_DONE
```


### `xrtJsonErrorLocation`

从 `xrt.json` 错误的机器数据中读取文本位置。

```c
bool xrtJsonErrorLocation(
	const xerror* pError,
	xjsonlocation* pLocation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.json` 域 | 解析错误 |
| `pLocation` | 输出 | 非空 | 接收 Line/Column |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 位置已写出 | — |
| `false` | 错误不带位置数据或参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[data/json_tour · data/report · 错误定位](../../examples/data/json_tour/main.c) · 观察

```c
			!xrtJsonErrorLocation(pError, &Location) ||
```


### `xrtJsonWriterCreate`

创建把增量结果保存在内存中的 JSON 写入器。

```c
xjsonwriter* xrtJsonWriterCreate(
	const xjsonwriteconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 空 = `WriteConfigInit` 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 内存型写入器 | — |
| `NULL` | 参数错误或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_MEMORY`

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
	pWriter = xrtJsonWriterCreate(&WriteConfig);
```


### `xrtJsonWriterCreateSink`

创建把增量结果同步提交给回调的 JSON 写入器。

```c
xjsonwriter* xrtJsonWriterCreateSink(
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 输出配置 |
| `pWrite` | 输入 | 非空 | 输出回调（返回前消费字节） |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 回调型写入器 | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_MEMORY`

#### 范例

[data/json_tour · data/json_tour · 写入器](../../examples/data/json_tour/main.c) · 观察

```c
	pWriter = xrtJsonWriterCreateSink(&WriteConfig, exampleCollect,
		&Sink);
```


### `xrtJsonWriterObject`

在当前位置开始对象；对象中必须先写 Name，数组中直接写值。

```c
bool xrtJsonWriterObject(xjsonwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已进入对象 | — |
| `false` | 嵌套超限或状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
		!xrtJsonWriterObject(pWriter) ||                     /* 开对象 */
```


### `xrtJsonWriterArray`

在当前位置开始数组。

```c
bool xrtJsonWriterArray(xjsonwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已进入数组 | — |
| `false` | 嵌套超限或状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 写入器](../../examples/data/json_tour/main.c) · 观察

```c
		!xrtJsonWriterArray(pWriter) ||
```


### `xrtJsonWriterEnd`

结束最近开始的对象或数组。

```c
bool xrtJsonWriterEnd(xjsonwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 容器已闭合 | — |
| `false` | 无未闭合容器或状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
		!xrtJsonWriterEnd(pWriter) ||                       /* 闭对象 */
```


### `xrtJsonWriterName`

为对象中的下一个值写入名称。

```c
bool xrtJsonWriterName(xjsonwriter* pWriter, xstrview Name);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器（对象上下文） |
| `Name` | 输入 | 借用、严格 UTF-8 | 成员名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置/UTF-8 非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
		!xrtJsonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
```


### `xrtJsonWriterNull`

写入 null。

```c
bool xrtJsonWriterNull(xjsonwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 写入器](../../examples/data/json_tour/main.c) · 观察

```c
		!xrtJsonWriterNull(pWriter) ||
```


### `xrtJsonWriterBool`

写入布尔值。

```c
bool xrtJsonWriterBool(xjsonwriter* pWriter, bool bValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |
| `bValue` | 输入 | — | 布尔值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 写入器](../../examples/data/json_tour/main.c) · 观察

```c
		!xrtJsonWriterBool(pWriter, true) ||
```


### `xrtJsonWriterInt`

写入 int64。

```c
bool xrtJsonWriterInt(xjsonwriter* pWriter, int64 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |
| `iValue` | 输入 | — | 整数值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
		!xrtJsonWriterInt(pWriter, 200) ||
```


### `xrtJsonWriterUInt`

写入 uint64。

```c
bool xrtJsonWriterUInt(xjsonwriter* pWriter, uint64 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |
| `iValue` | 输入 | — | 无符号值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 写入器](../../examples/data/json_tour/main.c) · 观察

```c
		!xrtJsonWriterUInt(pWriter, 1u) ||
```


### `xrtJsonWriterFloat`

按配置写入 double。

```c
bool xrtJsonWriterFloat(xjsonwriter* pWriter, double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |
| `fValue` | 输入 | 有限值 | 浮点值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | NaN/Inf（严格模式）或状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 写入器](../../examples/data/json_tour/main.c) · 观察

```c
		!xrtJsonWriterFloat(pWriter, 2.5) ) {
```


### `xrtJsonWriterString`

写入严格 UTF-8 字符串。

```c
bool xrtJsonWriterString(xjsonwriter* pWriter, xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |
| `Text` | 输入 | 借用、严格 UTF-8 | 字符串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已转义写出 | — |
| `false` | UTF-8 非法或状态非法 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
		!xrtJsonWriterString(pWriter, XRT_STR_LITERAL("OK")) ||
```


### `xrtJsonWriterValue`

在当前位置写入完整 Value 子树。

```c
bool xrtJsonWriterValue(
	xjsonwriter* pWriter,
	const xvalue* pValue
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |
| `pValue` | 输入 | 非空 | Value 子树 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已递归写出 | — |
| `false` | Value 类型非法或超限 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 写入器](../../examples/data/json_tour/main.c) · 观察

```c
		!xrtJsonWriterValue(pWriter, pInner) ||
```


### `xrtJsonWriterFinish`

验证根值和容器已经完整结束，并封闭写入器。

```c
bool xrtJsonWriterFinish(xjsonwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | 写入器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已封闭（后续只能 Take/Free） | — |
| `false` | 根值缺失或容器未闭合 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
		!xrtJsonWriterFinish(pWriter)                       /* 完整性校验 */
```


### `xrtJsonWriterTake`

从已完成的内存写入器移交文本；结果由 `xrtFree` 释放。

```c
str xrtJsonWriterTake(xjsonwriter* pWriter, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空、已 Finish | 内存型写入器 |
| `pSize` | 输出 | 可空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 零结尾 JSON 文本 | — |
| `NULL` | 未 Finish 或非内存型 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_STATE` — 未 Finish 或回调型写入器

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
	sText = xrtJsonWriterTake(pWriter, &iSize);
```


### `xrtJsonWriterFree`

释放写入器；空指针是空操作。未 Take 的内存结果一并释放。

```c
void xrtJsonWriterFree(xjsonwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 写入器与内部缓冲已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[data/json · data/json_tour · 写入器](../../examples/data/json/main.c) · 观察

```c
		xrtJsonWriterFree(pWriter);
```


### `xrtJsonWriteConfigInit`

初始化紧凑输出、严格类型和有限输出预算的写入配置。

```c
void xrtJsonWriteConfigInit(xjsonwriteconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[data/json · data/json_tour · 序列化](../../examples/data/json/main.c) · 观察

```c
	xrtJsonWriteConfigInit(&WriteConfig);
```


### `xrtJsonStringify`

紧凑或美化地序列化 Value，并返回由 `xrtFree` 释放的字符串。

```c
str xrtJsonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | Value 树 |
| `bPretty` | 输入 | — | 缩进美化 |
| `pSize` | 输出 | 可空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 零结尾 JSON 文本 | — |
| `NULL` | 类型非法或 OOM | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）
- `XERR_MEMORY`

#### 范例

[data/json · data/json · 序列化](../../examples/data/json/main.c) · 观察

```c
	sText = xrtJsonStringify(pRoot, true, &iSize);
```


### `xrtJsonWrite`

使用高级配置把 Value 同步写入调用方输出回调。

```c
bool xrtJsonWrite(
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | Value 树 |
| `pConfig` | 输入 | 允许空 | 写入配置 |
| `pWrite` | 输入 | 非空 | 输出回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已完整写出 | — |
| `false` | 类型非法、超限或回调中止 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 序列化](../../examples/data/json_tour/main.c) · 观察

```c
	if ( !xrtJsonWrite(pDom, &WriteConfig, exampleCollect, &Sink) ||
		(Sink.Size != sizeof(sText) - 1u) ||
		(memcmp(Sink.Buffer, sText,
			sizeof(sText) - 1u) != 0) ) {
```


### `xrtJsonQuoteWrite`

严格校验 UTF-8 并流式写出包含双引号的 JSON 字符串 token。

```c
bool xrtJsonQuoteWrite(
	xstrview Text,
	uint32 iFlags,
	xjsonwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用、严格 UTF-8 | 原始字符串 |
| `iFlags` | 输入 | `XJSON_WRITE_*` | 转义标志 |
| `pWrite` | 输入 | 非空 | 输出回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |
| `pWritten` | 输出 | 可空 | 写出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已流式写出（含首尾引号） | — |
| `false` | UTF-8 非法或回调失败 | `xrt.json` 域错误 |

#### 错误

- `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）

#### 范例

[data/json_tour · data/json_tour · 转义](../../examples/data/json_tour/main.c) · 观察

```c
	if ( !xrtJsonQuoteWrite(SV("a\"b\\c"), 0u, exampleCollect,
			&Sink, &iWritten) ||
		(Sink.Size != 9u) ||  /* 带引号转义 "a\"b\\c" 共 9 字节 */
		(memcmp(Sink.Buffer, "\"a\\\"b\\\\c\"", 9u) != 0) ) {
```


### `xrtJsonParseFile`

使用默认严格配置读取并解析 JSON 文件。

```c
xvalue* xrtJsonParseFile(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 文件路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有的 Value DOM | — |
| `NULL` | 读取或解析失败 | `xrt.io` / `xrt.json` 域错误 |

#### 错误

- `xrt.io` 域错误 — 文件读取失败
- - `xrt.json` 域错误 — 语法非法、超限或截断（Data 含 offset）
- `XERR_MEMORY`

#### 范例

[data/json_tour · data/json · 文件](../../examples/data/json_tour/main.c) · 观察

```c
	pFileDom = xrtJsonParseFile(sFile);
```


### `xrtJsonReadFile`

使用读取配置和其中的输入上限解析 JSON 文件。

```c
xvalue* xrtJsonReadFile(
	cstr sPath,
	const xjsonreadconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 文件路径 |
| `pConfig` | 输入 | 允许空 | 含 InputLimit |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | Value DOM | — |
| `NULL` | 失败 | 域错误 |

#### 错误

- `xrt.io` / `xrt.json` 域错误 / `XERR_MEMORY`

#### 范例

[data/json_tour · data/json_tour · 文件](../../examples/data/json_tour/main.c) · 观察

```c
	pReadDom = xrtJsonReadFile(sFile, &ReadConfig);
```


### `xrtJsonWriteFile`

使用高级配置序列化并原子替换 JSON 文件。

```c
bool xrtJsonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目标路径 |
| `pValue` | 输入 | 非空 | Value 树 |
| `pConfig` | 输入 | 允许空 | 写入配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已原子发布 | — |
| `false` | 序列化或写入失败 | 域错误 |

#### 错误

- `xrt.json` / `xrt.io` 域错误

#### 范例

[data/json_tour · data/json_tour · 文件](../../examples/data/json_tour/main.c) · 观察

```c
	if ( !xrtJsonWriteFile(sFile, pDom, &WriteConfig) ||
		((pInner = xrtJsonParseFile(sFile)) == NULL) ) {
```


### `xrtJsonStringifyFile`

紧凑或美化地序列化并原子替换 JSON 文件。

```c
bool xrtJsonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目标路径 |
| `pValue` | 输入 | 非空 | Value 树 |
| `bPretty` | 输入 | — | 缩进美化 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已原子发布 | — |
| `false` | 序列化或写入失败 | 域错误 |

#### 错误

- `xrt.json` / `xrt.io` 域错误

#### 范例

[data/json_tour · data/json · 文件](../../examples/data/json_tour/main.c) · 观察

```c
	if ( !xrtJsonStringifyFile(sFile, pDom, true) ) {
```


