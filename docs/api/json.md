# JSON

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
