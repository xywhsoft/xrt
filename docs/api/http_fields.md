# HTTP 字段与参数

`<xrt/http.h>` 提供 HTTP 线协议共享的零分配语法工具。所有结果借用输入，适合
HTTP/1、WebSocket 握手、代理协议和扩展库复用。

## 类型与常量

### `xhttpmethod`

常用 HTTP 方法使用互不重叠的单 bit 枚举值。非零值既表示一个解析后的 方法，也可以作为方法集合中的原子位；组合宏提供常用路由方法集合。 OTHER 表示语法合法但未内置分类的方法；INVALID 表示空值或非法 token， 在方法集合中也自然表示不匹配任何方法。

```c
typedef enum xhttpmethod {
	XHTTP_METHOD_INVALID = 0,
	XHTTP_METHOD_OTHER = UINT32_C(0x00000001),
	XHTTP_METHOD_GET = UINT32_C(0x00000002),
	XHTTP_METHOD_HEAD = UINT32_C(0x00000004),
	XHTTP_METHOD_POST = UINT32_C(0x00000008),
	XHTTP_METHOD_PUT = UINT32_C(0x00000010),
	XHTTP_METHOD_DELETE = UINT32_C(0x00000020),
	XHTTP_METHOD_CONNECT = UINT32_C(0x00000040),
	XHTTP_METHOD_OPTIONS = UINT32_C(0x00000080),
	XHTTP_METHOD_TRACE = UINT32_C(0x00000100),
	XHTTP_METHOD_PATCH = UINT32_C(0x00000200)
} xhttpmethod;
```

| 值 | 语义 |
|---|---|
| `XHTTP_METHOD_INVALID` | 无效 |
| `XHTTP_METHOD_OTHER` | OTHER |
| `XHTTP_METHOD_GET` | GET |
| `XHTTP_METHOD_HEAD` | HEAD |
| `XHTTP_METHOD_POST` | POST |
| `XHTTP_METHOD_PUT` | PUT |
| `XHTTP_METHOD_DELETE` | DELETE |
| `XHTTP_METHOD_CONNECT` | CONNECT |
| `XHTTP_METHOD_OPTIONS` | OPTIONS |
| `XHTTP_METHOD_TRACE` | 最详细级别 |

### `xhttpversion`

HTTP 版本使用可直接比较的主次版本编码。

```c
typedef enum xhttpversion {
	XHTTP_VERSION_1_0 = 10,
	XHTTP_VERSION_1_1 = 11
} xhttpversion;
```

| 值 | 语义 |
|---|---|
| `XHTTP_VERSION_1_0` | XHTTPVERSION10 |

### `xhttpstatus`

HTTP 状态常量只收录 IANA 已正式分配的通用状态。 未分配、临时分配和明确标记为 Unused 的数值仍可直接使用 uint16 表达。

```c
typedef enum xhttpstatus {
	/* 1xx：信息响应。 */
	XHTTP_STATUS_CONTINUE = 100,
	XHTTP_STATUS_SWITCHING_PROTOCOLS = 101,
	XHTTP_STATUS_PROCESSING = 102,
	XHTTP_STATUS_EARLY_HINTS = 103,

	/* 2xx：成功响应。 */
	XHTTP_STATUS_OK = 200,
	XHTTP_STATUS_CREATED = 201,
	XHTTP_STATUS_ACCEPTED = 202,
	XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
	XHTTP_STATUS_NO_CONTENT = 204,
	XHTTP_STATUS_RESET_CONTENT = 205,
	XHTTP_STATUS_PARTIAL_CONTENT = 206,
	XHTTP_STATUS_MULTI_STATUS = 207,
	XHTTP_STATUS_ALREADY_REPORTED = 208,
	XHTTP_STATUS_IM_USED = 226,

	/* 3xx：重定向响应。 */
	XHTTP_STATUS_MULTIPLE_CHOICES = 300,
	XHTTP_STATUS_MOVED_PERMANENTLY = 301,
	XHTTP_STATUS_FOUND = 302,
	XHTTP_STATUS_SEE_OTHER = 303,
	XHTTP_STATUS_NOT_MODIFIED = 304,
	XHTTP_STATUS_USE_PROXY = 305,
	XHTTP_STATUS_TEMPORARY_REDIRECT = 307,
	XHTTP_STATUS_PERMANENT_REDIRECT = 308,

	/* 4xx：客户端错误响应。 */
	XHTTP_STATUS_BAD_REQUEST = 400,
	XHTTP_STATUS_UNAUTHORIZED = 401,
	XHTTP_STATUS_PAYMENT_REQUIRED = 402,
	XHTTP_STATUS_FORBIDDEN = 403,
	XHTTP_STATUS_NOT_FOUND = 404,
	XHTTP_STATUS_METHOD_NOT_ALLOWED = 405,
	XHTTP_STATUS_NOT_ACCEPTABLE = 406,
	XHTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
	XHTTP_STATUS_REQUEST_TIMEOUT = 408,
	XHTTP_STATUS_CONFLICT = 409,
	XHTTP_STATUS_GONE = 410,
	XHTTP_STATUS_LENGTH_REQUIRED = 411,
	XHTTP_STATUS_PRECONDITION_FAILED = 412,
	XHTTP_STATUS_CONTENT_TOO_LARGE = 413,
	XHTTP_STATUS_URI_TOO_LONG = 414,
	XHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415,
	XHTTP_STATUS_RANGE_NOT_SATISFIABLE = 416,
	XHTTP_STATUS_EXPECTATION_FAILED = 417,
	XHTTP_STATUS_MISDIRECTED_REQUEST = 421,
	XHTTP_STATUS_UNPROCESSABLE_CONTENT = 422,
	XHTTP_STATUS_LOCKED = 423,
	XHTTP_STATUS_FAILED_DEPENDENCY = 424,
	XHTTP_STATUS_TOO_EARLY = 425,
	XHTTP_STATUS_UPGRADE_REQUIRED = 426,
	XHTTP_STATUS_PRECONDITION_REQUIRED = 428,
	XHTTP_STATUS_TOO_MANY_REQUESTS = 429,
	XHTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
	XHTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS = 451,

	/* 5xx：服务器错误响应。 */
	XHTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
	XHTTP_STATUS_NOT_IMPLEMENTED = 501,
	XHTTP_STATUS_BAD_GATEWAY = 502,
	XHTTP_STATUS_SERVICE_UNAVAILABLE = 503,
	XHTTP_STATUS_GATEWAY_TIMEOUT = 504,
	XHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED = 505,
	XHTTP_STATUS_VARIANT_ALSO_NEGOTIATES = 506,
	XHTTP_STATUS_INSUFFICIENT_STORAGE = 507,
	XHTTP_STATUS_LOOP_DETECTED = 508,
	XHTTP_STATUS_NOT_EXTENDED = 510,
	XHTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED = 511
} xhttpstatus;
```

| 值 | 语义 |
|---|---|
| `XHTTP_STATUS_CONTINUE` | CONTINUE |
| `XHTTP_STATUS_SWITCHING_PROTOCOLS` | SWITCHINGPROTOCOLS |
| `XHTTP_STATUS_PROCESSING` | PROCESSING |
| `XHTTP_STATUS_EARLY_HINTS` | EARLYHINTS |
| `XHTTP_STATUS_OK` | 成功 |
| `XHTTP_STATUS_CREATED` | CREATED |
| `XHTTP_STATUS_ACCEPTED` | ACCEPTED |
| `XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION` | NONAUTHORITATIVEINFORMATION |
| `XHTTP_STATUS_NO_CONTENT` | NOCONTENT |
| `XHTTP_STATUS_RESET_CONTENT` | RESETCONTENT |
| `XHTTP_STATUS_PARTIAL_CONTENT` | PARTIALCONTENT |
| `XHTTP_STATUS_MULTI_STATUS` | MULTISTATUS |
| `XHTTP_STATUS_ALREADY_REPORTED` | ALREADYREPORTED |
| `XHTTP_STATUS_IM_USED` | IMUSED |
| `XHTTP_STATUS_MULTIPLE_CHOICES` | MULTIPLECHOICES |
| `XHTTP_STATUS_MOVED_PERMANENTLY` | MOVEDPERMANENTLY |
| `XHTTP_STATUS_FOUND` | FOUND |
| `XHTTP_STATUS_SEE_OTHER` | SEEOTHER |
| `XHTTP_STATUS_NOT_MODIFIED` | NOTMODIFIED |
| `XHTTP_STATUS_USE_PROXY` | USEPROXY |
| `XHTTP_STATUS_TEMPORARY_REDIRECT` | TEMPORARYREDIRECT |
| `XHTTP_STATUS_PERMANENT_REDIRECT` | PERMANENTREDIRECT |
| `XHTTP_STATUS_BAD_REQUEST` | BADREQUEST |
| `XHTTP_STATUS_UNAUTHORIZED` | UNAUTHORIZED |
| `XHTTP_STATUS_PAYMENT_REQUIRED` | PAYMENTREQUIRED |
| `XHTTP_STATUS_FORBIDDEN` | FORBIDDEN |
| `XHTTP_STATUS_NOT_FOUND` | NOTFOUND |
| `XHTTP_STATUS_METHOD_NOT_ALLOWED` | METHODNOTALLOWED |
| `XHTTP_STATUS_NOT_ACCEPTABLE` | NOTACCEPTABLE |
| `XHTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED` | PROXYAUTHENTICATIONREQUIRED |
| `XHTTP_STATUS_REQUEST_TIMEOUT` | REQUEST超时 |
| `XHTTP_STATUS_CONFLICT` | CONFLICT |
| `XHTTP_STATUS_GONE` | GONE |
| `XHTTP_STATUS_LENGTH_REQUIRED` | LENGTHREQUIRED |
| `XHTTP_STATUS_PRECONDITION_FAILED` | PRECONDITION已失败 |
| `XHTTP_STATUS_CONTENT_TOO_LARGE` | CONTENTTOOLARGE |
| `XHTTP_STATUS_URI_TOO_LONG` | URITOOLONG |
| `XHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE` | 不支持MEDIA类型 |
| `XHTTP_STATUS_RANGE_NOT_SATISFIABLE` | 范围越界NOTSATISFIABLE |
| `XHTTP_STATUS_EXPECTATION_FAILED` | EXPECTATION已失败 |
| `XHTTP_STATUS_MISDIRECTED_REQUEST` | MISDIRECTEDREQUEST |
| `XHTTP_STATUS_UNPROCESSABLE_CONTENT` | UNPROCESSABLECONTENT |
| `XHTTP_STATUS_LOCKED` | LOCKED |
| `XHTTP_STATUS_FAILED_DEPENDENCY` | 已失败DEPENDENCY |
| `XHTTP_STATUS_TOO_EARLY` | TOOEARLY |
| `XHTTP_STATUS_UPGRADE_REQUIRED` | UPGRADEREQUIRED |
| `XHTTP_STATUS_PRECONDITION_REQUIRED` | PRECONDITIONREQUIRED |
| `XHTTP_STATUS_TOO_MANY_REQUESTS` | TOOMANYREQUESTS |
| `XHTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE` | REQUESTHEADER字段TOOLARGE |
| `XHTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS` | UNAVAILABLEFORLEGALREASONS |
| `XHTTP_STATUS_INTERNAL_SERVER_ERROR` | 内部错误服务端角色失败 |
| `XHTTP_STATUS_NOT_IMPLEMENTED` | NOTIMPLEMENTED |
| `XHTTP_STATUS_BAD_GATEWAY` | BADGATEWAY |
| `XHTTP_STATUS_SERVICE_UNAVAILABLE` | SERVICEUNAVAILABLE |
| `XHTTP_STATUS_GATEWAY_TIMEOUT` | GATEWAY超时 |
| `XHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED` | HTTPVERSIONNOTSUPPORTED |
| `XHTTP_STATUS_VARIANT_ALSO_NEGOTIATES` | VARIANTALSONEGOTIATES |
| `XHTTP_STATUS_INSUFFICIENT_STORAGE` | INSUFFICIENTSTORAGE |
| `XHTTP_STATUS_LOOP_DETECTED` | LOOPDETECTED |
| `XHTTP_STATUS_NOT_EXTENDED` | NOTEXTENDED |

### `xhttpfield`

字段名称和值都是借用视图，不要求零结尾。

```c
typedef struct xhttpfield {
	xstrview Name;
	xstrview Value;
} xhttpfield;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | Name |
| `Value` | `xstrview` | Value |

### `xhttpnext`

HTTP 值迭代结果明确区分条目、正常结束和语法错误。

```c
typedef enum xhttpnext {
	XHTTP_NEXT_ERROR = -1,
	XHTTP_NEXT_END = 0,
	XHTTP_NEXT_ITEM = 1
} xhttpnext;
```

| 值 | 语义 |
|---|---|
| `XHTTP_NEXT_ERROR` | 失败 |
| `XHTTP_NEXT_END` | END |

### `xhttpfieldtokencursor`

重复同名 token-list 字段游标由初始化函数建立，调用方不得直接修改。

```c
typedef struct xhttpfieldtokencursor {
	const void* Source;
	xstrview Name;
	size_t Count;
	size_t Field;
	size_t Offset;
	uint8 Validated;
	uint8 Required;
} xhttpfieldtokencursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Source` | `const void*` | Source |
| `Name` | `xstrview` | Name |
| `Count` | `size_t` | Count |
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | Offset |
| `Validated` | `uint8` | Validated |
| `Required` | `uint8` | Required |

### `xhttpweightedtoken`

加权 token 借用原字段值，Quality 使用 0 到 1000 的无浮点定点值。

```c
typedef struct xhttpweightedtoken {
	xstrview Token;
	uint16 Quality;
} xhttpweightedtoken;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Token` | `xstrview` | Token |
| `Quality` | `uint16` | Quality |

### `xhttpauthority`

HTTP authority 借用原始文本，不接受 userinfo。

```c
typedef struct xhttpauthority {
	uint32 Flags;
	uint16 Port;
	xstrview Text;
	xstrview Host;
	xstrview PortText;
} xhttpauthority;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | Flags |
| `Port` | `uint16` | Port |
| `Text` | `xstrview` | Text |
| `Host` | `xstrview` | Host |
| `PortText` | `xstrview` | PortText |

### `xhttptargetform`

Request-target 形式由方法与线路文本共同决定。

```c
typedef enum xhttptargetform {
	XHTTP_TARGET_ORIGIN = 1,
	XHTTP_TARGET_ABSOLUTE,
	XHTTP_TARGET_AUTHORITY,
	XHTTP_TARGET_ASTERISK
} xhttptargetform;
```

| 值 | 语义 |
|---|---|
| `XHTTP_TARGET_ORIGIN` | ORIGIN |
| `XHTTP_TARGET_ABSOLUTE` | ABSOLUTE |
| `XHTTP_TARGET_AUTHORITY` | AUTHORITY |

### `xhttptarget`

Target 借用原始方法与 request-target，并只保留 HTTP 路径需要的 URI 组件。

```c
typedef struct xhttptarget {
	xhttptargetform Form;
	uint32 Flags;
	xstrview Method;
	xstrview Text;
	xstrview Scheme;
	xstrview Authority;
	xstrview Path;
	xstrview Query;
	xhttpauthority Host;
} xhttptarget;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Form` | `xhttptargetform` | Form |
| `Flags` | `uint32` | Flags |
| `Method` | `xstrview` | Method |
| `Text` | `xstrview` | Text |
| `Scheme` | `xstrview` | Scheme |
| `Authority` | `xstrview` | Authority |
| `Path` | `xstrview` | Path |
| `Query` | `xstrview` | Query |
| `Host` | `xhttpauthority` | Host |

### `xhttpparamflags`

参数值标志区分省略值、token 值和 quoted-string 值。

```c
typedef enum xhttpparamflags {
	XHTTP_PARAM_NONE = 0,
	XHTTP_PARAM_HAS_VALUE = 0x01,
	XHTTP_PARAM_QUOTED = 0x02
} xhttpparamflags;
```

| 值 | 语义 |
|---|---|
| `XHTTP_PARAM_NONE` | 无 |
| `XHTTP_PARAM_HAS_VALUE` | HAS值非法 |

### `xhttpparam`

参数名称和值借用原文本；quoted-string 值不含双引号，但保留反斜杠转义。

```c
typedef struct xhttpparam {
	xstrview Name;
	xstrview Value;
	uint32 Flags;
} xhttpparam;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | Name |
| `Value` | `xstrview` | Value |
| `Flags` | `uint32` | Flags |

### `xhttpparamvaluecursor`

参数语义值游标由初始化函数建立；Offset 是下一次读取的原始值偏移。

```c
typedef struct xhttpparamvaluecursor {
	const void* Source;
	const void* Value;
	size_t ValueSize;
	size_t Offset;
	uint32 Flags;
	uint8 Validated;
} xhttpparamvaluecursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Source` | `const void*` | Source |
| `Value` | `const void*` | Value |
| `ValueSize` | `size_t` | ValueSize |
| `Offset` | `size_t` | Offset |
| `Flags` | `uint32` | Flags |
| `Validated` | `uint8` | Validated |

### `xhttpconnectionstatus`

连接持久性结果区分协议错误、当前响应后关闭和继续复用。

```c
typedef enum xhttpconnectionstatus {
	XHTTP_CONNECTION_ERROR = -1,
	XHTTP_CONNECTION_CLOSE = 0,
	XHTTP_CONNECTION_PERSIST = 1
} xhttpconnectionstatus;
```

| 值 | 语义 |
|---|---|
| `XHTTP_CONNECTION_ERROR` | 失败 |
| `XHTTP_CONNECTION_CLOSE` | CLOSE |

### `xhttpconnectionflag`

HTTP/1.0 持久性判断所需的消息方向、接收角色和本地策略。

```c
typedef enum xhttpconnectionflag {
	XHTTP_CONNECTION_RESPONSE = UINT32_C(0x00000001),
	XHTTP_CONNECTION_PROXY = UINT32_C(0x00000002),
	XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE = UINT32_C(0x00000004)
} xhttpconnectionflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_CONNECTION_RESPONSE` | RESPONSE |
| `XHTTP_CONNECTION_PROXY` | PROXY |

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XHTTP_QUALITY_MAX` | `1000u` | QUALITY上限 |
| `XHTTP_METHOD_CRUD` | `( \` | 常用 CRUD 路由方法集合；PUT 和 PATCH 都属于更新方法。 |
| `XHTTP_METHOD_ANY` | `( \` | 匹配任一内置方法或语法合法的扩展方法。 |
| `XHTTP_AUTHORITY_HAS_PORT` | `UINT32_C(0x00000001)` | Authority 包含显式端口分隔符。 |
| `XHTTP_AUTHORITY_IP_LITERAL` | `UINT32_C(0x00000002)` | Host 是 IPv6 或 IPvFuture 字面地址，Host 视图不包含方括号。 |
| `XHTTP_AUTHORITY_PORT_EMPTY` | `UINT32_C(0x00000004)` | 显式端口只有冒号而没有数字。 |
| `XHTTP_AUTHORITY_PORT_VALUE` | `UINT32_C(0x00000008)` | Port 保存可由 uint16 无损表达的显式端口。 |
| `XHTTP_TARGET_HAS_SCHEME` | `UINT32_C(0x00000001)` | Target 包含 scheme。 |
| `XHTTP_TARGET_HAS_AUTHORITY` | `UINT32_C(0x00000002)` | Target 包含双斜杠引入的 authority。 |
| `XHTTP_TARGET_HAS_QUERY` | `UINT32_C(0x00000004)` | Target 包含问号引入的 query，包括显式空 query。 |

## 字段

`xhttpfield` 只有 `Name` 和 `Value` 两个借用视图。字段名称不带冒号，字段值不带
两端 OWS 或 CRLF。

`xrtHttpFieldParse` 解析一条不含 CRLF 的字段行；`xrtHttpFieldNext` 逐行扫描字段
块；`xrtHttpFieldBlockCount` 在不保存描述符时完成严格验证和计数。

`xrtHttpFieldWrite` 写一条字段，`xrtHttpFieldBlockWrite` 写字段数组和可选最终空行。
两者都支持空输出测量，容量不足时不会部分写出。

查询函数包括：

- `xrtHttpFieldNameEqual`：ASCII 不区分大小写字段名比较；
- `xrtHttpFieldFind`、`Get`、`Count`：查找重复字段；
- `xrtHttpFieldGetUnique`：区分缺失、唯一和重复；
- `xrtHttpFieldValueValid`：验证字段值字节边界。

字段描述符可以未对齐，但描述符数组、借用文本和写出区不能发生不明确重叠。

## Content-Length

`xrtHttpContentLengthParse` 接受十进制值和 RFC 允许的重复相同列表值，拒绝空值、
符号、非数字、溢出和冲突值。

该函数只解析单个字段值。HTTP/1 parser 会合并全部重复 Content-Length 字段，
同时执行 Transfer-Encoding 冲突和请求分帧检查。

## token 与列表

- `xrtHttpTokenValid` 验证 RFC tchar；
- `xrtHttpTokenEqual` 和 `xrtHttpMethodEqual` 执行 ASCII 不区分大小写比较；
- `xrtHttpTokenNext` 迭代逗号 token-list；
- `xrtHttpFieldTokenNext` 跨重复同名字段迭代；
- `xrtHttpFieldTokenCount`、`Find` 处理重复字段集合；
- `xrtHttpQualityParse` 解析 0 到 1 的 qvalue 为 0 到 1000 定点数；
- `xrtHttpWeightedTokenNext` 迭代带权 token。

`xrtHttpOwsTrim` 只移除 SP 和 HTAB，不把其他空白字符当作 HTTP OWS。

## 方法与状态

`xrtHttpStatusText` 返回内置状态的标准 reason phrase。未知状态返回空视图，调用方
仍可用 `uint16` 写出扩展状态。

`xrtHttpMethodSafe`、`xrtHttpMethodIdempotent` 和
`xrtHttpResponseContentAllowed` 提供基础协议语义。它们不替代应用权限、重试和
缓存策略。

## 参数

启用 `XRT_MODULE_HTTP_PARAM` 后，`xhttpparam` 表达 `name[=value]`：

- `XHTTP_PARAM_HAS_VALUE`：存在等号和值；
- `XHTTP_PARAM_QUOTED`：值来自 quoted-string，视图不含外层双引号；
- `XHTTP_PARAM_NONE`：只有名称。

`xrtHttpParamNext` 迭代分号参数，`xrtHttpDirectiveNext` 迭代逗号指令。对应的
`Count` 和 `Find` 函数使用同一套严格语法。

`xrtHttpQuotedValid`、`Read`、`Write` 处理 quoted-string 和 quoted-pair；
`xrtHttpParamValueCursorInit` 与 `xrtHttpParamValueNext` 零分配逐字节读取语义值，
并通过 `Offset` 暴露已经消耗的原始值长度；`xrtHttpParamValueWrite` 一次性输出
参数语义值；`xrtHttpParamWrite` 写单个参数。

`Build` 便利函数返回由 `xrtFree` 释放的零结尾字符串。热路径应优先使用测量加
调用方缓冲的 `Write` 版本。

## Host 与 request-target

启用 `http_host` 后，`xrtHttpHostParse` 返回借用的 `xhttpauthority`；
`xrtHttpHostValid` 执行同样的严格验证。解析过程不分配内存，IPv6 与 IPvFuture
字面地址的 `Host` 视图不包含方括号，`PortText` 保留显式端口的原始文本。

`xrtHttpAuthorityValid` 验证解析或手工构造的 authority。手工构造数值端口时设置
`XHTTP_AUTHORITY_HAS_PORT | XHTTP_AUTHORITY_PORT_VALUE` 并填写 `Port`，不需要伪造
`PortText`。`xrtHttpAuthorityPort` 将可用的显式端口写入调用方变量；省略端口或显式
空端口时返回调用方提供的默认值，超出 `uint16` 的合法协议端口文本不能直接交给
网络层，因此返回失败。

`xrtHttpTargetParse` 区分 origin-form、absolute-form、authority-form 和
asterisk-form，并结合方法检查 CONNECT 与 OPTIONS 的专用约束。结果同样只借用输入，
核心层不构建 URL 对象，也不执行查询参数解码。

## 模块契约：线程

字段解析与编码 API 为无共享状态的纯函数，可任意线程并发调用。

## 已迁出能力

动态 Header 容器、RFC 8187 扩展值、MIME、Structured Fields、Digest Fields、
Forwarded、Link、Priority、Cache-Status、Proxy-Status 和 Content-Disposition
不属于 XRT HTTP 核心；需要这些高级协议能力时，请使用独立发布的 `xhttp` 扩展。

## API

### 方法与状态

### `xrtHttpMethodParse`

按大小写敏感规则分类 HTTP 方法；合法扩展方法返回 OTHER。

```c
xhttpmethod xrtHttpMethodParse(xstrview Method);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_METHOD_GET/POST/...` | 已知方法 | — |
| `XHTTP_METHOD_OTHER` | 合法扩展方法 | — |
| `XHTTP_METHOD_INVALID` | 空值或非法 token | 不设置错误 |

#### 错误

- 无 — INVALID 是分类结果

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		printf("parse=%u", (unsigned)xrtHttpMethodParse(SV("GET")));
```

### `xrtHttpMethodEqual`

按 HTTP 大小写敏感规则比较两个合法方法名。

```c
bool xrtHttpMethodEqual(
	xstrview Left,
	xstrview Right
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左方法名 |
| `Right` | 输入 | 借用 | 右方法名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 逐字节相等（HTTP 方法区分大小写） | — |
| `false` | 不等 | 纯比较 |

#### 错误

- 无 — 纯比较

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		xrtHttpMethodEqual(SV("PATCH"), SV("PATCH")) ? 1
```

### `xrtHttpMethodSafe`

判断方法是否只读取资源语义；GET、HEAD、OPTIONS 和 TRACE 属于安全方法。

```c
bool xrtHttpMethodSafe(xstrview Method);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 安全方法 | — |
| `false` | 非安全 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		printf(" safe=%d", xrtHttpMethodSafe(SV("GET")) ? 1
```

### `xrtHttpMethodIdempotent`

判断方法是否允许重复执行而不改变预期效果；安全方法、PUT 和 DELETE 属于幂等方法。

```c
bool xrtHttpMethodIdempotent(xstrview Method);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 幂等 | — |
| `false` | 非幂等 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		printf(" idem=%d\n", xrtHttpMethodIdempotent(SV("DELETE")) ? 1
```

### `xrtHttpStatusText`

返回已注册状态码的标准原因短语；未知、临时或未分配状态返回空视图。

```c
xstrview xrtHttpStatusText(uint16 iStatus);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iStatus` | 输入 | — | 状态码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 静态原因短语（人类可读，协议逻辑不得依赖） | — |
| 空视图 | 未注册状态码 | 纯查询 |

#### 错误

- 无 — 查询结果即答案

#### 范例

[http/method_tour · 状态](../../examples/http/method_tour/main.c) · 观察

```c
		xstrview Text = xrtHttpStatusText(200);
```

### `xrtHttpResponseContentAllowed`

判断最终响应是否允许携带内容；HEAD、1xx、204、205、304 和成功 CONNECT 返回假。

```c
bool xrtHttpResponseContentAllowed(
	xstrview Method,
	uint16 iStatus
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用、大小写敏感 | 请求方法 |
| `iStatus` | 输入 | 100–999 | 状态码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 允许携带内容 | — |
| `false` | 禁止内容状态或无效方法/状态 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/method_tour · 状态](../../examples/http/method_tour/main.c) · 观察

```c
		printf(" content-allowed=%d\n",
			xrtHttpResponseContentAllowed(SV("HEAD"), 204) ? 1 : 0);
```


### 令牌与 OWS

### `xrtHttpTokenValid`

判断文本是否是非空 HTTP token。

```c
bool xrtHttpTokenValid(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待判定文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 非空合法 token | — |
| `false` | 空或含非法字符 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/token_tour · 令牌](../../examples/http/token_tour/main.c) · 观察

```c
		printf("valid=%d", xrtHttpTokenValid(SV("gzip")) ? 1
```

### `xrtHttpTokenEqual`

按 ASCII 大小写不敏感规则比较两个 token。

```c
bool xrtHttpTokenEqual(xstrview Left, xstrview Right);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左 token |
| `Right` | 输入 | 借用 | 右 token |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 大小写不敏感相等 | — |
| `false` | 不等 | 纯比较 |

#### 错误

- 无 — 纯比较

#### 范例

[http/token_tour · 令牌](../../examples/http/token_tour/main.c) · 观察

```c
		printf(" eq=%d\n", xrtHttpTokenEqual(SV("GZIP"), SV("gzip")) ? 1
```

### `xrtHttpOwsTrim`

剥离两端的可选空白（SP/HTAB）并返回剩余视图。

```c
xstrview xrtHttpOwsTrim(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 子视图 | 去除两端 OWS 的借用视图 | 纯切片 |

#### 错误

- 无 — 纯切片

#### 范例

[http/token_tour · OWS](../../examples/http/token_tour/main.c) · 观察

```c
	Trimmed = xrtHttpOwsTrim(SV("  value  "));
```

### `xrtHttpTokenNext`

按 RFC 接收方规则迭代 token-list，忽略逗号空元素；`Offset` 初始为零。

```c
xhttpnext xrtHttpTokenNext(
	xstrview List,
	size_t* pOffset,
	xstrview* pToken
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | token-list 文本 |
| `pOffset` | 输入/输出 | 非空、初始零 | 游标 |
| `pToken` | 输出 | 非空 | 接收条目（借用输入） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 取得一个 token，游标前进 | — |
| `XHTTP_NEXT_END` | 迭代完成 | 不设错 |
| `XHTTP_NEXT_ERROR` | 非空元素语法错误 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 非空元素非法 token

#### 范例

[http/base · 字段值迭代](../../examples/http/base/main.c) · 观察

```c
	while ( (Next = xrtHttpTokenNext(
		Field.Value, &iOffset, &Token
	)) == XHTTP_NEXT_ITEM ) {
```

### `xrtHttpTokenListHas`

判断完整 token-list 是否包含指定 token；非空元素语法错误仍返回 false 并设置错误。

```c
bool xrtHttpTokenListHas(xstrview List, xstrview Token);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | token-list |
| `Token` | 输入 | 借用 | 查找目标（大小写不敏感） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 包含 | — |
| `false` | 不包含，或列表非法 | 非法时设置 `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 非空元素非法

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		printf("has=%d", xrtHttpTokenListHas(SV(sList), SV("deflate")) ? 1
```

### `xrtHttpTokenListCount`

统计 token-list 非空条目；空列表成功返回零。

```c
bool xrtHttpTokenListCount(xstrview List, size_t* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | token-list |
| `pCount` | 输出 | 非空 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 非空元素语法错误 | `*pCount` 不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		if ( xrtHttpTokenListCount(SV(sList), &iCount) ) {
```

### `xrtHttpTokenListWrite`

规范写出逗号空格分隔的 token-list；空输出可精确查询长度。

```c
bool xrtHttpTokenListWrite(
	const xstrview* pTokens,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTokens` | 输入 | 借用数组 | token 视图数组 |
| `iCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 只查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 实际/所需长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出或长度已发布 | — |
| `false` | 参数或容量错误 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足（给出所需长度）

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		if ( xrtHttpTokenListWrite(Tokens, 2u, Buffer, sizeof(Buffer), &iSize) ) {
```

### `xrtHttpTokenListBuild`

构建零结尾 token-list，返回值由 `xrtFree` 释放。

```c
str xrtHttpTokenListBuild(
	const xstrview* pTokens,
	size_t iCount,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTokens` | 输入 | 借用数组 | token 数组 |
| `iCount` | 输入 | — | 条目数 |
| `pSize` | 输出 | 可空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 零结尾列表 | — |
| `NULL` | 参数错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_MEMORY`

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		sBuilt = xrtHttpTokenListBuild(Tokens, 3u, NULL);
```


### 权重与长度

### `xrtHttpQualityParse`

严格解析 RFC qvalue（接受两端 OWS），结果范围 0–1000。

```c
bool xrtHttpQualityParse(
	xstrview Text,
	uint16* pQuality
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | qvalue 文本 |
| `pQuality` | 输出 | 非空、不得与 Text 重叠 | 接收千分值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出（0–1000） | — |
| `false` | 语法错误或参数错误 | 输出保持为零/不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `xrt.http` 域错误 — 非法 qvalue

#### 范例

[http/method_tour · 权重](../../examples/http/method_tour/main.c) · 观察

```c
		if ( !xrtHttpQualityParse(SV("0.5"), &iQuality) ) {
```

### `xrtHttpWeightedTokenNext`

迭代 token [ weight ] 列表并忽略空成员；缺省 Quality 为 1000。可直接用于 Accept-Encoding 等字段。

```c
xhttpnext xrtHttpWeightedTokenNext(
	xstrview List,
	size_t* pOffset,
	xhttpweightedtoken* pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | 加权列表文本 |
| `pOffset` | 输入/输出 | 初始零 | 游标 |
| `pItem` | 输出 | 非空 | 接收 Token+Quality |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 取得一项 | — |
| `XHTTP_NEXT_END` | 完成 | 不设错 |
| `XHTTP_NEXT_ERROR` | 语法错误 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/token_tour · 加权迭代](../../examples/http/token_tour/main.c) · 观察

```c
		while ( xrtHttpWeightedTokenNext(SV(sWeighted), &iOffset,
			&Item) == XHTTP_NEXT_ITEM ) {
```

### `xrtHttpContentLengthParse`

解析 Content-Length；逗号分隔的重复值只有完全一致时才成功。

```c
bool xrtHttpContentLengthParse(
	xstrview Value,
	uint64* pLength
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pLength` | 输出 | 非空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 长度已写出 | — |
| `false` | 非法/重复不一致 | 输出保持为零 |

#### 错误

- `xrt.http` 域错误 — 非数字或重复值不一致

#### 范例

[http/method_tour · 长度](../../examples/http/method_tour/main.c) · 观察

```c
		if ( !xrtHttpContentLengthParse(SV("42"), &iLength) ) {
```


### Host 与 Authority

### `xrtHttpHostParse`

解析单个 Host 字段值为借用 authority 结构；空字段值与空端口按 RFC 保留，PORT_VALUE 表示可用网络数值。

```c
bool xrtHttpHostParse(
	xstrview Value,
	xhttpauthority* pHost
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用、不含 OWS/字段名 | 字段值 |
| `pHost` | 输出 | 非空、支持未对齐存储 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 结构已发布（视图借用输入） | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 非法 authority

#### 范例

[http/host · 解析](../../examples/http/host/main.c) · 观察

```c
	if ( !xrtHttpHostParse(
		XRT_STR_LITERAL("[2001:db8::1]:8443"), &Host
	) || !xrtHttpAuthorityPort(&Host, 80u, &iPort) ) {
```

### `xrtHttpHostValid`

验证 Host 字段值是单个、无 userinfo 的 URI authority。

```c
bool xrtHttpHostValid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · Host](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpHostValid(SV("example.com")) ? 1
```

### `xrtHttpIpv4Valid`

严格验证 RFC 3986 IPv4 文本；拒绝多段、越界值和前导零。

```c
bool xrtHttpIpv4Valid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | IPv4 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法（含 `01.2.3.4` 前导零） | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · IPv4](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpIpv4Valid(SV("01.2.3.4")) ? 1
```

### `xrtHttpIpv6Valid`

严格验证 IPv6 文本，支持压缩和嵌入式 IPv4，不接受 ZoneID。

```c
bool xrtHttpIpv6Valid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | IPv6 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · IPv6](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpIpv6Valid(SV("
```

### `xrtHttpHostEqual`

按 ASCII 大小写不敏感规则比较两个已拆分 Host 视图。

```c
bool xrtHttpHostEqual(xstrview Left, xstrview Right);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左 Host |
| `Right` | 输入 | 借用 | 右 Host |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 相等（host 大小写不敏感） | — |
| `false` | 不等 | 纯比较 |

#### 错误

- 无 — 纯比较

#### 范例

[http/validate_tour · Host](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpHostEqual(SV("EXAMPLE.com"), SV("example.com")) ? 1
```

### `xrtHttpAuthorityValid`

验证拆分后的 authority 字段、标志与端口数值保持一致。

```c
bool xrtHttpAuthorityValid(
	const xhttpauthority* pAuthority
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAuthority` | 输入 | 非空 | 待验证结构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 一致 | — |
| `false` | 不一致 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · Authority](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpAuthorityValid(&Auth) ? 1
```

### `xrtHttpAuthorityPort`

取得显式端口；省略或空端口使用调用方给出的默认值。

```c
bool xrtHttpAuthorityPort(
	const xhttpauthority* pAuthority,
	uint16 iDefaultPort,
	uint16* pPort
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAuthority` | 输入 | 非空 | authority |
| `iDefaultPort` | 输入 | — | 默认端口 |
| `pPort` | 输出 | 非空 | 接收端口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 端口已写出 | — |
| `false` | 端口越界或参数错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 端口超出 `uint16`

#### 范例

[http/host · 端口](../../examples/http/host/main.c) · 观察

```c
	) || !xrtHttpAuthorityPort(&Host, 80u, &iPort) ) {
```

### `xrtHttpTargetParse`

按方法严格解析 request-target；CONNECT 只接受带非空端口 authority，OPTIONS 星号须精确为 "*"。

```c
bool xrtHttpTargetParse(
	xstrview Method,
	xstrview Text,
	xhttptarget* pTarget
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |
| `Text` | 输入 | 借用 | target 文本 |
| `pTarget` | 输出 | 支持未对齐存储、不得覆盖输入 | 接收解析结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解析 | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 非法 target

#### 范例

[http/target · 解析](../../examples/http/target/main.c) · 观察

```c
	if ( !xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"https://example.test:8443/items?q=1"
```

### `xrtHttpTargetAuthority`

解析请求的有效 authority：absolute/CONNECT 用 target，origin/星号用 Host 字段值。

```c
bool xrtHttpTargetAuthority(
	const xhttptarget* pTarget,
	xstrview Host,
	xhttpauthority* pAuthority
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入 | 非空 | 已解析 target |
| `Host` | 输入 | 借用 | Host 字段值 |
| `pAuthority` | 输出 | 未对齐存储、不得覆盖输入 | 接收 authority |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 无可用 authority 或非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/target · authority](../../examples/http/target/main.c) · 观察

```c
	) || !xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("ignored.test"),
```


### 字段解析与写出

### `xrtHttpFieldValueValid`

判断合法连续文本是否能安全作为字段值或 reason-phrase；空视图允许为 NULL/0。

```c
bool xrtHttpFieldValueValid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 待判定文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 可安全作为字段值 | — |
| `false` | 含 CR/LF/NUL 等非法字节 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/field_tour · 字段值](../../examples/http/field_tour/main.c) · 观察

```c
		xrtHttpFieldValueValid(Fields[0].Value) ? 1
```

### `xrtHttpFieldParse`

严格解析一行不含 CRLF 的 HTTP 字段；未对齐输出在返回前一次性发布。

```c
bool xrtHttpFieldParse(xstrview Line, xhttpfield* pField);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Line` | 输入 | 借用、不含 CRLF | 字段行 |
| `pField` | 输出 | 非空、支持未对齐 | 接收 Name/Value 借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解析 | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 名称非法或缺少冒号

#### 范例

[http/base · 字段解析](../../examples/http/base/main.c) · 观察

```c
	if ( !xrtHttpFieldParse(
		XRT_STR_LITERAL("Connection: keep-alive, Upgrade"), &Field
	) ) {
```

### `xrtHttpFieldNext`

严格读取不含终止空行的字段块；游标和字段输出支持未对齐存储。

```c
xhttpnext xrtHttpFieldNext(
	xstrview Block,
	size_t* pOffset,
	xhttpfield* pField
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Block` | 输入 | 借用 | 字段块（CRLF 分隔，不含终止空行） |
| `pOffset` | 输入/输出 | 初始零 | 游标 |
| `pField` | 输出 | 非空、未对齐可用 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 取得一个字段 | — |
| `XHTTP_NEXT_END` | 块耗尽 | 不设错 |
| `XHTTP_NEXT_ERROR` | 语法错误 | 游标不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/field_tour · 块迭代](../../examples/http/field_tour/main.c) · 观察

```c
		while ( xrtHttpFieldNext(SV(sBlock), &iOffset, &Field) ==
```

### `xrtHttpFieldBlockCount`

严格统计完整字段块；空字段块成功返回零。

```c
bool xrtHttpFieldBlockCount(
	xstrview Block,
	size_t* pCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Block` | 输入 | 借用 | 字段块 |
| `pCount` | 输出 | 非空、未对齐可用 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 语法错误 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/field_tour · 块统计](../../examples/http/field_tour/main.c) · 观察

```c
	if ( !xrtHttpFieldBlockCount(SV(sBlock), &iCount) || (iCount != 3u) ) {
```

### `xrtHttpFieldWrite`

写出单个字段行及 CRLF；描述符和长度输出支持未对齐存储。

```c
bool xrtHttpFieldWrite(
	const xhttpfield* pField,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pField` | 输入 | 非空 | 字段描述符 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 实际/所需长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出（含 CRLF） | — |
| `false` | 容量不足或参数错误 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足

#### 范例

[http/field_tour · 写出](../../examples/http/field_tour/main.c) · 观察

```c
		(void)xrtHttpFieldWrite(&Fields[0], Buffer, sizeof(Buffer), &iSize);
```

### `xrtHttpFieldBlockWrite`

写出字段数组及最终空行（块终止 CRLFCRLF）。

```c
bool xrtHttpFieldBlockWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段描述符数组 |
| `iCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出全部字段及终止空行 | — |
| `false` | 容量不足或参数错误 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足

#### 范例

[http/base · 写出](../../examples/http/base/main.c) · 观察

```c
	if ( (Next != XHTTP_NEXT_END) || !xrtHttpFieldBlockWrite(
		&Field, 1, Output, sizeof(Output), &iSize
	) ) {
```


### 字段查找

### `xrtHttpFieldNameEqual`

按 ASCII 大小写不敏感规则比较字段名称。

```c
bool xrtHttpFieldNameEqual(xstrview Left, xstrview Right);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左名称 |
| `Right` | 输入 | 借用 | 右名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 相等 | — |
| `false` | 不等 | 纯比较 |

#### 错误

- 无 — 纯比较

#### 范例

[http/field_tour · 查找](../../examples/http/field_tour/main.c) · 观察

```c
		xrtHttpFieldNameEqual(SV("connection"), SV("Connection")) ? 1
```

### `xrtHttpFieldFind`

从指定位置查找字段；未找到返回 `XRT_NPOS`。

```c
size_t xrtHttpFieldFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t iStart
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组、可未对齐 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 查找名称 |
| `iStart` | 输入 | — | 起始下标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 首个命中下标 | — |
| `XRT_NPOS` | 未找到（正常结果） | 不设错 |

#### 错误

- 无 — 未找到是查询结果

#### 范例

[http/field_tour · 查找](../../examples/http/field_tour/main.c) · 观察

```c
		iFound = xrtHttpFieldFind(Fields, 3u, SV("Connection"), 0u);
```

### `xrtHttpFieldGet`

返回原数组中第一个同名字段的借用地址，未找到返回空指针。

```c
const xhttpfield* xrtHttpFieldGet(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 查找名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 借用字段地址 | — |
| `NULL` | 未找到 | 不设错 |

#### 错误

- 无 — 未找到是查询结果

#### 范例

[http/field_tour · 查找](../../examples/http/field_tour/main.c) · 观察

```c
		const xhttpfield* pGet = xrtHttpFieldGet(Fields, 3u,
```

### `xrtHttpFieldGetUnique`

返回唯一同名字段的借用地址；重复时区分命中/重复/未找到。

```c
xhttpnext xrtHttpFieldGetUnique(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	const xhttpfield** ppField
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 查找名称 |
| `ppField` | 输出 | 非空、未对齐可用 | 接收唯一命中地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 唯一命中 | — |
| `XHTTP_NEXT_END` | 未找到 | 不设错 |
| `XHTTP_NEXT_ERROR` | 同名重复（协议错误） | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 同名字段出现多次

#### 范例

[http/field_tour · 查找](../../examples/http/field_tour/main.c) · 观察

```c
			xrtHttpFieldGetUnique(Fields, 3u, SV("Accept-Encoding"),
```

### `xrtHttpFieldCount`

统计同名字段数量。

```c
size_t xrtHttpFieldCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 统计名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 同名字段数（零 = 无） | — |

#### 错误

- 无 — 纯统计

#### 范例

[http/field_tour · 查找](../../examples/http/field_tour/main.c) · 观察

```c
		xrtHttpFieldCount(Fields, 3u, SV("Connection")));
```


### 同名字段 token 游标

### `xrtHttpFieldTokenCursorInit`

初始化可重复使用的同名字段 token-list 游标。

```c
void xrtHttpFieldTokenCursorInit(
	xhttpfieldtokencursor* pCursor
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输出 | 非空 | 调用方存储 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[http/field_tour · token 游标](../../examples/http/field_tour/main.c) · 观察

```c
		xrtHttpFieldTokenCursorInit(&Cursor);
```

### `xrtHttpFieldTokenNext`

跨重复同名字段读取 token-list 条目，保持字段与条目线路顺序；首次发布前完整验证全部同名字段。

```c
xhttpnext xrtHttpFieldTokenNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpfieldtokencursor* pCursor,
	xstrview* pToken
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 目标字段名 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pToken` | 输出 | 非空 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态；输入在游标结束前必须不变 | 错误时 `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 任一同名字段值非法

#### 范例

[http/field_tour · token 游标](../../examples/http/field_tour/main.c) · 观察

```c
		while ( xrtHttpFieldTokenNext(Dup, 2u, SV("Accept-Encoding"),
```

### `xrtHttpFieldTokenCount`

完整验证并统计全部重复同名字段中的非空 token 条目。

```c
bool xrtHttpFieldTokenCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t* pTokenCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 目标字段名 |
| `pTokenCount` | 输出 | 非空 | 接收合计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合计已写出 | — |
| `false` | 任一字段非法 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/field_tour · token 游标](../../examples/http/field_tour/main.c) · 观察

```c
			(void)xrtHttpFieldTokenCount(Dup, 2u,
```

### `xrtHttpFieldTokenFind`

完整验证并在重复同名字段中查找 token；返回值区分找到/未找到/错误。

```c
xhttpnext xrtHttpFieldTokenFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xstrview Token
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 目标字段名 |
| `Token` | 输入 | 借用 | 查找 token |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 找到/未找到（不设错）/字段非法 | — |

#### 错误

- `xrt.http` 域错误 — 字段值非法

#### 范例

[http/field_tour · token 游标](../../examples/http/field_tour/main.c) · 观察

```c
			xrtHttpFieldTokenFind(Dup, 2u, SV("Accept-Encoding"),
```


### Connection 选项

### `xrtHttpConnectionCursorInit`

初始化重复 Connection 字段选项游标（复用通用 `xhttpfieldtokencursor`）。

```c
void xrtHttpConnectionCursorInit(
	xhttpfieldtokencursor* pCursor
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输出 | 非空 | 调用方存储 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[http/connection_cursor · 迭代](../../examples/http/connection_cursor/main.c) · 观察

```c
	xrtHttpConnectionCursorInit(&Cursor);
```

### `xrtHttpConnectionNext`

跨重复 Connection 字段行按线路顺序迭代连接选项；首次发布前完整验证所有字段，选项借用原字段值。

```c
xhttpnext xrtHttpConnectionNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpfieldtokencursor* pCursor,
	xstrview* pOption
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pCursor` | 输入/输出 | 已初始化 | 游标（输入在结束前不变） |
| `pOption` | 输出 | 非空 | 接收选项（借用字段值） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 发布一个选项 | — |
| `XHTTP_NEXT_END` | 正常结束并清空输出 | 不设错 |
| `XHTTP_NEXT_ERROR` | 参数/游标/字段语法错误 | 游标不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `xrt.http` 域错误 — Connection 字段值语法非法

#### 范例

[http/connection_cursor · 迭代](../../examples/http/connection_cursor/main.c) · 观察

```c
	while ( xrtHttpConnectionNext(Fields, 2u, &Cursor, &Option) ==
		XHTTP_NEXT_ITEM ) {
```

### `xrtHttpConnectionCount`

完整验证并统计全部重复 Connection 字段中的非空选项。

```c
bool xrtHttpConnectionCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pOptionCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pOptionCount` | 输出 | 非空、可未对齐 | 接收合计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合计已写出 | — |
| `false` | 任一字段非法 | 计数不变 |

#### 错误

- `xrt.http` 域错误 — Connection 字段值语法非法

#### 范例

[http/connection · 统计](../../examples/http/connection/main.c) · 观察

```c
	if ( !xrtHttpConnectionCount(
		Fields, 2u, &iCount
	) || (iCount != 2u) ||
```

### `xrtHttpConnectionFind`

在全部重复 Connection 字段中查找大小写不敏感的连接选项；先验证所有值再返回。

```c
xhttpnext xrtHttpConnectionFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Option
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Option` | 输入 | 借用 | 查找选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 找到 | — |
| `XHTTP_NEXT_END` | 未找到（不设错） | — |
| `XHTTP_NEXT_ERROR` | 字段值非法 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — Connection 字段值语法非法

#### 范例

[http/connection · 查找](../../examples/http/connection/main.c) · 观察

```c
	xrtHttpConnectionFind(
		Fields, 2u, XRT_STR_LITERAL("te")
	) != XHTTP_NEXT_ITEM ) {
```

### `xrtHttpConnectionPersistence`

按 RFC 9112 判断 HTTP/1 连接能否在当前响应后继续复用。HTTP/1.0 只有显式允许、含 keep-alive 且满足代理方向限制才持久。

```c
xhttpconnectionstatus xrtHttpConnectionPersistence(
	xhttpversion Version,
	const xhttpfield* pFields,
	size_t iCount,
	uint32 iFlags
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | HTTP 版本 | 1.0 或 1.1 |
| `pFields` | 输入 | 借用数组 | Connection 字段 |
| `iCount` | 输入 | — | 条目数 |
| `iFlags` | 输入 | `XHTTP_CONNECTION_*` | RESPONSE/PROXY/ALLOW_HTTP10_KEEP_ALIVE |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_CONNECTION_PERSIST` | 可复用（close 不存在且版本策略满足） | — |
| `XHTTP_CONNECTION_CLOSE` | `close` 存在或版本默认关闭 | — |
| `XHTTP_CONNECTION_ERROR` | 非法版本/标志/字段 | `XERR_ARGUMENT` 或 `xrt.http` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `xrt.http` 域错误 — Connection 字段值语法非法
- 注：只判断版本与字段，不替代分帧完整性检查

#### 范例

[http/connection · 持久性](../../examples/http/connection/main.c) · 观察

```c
	Status = xrtHttpConnectionPersistence(
		XHTTP_VERSION_1_1, Fields, 2u, 0
	);
```


### quoted-string

### `xrtHttpQuotedValid`

判断文本是否是一段完整、合法的 HTTP quoted-string。

```c
bool xrtHttpQuotedValid(xstrview Quoted);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Quoted` | 输入 | 借用 | 含首尾引号的文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/param_tour · quoted](../../examples/http/param_tour/main.c) · 观察

```c
	(void)xrtHttpQuotedValid(SV("\"part;42\""));
```

### `xrtHttpQuotedRead`

解码完整 quoted-string；空输出查询长度且不附加零字符。

```c
bool xrtHttpQuotedRead(
	xstrview Quoted,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Quoted` | 输入 | 借用 | quoted-string |
| `pOutput` | 输出 | 可空 | 解码输出 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 解码长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解码（删除转义） | — |
| `false` | 非法或容量不足 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 非法 quoted-string
- `XERR_RANGE` — 容量不足

#### 范例

[http/param_tour · quoted](../../examples/http/param_tour/main.c) · 观察

```c
	if ( xrtHttpQuotedRead(SV("\"part;42\""), Buffer, sizeof(Buffer) - 1u,
```

### `xrtHttpQuotedWrite`

写出带引号和必要转义的 quoted-string；不附加零字符。

```c
bool xrtHttpQuotedWrite(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 未转义语义值 |
| `pOutput` | 输出 | 可空 | 编码输出 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 编码长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已编码 | — |
| `false` | 容量不足或参数错误 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足

#### 范例

[http/param_tour · quoted](../../examples/http/param_tour/main.c) · 观察

```c
	(void)xrtHttpQuotedWrite(SV("a\"b"), Buffer, sizeof(Buffer), &iSize);
```

### `xrtHttpQuotedBuild`

构建零结尾 quoted-string；返回值由 `xrtFree` 释放。

```c
str xrtHttpQuotedBuild(
	xstrview Value,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 语义值 |
| `pSize` | 输出 | 可空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 零结尾编码结果 | — |
| `NULL` | 参数错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_MEMORY`

#### 范例

[http/param_tour · quoted](../../examples/http/param_tour/main.c) · 观察

```c
	sBuilt = xrtHttpQuotedBuild(SV("a\"b"), NULL);
```


### 参数

### `xrtHttpParamNext`

严格读取分号参数；错误不推进游标并清空结果。

```c
xhttpnext xrtHttpParamNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Parameters` | 输入 | 借用 | 参数串 |
| `pOffset` | 输入/输出 | 初始零 | 游标 |
| `pParam` | 输出 | 非空、未对齐可用 | 接收 Name/Value/Flags |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态；错误时游标与输出不变 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/param · 迭代](../../examples/http/param/main.c) · 观察

```c
	while ( (Next = xrtHttpParamNext(
		Text, &iOffset, &Param
	)) == XHTTP_NEXT_ITEM ) {
```

### `xrtHttpParamCount`

严格统计完整参数列表；空列表或失败分别发布零。

```c
bool xrtHttpParamCount(
	xstrview Parameters,
	size_t* pCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Parameters` | 输入 | 借用 | 参数串 |
| `pCount` | 输出 | 非空、未对齐可用 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 语法错误 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/param_tour · 统计](../../examples/http/param_tour/main.c) · 观察

```c
	if ( !xrtHttpParamCount(SV(sParams), &iCount) || (iCount != 2u) ) {
```

### `xrtHttpParamFind`

严格查找参数并验证全部后缀；未命中或错误时清空结果。

```c
xhttpnext xrtHttpParamFind(
	xstrview Parameters,
	xstrview Name,
	xhttpparam* pParam
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Parameters` | 输入 | 借用 | 参数串 |
| `Name` | 输入 | 借用 | 查找名称 |
| `pParam` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 命中 | — |
| `XHTTP_NEXT_END` | 未命中（不设错） | — |
| `XHTTP_NEXT_ERROR` | 语法错误 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/param_tour · 查找](../../examples/http/param_tour/main.c) · 观察

```c
	if ( xrtHttpParamFind(SV(sParams), SV("boundary"), &Param) ==
```

### `xrtHttpParamTokenValid`

判断参数是否带值且解码语义值为非空 token；不修改线程错误。

```c
bool xrtHttpParamTokenValid(const xhttpparam* pParam);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pParam` | 输入 | 非空、可未对齐 | 参数描述符 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 值是非空 token | — |
| `false` | 无值/非 token 语义 | 纯谓词 |

#### 错误

- 无 — 纯谓词（协议参数要求 token 语义时使用）

#### 范例

[http/param_tour · token 语义](../../examples/http/param_tour/main.c) · 观察

```c
		xrtHttpParamTokenValid(&TokenParam) ? 1
```

### `xrtHttpParamTokenEqual`

按 ASCII 大小写不敏感规则比较参数的解码 token 值；纯谓词。

```c
bool xrtHttpParamTokenEqual(
	const xhttpparam* pParam,
	xstrview Token
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pParam` | 输入 | 非空 | 参数描述符 |
| `Token` | 输入 | 借用 | 比较目标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 相等 | — |
| `false` | 不等 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/param_tour · token 语义](../../examples/http/param_tour/main.c) · 观察

```c
		xrtHttpParamTokenEqual(&TokenParam, SV("utf-8")) ? 1
```

### `xrtHttpParamValueCursorInit`

初始化参数值逐字节游标。

```c
void xrtHttpParamValueCursorInit(
	xhttpparamvaluecursor* pCursor
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输出 | 非空 | 调用方存储 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[http/param_tour · 值游标](../../examples/http/param_tour/main.c) · 观察

```c
		xrtHttpParamValueCursorInit(&Cursor);
```

### `xrtHttpParamValueNext`

逐字节读取参数的解码语义值；首次调用完整验证并绑定参数；输入在迭代结束前必须不变。

```c
xhttpnext xrtHttpParamValueNext(
	const xhttpparam* pParam,
	xhttpparamvaluecursor* pCursor,
	uint8* pByte
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pParam` | 输入 | 非空 | 参数描述符 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pByte` | 输出 | 非空 | 接收一个解码字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 值非法

#### 范例

[http/param_tour · 值游标](../../examples/http/param_tour/main.c) · 观察

```c
		while ( xrtHttpParamValueNext(&Param, &Cursor, &iByte) ==
```

### `xrtHttpParamValueWrite`

解码参数值；token 复制，quoted-string 删除转义。

```c
bool xrtHttpParamValueWrite(
	const xhttpparam* pParam,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pParam` | 输入 | 非空、可未对齐 | 参数描述符 |
| `pOutput` | 输出 | 可空 | 解码输出 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 解码长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解码 | — |
| `false` | 容量不足或值非法 | 输出不变 |

#### 错误

- `XERR_RANGE` — 容量不足
- `xrt.http` 域错误 — quoted-string 非法

#### 范例

[http/param · 解码](../../examples/http/param/main.c) · 观察

```c
		if ( !xrtHttpParamValueWrite(
			&Param, Value, sizeof(Value), &iSize
		) ) {
```

### `xrtHttpParamWrite`

写出单个参数；QUOTED 转义正文，NONE 省略等号和值。

```c
bool xrtHttpParamWrite(
	xstrview Name,
	xstrview Value,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 参数名 |
| `Value` | 输入 | 借用 | 参数值 |
| `iFlags` | 输入 | `XHTTP_PARAM_*` | HAS_VALUE/QUOTED/NONE |
| `pOutput` | 输出 | 可空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 容量不足或参数错误 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足

#### 范例

[http/param_tour · 写出](../../examples/http/param_tour/main.c) · 观察

```c
	if ( xrtHttpParamWrite(SV("charset"), SV("UTF-8"), XHTTP_PARAM_HAS_VALUE, Buffer,
```

### `xrtHttpParamBuild`

构建零结尾参数文本；返回值由 `xrtFree` 释放。

```c
str xrtHttpParamBuild(
	xstrview Name,
	xstrview Value,
	uint32 iFlags,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 参数名 |
| `Value` | 输入 | 借用 | 参数值 |
| `iFlags` | 输入 | — | 形态标志 |
| `pSize` | 输出 | 可空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 零结尾结果 | — |
| `NULL` | 参数错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_MEMORY`

#### 范例

[http/param_tour · 构建](../../examples/http/param_tour/main.c) · 观察

```c
	sBuilt = xrtHttpParamBuild(SV("charset"), SV("UTF-8"), XHTTP_PARAM_HAS_VALUE, NULL);
```

### `xrtHttpParamHostValid`

判断参数解码值是否为合法 Host（用于 URI 解析层的 host 参数）。

```c
bool xrtHttpParamHostValid(const xhttpparam* pParam);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pParam` | 输入 | 非空 | 参数描述符 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 值是合法 Host | — |
| `false` | 无值或非法 Host | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/param_tour · host 参数](../../examples/http/param_tour/main.c) · 观察

```c
		xrtHttpParamHostValid(&HostParam) ? 1
```


### 指令

### `xrtHttpDirectiveNext`

读取逗号分隔 name[=value] 指令的下一项；空列表项被忽略，值可为 token 或 quoted-string。

```c
xhttpnext xrtHttpDirectiveNext(
	xstrview Directives,
	size_t* pOffset,
	xhttpparam* pDirective
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Directives` | 输入 | 借用 | 指令列表 |
| `pOffset` | 输入/输出 | 初始零 | 游标 |
| `pDirective` | 输出 | 非空、未对齐可用 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/param_tour · 指令](../../examples/http/param_tour/main.c) · 观察

```c
		while ( xrtHttpDirectiveNext(SV(sDirectives), &iOffset,
```

### `xrtHttpDirectiveCount`

严格统计完整指令列表；空项不计数，失败发布零。

```c
bool xrtHttpDirectiveCount(
	xstrview Directives,
	size_t* pCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Directives` | 输入 | 借用 | 指令列表 |
| `pCount` | 输出 | 非空 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 语法错误 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/param_tour · 指令](../../examples/http/param_tour/main.c) · 观察

```c
		(void)xrtHttpDirectiveCount(SV(sDirectives), &iCount);
```

### `xrtHttpDirectiveFind`

查找首个指令并验证全部后缀；未命中或错误时清空。

```c
xhttpnext xrtHttpDirectiveFind(
	xstrview Directives,
	xstrview Name,
	xhttpparam* pDirective
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Directives` | 输入 | 借用 | 指令列表 |
| `Name` | 输入 | 借用 | 查找名称 |
| `pDirective` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 命中/未命中（不设错）/错误 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/param_tour · 指令](../../examples/http/param_tour/main.c) · 观察

```c
			xrtHttpDirectiveFind(SV(sDirectives), SV("no-store"),
```

## 模块契约：错误

本文件各节复用自 [http.md](http.md) 与 [http_connection.md](http_connection.md)；失败经 `xrtGetError()` 报告：

| 域/种类 | 触发场景 |
|---|---|
| `XERR_ARGUMENT` / `XERR_STATE` / `XERR_RANGE` | 参数、字段表状态与游标范围 |
| `XERR_MEMORY` | 字段数组扩容失败 |
| `xrt.http` 域错误 | 名称、值与连接语义错误 |
