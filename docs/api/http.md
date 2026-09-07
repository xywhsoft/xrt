# HTTP 协议底座

XRT 的 HTTP 模块是可直接组合 TCP、TLS 和自定义传输的 HTTP/1 线协议底座，
不提供客户端对象、服务器对象、路由、中间件或请求/响应拥有型模型。

核心目标是：严格解析不可信输入、完整表达 HTTP/1.0 和 HTTP/1.1 分帧、允许
调用方直接发送原始报文，并保证常用路径不分配正文缓冲。

完整函数、常量和类型索引见 [HTTP 公共符号参考](http-reference.md)。精确参数、所有权和
失败契约以对应公共头的中文注释为准。

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
| `XHTTP_METHOD_OTHER` | 扩展/未知方法 |
| `XHTTP_METHOD_GET` | GET 方法 |
| `XHTTP_METHOD_HEAD` | HEAD 方法 |
| `XHTTP_METHOD_POST` | POST 方法 |
| `XHTTP_METHOD_PUT` | PUT 方法 |
| `XHTTP_METHOD_DELETE` | DELETE 方法 |
| `XHTTP_METHOD_CONNECT` | CONNECT 方法 |
| `XHTTP_METHOD_OPTIONS` | OPTIONS 方法 |
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
| `XHTTP_VERSION_1_0` | HTTP/1.0 |

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
| `XHTTP_STATUS_CONTINUE` | CONTINUE（100 继续） |
| `XHTTP_STATUS_SWITCHING_PROTOCOLS` | 101 协议切换 |
| `XHTTP_STATUS_PROCESSING` | 102 处理中 |
| `XHTTP_STATUS_EARLY_HINTS` | 103 早期提示 |
| `XHTTP_STATUS_OK` | 成功 |
| `XHTTP_STATUS_CREATED` | 201 已创建 |
| `XHTTP_STATUS_ACCEPTED` | 202 已接受 |
| `XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION` | 203 非权威信息 |
| `XHTTP_STATUS_NO_CONTENT` | 204 无内容 |
| `XHTTP_STATUS_RESET_CONTENT` | 205 重置内容 |
| `XHTTP_STATUS_PARTIAL_CONTENT` | 206 部分内容 |
| `XHTTP_STATUS_MULTI_STATUS` | 207 多状态 |
| `XHTTP_STATUS_ALREADY_REPORTED` | 208 已报告 |
| `XHTTP_STATUS_IM_USED` | 226 IM 已使用 |
| `XHTTP_STATUS_MULTIPLE_CHOICES` | 300 多选项 |
| `XHTTP_STATUS_MOVED_PERMANENTLY` | 301 永久移动 |
| `XHTTP_STATUS_FOUND` | 302 找到（临时移动） |
| `XHTTP_STATUS_SEE_OTHER` | 303 见其他 |
| `XHTTP_STATUS_NOT_MODIFIED` | 304 未修改 |
| `XHTTP_STATUS_USE_PROXY` | 305 使用代理 |
| `XHTTP_STATUS_TEMPORARY_REDIRECT` | 307 临时重定向 |
| `XHTTP_STATUS_PERMANENT_REDIRECT` | 308 永久重定向 |
| `XHTTP_STATUS_BAD_REQUEST` | 400 错误请求 |
| `XHTTP_STATUS_UNAUTHORIZED` | 401 未认证 |
| `XHTTP_STATUS_PAYMENT_REQUIRED` | 402 需要付费 |
| `XHTTP_STATUS_FORBIDDEN` | 403 禁止 |
| `XHTTP_STATUS_NOT_FOUND` | 未找到 |
| `XHTTP_STATUS_METHOD_NOT_ALLOWED` | 405 方法不允许 |
| `XHTTP_STATUS_NOT_ACCEPTABLE` | 406 不可接受 |
| `XHTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED` | 407 需代理认证 |
| `XHTTP_STATUS_REQUEST_TIMEOUT` | REQUEST超时 |
| `XHTTP_STATUS_CONFLICT` | 冲突 |
| `XHTTP_STATUS_GONE` | 410 已消失 |
| `XHTTP_STATUS_LENGTH_REQUIRED` | 411 需要 Content-Length |
| `XHTTP_STATUS_PRECONDITION_FAILED` | PRECONDITION已失败 |
| `XHTTP_STATUS_CONTENT_TOO_LARGE` | 413 内容过大 |
| `XHTTP_STATUS_URI_TOO_LONG` | 414 URI 过长 |
| `XHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE` | 不支持MEDIA类型 |
| `XHTTP_STATUS_RANGE_NOT_SATISFIABLE` | 范围越界NOTSATISFIABLE |
| `XHTTP_STATUS_EXPECTATION_FAILED` | EXPECTATION已失败 |
| `XHTTP_STATUS_MISDIRECTED_REQUEST` | 421 请求被误送 |
| `XHTTP_STATUS_UNPROCESSABLE_CONTENT` | 422 无法处理 |
| `XHTTP_STATUS_LOCKED` | 423 已锁定 |
| `XHTTP_STATUS_FAILED_DEPENDENCY` | 已失败DEPENDENCY |
| `XHTTP_STATUS_TOO_EARLY` | 425 过早 |
| `XHTTP_STATUS_UPGRADE_REQUIRED` | 426 需要升级 |
| `XHTTP_STATUS_PRECONDITION_REQUIRED` | 428 需要前提条件 |
| `XHTTP_STATUS_TOO_MANY_REQUESTS` | 429 请求过多 |
| `XHTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE` | REQUESTHEADER字段TOOLARGE |
| `XHTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS` | 451 因法律原因不可用 |
| `XHTTP_STATUS_INTERNAL_SERVER_ERROR` | 内部错误服务端角色失败 |
| `XHTTP_STATUS_NOT_IMPLEMENTED` | 501 未实现 |
| `XHTTP_STATUS_BAD_GATEWAY` | 502 网关错误 |
| `XHTTP_STATUS_SERVICE_UNAVAILABLE` | 503 服务不可用 |
| `XHTTP_STATUS_GATEWAY_TIMEOUT` | GATEWAY超时 |
| `XHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED` | 505 版本不支持 |
| `XHTTP_STATUS_VARIANT_ALSO_NEGOTIATES` | 506 变体协商错误 |
| `XHTTP_STATUS_INSUFFICIENT_STORAGE` | 507 存储不足 |
| `XHTTP_STATUS_LOOP_DETECTED` | 508 检测到循环 |
| `XHTTP_STATUS_NOT_EXTENDED` | 510 未扩展 |

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
| `Name` | `xstrview` | 名称 |
| `Value` | `xstrview` | 值 |

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
| `XHTTP_NEXT_END` | 遍历结束 |

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
| `Source` | `const void*` | 源视图 |
| `Name` | `xstrview` | 名称 |
| `Count` | `size_t` | 数量 |
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |
| `Required` | `uint8` | 是否必需 |

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
| `Flags` | `uint32` | 标志位 |
| `Port` | `uint16` | 端口 |
| `Text` | `xstrview` | 文本视图 |
| `Host` | `xstrview` | 主机名 |
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
| `XHTTP_TARGET_ORIGIN` | origin-form（最常见） |
| `XHTTP_TARGET_ABSOLUTE` | absolute-form（代理） |
| `XHTTP_TARGET_AUTHORITY` | authority-form（CONNECT） |

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
| `Flags` | `uint32` | 标志位 |
| `Method` | `xstrview` | 方法 |
| `Text` | `xstrview` | 文本视图 |
| `Scheme` | `xstrview` | 协议方案 |
| `Authority` | `xstrview` | Authority |
| `Path` | `xstrview` | 路径 |
| `Query` | `xstrview` | 查询串 |
| `Host` | `xhttpauthority` | 主机名 |

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
| `Name` | `xstrview` | 名称 |
| `Value` | `xstrview` | 值 |
| `Flags` | `uint32` | 标志位 |

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
| `Source` | `const void*` | 源视图 |
| `Value` | `const void*` | 值 |
| `ValueSize` | `size_t` | ValueSize |
| `Offset` | `size_t` | 偏移量 |
| `Flags` | `uint32` | 标志位 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttp1status`

HTTP/1 解析返回值区分数据不足、字段描述符不足和真正的协议错误。

```c
typedef enum xhttp1status {
	XHTTP1_ERROR = -1,
	XHTTP1_MORE = 0,
	XHTTP1_READY = 1,
	XHTTP1_FIELDS = 2
} xhttp1status;
```

| 值 | 语义 |
|---|---|
| `XHTTP1_ERROR` | 失败 |
| `XHTTP1_MORE` | 需要更多输入 |
| `XHTTP1_READY` | 就绪 |

### `xhttpkind`

起始行决定消息方向，调用方不需要依赖启发式自动识别。

```c
typedef enum xhttpkind {
	XHTTP_REQUEST = 1,
	XHTTP_RESPONSE
} xhttpkind;
```

| 值 | 语义 |
|---|---|
| `XHTTP_REQUEST` | 请求方向 |

### `xhttp1flag`

Header 只描述线上的显式语义，完整消息体计划由上层结合请求方法计算。

```c
typedef enum xhttp1flag {
	XHTTP1_KEEP_ALIVE = UINT32_C(0x00000001),
	XHTTP1_CONNECTION_CLOSE = UINT32_C(0x00000002),
	XHTTP1_UPGRADE = UINT32_C(0x00000004),
	XHTTP1_CONTENT_LENGTH = UINT32_C(0x00000008),
	XHTTP1_CHUNKED = UINT32_C(0x00000010),
	XHTTP1_TRANSFER_ENCODING = UINT32_C(0x00000020),
	XHTTP1_TRANSFER_OTHER = UINT32_C(0x00000040)
} xhttp1flag;
```

| 值 | 语义 |
|---|---|
| `XHTTP1_KEEP_ALIVE` | keep-alive |
| `XHTTP1_CONNECTION_CLOSE` | close |
| `XHTTP1_UPGRADE` | Upgrade |
| `XHTTP1_CONTENT_LENGTH` | Content-Length |
| `XHTTP1_CHUNKED` | chunked |
| `XHTTP1_TRANSFER_ENCODING` | TRANSFERENCODING |

### `xhttp1error`

HTTP/1 解析与封包使用稳定错误码，Offset 和 Line 提供精确协议位置。

```c
typedef enum xhttp1error {
	XHTTP1_ERROR_ARGUMENT = 1,
	XHTTP1_ERROR_HEAD_INCOMPLETE,
	XHTTP1_ERROR_HEAD_TOO_LARGE,
	XHTTP1_ERROR_START_LINE_TOO_LARGE,
	XHTTP1_ERROR_FIELD_LINE_TOO_LARGE,
	XHTTP1_ERROR_TOO_MANY_FIELDS,
	XHTTP1_ERROR_LINE_END,
	XHTTP1_ERROR_START_LINE,
	XHTTP1_ERROR_METHOD,
	XHTTP1_ERROR_TARGET,
	XHTTP1_ERROR_VERSION,
	XHTTP1_ERROR_STATUS,
	XHTTP1_ERROR_REASON,
	XHTTP1_ERROR_FIELD_NAME,
	XHTTP1_ERROR_FIELD_VALUE,
	XHTTP1_ERROR_CONTENT_LENGTH,
	XHTTP1_ERROR_CONFLICTING_CONTENT_LENGTH,
	XHTTP1_ERROR_TRANSFER_LENGTH,
	XHTTP1_ERROR_TRANSFER_ENCODING,
	XHTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING,
	XHTTP1_ERROR_CONNECTION,
	XHTTP1_ERROR_OUTPUT_SIZE,
	XHTTP1_ERROR_REQUEST_TRANSFER_ENCODING,
	XHTTP1_ERROR_BODY_TOO_LARGE,
	XHTTP1_ERROR_BODY_INCOMPLETE,
	XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE,
	XHTTP1_ERROR_CHUNK_SIZE,
	XHTTP1_ERROR_CHUNK_EXTENSION,
	XHTTP1_ERROR_CHUNK_TERMINATOR,
	XHTTP1_ERROR_TRAILER_TOO_LARGE,
	XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE,
	XHTTP1_ERROR_TOO_MANY_TRAILERS,
	XHTTP1_ERROR_FORBIDDEN_TRAILER,
	XHTTP1_ERROR_UPGRADE
} xhttp1error;
```

| 值 | 语义 |
|---|---|
| `XHTTP1_ERROR_ARGUMENT` | 参数非法 |
| `XHTTP1_ERROR_HEAD_INCOMPLETE` | HeadIncomplete失败 |
| `XHTTP1_ERROR_HEAD_TOO_LARGE` | HeadTooLarge失败 |
| `XHTTP1_ERROR_START_LINE_TOO_LARGE` | 失败 |
| `XHTTP1_ERROR_FIELD_LINE_TOO_LARGE` | 失败 |
| `XHTTP1_ERROR_TOO_MANY_FIELDS` | TOOMANY字段 |
| `XHTTP1_ERROR_LINE_END` | 失败 |
| `XHTTP1_ERROR_START_LINE` | 失败 |
| `XHTTP1_ERROR_METHOD` | 失败 |
| `XHTTP1_ERROR_TARGET` | 失败 |
| `XHTTP1_ERROR_VERSION` | 失败 |
| `XHTTP1_ERROR_STATUS` | 失败 |
| `XHTTP1_ERROR_REASON` | 失败 |
| `XHTTP1_ERROR_FIELD_NAME` | FIELD名称 |
| `XHTTP1_ERROR_FIELD_VALUE` | FIELD值非法 |
| `XHTTP1_ERROR_CONTENT_LENGTH` | 失败 |
| `XHTTP1_ERROR_CONFLICTING_CONTENT_LENGTH` | 失败 |
| `XHTTP1_ERROR_TRANSFER_LENGTH` | 失败 |
| `XHTTP1_ERROR_TRANSFER_ENCODING` | 失败 |
| `XHTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING` | 不支持TRANSFERENCODING |
| `XHTTP1_ERROR_CONNECTION` | 失败 |
| `XHTTP1_ERROR_OUTPUT_SIZE` | 输出失败尺寸 |
| `XHTTP1_ERROR_REQUEST_TRANSFER_ENCODING` | 失败 |
| `XHTTP1_ERROR_BODY_TOO_LARGE` | 失败 |
| `XHTTP1_ERROR_BODY_INCOMPLETE` | 失败 |
| `XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE` | 失败 |
| `XHTTP1_ERROR_CHUNK_SIZE` | CHUNK尺寸 |
| `XHTTP1_ERROR_CHUNK_EXTENSION` | 失败 |
| `XHTTP1_ERROR_CHUNK_TERMINATOR` | 失败 |
| `XHTTP1_ERROR_TRAILER_TOO_LARGE` | 失败 |
| `XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE` | 失败 |
| `XHTTP1_ERROR_TOO_MANY_TRAILERS` | 失败 |
| `XHTTP1_ERROR_FORBIDDEN_TRAILER` | 失败 |

### `xhttp1limits`

默认限额面向公网协议输入，调用方可以按服务端路由或客户端策略收紧。

```c
typedef struct xhttp1limits {
	size_t MaxHead;
	size_t MaxStartLine;
	size_t MaxFieldLine;
	size_t MaxFields;
} xhttp1limits;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `MaxHead` | `size_t` | MaxHead |
| `MaxStartLine` | `size_t` | MaxStartLine |
| `MaxFieldLine` | `size_t` | MaxFieldLine |
| `MaxFields` | `size_t` | MaxFields |

### `xhttp1errorinfo`

解析错误位置从消息首字节开始计数，Line 从一开始计数。

```c
typedef struct xhttp1errorinfo {
	xhttp1error Code;
	size_t Offset;
	size_t Line;
} xhttp1errorinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Code` | `xhttp1error` | 错误码 |
| `Offset` | `size_t` | 偏移量 |
| `Line` | `size_t` | 行号 |

### `xhttp1transfercoding`

Transfer Coding 名称和原样参数都借用字段值，Parameters 不含首个分号。

```c
typedef struct xhttp1transfercoding {
	xstrview Name;
	xstrview Parameters;
} xhttp1transfercoding;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | 名称 |
| `Parameters` | `xstrview` | Parameters |

### `xhttp1head`

Head 只借用输入和字段数组；输入与数组必须覆盖 Head 的使用期。 FIELDS 状态下 FieldCount 是需要的描述符数量，其余字段已经可读取。

```c
typedef struct xhttp1head {
	xhttpkind Kind;
	xhttpversion Version;
	uint32 Flags;
	uint16 Status;
	uint64 ContentLength;
	size_t Bytes;
	xstrview Method;
	xstrview Target;
	xstrview Reason;
	xhttpfield* Fields;
	size_t FieldCount;
	size_t FieldCapacity;
	xhttpmethod MethodCode;
} xhttp1head;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Kind` | `xhttpkind` | 错误种类 |
| `Version` | `xhttpversion` | 结构版本 |
| `Flags` | `uint32` | 标志位 |
| `Status` | `uint16` | 状态输出 |
| `ContentLength` | `uint64` | ContentLength |
| `Bytes` | `size_t` | Bytes |
| `Method` | `xstrview` | 方法 |
| `Target` | `xstrview` | 目标视图 |
| `Reason` | `xstrview` | 原因文本 |
| `Fields` | `xhttpfield*` | Fields |
| `FieldCount` | `size_t` | FieldCount |
| `FieldCapacity` | `size_t` | FieldCapacity |
| `MethodCode` | `xhttpmethod` | MethodCode |

### `xhttp1bodymode`

Body Plan 明确区分无正文、定长、分块、关闭定界和升级后的非 HTTP 字节。

```c
typedef enum xhttp1bodymode {
	XHTTP1_BODY_NONE = 0,
	XHTTP1_BODY_FIXED,
	XHTTP1_BODY_CHUNKED,
	XHTTP1_BODY_CLOSE,
	XHTTP1_BODY_TUNNEL
} xhttp1bodymode;
```

| 值 | 语义 |
|---|---|
| `XHTTP1_BODY_NONE` | 无 |
| `XHTTP1_BODY_FIXED` | FIXED |
| `XHTTP1_BODY_CHUNKED` | CHUNKED |
| `XHTTP1_BODY_CLOSE` | CLOSE |

### `xhttp1bodystatus`

Body Reader 每次只发布一个借用数据片段或一个终态。

```c
typedef enum xhttp1bodystatus {
	XHTTP1_BODY_ERROR = -1,
	XHTTP1_BODY_MORE = 0,
	XHTTP1_BODY_DATA = 1,
	XHTTP1_BODY_DONE = 2,
	XHTTP1_BODY_FIELDS = 3
} xhttp1bodystatus;
```

| 值 | 语义 |
|---|---|
| `XHTTP1_BODY_ERROR` | 失败 |
| `XHTTP1_BODY_MORE` | 需要更多输入 |
| `XHTTP1_BODY_DATA` | 数据损坏 |
| `XHTTP1_BODY_DONE` | 完成 |

### `xhttp1bodyplan`

Body Plan 是 Header 事实结合请求方法和响应状态后的唯一分帧结论。

```c
typedef struct xhttp1bodyplan {
	xhttp1bodymode Mode;
	uint64 Length;
} xhttp1bodyplan;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Mode` | `xhttp1bodymode` | 模式 |
| `Length` | `uint64` | 长度 |

### `xhttp1bodylimits`

流式正文不预分配内存；限额约束累计正文、chunk 行和 trailer 区。

```c
typedef struct xhttp1bodylimits {
	uint64 MaxBody;
	size_t MaxChunkLine;
	size_t MaxTrailer;
	size_t MaxTrailerLine;
	size_t MaxTrailers;
} xhttp1bodylimits;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `MaxBody` | `uint64` | MaxBody |
| `MaxChunkLine` | `size_t` | MaxChunkLine |
| `MaxTrailer` | `size_t` | MaxTrailer |
| `MaxTrailerLine` | `size_t` | MaxTrailerLine |
| `MaxTrailers` | `size_t` | MaxTrailers |

### `xhttp1body`

Body Reader 由调用方持有且不分配内存；Trailers 借用完成调用中的输入。 公开计数可用于进度与诊断，其余状态只能由本模块推进。

```c
typedef struct xhttp1body {
	xhttp1bodymode Mode;
	uint64 Remaining;
	uint64 Received;
	uint64 WireBytes;
	xhttpfield* Trailers;
	size_t TrailerCount;
	size_t TrailerCapacity;
	xhttp1bodylimits Limits;
	uint64 ChunkSize;
	size_t ChunkLineBytes;
	uint32 State;
} xhttp1body;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Mode` | `xhttp1bodymode` | 模式 |
| `Remaining` | `uint64` | Remaining |
| `Received` | `uint64` | Received |
| `WireBytes` | `uint64` | WireBytes |
| `Trailers` | `xhttpfield*` | Trailers |
| `TrailerCount` | `size_t` | TrailerCount |
| `TrailerCapacity` | `size_t` | TrailerCapacity |
| `Limits` | `xhttp1bodylimits` | Limits |
| `ChunkSize` | `uint64` | ChunkSize |
| `ChunkLineBytes` | `size_t` | ChunkLineBytes |
| `State` | `uint32` | 状态 |

### `xhttp1message`

完整消息便利层借用连续输入、Header 和 trailer 描述符，不持有堆内存。 Wire 只覆盖第一条完整消息，BodyBytes 是移除 chunked 分帧后的正文长度。

```c
typedef struct xhttp1message {
	xhttp1head Head;
	xhttp1bodyplan Plan;
	xhttp1bodylimits Limits;
	xbytesview Wire;
	xhttpfield* Trailers;
	size_t TrailerCount;
	size_t TrailerCapacity;
	size_t BodyBytes;
} xhttp1message;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Head` | `xhttp1head` | 头指针 |
| `Plan` | `xhttp1bodyplan` | Plan |
| `Limits` | `xhttp1bodylimits` | Limits |
| `Wire` | `xbytesview` | Wire |
| `Trailers` | `xhttpfield*` | Trailers |
| `TrailerCount` | `size_t` | TrailerCount |
| `TrailerCapacity` | `size_t` | TrailerCapacity |
| `BodyBytes` | `size_t` | BodyBytes |

### `xhttpdecodemode`

解码模式明确区分无编码、成功接管的内置编码和显式允许的原样回退。

```c
typedef enum xhttpdecodemode {
	XHTTP_DECODE_IDENTITY = 0,
	XHTTP_DECODE_CONTENT,
	XHTTP_DECODE_RAW
} xhttpdecodemode;
```

| 值 | 语义 |
|---|---|
| `XHTTP_DECODE_IDENTITY` | 无变换 |
| `XHTTP_DECODE_CONTENT` | 按内容编码解码 |

### `xhttpdecodeflag`

默认拒绝未知编码；调用方可显式选择保留整个原始表示。

```c
typedef enum xhttpdecodeflag {
	XHTTP_DECODE_ALLOW_RAW = UINT32_C(0x00000001)
} xhttpdecodeflag;
```

| 值 | 语义 |
|---|---|

### `xhttpdecodeerror`

错误码覆盖配置、Content-Encoding、状态和输出边界。

```c
typedef enum xhttpdecodeerror {
	XHTTP_DECODE_ERROR_ARGUMENT = 1,
	XHTTP_DECODE_ERROR_CONFIG,
	XHTTP_DECODE_ERROR_CONTENT_ENCODING,
	XHTTP_DECODE_ERROR_UNSUPPORTED,
	XHTTP_DECODE_ERROR_STATE,
	XHTTP_DECODE_ERROR_LIMIT,
	XHTTP_DECODE_ERROR_OUTPUT
} xhttpdecodeerror;
```

| 值 | 语义 |
|---|---|
| `XHTTP_DECODE_ERROR_ARGUMENT` | 参数非法 |
| `XHTTP_DECODE_ERROR_CONFIG` | 配置非法 |
| `XHTTP_DECODE_ERROR_CONTENT_ENCODING` | 失败 |
| `XHTTP_DECODE_ERROR_UNSUPPORTED` | 不支持 |
| `XHTTP_DECODE_ERROR_STATE` | 状态非法 |
| `XHTTP_DECODE_ERROR_LIMIT` | 超限 |

### `xhttpdecodeconfig`

每个解码层和最终明文都受同一个硬限额约束。

```c
typedef struct xhttpdecodeconfig {
	uint64 OutputLimit;
	uint32 GzipHeaderLimit;
	uint32 MaxCodings;
	uint32 Flags;
} xhttpdecodeconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `OutputLimit` | `uint64` | OutputLimit |
| `GzipHeaderLimit` | `uint32` | GzipHeaderLimit |
| `MaxCodings` | `uint32` | MaxCodings |
| `Flags` | `uint32` | 标志位 |

### `xhttpdecode`

HTTP 解码器拥有并复用底层 Inflate 状态。

```c
typedef struct xhttpdecode xhttpdecode;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xhttpdecodeoutputproc`

输出视图只在回调期间有效，返回 false 会终止当前解码器。

```c
typedef bool (*xhttpdecodeoutputproc)(xbytesview Data, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xhttpcoding`

内置编码值同时可作为可用编码位掩码，NONE 表示没有可接受表示。

```c
typedef enum xhttpcoding {
	XHTTP_CODING_NONE = 0,
	XHTTP_CODING_IDENTITY = UINT32_C(0x00000001),
	XHTTP_CODING_GZIP = UINT32_C(0x00000002),
	XHTTP_CODING_DEFLATE = UINT32_C(0x00000004)
} xhttpcoding;
```

| 值 | 语义 |
|---|---|
| `XHTTP_CODING_NONE` | 无 |
| `XHTTP_CODING_IDENTITY` | 无编码 |
| `XHTTP_CODING_GZIP` | gzip 包装 |

### `xhttpacceptencodingflag`

解析标志区分 Header 缺失与各个显式编码成员。

```c
typedef enum xhttpacceptencodingflag {
	XHTTP_ACCEPT_ENCODING_NONE = 0,
	XHTTP_ACCEPT_ENCODING_PRESENT = UINT32_C(0x00000001),
	XHTTP_ACCEPT_ENCODING_GZIP = UINT32_C(0x00000002),
	XHTTP_ACCEPT_ENCODING_DEFLATE = UINT32_C(0x00000004),
	XHTTP_ACCEPT_ENCODING_IDENTITY = UINT32_C(0x00000008),
	XHTTP_ACCEPT_ENCODING_WILDCARD = UINT32_C(0x00000010)
} xhttpacceptencodingflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_ACCEPT_ENCODING_NONE` | 无 |
| `XHTTP_ACCEPT_ENCODING_PRESENT` | 请求携带 Accept-Encoding |
| `XHTTP_ACCEPT_ENCODING_GZIP` | gzip 包装 |
| `XHTTP_ACCEPT_ENCODING_DEFLATE` | deflate 包装 |
| `XHTTP_ACCEPT_ENCODING_IDENTITY` | 允许 identity |

### `xhttpacceptencoding`

质量值使用 0 到 1000 的定点表示。 同一编码重复出现时保留最高质量，Flags 记录是否显式出现。

```c
typedef struct xhttpacceptencoding {
	uint16 Gzip;
	uint16 Deflate;
	uint16 Identity;
	uint16 Wildcard;
	uint32 Flags;
} xhttpacceptencoding;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Gzip` | `uint16` | Gzip |
| `Deflate` | `uint16` | Deflate |
| `Identity` | `uint16` | Identity |
| `Wildcard` | `uint16` | Wildcard |
| `Flags` | `uint32` | 标志位 |

### `xhttpcontentencodingflag`

Content-Encoding 计划保留字段存在性、容错层和未知扩展。

```c
typedef enum xhttpcontentencodingflag {
	XHTTP_CONTENT_ENCODING_NONE = 0,
	XHTTP_CONTENT_ENCODING_PRESENT = UINT32_C(0x00000001),
	XHTTP_CONTENT_ENCODING_IDENTITY = UINT32_C(0x00000002),
	XHTTP_CONTENT_ENCODING_UNKNOWN = UINT32_C(0x00000004),
	XHTTP_CONTENT_ENCODING_LEGACY = UINT32_C(0x00000008)
} xhttpcontentencodingflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_CONTENT_ENCODING_NONE` | 无 |
| `XHTTP_CONTENT_ENCODING_PRESENT` | 响应携带 Content-Encoding |
| `XHTTP_CONTENT_ENCODING_IDENTITY` | 内容未编码 |
| `XHTTP_CONTENT_ENCODING_UNKNOWN` | 未知 |

### `xhttpcontentencodingcursor`

游标可在重复 Content-Encoding 字段之间无分配前向迭代。

```c
typedef struct xhttpcontentencodingcursor {
	size_t Field;
	size_t Offset;
} xhttpcontentencodingcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | 偏移量 |

### `xhttpcontentencodingitem`

每个成员保留原 token，并把内置编码映射到统一枚举。

```c
typedef struct xhttpcontentencodingitem {
	xstrview Token;
	xhttpcoding Coding;
} xhttpcontentencodingitem;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Token` | `xstrview` | Token |
| `Coding` | `xhttpcoding` | Coding |

### `xhttpcontentencodingplan`

计划只保存解析事实，不绑定具体解码算法或未知编码策略。

```c
typedef struct xhttpcontentencodingplan {
	size_t FieldCount;
	size_t CodingCount;
	size_t DecoderCount;
	size_t UnknownCount;
	size_t JoinedSize;
	uint32 Flags;
} xhttpcontentencodingplan;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `FieldCount` | `size_t` | FieldCount |
| `CodingCount` | `size_t` | CodingCount |
| `DecoderCount` | `size_t` | DecoderCount |
| `UnknownCount` | `size_t` | UnknownCount |
| `JoinedSize` | `size_t` | JoinedSize |
| `Flags` | `uint32` | 标志位 |

### `xhttpexpectflag`

Expectation 标志区分扩展值、quoted-string 和参数。

```c
typedef enum xhttpexpectflag {
	XHTTP_EXPECT_BARE = 0,
	XHTTP_EXPECT_HAS_VALUE = UINT32_C(0x00000001),
	XHTTP_EXPECT_VALUE_QUOTED = UINT32_C(0x00000002),
	XHTTP_EXPECT_HAS_PARAMETERS = UINT32_C(0x00000004)
} xhttpexpectflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_EXPECT_BARE` | 裸 Expect 头 |
| `XHTTP_EXPECT_HAS_VALUE` | HAS值非法 |
| `XHTTP_EXPECT_VALUE_QUOTED` | 值非法QUOTED |

### `xhttpexpectation`

Expectation 借用完整元素、名称、线路值和原始参数片段。

```c
typedef struct xhttpexpectation {
	xstrview Element;
	xstrview Name;
	xstrview Value;
	xstrview Parameters;
	uint32 Flags;
} xhttpexpectation;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Element` | `xstrview` | Element |
| `Name` | `xstrview` | 名称 |
| `Value` | `xstrview` | 值 |
| `Parameters` | `xstrview` | Parameters |
| `Flags` | `uint32` | 标志位 |

### `xhttpexpectcursor`

单字段游标由初始化函数建立，调用方不得直接修改。

```c
typedef struct xhttpexpectcursor {
	size_t Offset;
	uint8 Validated;
} xhttpexpectcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttpexpectfieldcursor`

重复字段游标同时记录当前字段和字段内位置。

```c
typedef struct xhttpexpectfieldcursor {
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttpexpectfieldcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttpexpectresult`

字段分类保留语法错误与语法正确但不受支持的扩展差异。

```c
typedef enum xhttpexpectresult {
	XHTTP_EXPECT_ERROR = -1,
	XHTTP_EXPECT_NONE = 0,
	XHTTP_EXPECT_CONTINUE = 1,
	XHTTP_EXPECT_UNSUPPORTED = 2
} xhttpexpectresult;
```

| 值 | 语义 |
|---|---|
| `XHTTP_EXPECT_ERROR` | 失败 |
| `XHTTP_EXPECT_NONE` | 无 |
| `XHTTP_EXPECT_CONTINUE` | CONTINUE（100 继续） |

### `xhttptecodingflag`

单个 TE 成员标志区分 trailers、传输参数和显式权重。

```c
typedef enum xhttptecodingflag {
	XHTTP_TE_CODING_NONE = 0,
	XHTTP_TE_CODING_TRAILERS = UINT32_C(0x00000001),
	XHTTP_TE_CODING_HAS_PARAMETERS = UINT32_C(0x00000002),
	XHTTP_TE_CODING_HAS_WEIGHT = UINT32_C(0x00000004)
} xhttptecodingflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_TE_CODING_NONE` | 无 |
| `XHTTP_TE_CODING_TRAILERS` | 支持 trailer |
| `XHTTP_TE_CODING_HAS_PARAMETERS` | 带参数 |

### `xhttptecoding`

TE 成员借用完整元素、编码名称和不含 q 权重的传输参数。

```c
typedef struct xhttptecoding {
	xstrview Element;
	xstrview Coding;
	xstrview Parameters;
	size_t ParameterCount;
	uint16 Quality;
	uint32 Flags;
} xhttptecoding;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Element` | `xstrview` | Element |
| `Coding` | `xstrview` | Coding |
| `Parameters` | `xstrview` | Parameters |
| `ParameterCount` | `size_t` | ParameterCount |
| `Quality` | `uint16` | Quality |
| `Flags` | `uint32` | 标志位 |

### `xhttptecursor`

单字段游标由初始化函数建立，调用方不得直接修改。

```c
typedef struct xhttptecursor {
	size_t Offset;
	uint8 Validated;
} xhttptecursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttptefieldcursor`

重复字段游标同时记录当前字段和字段内位置。

```c
typedef struct xhttptefieldcursor {
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttptefieldcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttpteflag`

TE 汇总标志明确区分字段缺失、空字段和 trailers 能力。

```c
typedef enum xhttpteflag {
	XHTTP_TE_NONE = 0,
	XHTTP_TE_PRESENT = UINT32_C(0x00000001),
	XHTTP_TE_ACCEPTS_TRAILERS = UINT32_C(0x00000002),
	XHTTP_TE_HAS_TRANSFER_CODINGS = UINT32_C(0x00000004)
} xhttpteflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_TE_NONE` | 无 |
| `XHTTP_TE_PRESENT` | 请求携带 TE |
| `XHTTP_TE_ACCEPTS_TRAILERS` | 接受 trailer |

### `xhttpteinfo`

TE 汇总保留字段、总成员和实际传输编码数量。

```c
typedef struct xhttpteinfo {
	size_t FieldCount;
	size_t CodingCount;
	size_t TransferCodingCount;
	uint32 Flags;
} xhttpteinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `FieldCount` | `size_t` | FieldCount |
| `CodingCount` | `size_t` | CodingCount |
| `TransferCodingCount` | `size_t` | TransferCodingCount |
| `Flags` | `uint32` | 标志位 |

### `xhttpupgradeitem`

一个 Upgrade 协议借用原字段值；空 Version 表示线路中没有版本。

```c
typedef struct xhttpupgradeitem {
	xstrview Protocol;
	xstrview Version;
} xhttpupgradeitem;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Protocol` | `xstrview` | Protocol |
| `Version` | `xstrview` | 结构版本 |

### `xhttpupgradecursor`

单字段游标由初始化函数建立，调用方不得直接修改。

```c
typedef struct xhttpupgradecursor {
	size_t Offset;
	uint8 Validated;
} xhttpupgradecursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttpupgradefieldcursor`

重复字段游标同时记录当前字段和字段内位置。

```c
typedef struct xhttpupgradefieldcursor {
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttpupgradefieldcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xnetproxytype`

代理类型只描述协议；TCP、TLS 和上层客户端决定如何承载协议。

```c
typedef enum xnetproxytype {
	XNET_PROXY_SOCKS5 = 1,
	XNET_PROXY_HTTP_CONNECT
} xnetproxytype;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_SOCKS5` | SOCKS5 代理 |

### `xnetproxyauth`

AUTO 在存在凭据时要求认证，否则只允许匿名；OPTIONAL 显式允许降级为匿名。

```c
typedef enum xnetproxyauth {
	XNET_PROXY_AUTH_AUTO = 0,
	XNET_PROXY_AUTH_NONE,
	XNET_PROXY_AUTH_REQUIRED,
	XNET_PROXY_AUTH_OPTIONAL
} xnetproxyauth;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_AUTH_AUTO` | 自动 |
| `XNET_PROXY_AUTH_NONE` | 无 |
| `XNET_PROXY_AUTH_REQUIRED` | 需要代理认证 |

### `xnetproxyconfig`

代理对象持有配置深拷贝；主机不要求零结尾，凭据允许任意字节。

```c
typedef struct xnetproxyconfig {
	xnetproxytype Type;
	xstrview Host;
	uint16 Port;
	xnetproxyauth Auth;
	xbytesview Username;
	xbytesview Password;
} xnetproxyconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xnetproxytype` | 类型 |
| `Host` | `xstrview` | 主机名 |
| `Port` | `uint16` | 端口 |
| `Auth` | `xnetproxyauth` | Auth |
| `Username` | `xbytesview` | Username |
| `Password` | `xbytesview` | Password |

### `xnetproxyinfo`

信息视图由代理对象持有，只能在至少一个对象引用存活时借用。

```c
typedef struct xnetproxyinfo {
	xnetproxytype Type;
	xstrview Host;
	uint16 Port;
	xnetproxyauth Auth;
	xbytesview Username;
	xbytesview Password;
} xnetproxyinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xnetproxytype` | 类型 |
| `Host` | `xstrview` | 主机名 |
| `Port` | `uint16` | 端口 |
| `Auth` | `xnetproxyauth` | Auth |
| `Username` | `xbytesview` | Username |
| `Password` | `xbytesview` | Password |

### `xnetproxyhandshakestate`

握手状态同时告诉传输层下一步应发送、接收还是发布隧道。

```c
typedef enum xnetproxyhandshakestate {
	XNET_PROXY_HANDSHAKE_WRITE = 1,
	XNET_PROXY_HANDSHAKE_READ,
	XNET_PROXY_HANDSHAKE_READY,
	XNET_PROXY_HANDSHAKE_ERROR
} xnetproxyhandshakestate;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_HANDSHAKE_WRITE` | 写方向 |
| `XNET_PROXY_HANDSHAKE_READ` | 读方向 |
| `XNET_PROXY_HANDSHAKE_READY` | 就绪 |

### `xnetproxyendpoint`

域名端点使用 Host；数字端点使用 Address，端口始终保存在 Address.Port。

```c
typedef struct xnetproxyendpoint {
	xnetaddr Address;
	xstrview Host;
} xnetproxyendpoint;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Address` | `xnetaddr` | 地址 |
| `Host` | `xstrview` | 主机名 |

### `xnetproxyhandshakeconfig`

输入缓冲池由调用方借用，并且必须比握手对象存活更久。

```c
typedef struct xnetproxyhandshakeconfig {
	const xnetproxy* Proxy;
	xstrview TargetHost;
	uint16 TargetPort;
	size_t ReceiveLimit;
	xnetbufpool* Pool;
} xnetproxyhandshakeconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Proxy` | `const xnetproxy*` | Proxy |
| `TargetHost` | `xstrview` | TargetHost |
| `TargetPort` | `uint16` | TargetPort |
| `ReceiveLimit` | `size_t` | ReceiveLimit |
| `Pool` | `xnetbufpool*` | Pool |

### `xnetsocks5reply`

SOCKS5 CONNECT 回复码保留 RFC 1928 的线路值，便于日志和策略判断。

```c
typedef enum xnetsocks5reply {
	XNET_SOCKS5_SUCCEEDED = 0,
	XNET_SOCKS5_GENERAL_FAILURE = 1,
	XNET_SOCKS5_RULESET_DENIED = 2,
	XNET_SOCKS5_NETWORK_UNREACHABLE = 3,
	XNET_SOCKS5_HOST_UNREACHABLE = 4,
	XNET_SOCKS5_CONNECTION_REFUSED = 5,
	XNET_SOCKS5_TTL_EXPIRED = 6,
	XNET_SOCKS5_COMMAND_UNSUPPORTED = 7,
	XNET_SOCKS5_ADDRESS_UNSUPPORTED = 8
} xnetsocks5reply;
```

| 值 | 语义 |
|---|---|
| `XNET_SOCKS5_SUCCEEDED` | 成功 |
| `XNET_SOCKS5_GENERAL_FAILURE` | 通用失败 |
| `XNET_SOCKS5_RULESET_DENIED` | 被规则集拒绝 |
| `XNET_SOCKS5_NETWORK_UNREACHABLE` | 网络不可达 |
| `XNET_SOCKS5_HOST_UNREACHABLE` | 主机不可达 |
| `XNET_SOCKS5_CONNECTION_REFUSED` | 连接被拒绝 |
| `XNET_SOCKS5_TTL_EXPIRED` | TTL 过期 |
| `XNET_SOCKS5_COMMAND_UNSUPPORTED` | COMMAND不支持 |

### `xnetproxydialstate`

Proxy Dial 状态区分代理端点解析、TCP 连接和协议握手。

```c
typedef enum xnetproxydialstate {
	XNET_PROXY_DIAL_RESOLVING = 0,
	XNET_PROXY_DIAL_CONNECTING,
	XNET_PROXY_DIAL_HANDSHAKE,
	XNET_PROXY_DIAL_CONNECTED,
	XNET_PROXY_DIAL_FAILED,
	XNET_PROXY_DIAL_CANCELLED
} xnetproxydialstate;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_DIAL_RESOLVING` | 解析中 |
| `XNET_PROXY_DIAL_CONNECTING` | 连接中 |
| `XNET_PROXY_DIAL_HANDSHAKE` | 握手阶段 |
| `XNET_PROXY_DIAL_CONNECTED` | 已连接 |
| `XNET_PROXY_DIAL_FAILED` | 已失败 |

### `xnetproxydialconfig`

Timeout 覆盖 DNS、TCP 和代理握手全过程；零值保留各内层超时。

```c
typedef struct xnetproxydialconfig {
	xnetdialconfig Transport;
	uint64 Timeout;
	size_t ReceiveLimit;
} xnetproxydialconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Transport` | `xnetdialconfig` | Transport |
| `Timeout` | `uint64` | 超时（微秒） |
| `ReceiveLimit` | `size_t` | ReceiveLimit |

### `xnetproxydialstats`

Proxy Dial 保持底层 TCP Dial 统计，并补充当前协议阶段。

```c
typedef struct xnetproxydialstats {
	xnetproxydialstate State;
	xnetdialstats Transport;
} xnetproxydialstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xnetproxydialstate` | 状态 |
| `Transport` | `xnetdialstats` | Transport |

### `xnetproxy`

不可变代理端点可以跨请求和线程共享。

```c
typedef struct xnetproxy xnetproxy;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetproxyhandshake`

单个握手由一个传输执行上下文独占驱动。

```c
typedef struct xnetproxyhandshake xnetproxyhandshake;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetproxydial`

托管代理拨号对象（不透明）：内部串联名称解析、TCP 连接与代理握手。


```c
typedef struct xnetproxydial xnetproxydial;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetproxydialproc`

完成回调在代理传输 Worker 上至多执行一次，不会从提交调用栈重入。 pDial 和 Error 只在回调期间借用；成功回调接管隧道 Stream 引用。

```c
typedef void (*xnetproxydialproc)(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XHTTP_QUALITY_MAX` | `1000u` | QUALITY上限 |
| `XHTTP_AUTHORITY_HAS_PORT` | `UINT32_C(0x00000001)` | Authority 包含显式端口分隔符。 |
| `XHTTP_AUTHORITY_IP_LITERAL` | `UINT32_C(0x00000002)` | Host 是 IPv6 或 IPvFuture 字面地址，Host 视图不包含方括号。 |
| `XHTTP_AUTHORITY_PORT_EMPTY` | `UINT32_C(0x00000004)` | 显式端口只有冒号而没有数字。 |
| `XHTTP_TARGET_HAS_SCHEME` | `UINT32_C(0x00000001)` | Target 包含 scheme。 |
| `XHTTP_TARGET_HAS_AUTHORITY` | `UINT32_C(0x00000002)` | Target 包含双斜杠引入的 authority。 |
| `XHTTP_TARGET_HAS_QUERY` | `UINT32_C(0x00000004)` | Target 包含问号引入的 query，包括显式空 query。 |
| `XHTTP_DECODE_OUTPUT_SAFE_DEFAULT` | `(UINT64_C(16) * 1024u * 1024u)` | DECODE输出失败SAFE默认值 |
| `XHTTP_CONTENT_CODINGS_DEFAULT` | `4u` | Content-Encoding 解析与通用 Body 解码共享的安全层数边界。 |
| `XHTTP_CONTENT_CODINGS_MAX` | `16u` | CONTENTCODINGS上限 |

## 模块边界

| 模块 | 裁剪宏 | 作用 |
| --- | --- | --- |
| `http` | `XRT_MODULE_HTTP` | token、字段、方法、状态和 Content-Length |
| `http_param` | `XRT_MODULE_HTTP_PARAM` | 参数与 quoted-string 语法 |
| `http_param_host` | `XRT_MODULE_HTTP_PARAM_HOST` | 参数解码后的无分配 Host 验证 |
| `http_expect` | `XRT_MODULE_HTTP_EXPECT` | Expect 字段 |
| `http_upgrade` | `XRT_MODULE_HTTP_UPGRADE` | Upgrade 字段与写出 |
| `http_te` | `XRT_MODULE_HTTP_TE` | TE 与 transfer-coding 参数 |
| `http_connection` | `XRT_MODULE_HTTP_CONNECTION` | Connection 选项与持久连接 |
| `http_trailer` | `XRT_MODULE_HTTP_TRAILER` | Trailer 声明和尾字段约束 |
| `http_encoding` | `XRT_MODULE_HTTP_ENCODING` | Accept-Encoding 与 Content-Encoding |
| `http_decode` | `XRT_MODULE_HTTP_DECODE` | 流式 gzip/deflate 自动解码 |
| `http_host` | `XRT_MODULE_HTTP_HOST` | Host authority |
| `http_target` | `XRT_MODULE_HTTP_TARGET` | 四种 request-target |
| `http1_head` | `XRT_MODULE_HTTP1_HEAD` | 起始行、Header 解析与写入 |
| `http1_net` | `XRT_MODULE_HTTP1_NET` | TCP 块链 Header 解析与余量保留 |
| `http1_tls` | `XRT_MODULE_HTTP1_TLS` | TLS 明文块链增量 Header 解析 |
| `http1_body` | `XRT_MODULE_HTTP1_BODY` | 正文计划、chunked 和 trailer |
| `http1_message` | `XRT_MODULE_HTTP1_MESSAGE` | 连续内存完整消息便利层 |

`http_decode` 才依赖 Inflate，`http1_net` 才依赖网络块链，`http1_tls` 才依赖 TLS Stream。
只启用连续内存 HTTP/1 解析不会带入压缩、网络、TLS 或 WebSocket。

## 借用模型

`xhttpfield`、`xhttp1head`、`xhttp1message` 和所有语法游标都借用调用方输入。
解析器不复制字段名称和值，输入缓冲和字段描述符数组必须覆盖借用视图的使用期。

请求方法同时以原始 `xhttp1head.Method` 视图和 `xhttp1head.MethodCode` 枚举发布。
`xrtHttpMethodParse` 按大小写敏感规则识别 GET、HEAD、POST、PUT、DELETE、CONNECT、
OPTIONS、TRACE 和 PATCH；合法但未内置的方法返回 `XHTTP_METHOD_OTHER`，非法 token
返回 `XHTTP_METHOD_INVALID`。每个非零枚举值占用独立 bit，既可以作为实际方法直接
比较，也可以通过位或组合成路由方法集合。`XHTTP_METHOD_CRUD` 覆盖 GET、POST、PUT、
PATCH 和 DELETE，`XHTTP_METHOD_ANY` 覆盖全部内置方法及 `OTHER`；应用仍应保留原始
视图处理具体的扩展方法。

字段描述符允许位于合法的未对齐存储。实现使用安全加载，不要求调用方为了协议
解析调整缓冲布局。

XRT 不再提供动态 Header 容器。需要拥有、修改或索引 Header 的框架应在 `xhttp`
中建立对象模型；快速路径可以直接使用栈数组或应用自己的存储。

## Host 语法

`xrtHttpHostParse` 保留 Host、可选端口和 IP-literal 分类，任意长度十进制端口在
协议层仍然合法，只有可放入 `uint16` 的值才带 `XHTTP_AUTHORITY_PORT_VALUE`。
`xrtHttpIpv4Valid` 与 `xrtHttpIpv6Valid` 是无错误副作用的纯谓词，公开同一套严格
IP 文本规则，扩展协议无需复制 Host 内部解析器。

`xrtHttpParamHostValid` 直接消费 `xhttpparam` 的语义值。quoted-pair 会在验证时
流式解码，任意长度 reg-name 与 IPvFuture 不进入固定缓冲，也不申请堆内存。

## Header 解析

`xrtHttp1RequestParse` 和 `xrtHttp1ResponseParse` 接受从消息首字节开始的累计输入：

- `XHTTP1_MORE`：输入尚未包含完整 Header；
- `XHTTP1_FIELDS`：Header 完整，但字段描述符容量不足，`FieldCount` 给出需求；
- `XHTTP1_READY`：解析完成；
- `XHTTP1_ERROR`：协议、限额或参数错误。

解析器只接受 CRLF，拒绝 obs-fold、裸 LF、非法字段名、控制字符、冲突的
Content-Length、歧义 Transfer-Encoding 和非法 Upgrade。`xhttp1errorinfo`
提供稳定错误码、消息相对偏移和行号。

`xhttp1limits` 分别限制 Header 总长度、起始行、单字段行和字段数量。默认值面向
公网输入，服务端应按路由策略进一步收紧，而不是无上限增长接收缓冲。

网络热路径使用 `xrtHttp1RequestParseBuffer`、`xrtHttp1ResponseParseBuffer` 直接扫描
`xnetbuf`。只有完整 Header 跨块时才连续化实际 Header 前缀，函数不消费输入，Upgrade 后的余量
保持原位。TLS 对应入口是 `xrtHttp1RequestParseTls`、`xrtHttp1ResponseParseTls`；输入不足时它们
通过 `xrtTlsStreamReadMore` 请求下一段受限明文，Header 完成后仍由调用方消费 `Head.Bytes`。

## 正文计划

Header 完成后必须调用：

- `xrtHttp1RequestBodyPlan` 处理请求；
- `xrtHttp1ResponseBodyPlan` 处理响应，并传入原请求方法。

`xhttp1bodyplan` 的模式是唯一分帧结论：

- `XHTTP1_BODY_NONE`：没有正文；
- `XHTTP1_BODY_FIXED`：Content-Length 定长；
- `XHTTP1_BODY_CHUNKED`：分块编码；
- `XHTTP1_BODY_CLOSE`：由可靠 EOF 定界；
- `XHTTP1_BODY_TUNNEL`：CONNECT 或 101 后的字节不再属于 HTTP。

响应计划覆盖 HEAD、成功 CONNECT、1xx、204 和 304。调用方不应仅凭
Content-Length 判断响应正文。

## 流式接收

`xrtHttp1BodyInit` 创建无分配 reader。`xrtHttp1BodyRead` 每次返回一个借用正文
片段、容量请求或终态，并通过 `Consumed` 精确说明已消费线路字节。

`xrtHttp1BodyLimitsInit` 的 `MaxBody` 默认为 `UINT64_MAX`，因为底层 reader 不拥有、
聚合或分配正文。服务端、代理以及任何会持有正文的上层必须在接收前按路由和内存预算
设置有限上限，不能把该协议层默认值直接作为公网接收策略。

对于 chunked，reader 会验证 chunk-size、扩展语法、CRLF、last-chunk 和 trailer，
并严格拒绝 chunk-size 与分号或 CRLF 之间的空白，避免端点对报文边界产生分歧。
正文视图不包含分块元数据。`Received` 是应用正文长度，`WireBytes` 是正文区实际
线路长度。

当返回 `XHTTP1_BODY_FIELDS` 时，`TrailerCount` 给出所需描述符数量。调用
`xrtHttp1BodyTrailers` 绑定足够存储后，用同一输入继续解析，不会丢失进度。

close-delimited 模式只有在传输层给出可靠 EOF 时才传入 `bEnd = true`。超时、取消、
RST 和普通 EOF 不能混为同一种上层状态。

## gzip 自动解码

`xrtHttpDecodeCreate` 直接读取解析后的字段数组。把 `xrtHttp1BodyRead` 发布的每个
正文片段交给 `xrtHttpDecodeWrite`，并只在 body reader 完成时设置 `bFinal`。

identity 和显式原样回退路径不复制输入。gzip/deflate 路径按 Content-Encoding
逆序流式解码，验证 gzip Header、CRC、ISIZE 和压缩流终态。每个中间层与最终
输出都受同一明文硬限额约束。

详见 [HTTP 正文解码](http_decode.md)。

## 原始写出

`xrtHttp1RequestWrite` 和 `xrtHttp1ResponseWrite` 不添加 Host、Date、Server 或其他
策略字段。空输出用于精确测量，容量不足不会产生半个 Header。

高性能发送建议：

1. 在连接或协程栈上的小缓冲中写 Header；
2. 用 `xrtNetStreamSendVec` 一次提交 Header 与小正文；
3. 大正文用 `xrtNetStreamSendRef`、`SendRefs`、`SendTake` 或 `SendBuffer`；
4. chunked 正文只用 `xrtHttp1ChunkLineWrite` 生成短前缀，数据本身保持引用发送；
5. `XRT_NET_AGAIN` 时等待 writable/drain，不绕过网络队列硬上限。

这条路径只构造必需的线路字节，不创建请求、响应、字典或正文对象。

## 报文长度

- `xhttp1head.Bytes`：起始行、Header 和最终空行的线路长度；
- `xhttp1bodyplan.Length`：定长正文长度，仅在 `FIXED` 模式有效；
- `xhttp1body.Received`：已经发布的应用正文长度；
- `xhttp1body.WireBytes`：已经消费的正文线路长度；
- `xhttp1message.Wire.Size`：连续输入中第一条完整消息的线路总长度；
- `xhttp1message.BodyBytes`：移除 chunked 元数据后的正文长度。

chunked 和 close-delimited 消息在完成前不存在可提前得知的总线路长度。

## TLS 与 Upgrade

HTTP 不拥有 TLS。`http1_tls` 只是块链适配器，不创建连接或安全策略；证书验证、SNI、会话恢复和
ALPN 仍属于通用 TLS 模块。它按需连续化实际 Header，不增加连接级固定缓冲。

`xrtHttpUpgrade*`、`xrtHttpConnection*` 和 WebSocket 握手函数共同完成 Upgrade
验证。101 后的剩余字节必须原样交给新协议，不能继续送入 HTTP body reader。

## 完整消息便利层

`xrtHttp1RequestMessageParse` 和 `xrtHttp1ResponseMessageParse` 适合已经连续驻留在
内存中的报文、测试和协议网关。它们仍只借用输入，不持有堆对象。

固定长度或 close-delimited 正文可由 `xrtHttp1MessageBodyView` 直接查看；chunked
正文使用 `xrtHttp1MessageBodyCopy` 去除分帧。真正的网络热路径应优先使用流式
body reader，避免等待和复制整个消息。

## 方法与状态

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


## 令牌与 OWS

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


## 权重与长度

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


## Host 与 Authority

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


## 编码枚举

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


### `xrtHttpCodingParse`

把编码 token 解析为内置枚举（identity/gzip/deflate，x-gzip 为 gzip 别名）；未知返回 NONE。

```c
xhttpcoding xrtHttpCodingParse(xstrview Token);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Token` | 输入 | 借用 | 编码 token（大小写不敏感） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_CODING_IDENTITY/GZIP/DEFLATE` | 内置编码 | — |
| `XHTTP_CODING_NONE` | 未知编码（如 zstd） | 不设错 |

#### 错误

- 无 — NONE 是分类结果

#### 范例

[http/small_fields · 编码枚举](../../examples/http/small_fields/main.c) · 观察

```c
		if ( (xrtHttpCodingParse(SV("gzip")) !=
				XHTTP_CODING_GZIP) ||
```



## 字段解析与写出

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


## 字段查找

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


## 同名字段 token 游标

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


## quoted-string

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


## 参数

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


## 指令

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



## TE 传输编码

### `xrtHttpTeCursorInit`

初始化单个 TE 字段值游标。

```c
void xrtHttpTeCursorInit(xhttptecursor* pCursor);
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

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	xrtHttpTeCursorInit(&TeCursor);
```


### `xrtHttpTeFieldCursorInit`

初始化跨重复 TE 字段游标。

```c
void xrtHttpTeFieldCursorInit(
	xhttptefieldcursor* pCursor
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

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	xrtHttpTeFieldCursorInit(&TeFieldCursor);
```


### `xrtHttpTeCodingParse`

严格解析一个不含列表分隔逗号的 TE 成员。

```c
bool xrtHttpTeCodingParse(
	xstrview Element,
	xhttptecoding* pCoding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Element` | 输入 | 借用 | 单个成员文本 |
| `pCoding` | 输出 | 非空 | 接收名称/权重 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解析 | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	if ( !xrtHttpTeCodingParse(SV("trailers"), &TeCoding) ||
		(TeCoding.Quality != 1000u) ||
		!xrtHttpTeCodingParse(SV("gzip;q=0.8"), &TeCoding) ||
		(TeCoding.Quality != 800u) ) {
```


### `xrtHttpTeValid`

完整验证一个 TE 字段值；HTTP 列表空成员会被忽略。

```c
bool xrtHttpTeValid(xstrview Value);
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

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	if ( !xrtHttpTeValid(SV("gzip, deflate")) ||
		!xrtHttpTeValid(SV("gzip,, ,")) ||
		xrtHttpTeValid(SV("gzip;;")) ||
		!xrtHttpTeCount(SV("gzip, deflate, br"), &iCount) ||
		(iCount != 3u) ) {
```


### `xrtHttpTeCount`

完整验证并统计一个 TE 字段值中的非空成员。

```c
bool xrtHttpTeCount(
	xstrview Value,
	size_t* pCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCount` | 输出 | 非空 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 语法错误 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
		!xrtHttpTeCount(SV("gzip, deflate, br"), &iCount) ||
		(iCount != 3u) ) {
```


### `xrtHttpTeNext`

按线路顺序迭代一个完整 TE 字段值。

```c
xhttpnext xrtHttpTeNext(
	xstrview Value,
	xhttptecursor* pCursor,
	xhttptecoding* pCoding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pCoding` | 输出 | 非空 | 接收成员 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	while ( xrtHttpTeNext(SV("gzip, trailers"), &TeCursor,
			&TeCoding) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpTeFieldNext`

跨重复 TE 字段行按线路顺序迭代全部成员。

```c
xhttpnext xrtHttpTeFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttptefieldcursor* pCursor,
	xhttptecoding* pCoding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pCoding` | 输出 | 非空 | 接收成员 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	while ( xrtHttpTeFieldNext(arrTeFields, 2u, &TeFieldCursor,
			&TeCoding) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpTeParse`

完整解析全部重复 TE 字段并发布零分配汇总。

```c
bool xrtHttpTeParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpteinfo* pInfo
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pInfo` | 输出 | 非空 | 接收汇总（chunked/编码集等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 汇总已发布 | — |
| `false` | 任一字段非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/te · 汇总](../../examples/http/te/main.c) · 观察

```c
	if ( !xrtHttpTeParse(Fields, 2u, &Info) ) {
```


### `xrtHttpTeQuality`

返回指定传输编码的最高有效权重；缺失或不匹配返回零。

```c
uint16 xrtHttpTeQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Coding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | TE 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Coding` | 输入 | 借用 | 编码名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `0–1000` | 最高有效权重 | — |
| `0` | 缺失或不匹配（或参数错误） | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `xrt.http` 域错误 — 字段值非法

#### 范例

[http/te · 权重](../../examples/http/te/main.c) · 观察

```c
		(unsigned int)xrtHttpTeQuality(
			Fields, 2u, XRT_STR_LITERAL("gzip")
		)
```


### `xrtHttpTeAcceptsTrailers`

完整验证并判断客户端是否声明不会丢弃 Trailer。

```c
xhttpnext xrtHttpTeAcceptsTrailers(
	const xhttpfield* pFields,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 声明 trailers（`TE: trailers`） | — |
| `XHTTP_NEXT_END` | 未声明 | 不设错 |
| `XHTTP_NEXT_ERROR` | 字段非法 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	if ( xrtHttpTeAcceptsTrailers(arrTeFields, 2u) !=
		XHTTP_NEXT_ITEM ) {
```


## Expect

### `xrtHttpExpectCursorInit`

初始化单个 Expect 字段值游标。

```c
void xrtHttpExpectCursorInit(
	xhttpexpectcursor* pCursor
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

- 无

#### 范例

[http/small_fields · Expect](../../examples/http/small_fields/main.c) · 观察

```c
		xrtHttpExpectCursorInit(&ExCursor);
```


### `xrtHttpExpectFieldCursorInit`

初始化跨重复 Expect 字段游标。

```c
void xrtHttpExpectFieldCursorInit(
	xhttpexpectfieldcursor* pCursor
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

- 无

#### 范例

[http/expect · 字段游标](../../examples/http/expect/main.c) · 观察

```c
	xrtHttpExpectFieldCursorInit(&Cursor);
```


### `xrtHttpExpectationParse`

严格解析一个不含列表分隔逗号的 expectation。

```c
bool xrtHttpExpectationParse(
	xstrview Element,
	xhttpexpectation* pExpectation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Element` | 输入 | 借用 | 单个 expectation |
| `pExpectation` | 输出 | 非空 | 接收名称/参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解析 | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Expect](../../examples/http/small_fields/main.c) · 观察

```c
		if ( !xrtHttpExpectationParse(SV("100-continue"),
				&Expectation) ||
			(Expectation.Name.Size != 12u) ||
			(memcmp(Expectation.Name.Data, "100-continue",
					12u) != 0) ) {
```


### `xrtHttpExpectValid`

完整验证一个 Expect 字段值；空列表符合 HTTP 列表语法。

```c
bool xrtHttpExpectValid(xstrview Value);
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

[http/small_fields · Expect](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpExpectValid(arrExpect[0].Value) ) {
```


### `xrtHttpExpectCount`

完整验证并统计一个 Expect 字段值中的 expectation 数量。

```c
bool xrtHttpExpectCount(
	xstrview Value,
	size_t* pCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCount` | 输出 | 非空 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 语法错误 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Expect](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpExpectCount(arrExpect[0].Value, &iCount) ||
			(iCount != 2u) ||
```


### `xrtHttpExpectNext`

按线路顺序迭代一个完整 Expect 字段值。

```c
xhttpnext xrtHttpExpectNext(
	xstrview Value,
	xhttpexpectcursor* pCursor,
	xhttpexpectation* pExpectation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pExpectation` | 输出 | 非空 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Expect](../../examples/http/small_fields/main.c) · 观察

```c
		while ( xrtHttpExpectNext(arrExpect[0].Value, &ExCursor,
				&Expectation) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpExpectFieldNext`

跨重复 Expect 字段行按线路顺序迭代 expectation。

```c
xhttpnext xrtHttpExpectFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpexpectfieldcursor* pCursor,
	xhttpexpectation* pExpectation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pExpectation` | 输出 | 非空 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/expect · 字段游标](../../examples/http/expect/main.c) · 观察

```c
	while ( xrtHttpExpectFieldNext(
		Fields, 2u, &Cursor, &Expectation
	) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpExpectFields`

分类全部重复 Expect 字段并完整验证所有元素。

```c
xhttpexpectresult xrtHttpExpectFields(
	const xhttpfield* pFields,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_EXPECT_NONE` | 无 Expect 字段 | — |
| `XHTTP_EXPECT_100_CONTINUE` | 声明 100-continue | — |
| `XHTTP_EXPECT_UNSUPPORTED` | 合法但服务器不支持的 expectation | — |
| 参数非法时 | — | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/expect · 分类](../../examples/http/expect/main.c) · 观察

```c
		xrtHttpExpectFields(Fields, 2u) ==
			XHTTP_EXPECT_CONTINUE ? "yes" : "no"
```


## Upgrade

### `xrtHttpUpgradeCursorInit`

初始化单个 Upgrade 字段值游标。

```c
void xrtHttpUpgradeCursorInit(
	xhttpupgradecursor* pCursor
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

- 无

#### 范例

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
		xrtHttpUpgradeCursorInit(&UpCursor);
```


### `xrtHttpUpgradeFieldCursorInit`

初始化跨重复 Upgrade 字段游标。

```c
void xrtHttpUpgradeFieldCursorInit(
	xhttpupgradefieldcursor* pCursor
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

- 无

#### 范例

[http/upgrade · 字段游标](../../examples/http/upgrade/main.c) · 观察

```c
	xrtHttpUpgradeFieldCursorInit(&Cursor);
```


### `xrtHttpUpgradeParse`

严格解析一个 protocol-name[/protocol-version] 元素。

```c
bool xrtHttpUpgradeParse(
	xstrview Text,
	xhttpupgradeitem* pUpgrade
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 单个协议元素 |
| `pUpgrade` | 输出 | 非空 | 接收名称/版本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解析 | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
		if ( !xrtHttpUpgradeParse(SV("websocket"), &Upgrade) ||
			(Upgrade.Protocol.Size != 9u) ) {
```


### `xrtHttpUpgradeValid`

完整验证一个 Upgrade 字段值；空列表符合列表语法。

```c
bool xrtHttpUpgradeValid(xstrview Value);
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

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpUpgradeValid(SV("websocket")) ||
			xrtHttpUpgradeValid(SV("bad token")) ) {
```


### `xrtHttpUpgradeCount`

完整验证并统计一个 Upgrade 字段值中的协议数量。

```c
bool xrtHttpUpgradeCount(
	xstrview Value,
	size_t* pCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCount` | 输出 | 非空 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 语法错误 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpUpgradeCount(SV("websocket, h2c"),
				&iCount) ||
			(iCount != 2u) ||
```


### `xrtHttpUpgradeNext`

按线路顺序迭代一个完整 Upgrade 字段值。

```c
xhttpnext xrtHttpUpgradeNext(
	xstrview Value,
	xhttpupgradecursor* pCursor,
	xhttpupgradeitem* pUpgrade
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pUpgrade` | 输出 | 非空 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
		while ( xrtHttpUpgradeNext(SV("websocket, h2c"),
				&UpCursor, &Upgrade) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpUpgradeFieldNext`

跨重复 Upgrade 字段行按线路顺序迭代协议。

```c
xhttpnext xrtHttpUpgradeFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpupgradefieldcursor* pCursor,
	xhttpupgradeitem* pUpgrade
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pUpgrade` | 输出 | 非空 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/upgrade · 字段游标](../../examples/http/upgrade/main.c) · 观察

```c
	while ( (Next = xrtHttpUpgradeFieldNext(
		Fields, 2u, &Cursor, &Upgrade
	)) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpUpgradeWrite`

规范写出一个或多个 Upgrade 协议；空输出可精确查询长度。

```c
bool xrtHttpUpgradeWrite(
	const xhttpupgradeitem* pUpgrades,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUpgrades` | 输入 | 借用数组 | 协议元素数组 |
| `iCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
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

[http/upgrade · 写出](../../examples/http/upgrade/main.c) · 观察

```c
		!xrtHttpUpgradeWrite(
			Offered,
			2u,
			sOutput,
			sizeof(sOutput),
			&iSize
		) ) {
```


### `xrtHttpUpgradeElementWrite`

规范写出一个 Upgrade 协议元素。

```c
bool xrtHttpUpgradeElementWrite(
	const xhttpupgradeitem* pUpgrade,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUpgrade` | 输入 | 非空 | 协议元素 |
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
- `XERR_RANGE`

#### 范例

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
			if ( !xrtHttpUpgradeElementWrite(
					&(xhttpupgradeitem){ SV("h2c"), SV("v2") },
					Buffer, sizeof(Buffer), &iCount) ||
				(iCount != 6u) ||
				(memcmp(Buffer, "h2c/v2", 6u) != 0) ) {
```


### `xrtHttpUpgradeBuild`

构建零结尾 Upgrade 字段值，返回值由 `xrtFree` 释放。

```c
str xrtHttpUpgradeBuild(
	const xhttpupgradeitem* pUpgrades,
	size_t iCount,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUpgrades` | 输入 | 借用数组 | 协议元素数组 |
| `iCount` | 输入 | — | 条目数 |
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

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
			str sBuilt = xrtHttpUpgradeBuild(arrUp, 2u, &iCount);
```


## Trailer

### `xrtHttpTrailerNameValid`

判断字段名是否可作为通用 HTTP trailer 发送。

```c
bool xrtHttpTrailerNameValid(xstrview Name);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 可发送（非禁投递集合） | — |
| `false` | 禁止作为 trailer | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/small_fields · Trailer](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpTrailerNameValid(SV("X-Checksum")) ||
			xrtHttpTrailerNameValid(SV("Bad Name")) ) {
```


### `xrtHttpTrailerSectionValid`

完整验证实际 trailer section 的字段名称和值。

```c
bool xrtHttpTrailerSectionValid(
	const xhttpfield* pTrailers,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTrailers` | 输入 | 借用数组 | 实际 trailer 字段 |
| `iCount` | 输入 | — | 条目数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 名称与值全部合法 | — |
| `false` | 存在禁投递名或非法值 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/small_fields · Trailer](../../examples/http/small_fields/main.c) · 观察

```c
			if ( !xrtHttpTrailerSectionValid(arrSection, 1u) ||
				xrtHttpTrailerSectionValid(arrBad, 1u) ) {
```


### `xrtHttpTrailerCount`

完整验证重复 Trailer 字段行并统计其中声明的名称。

```c
bool xrtHttpTrailerCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pNameCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 声明字段 |
| `iCount` | 输入 | — | 条目数 |
| `pNameCount` | 输出 | 非空 | 接收声明计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 声明非法 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Trailer](../../examples/http/small_fields/main.c) · 观察

```c
		if ( !xrtHttpTrailerCount(arrTrailer, 2u, &iCount) ||
			(iCount != 3u) ||
			!xrtHttpTrailerFind(arrTrailer, 2u,
				SV("X-Checksum")) ||
			xrtHttpTrailerFind(arrTrailer, 2u,
				SV("X-Missing")) ||
			!xrtHttpTrailerNameValid(SV("X-Checksum")) ||
			xrtHttpTrailerNameValid(SV("Bad Name")) ) {
```


### `xrtHttpTrailerFind`

查找已声明的 trailer 字段名；返回 ITEM、END 或 ERROR。

```c
xhttpnext xrtHttpTrailerFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 声明字段 |
| `iCount` | 输入 | — | 条目数 |
| `Name` | 输入 | 借用 | 查找名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 找到/未找到（不设错）/声明非法 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Trailer](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpTrailerFind(arrTrailer, 2u,
				SV("X-Checksum")) ||
			xrtHttpTrailerFind(arrTrailer, 2u,
```


### `xrtHttpTrailerNamesWrite`

从实际 trailer 字段写出规范的 Trailer 声明值；同名按大小写不敏感去重并保留首现顺序。

```c
bool xrtHttpTrailerNamesWrite(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTrailers` | 输入 | 借用数组 | 实际字段 |
| `iTrailerCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空、不得与描述符重叠 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 容量不足或重叠 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足

#### 范例

[http/small_fields · Trailer](../../examples/http/small_fields/main.c) · 观察

```c
			if ( !xrtHttpTrailerNamesWrite(arrActual, 2u,
					Buffer, sizeof(Buffer), &iCount) ||
				(iCount != 19u) ||
				(memcmp(Buffer, "X-Checksum, X-Total",
					19u) != 0) ) {
```


### `xrtHttpTrailerNamesBuild`

构建零结尾的 Trailer 声明值，返回值由 `xrtFree` 释放。

```c
str xrtHttpTrailerNamesBuild(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTrailers` | 输入 | 借用数组 | 实际字段 |
| `iTrailerCount` | 输入 | — | 条目数 |
| `pSize` | 输出 | 可空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 零结尾声明值 | — |
| `NULL` | 参数错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_MEMORY`

#### 范例

[http/trailer · 构建](../../examples/http/trailer/main.c) · 观察

```c
	sNames = xrtHttpTrailerNamesBuild(Trailers, 2u, NULL);
```


## Accept-Encoding 协商

### `xrtHttpAcceptEncodingInit`

初始化为 Header 缺失状态；按 RFC 该状态接受任意内容编码。

```c
void xrtHttpAcceptEncodingInit(
	xhttpacceptencoding* pAccept
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输出 | 非空 | 协商状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/encoding · 协商](../../examples/http/encoding/main.c) · 观察

```c
	xrtHttpAcceptEncodingInit(&Accept);
```


### `xrtHttpAcceptEncodingValid`

判断公开协商状态字段是否自洽；纯查询不修改线程原有错误。

```c
bool xrtHttpAcceptEncodingValid(
	const xhttpacceptencoding* pAccept
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入 | 非空 | 协商状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 自洽 | — |
| `false` | 不自洽 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/small_fields · 协商](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpAcceptEncodingValid(&Accept) ) {
```


### `xrtHttpAcceptEncodingAdd`

失败原子地合并一个 Accept-Encoding 字段值；空值只记录 Header 存在，未知编码语法有效但不进入内置集合。

```c
bool xrtHttpAcceptEncodingAdd(
	xhttpacceptencoding* pAccept,
	xstrview Value
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入/输出 | 非空 | 协商状态 |
| `Value` | 输入 | 借用 | 单个字段值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已合并（失败原子） | — |
| `false` | 语法错误 | 状态不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/encoding · 协商](../../examples/http/encoding/main.c) · 观察

```c
	if ( !xrtHttpAcceptEncodingAdd(
		&Accept,
		XRT_STR_LITERAL(
			"gzip;q=0.8, deflate;q=0.4, identity;q=0.1"
		)
	) ) {
```


### `xrtHttpAcceptEncodingParse`

扫描全部同名字段并构建零分配协商状态；Fields 为空且 Count 为零表示没有任何字段。

```c
bool xrtHttpAcceptEncodingParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpacceptencoding* pAccept
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pAccept` | 输出 | 非空 | 接收协商状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 状态已构建 | — |
| `false` | 任一字段非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · 协商](../../examples/http/small_fields/main.c) · 观察

```c
		if ( !xrtHttpAcceptEncodingParse(arrAe, 1u, &Accept) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_GZIP) != 900u) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_IDENTITY) != 1000u) ||
			!xrtHttpAcceptEncodingValid(&Accept) ) {
```


### `xrtHttpAcceptEncodingQuality`

返回指定内置编码的有效质量；参数错误返回零并设置错误。

```c
uint16 xrtHttpAcceptEncodingQuality(
	const xhttpacceptencoding* pAccept,
	xhttpcoding Coding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入 | 非空 | 协商状态 |
| `Coding` | 输入 | — | 内置编码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `0–1000` | 有效质量（缺失按 RFC 缺省 1000） | — |
| `0` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/small_fields · 协商](../../examples/http/small_fields/main.c) · 观察

```c
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_GZIP) != 900u) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
```


### `xrtHttpAcceptEncodingSelect`

从 Available 位掩码中选择最高质量编码；等质量时先选 Preferred，再按 gzip、deflate、identity 顺序。

```c
xhttpcoding xrtHttpAcceptEncodingSelect(
	const xhttpacceptencoding* pAccept,
	uint32 iAvailable,
	xhttpcoding Preferred
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入 | 非空 | 协商状态 |
| `iAvailable` | 输入 | `XHTTP_CODING_*` 位掩码 | 服务器可用集 |
| `Preferred` | 输入 | — | 等质量偏好 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 编码值 | 选中的编码 | — |
| `XHTTP_CODING_NONE` | 无可用匹配 | `XERR_ARGUMENT`（参数错误时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/encoding · 选择](../../examples/http/encoding/main.c) · 观察

```c
	Coding = xrtHttpAcceptEncodingSelect(
		&Accept,
		XHTTP_CODING_IDENTITY |
			XHTTP_CODING_GZIP |
			XHTTP_CODING_DEFLATE,
		XHTTP_CODING_GZIP
	);
```


## Content-Encoding 响应链

### `xrtHttpContentEncodingCursorInit`

初始化可重复使用的 Content-Encoding 前向游标。

```c
void xrtHttpContentEncodingCursorInit(
	xhttpcontentencodingcursor* pCursor
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

- 无

#### 范例

[http/small_fields · 响应链](../../examples/http/small_fields/main.c) · 观察

```c
			xrtHttpContentEncodingCursorInit(&Cursor);
```


### `xrtHttpContentEncodingNext`

按字段出现顺序迭代全部 Content-Encoding 成员；未知扩展仍返回 ITEM 且 Coding 为 NONE。

```c
xhttpnext xrtHttpContentEncodingNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingcursor* pCursor,
	xhttpcontentencodingitem* pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pItem` | 输出 | 非空 | 接收编码条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态（未知扩展 Coding=NONE） | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · 响应链](../../examples/http/small_fields/main.c) · 观察

```c
			if ( xrtHttpContentEncodingNext(arrCe, 1u,
					&Cursor, &Item) != XHTTP_NEXT_ITEM ||
				(Item.Token.Size != 4u) ||
				(Item.Coding != XHTTP_CODING_GZIP) ) {
```


### `xrtHttpContentEncodingPlan`

无分配构建完整 Content-Encoding 计划；DecoderCount 只统计 gzip/deflate。

```c
bool xrtHttpContentEncodingPlan(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingplan* pPlan
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pPlan` | 输出 | 非空 | 接收解码器数/连接串大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计划已发布 | — |
| `false` | 字段非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/encoding · 响应链](../../examples/http/encoding/main.c) · 观察

```c
	if ( !xrtHttpContentEncodingPlan(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Plan
	) ) {
```


### `xrtHttpContentEncodingWrite`

按字段出现顺序写出以逗号空格连接的原始值；空输出可查大小，容量不足不写部分结果。

```c
bool xrtHttpContentEncodingWrite(
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
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出（不附加零） | — |
| `false` | 容量不足 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE`

#### 范例

[http/small_fields · 响应链](../../examples/http/small_fields/main.c) · 观察

```c
		if ( !xrtHttpContentEncodingWrite(arrCe, 1u, Buffer,
				sizeof(Buffer), &iCount) ||
			(iCount != 8u) ||
			(memcmp(Buffer, "gzip, br", 8u) != 0) ) {
```


### `xrtHttpCodingName`

返回 identity、gzip 或 deflate 的静态小写 token；NONE 返回空视图。

```c
xstrview xrtHttpCodingName(xhttpcoding Coding);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Coding` | 输入 | — | 编码枚举 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 静态小写 token | — |
| 空视图 | `XHTTP_CODING_NONE` | 纯查询 |

#### 错误

- 无 — 纯查询

#### 范例

[http/encoding · 编码名](../../examples/http/encoding/main.c) · 观察

```c
	Name = xrtHttpCodingName(Coding);
```


## 正文解码器

### `xrtHttpDecodeConfigInit`

初始化兼容配置：最多四层、64 KiB gzip Header、明文长度不设上限。

```c
void xrtHttpDecodeConfigInit(xhttpdecodeconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收兼容配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/decode_tour · 配置](../../examples/http/decode_tour/main.c) · 观察

```c
	xrtHttpDecodeConfigInit(&Compat);
```


### `xrtHttpDecodeConfigInitSafe`

初始化面向不可信对端的安全配置；明文最多 16 MiB，更大正文需显式修改 OutputLimit。

```c
void xrtHttpDecodeConfigInitSafe(xhttpdecodeconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收安全配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无；无限明文须显式设 `XHTTP_DECODE_OUTPUT_UNLIMITED`

#### 范例

[http/decode_tour · 配置](../../examples/http/decode_tour/main.c) · 观察

```c
	xrtHttpDecodeConfigInitSafe(&Safe);
```


### `xrtHttpDecodeCreate`

根据全部 Header 创建解码器；字段和值只在本次调用期间借用。

```c
xhttpdecode* xrtHttpDecodeCreate(
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用（仅调用期间） | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pConfig` | 输入 | 允许空 | 空 = 兼容配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 解码器（按 Content-Encoding/Content-Length 决定模式） | — |
| `NULL` | Header 不一致或 OOM | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 编码组合非法或长度矛盾
- `XERR_MEMORY`

#### 范例

[http/decode · 创建](../../examples/http/decode/main.c) · 观察

```c
	xhttpdecode* pDecode = xrtHttpDecodeCreate(Fields, 1, NULL);
```


### `xrtHttpDecodeReset`

为下一条消息复位并复用已经分配的 Inflate 窗口。

```c
bool xrtHttpDecodeReset(
	xhttpdecode* pDecode,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入/输出 | 非空 | 解码器 |
| `pFields` | 输入 | 借用 | 新消息字段 |
| `iCount` | 输入 | — | 条目数 |
| `pConfig` | 输入 | 允许空 | 新配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已复位可处理下一条 | — |
| `false` | 前一条未完成或 Header 非法 | 状态不变 |

#### 错误

- `XERR_STATE` — 前一条消息未终结
- `xrt.http` 域错误

#### 范例

[http/decode_tour · 复用](../../examples/http/decode_tour/main.c) · 观察

```c
	if ( !xrtHttpDecodeReset(pDecode, Fields, 1u, &Compat) ||
		(xrtHttpDecodeMode(pDecode) != XHTTP_DECODE_IDENTITY) ||
		!xrtHttpDecodeWrite(pDecode,
			(xbytesview) { arrGzip, 4u }, true,
			exampleOutput, (ptr)&Out) ||
		!xrtHttpDecodeDone(pDecode) ||
		(Out.iBytes != 4u) ||
		(xrtHttpDecodeInputSize(pDecode) != 4u) ) {
```


### `xrtHttpDecodeWrite`

同步消费完整输入片段；`bFinal` 表示正文已达协议边界。无编码和原样回退路径直接调用 Output，不复制输入。

```c
bool xrtHttpDecodeWrite(
	xhttpdecode* pDecode,
	xbytesview Input,
	bool bFinal,
	xhttpdecodeoutputproc pOutput,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入/输出 | 非空 | 解码器 |
| `Input` | 输入 | 借用 | 本段线路字节 |
| `bFinal` | 输入 | — | 末段标记 |
| `pOutput` | 输入 | 允许空 | 明文回调；空 = 丢弃 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已消费（终段校验通过） | — |
| `false` | 数据损坏、超限或回调中止 | 进入失败终态 |

#### 错误

- `xrt.http` 域错误 — 压缩流损坏/截断
- `XERR_RANGE` — 超 OutputLimit
- `XERR_CANCELLED` — 回调中止

#### 范例

[http/decode · 解码](../../examples/http/decode/main.c) · 观察

```c
	bSuccess = xrtHttpDecodeWrite(
		pDecode,
		(xbytesview){ Gzip, sizeof(Gzip) },
		true,
		printBody,
		stdout
	) && xrtHttpDecodeDone(pDecode);
```


### `xrtHttpDecodeMode`

返回当前消息的交付模式。

```c
xhttpdecodemode xrtHttpDecodeMode(
	const xhttpdecode* pDecode
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_DECODE_*` | 直通/identity/解码模式 | — |
| 零值 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/decode_tour · 自省](../../examples/http/decode_tour/main.c) · 观察

```c
	if ( (xrtHttpDecodeMode(pDecode) != XHTTP_DECODE_CONTENT) ||
		!xrtHttpDecodeWrite(pDecode,
			(xbytesview) { arrGzip, sizeof(arrGzip) }, true,
			exampleOutput, (ptr)&Out) ||
		!xrtHttpDecodeDone(pDecode) ||
		(Out.iBytes != 26u) ||
		(memcmp(Out.arrText, "identity-passthrough-check",
			26u) != 0) ||
		(xrtHttpDecodeInputSize(pDecode) != 46u) ||
		(xrtHttpDecodeOutputSize(pDecode) != 26u) ) {
```


### `xrtHttpDecodeDone`

判断最终正文边界和全部压缩流 trailer 均已验证。

```c
bool xrtHttpDecodeDone(const xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 消息完整终结 | — |
| `false` | 未完成或失败终态 | 纯查询 |

#### 错误

- 无 — 纯查询

#### 范例

[http/decode · 完成](../../examples/http/decode/main.c) · 观察

```c
	) && xrtHttpDecodeDone(pDecode);
```


### `xrtHttpDecodeInputSize`

返回成功提交给当前消息的线路正文总字节数。

```c
uint64 xrtHttpDecodeInputSize(const xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 线路字节数 | — |
| `0` | 无或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/decode_tour · 自省](../../examples/http/decode_tour/main.c) · 观察

```c
		(xrtHttpDecodeInputSize(pDecode) != 46u) ||
		(xrtHttpDecodeOutputSize(pDecode) != 26u) ) {
```


### `xrtHttpDecodeOutputSize`

返回已经被输出回调接受或明确丢弃的正文总字节数。

```c
uint64 xrtHttpDecodeOutputSize(const xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 明文字节数（含丢弃） | — |
| `0` | 无或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/decode_tour · 自省](../../examples/http/decode_tour/main.c) · 观察

```c
		(xrtHttpDecodeOutputSize(pDecode) != 26u) ) {
```


### `xrtHttpDecodeDestroy`

销毁解码器；空指针是安全的空操作。

```c
void xrtHttpDecodeDestroy(xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 引用与窗口已释放 | — |

#### 错误

- 无 — 销毁不失败

#### 范例

[http/decode · 收尾](../../examples/http/decode/main.c) · 观察

```c
	xrtHttpDecodeDestroy(pDecode);
```



## HTTP/1 起始行与 Header

### `xrtHttp1TargetValid`

验证非空 request-target 不含空白、控制字符或 fragment。

```c
bool xrtHttp1TargetValid(xstrview Target);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Target` | 输入 | 借用 | 目标文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http1/head_tour · 目标](../../examples/http1/head_tour/main.c) · 观察

```c
		xrtHttp1TargetValid(XRT_STR_LITERAL("/a b")) ||
```


### `xrtHttp1LimitsInit`

初始化适合公网输入的限额：8 KiB 起始行、64 KiB Header、100 字段。

```c
void xrtHttp1LimitsInit(xhttp1limits* pLimits);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLimits` | 输出 | 非空、可未对齐 | 接收限额（解析前复制） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http1/head_tour · 限额](../../examples/http1/head_tour/main.c) · 观察

```c
	xrtHttp1LimitsInit(&Limits);
```


### `xrtHttp1HeadInit`

初始化借用调用方字段数组的空 Head。

```c
void xrtHttp1HeadInit(
	xhttp1head* pHead,
	xhttpfield* pFields,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHead` | 输出 | 非空 | 接收 Head |
| `pFields` | 输入 | 借用存储 | 字段描述符数组 |
| `iCapacity` | 输入 | — | 数组容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http1/head_tour · 初始化](../../examples/http1/head_tour/main.c) · 观察

```c
	xrtHttp1HeadInit(&Head, Fields, 8);
```


### `xrtHttp1RequestParse`

严格增量解析 HTTP/1.0 或 HTTP/1.1 请求 Header。

```c
xhttp1status xrtHttp1RequestParse(
	xbytesview Input,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用 | 当前累积字节 |
| `pHead` | 输入/输出 | 已初始化 | Head（成功后 `Bytes` 为消费量） |
| `pLimits` | 输入 | 允许空 | 空 = `LimitsInit` 默认 |
| `pError` | 输出 | 可空 | 接收错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK` | Header 完整解析 | — |
| `XHTTP1_MORE` | 需要更多输入（不设错） | — |
| `XHTTP1_ERROR` | 协议错误 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误 — 起始行/Header 语法或超限

#### 范例

[http/http1 · 请求解析](../../examples/http/http1/main.c) · 观察

```c
	if ( xrtHttp1RequestParse(
		Input, &Head, NULL, NULL
	) != XHTTP1_READY ) {
```


### `xrtHttp1ResponseParse`

严格增量解析 HTTP/1.0 或 HTTP/1.1 响应 Header。

```c
xhttp1status xrtHttp1ResponseParse(
	xbytesview Input,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用 | 当前累积字节 |
| `pHead` | 输入/输出 | 已初始化 | Head |
| `pLimits` | 输入 | 允许空 | 限额 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 同请求解析三态 | — |

#### 错误

- `xrt.http1` 域错误

#### 范例

[http1_body](../../examples/http/http1_body/main.c) · 观察

```c
	if ( xrtHttp1ResponseParse(
		(xbytesview){ Message, sizeof(Message) - 1u },
		&Head, NULL, NULL
	) != XHTTP1_READY ) {
```


### `xrtHttp1TransferCodingNext`

严格迭代一个 Transfer-Encoding 字段值，`Offset` 初始为零。

```c
xhttpnext xrtHttp1TransferCodingNext(
	xstrview Value,
	size_t* pOffset,
	xhttp1transfercoding* pCoding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pOffset` | 输入/输出 | 初始零 | 游标 |
| `pCoding` | 输出 | 非空 | 接收编码条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误

#### 范例

[head_tour](../../examples/http1/head_tour/main.c) · 观察

```c
	if ( ((Next = xrtHttp1TransferCodingNext(
			pField->Value, &iOffset, &Coding)) != XHTTP_NEXT_ITEM) ||
		(Coding.Name.Size != 4u) ||
		(memcmp(Coding.Name.Data, "gzip", 4u) != 0) ||
		(xrtHttp1TransferCodingNext(pField->Value, &iOffset,
			&Coding) != XHTTP_NEXT_ITEM) ||
		(Coding.Name.Size != 7u) ||
		(memcmp(Coding.Name.Data, "chunked", 7u) != 0) ||
		(xrtHttp1TransferCodingNext(pField->Value, &iOffset,
			&Coding) != XHTTP_NEXT_END) ) {
```


### `xrtHttp1Field`

返回第一个同名 Header，未找到返回空指针。

```c
const xhttpfield* xrtHttp1Field(
	const xhttp1head* pHead,
	xstrview Name
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHead` | 输入 | 非空 | 已解析 Head |
| `Name` | 输入 | 借用 | 查找名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 借用字段地址 | — |
| `NULL` | 未找到 | 不设错 |

#### 错误

- 无 — 未找到是查询结果

#### 范例

[http1/head_tour · 字段](../../examples/http1/head_tour/main.c) · 观察

```c
		((pField = xrtHttp1Field(&Head,
			XRT_STR_LITERAL("host"))) == NULL) ||
```


### `xrtHttp1RequestWrite`

校验并写入完整请求 Header；不自动添加 Host 等策略字段。空输出查长度；容量不足不写半个报文。

```c
bool xrtHttp1RequestWrite(
	xstrview Method,
	xstrview Target,
	xhttpversion Version,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法 |
| `Target` | 输入 | 借用 | 目标 |
| `Version` | 输入 | — | HTTP 版本 |
| `pFields` | 输入 | 借用数组 | 字段 |
| `iFieldCount` | 输入 | — | 字段数 |
| `pOutput` | 输出 | 可空、不得与输入重叠 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 完整报文已写出 | — |
| `false` | 校验失败或容量不足 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `xrt.http1` 域错误 — 方法/目标非法
- `XERR_RANGE` — 容量不足

#### 范例

[upgrade](../../examples/websocket/upgrade/main.c) · 观察

```c
		) || !xrtHttp1RequestWrite(
```


### `xrtHttp1ResponseWrite`

校验并写入完整响应 Header；不自动添加 Content-Length 或连接策略。`Reason` 允许为空。

```c
bool xrtHttp1ResponseWrite(
	xhttpversion Version,
	uint16 iStatus,
	xstrview Reason,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | HTTP 版本 |
| `iStatus` | 输入 | 100–999 | 状态码 |
| `Reason` | 输入 | 可空 | 原因短语 |
| `pFields` | 输入 | 借用数组 | 字段 |
| `iFieldCount` | 输入 | — | 字段数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 完整报文已写出 | — |
| `false` | 校验失败或容量不足 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 状态越界或容量不足

#### 范例

[http/http1 · 写出](../../examples/http/http1/main.c) · 观察

```c
	if ( !xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1, 200, XRT_STR_LITERAL("OK"),
		ResponseFields, 2, Response, sizeof(Response), &iSize
	) ) {
```


## HTTP/1 正文分帧

### `xrtHttp1BodyLimitsInit`

初始化无正文预设上限的流式 Body 限额；上层可按路由收紧。

```c
void xrtHttp1BodyLimitsInit(xhttp1bodylimits* pLimits);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLimits` | 输出 | 非空、可未对齐 | 接收限额（Init 时复制） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/http1_body · 限额](../../examples/http/http1_body/main.c) · 观察

```c
	xrtHttp1BodyLimitsInit(&Limits);
```


### `xrtHttp1RequestBodyPlan`

按 RFC 9112 请求分帧优先级生成 Body Plan。

```c
bool xrtHttp1RequestBodyPlan(
	const xhttp1head* pHead,
	xhttp1bodyplan* pPlan
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHead` | 输入 | 非空、已完整解析 | 请求 Head |
| `pPlan` | 输出 | 非空 | 接收分帧计划 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计划已生成 | — |
| `false` | 分帧字段矛盾（如 TE+CL 并存） | 输出不变 |

#### 错误

- `xrt.http1` 域错误 — Transfer-Encoding/Content-Length 冲突

#### 范例

[http1/head_tour · 分帧](../../examples/http1/head_tour/main.c) · 观察

```c
	if ( !xrtHttp1RequestBodyPlan(&Head, &Plan) ||
		(Plan.Mode != XHTTP1_BODY_CHUNKED) ) {
```


### `xrtHttp1ResponseBodyPlan`

按请求方法与响应状态生成 Body Plan；HEAD 和 CONNECT 必须传入原请求方法。

```c
bool xrtHttp1ResponseBodyPlan(
	const xhttp1head* pHead,
	xstrview RequestMethod,
	xhttp1bodyplan* pPlan
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHead` | 输入 | 非空、已完整解析 | 响应 Head |
| `RequestMethod` | 输入 | 借用 | 原请求方法 |
| `pPlan` | 输出 | 非空 | 接收分帧计划 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计划已生成 | — |
| `false` | 分帧字段矛盾 | 输出不变 |

#### 错误

- `xrt.http1` 域错误

#### 范例

[http/http1_body · 响应分帧](../../examples/http/http1_body/main.c) · 观察

```c
	if ( !xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) ) {
```


### `xrtHttp1BodyInit`

初始化无分配 Body Reader；trailer 描述符可为空并在 FIELDS 后重新绑定。

```c
bool xrtHttp1BodyInit(
	xhttp1body* pBody,
	const xhttp1bodyplan* pPlan,
	xhttpfield* pTrailers,
	size_t iTrailerCapacity,
	const xhttp1bodylimits* pLimits
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBody` | 输出 | 非空 | 接收 Reader |
| `pPlan` | 输入 | 已生成 | 分帧计划 |
| `pTrailers` | 输入 | 可空 | trailer 描述符存储 |
| `iTrailerCapacity` | 输入 | — | trailer 容量 |
| `pLimits` | 输入 | 允许空 | 空 = `BodyLimitsInit` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | Reader 已就绪 | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/http1_body · Reader](../../examples/http/http1_body/main.c) · 观察

```c
	if ( !xrtHttp1BodyInit(
		&Body, &Plan, Trailers, 4, &Limits
	) ) {
```


### `xrtHttp1BodyTrailers`

在 FIELDS 状态后替换 trailer 描述符存储，不重置正文解码进度。

```c
bool xrtHttp1BodyTrailers(
	xhttp1body* pBody,
	xhttpfield* pTrailers,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBody` | 输入/输出 | 非空 | Reader |
| `pTrailers` | 输入 | 非空 | 新存储 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已重绑 | — |
| `false` | 状态非 FIELDS 或参数错误 | 进度不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_STATE` — 当前状态不可重绑

#### 范例

[http1/head_tour · trailer](../../examples/http1/head_tour/main.c) · 观察

```c
			!xrtHttp1BodyTrailers(&Body, Trailers, 4u) ) {
```


### `xrtHttp1TrailersParse`

严格解析从第一行开始并由空行结束的 trailer 区；所有字段均借用 Input。

```c
xhttp1status xrtHttp1TrailersParse(
	xbytesview Input,
	xhttpfield* pFields,
	size_t iCapacity,
	const xhttp1bodylimits* pLimits,
	size_t* pBytes,
	size_t* pCount,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用 | trailer 区字节 |
| `pFields` | 输出 | 借用存储 | 接收字段 |
| `iCapacity` | 输入 | — | 字段容量 |
| `pLimits` | 输入 | 允许空 | 限额 |
| `pBytes` | 输出 | 可空 | 消费字节数 |
| `pCount` | 输出 | 可空 | 字段数 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 三态 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误 — 字段名非法或超限

#### 范例

[http1/head_tour · trailer](../../examples/http1/head_tour/main.c) · 观察

```c
		if ( (xrtHttp1TrailersParse(
				(xbytesview) { (const uint8*)sTrailer,
					sizeof(sTrailer) - 1u },
				Trailers, 4u, &BodyLimits, &iBytes, &iCount,
				&Error) != XHTTP1_READY) ||
			(iCount != 1u) ||
			(Trailers[0].Value.Size != 1u) ) {
```


### `xrtHttp1ChunkLineWrite`

写入十六进制 chunk-size 行；只写 size 行，正文可向量发送零复制。

```c
bool xrtHttp1ChunkLineWrite(
	uint64 iSize,
	xstrview Extensions,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSize` | 输入 | — | chunk 数据长度 |
| `Extensions` | 输入 | 空或以分号起始 | 扩展后缀 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 容量不足或扩展非法 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足

#### 范例

[http1/head_tour · 分块](../../examples/http1/head_tour/main.c) · 观察

```c
	if ( !xrtHttp1ChunkLineWrite(5u, XRT_STR_LITERAL(""), arrLine,
			sizeof(arrLine), &iSize) ||
		(iSize != 3u) ||
		(memcmp(arrLine, "5\r\n", 3u) != 0) ||
		!xrtHttp1ChunkLineWrite(5u, XRT_STR_LITERAL(";x"),
			arrLine, sizeof(arrLine), &iSize) ||
		(iSize != 5u) ||
		(memcmp(arrLine, "5;x\r\n", 5u) != 0) ) {
```


### `xrtHttp1ChunkWrite`

把一段非空正文封装为完整 chunk；空正文是成功空操作，不结束消息。

```c
bool xrtHttp1ChunkWrite(
	xbytesview Data,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | 借用 | 非空正文段 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 完整 chunk 已写出 | — |
| `false` | 空正文配非空操作以外错误 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足

#### 范例

[http/http1_body · 分块](../../examples/http/http1_body/main.c) · 观察

```c
	if ( !xrtHttp1ChunkWrite(
		(xbytesview){ (cbytes)"hello", 5 },
		Output, sizeof(Output), &iChunk
	) || !xrtHttp1ChunkEndWrite(
		Trailers, 1, Output + iChunk,
		sizeof(Output) - iChunk, &iEnd
	) ) {
```


### `xrtHttp1ChunkEndWrite`

写入 last-chunk、可选 trailer 和最终空行；调用方须确认字段定义明确允许 trailer。

```c
bool xrtHttp1ChunkEndWrite(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTrailers` | 输入 | 允许空 | trailer 字段 |
| `iTrailerCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 结束序列已写出 | — |
| `false` | 容量不足 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE`

#### 范例

[http/http1_body · 分块](../../examples/http/http1_body/main.c) · 观察

```c
	) || !xrtHttp1ChunkEndWrite(
```


### `xrtHttp1BodyRead`

推进正文状态机；`Consumed` 是本次可移除的线缆字节，`Data` 仅 DATA 状态有效。定长/分块正文过早结束返回协议错误。

```c
xhttp1bodystatus xrtHttp1BodyRead(
	xhttp1body* pBody,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xbytesview* pData,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBody` | 输入/输出 | 已初始化 | Reader |
| `Input` | 输入 | 借用 | 当前线缆字节 |
| `bEnd` | 输入 | — | 可靠传输已结束 |
| `pConsumed` | 输出 | 可空 | 本次消费量 |
| `pData` | 输出 | 可空 | 接收明文段（借用输入） |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_BODY_DATA` | 一段明文已发布 | — |
| `XHTTP1_BODY_FIELDS` | 进入 trailer 解析 | — |
| `XHTTP1_BODY_DONE` | 正文与 trailer 完整消费 | — |
| `XHTTP1_BODY_ERROR` | 协议错误或过早结束 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误 — 截断、超限或非法 chunk

#### 范例

[http/http1_body · 状态机](../../examples/http/http1_body/main.c) · 观察

```c
		xhttp1bodystatus Status = xrtHttp1BodyRead(
			&Body,
			(xbytesview){ Message + iOffset, sizeof(Message) - 1u - iOffset },
			false, &iConsumed, &Data, NULL
		);
```


### `xrtHttp1BodyDone`

判断 Reader 是否已经完整消费 HTTP 正文与 trailer。

```c
bool xrtHttp1BodyDone(const xhttp1body* pBody);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBody` | 输入 | 允许空 | Reader |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已完整消费 | — |
| `false` | 未完成、失败或参数非法 | 纯查询 |

#### 错误

- 无 — 纯查询

#### 范例

[http/http1_body · 完成](../../examples/http/http1_body/main.c) · 观察

```c
	while ( !xrtHttp1BodyDone(&Body) ) {
```


## HTTP/1 完整消息

### `xrtHttp1MessageInit`

初始化借用调用方 Header 与 trailer 描述符数组的空完整消息。

```c
void xrtHttp1MessageInit(
	xhttp1message* pMessage,
	xhttpfield* pFields,
	size_t iFieldCapacity,
	xhttpfield* pTrailers,
	size_t iTrailerCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMessage` | 输出 | 非空 | 接收消息 |
| `pFields` | 输入 | 借用存储 | Header 数组 |
| `iFieldCapacity` | 输入 | — | Header 容量 |
| `pTrailers` | 输入 | 借用存储 | trailer 数组 |
| `iTrailerCapacity` | 输入 | — | trailer 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/http1_message · 初始化](../../examples/http/http1_message/main.c) · 观察

```c
	xrtHttp1MessageInit(&Message, Fields, 8, Trailers, 4);
```


### `xrtHttp1RequestMessageParse`

扫描第一条完整请求；`bEnd` 表示可靠 EOF，拒绝被截断的 Header 或正文。

```c
xhttp1status xrtHttp1RequestMessageParse(
	xbytesview Input,
	bool bEnd,
	xhttp1message* pMessage,
	const xhttp1limits* pHeadLimits,
	const xhttp1bodylimits* pBodyLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用 | 消息字节 |
| `bEnd` | 输入 | — | 输入即全部字节 |
| `pMessage` | 输出 | 已初始化 | 接收完整消息 |
| `pHeadLimits` | 输入 | 允许空 | Head 限额 |
| `pBodyLimits` | 输入 | 允许空 | Body 限额 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 完整/需更多/错误 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误 — 截断（bEnd 下 MORE 升级为错误）或协议错误

#### 范例

[http1/head_tour · 完整解析](../../examples/http1/head_tour/main.c) · 观察

```c
	if ( (xrtHttp1RequestMessageParse(
			(xbytesview) { (const uint8*)sFixed,
				sizeof(sFixed) - 1u },
			true, &Message, &Limits, &BodyLimits,
			&Error) != XHTTP1_READY) ||
		((View = xrtHttp1MessageBodyView(&Message)).Size != 2u) ||
		(memcmp(View.Data, "ok", 2u) != 0) ) {
```


### `xrtHttp1ResponseMessageParse`

扫描第一条完整响应；`RequestMethod` 用于 HEAD、CONNECT 和普通响应分帧。

```c
xhttp1status xrtHttp1ResponseMessageParse(
	xbytesview Input,
	bool bEnd,
	xstrview RequestMethod,
	xhttp1message* pMessage,
	const xhttp1limits* pHeadLimits,
	const xhttp1bodylimits* pBodyLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用 | 消息字节 |
| `bEnd` | 输入 | — | 输入即全部字节 |
| `RequestMethod` | 输入 | 借用 | 原请求方法 |
| `pMessage` | 输出 | 已初始化 | 接收完整消息 |
| `pHeadLimits` | 输入 | 允许空 | Head 限额 |
| `pBodyLimits` | 输入 | 允许空 | Body 限额 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 三态 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误

#### 范例

[http/http1_message · 完整解析](../../examples/http/http1_message/main.c) · 观察

```c
	if ( xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, NULL
	) != XHTTP1_READY ) {
```


### `xrtHttp1MessageBodyView`

返回无需移除 chunked 分帧时的借用正文；chunked 或空正文返回空视图。

```c
xbytesview xrtHttp1MessageBodyView(const xhttp1message* pMessage);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMessage` | 输入 | 非空 | 完整消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 借用正文视图 | — |
| 空视图 | chunked/空正文（应改用 BodyCopy） | 纯查询 |

#### 错误

- 无 — 纯查询

#### 范例

[http1/head_tour · 正文](../../examples/http1/head_tour/main.c) · 观察

```c
		((View = xrtHttp1MessageBodyView(&Message)).Size != 2u) ||
```


### `xrtHttp1MessageBodyCopy`

把正文复制到连续输出并移除 chunked 分帧；空输出可精确查询所需长度。

```c
bool xrtHttp1MessageBodyCopy(
	const xhttp1message* pMessage,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMessage` | 输入 | 非空 | 完整消息 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已复制（含去分帧） | — |
| `false` | 容量不足或超限 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足或正文超限

#### 范例

[http/http1_message · 正文](../../examples/http/http1_message/main.c) · 观察

```c
	if ( !xrtHttp1MessageBodyCopy(
		&Message, Body, sizeof(Body), &iSize
	) ) {
```


## HTTP/1 缓冲链与 TLS 解析

### `xrtHttp1RequestParseBuffer`

从网络缓冲链严格解析请求 Header；不消费输入，Head 视图借用 Buffer，完成后按 Head.Bytes 消费。

```c
xhttp1status xrtHttp1RequestParseBuffer(
	xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 缓冲链（跨块时按需合并前缀） |
| `pHead` | 输入/输出 | 已初始化 | Head |
| `pLimits` | 输入 | 允许空 | 限额 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 三态 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误

#### 范例

[http1/parse_buffer · 缓冲链](../../examples/http1/parse_buffer/main.c) · 观察

```c
		if ( xrtHttp1RequestParseBuffer(&ReqBuf, &Head, NULL, NULL) ==
			XHTTP1_READY ) {
```


### `xrtHttp1ResponseParseBuffer`

从网络缓冲链严格解析响应 Header；Upgrade 后的任何余量保持在 Buffer 中。

```c
xhttp1status xrtHttp1ResponseParseBuffer(
	xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 缓冲链 |
| `pHead` | 输入/输出 | 已初始化 | Head |
| `pLimits` | 输入 | 允许空 | 限额 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 三态 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误

#### 范例

[http1/parse_buffer · 缓冲链](../../examples/http1/parse_buffer/main.c) · 观察

```c
		if ( xrtHttp1ResponseParseBuffer(&RspBuf, &Head, NULL, NULL) ==
			XHTTP1_READY ) {
```


### `xrtHttp1RequestParseTls`

从当前 TLS 明文块链严格解析请求 Header，只连续化实际 Header 前缀；不消费明文，可用 Head.Bytes 原子接管。

```c
xhttp1status xrtHttp1RequestParseTls(
	xtlsstream* pStream,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空、Worker 上下文 | TLS Stream |
| `pHead` | 输入/输出 | 已初始化 | Head |
| `pLimits` | 输入 | 允许空 | 限额 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 三态 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误

#### 范例

[tls/stream_tour · TLS 解析](../../examples/tls/stream_tour/main.c) · 观察

```c
	if ( (xrtHttp1RequestParseTls(pStream, &Head, &Limits,
			&Error) != XHTTP1_READY) ||
		(Head.Method.Size != 3u) ||
		(memcmp(Head.Method.Data, "GET", 3u) != 0) ||
		(Head.Target.Size != 2u) ||
		(memcmp(Head.Target.Data, "/x", 2u) != 0) ||
		(xrtTlsStreamSend(pStream, arrResponse,
			sizeof(arrResponse) - 1u,
			&iWritten) != XTLS_OK) ||
		(iWritten != sizeof(arrResponse) - 1u) ) {
```


### `xrtHttp1ResponseParseTls`

从当前 TLS 明文块链严格解析响应 Header，完整保留 Upgrade 后明文余量。

```c
xhttp1status xrtHttp1ResponseParseTls(
	xtlsstream* pStream,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空、Worker 上下文 | TLS Stream |
| `pHead` | 输入/输出 | 已初始化 | Head |
| `pLimits` | 输入 | 允许空 | 限额 |
| `pError` | 输出 | 可空 | 错误位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP1_OK/MORE/ERROR` | 三态 | `xrt.http1` 域错误 |

#### 错误

- `xrt.http1` 域错误

#### 范例

[tls/stream_tour · TLS 解析](../../examples/tls/stream_tour/main.c) · 观察

```c
	if ( (xrtHttp1ResponseParseTls(pStream, &Head, &Limits,
			&Error) == XHTTP1_READY) &&
		(Head.Status == 200u) &&
		(Head.ContentLength == 2u) ) {
```


## 代理对象

### `xrtNetProxyConfigInit`

初始化 SOCKS5、自动认证且没有固定容量字段的代理配置。

```c
void xrtNetProxyConfigInit(xnetproxyconfig* pConfig);
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

[network/proxy_tour · 配置](../../examples/network/proxy_tour/main.c) · 观察

```c
	xrtNetProxyConfigInit(&ProxyConfig);
```


### `xrtNetProxyCreate`

深拷贝代理端点和凭据，创建可跨线程共享的不可变对象。

```c
xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 代理配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 不可变代理对象（引用计数） | — |
| `NULL` | 配置非法或 OOM | `xrt.proxy` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 配置非法
- `XERR_MEMORY`

#### 范例

[network/proxy_tour · 创建](../../examples/network/proxy_tour/main.c) · 观察

```c
	pProxy = xrtNetProxyCreate(&ProxyConfig);
```


### `xrtNetProxyRetain`

增加代理对象引用并返回原指针。

```c
xnetproxy* xrtNetProxyRetain(const xnetproxy* pProxy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProxy` | 输入 | 非空 | 代理对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_tour · 引用](../../examples/network/proxy_tour/main.c) · 观察

```c
		((pRetained = xrtNetProxyRetain(pProxy)) != pProxy) ) {
```


### `xrtNetProxyRelease`

释放代理对象引用；最后一个引用会清零整块配置存储。

```c
void xrtNetProxyRelease(xnetproxy* pProxy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProxy` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[network/proxy_tour · 收尾](../../examples/network/proxy_tour/main.c) · 观察

```c
	xrtNetProxyRelease(pRetained);
```


### `xrtNetProxyInfo`

复制代理对象的只读信息视图。

```c
bool xrtNetProxyInfo(
	const xnetproxy* pProxy,
	xnetproxyinfo* pInfo
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProxy` | 输入 | 非空 | 代理对象 |
| `pInfo` | 输出 | 非空 | 接收只读视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 视图已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_tour · 自省](../../examples/network/proxy_tour/main.c) · 观察

```c
	if ( !xrtNetProxyInfo(pProxy, &Info) ||
		(Info.Type != XNET_PROXY_SOCKS5) ||
		(Info.Host.Size != 12u) ||
		(memcmp(Info.Host.Data, "socks5.local", 12u) != 0) ||
		(Info.Port != 1080u) ) {
```


## 代理握手状态机

### `xrtNetProxyHandshakeConfigInit`

初始化握手配置；64 KiB 上限主要约束后续 HTTP CONNECT Header。

```c
void xrtNetProxyHandshakeConfigInit(
	xnetproxyhandshakeconfig* pConfig
);
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

[network/proxy_socks5 · 配置](../../examples/network/proxy_socks5/main.c) · 观察

```c
	xrtNetProxyHandshakeConfigInit(&HandshakeConfig);
```


### `xrtNetProxyHandshakeCreate`

创建握手并立即生成首个协议报文；目标主机会被深拷贝。

```c
xnetproxyhandshake* xrtNetProxyHandshakeCreate(
	const xnetproxyhandshakeconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 握手配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 握手对象（首报文待发送） | — |
| `NULL` | 配置非法或 OOM | `xrt.proxy` 域错误 |

#### 错误

- `XERR_ARGUMENT` / `XERR_MEMORY`

#### 范例

[network/proxy_socks5 · 创建](../../examples/network/proxy_socks5/main.c) · 观察

```c
	pHandshake = xrtNetProxyHandshakeCreate(&HandshakeConfig);
```


### `xrtNetProxyHandshakeDestroy`

销毁握手，并清零尚未发送的认证报文和内部目标信息。

```c
void xrtNetProxyHandshakeDestroy(xnetproxyhandshake* pHandshake);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 全部敏感状态已清零并释放 | — |

#### 错误

- 无 — 销毁不失败

#### 范例

[network/proxy_socks5 · 收尾](../../examples/network/proxy_socks5/main.c) · 观察

```c
		xrtNetProxyHandshakeDestroy(pHandshake);
```


### `xrtNetProxyHandshakeState`

返回当前握手状态；空指针返回 ERROR。

```c
xnetproxyhandshakestate xrtNetProxyHandshakeState(
	const xnetproxyhandshake* pHandshake
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 允许空 | 握手对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `WRITE/READ/READY/ERROR` | 状态枚举 | 零值 = 参数非法 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_tour · 状态机](../../examples/network/proxy_tour/main.c) · 观察

```c
		(xrtNetProxyHandshakeState(pHandshake) !=
			XNET_PROXY_HANDSHAKE_WRITE) ||
```


### `xrtNetProxyHandshakeStep`

处理输入链中的完整协议前缀；只消费代理回复，成功后的应用数据保持原位。WRITE 必须先发送完全部输出。

```c
xnetproxyhandshakestate xrtNetProxyHandshakeStep(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入/输出 | 非空 | 握手对象 |
| `pInput` | 输入/输出 | 非空 | 输入缓冲链 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `WRITE/READ/READY/ERROR` | 处理后的状态 | `xrt.proxy` 域错误 |

#### 错误

- `xrt.proxy` 域错误 — 协议回复非法

#### 范例

[network/proxy_socks5 · 状态机](../../examples/network/proxy_socks5/main.c) · 观察

```c
		(xrtNetProxyHandshakeStep(pHandshake, &Input) !=
		 XNET_PROXY_HANDSHAKE_WRITE) ||
```


### `xrtNetProxyHandshakeOutput`

借用当前待发送的首段连续输出；失败时把非空输出规范化为空 Span。

```c
bool xrtNetProxyHandshakeOutput(
	const xnetproxyhandshake* pHandshake,
	xnetspan* pOutput
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空 | 握手对象 |
| `pOutput` | 输出 | 非空 | 接收借用 Span |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | Span 已发布（可为空 = 无待发送） | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_socks5 · 输出](../../examples/network/proxy_socks5/main.c) · 观察

```c
	if ( !xrtNetProxyHandshakeOutput(pHandshake, &Output) ) {
```


### `xrtNetProxyHandshakeSent`

确认已经发送的输出前缀；支持 Socket 部分写入。

```c
size_t xrtNetProxyHandshakeSent(
	xnetproxyhandshake* pHandshake,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入/输出 | 非空 | 握手对象 |
| `iSize` | 输入 | — | 已发送字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 剩余待发送字节数 | — |
| `0` | 全部已确认或参数错误 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_socks5 · 发送确认](../../examples/network/proxy_socks5/main.c) · 观察

```c
	(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);
```


### `xrtNetProxyHandshakeBound`

READY 后复制可用的绑定端点；HTTP CONNECT 没有该信息并返回 NOT_FOUND。

```c
bool xrtNetProxyHandshakeBound(
	const xnetproxyhandshake* pHandshake,
	xnetproxyendpoint* pEndpoint
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空 | 握手对象 |
| `pEndpoint` | 输出 | 非空 | 接收绑定端点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 端点已复制 | — |
| `false` | 未 READY 或无端点信息 | `XERR_NOT_FOUND`（HTTP CONNECT） |

#### 错误

- `XERR_NOT_FOUND` — HTTP CONNECT 不提供绑定端点

#### 范例

[network/proxy_tour · 端点](../../examples/network/proxy_tour/main.c) · 观察

```c
	if ( xrtNetProxyHandshakeBound(pHandshake, &Endpoint) ||
		xrtNetProxyHandshakeCode(pHandshake, &iCode) ||
		(xrtNetProxyHandshakeError(pHandshake) != NULL) ) {
```


### `xrtNetProxyHandshakeError`

返回协议失败时捕获的不可变错误；对象所有权仍属于握手。

```c
const xerror* xrtNetProxyHandshakeError(
	const xnetproxyhandshake* pHandshake
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 允许空 | 握手对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 错误借用 | — |
| `NULL` | 无失败或参数非法 | 纯查询 |

#### 错误

- 无 — 非失败状态返回空是查询结果

#### 范例

[network/proxy_tour · 失败](../../examples/network/proxy_tour/main.c) · 观察

```c
		(xrtNetProxyHandshakeError(pHandshake) != NULL) ) {
```


### `xrtNetProxyHandshakeCode`

复制 SOCKS5 线路回复码或 HTTP 状态码；尚未收到回复时返回 false。

```c
bool xrtNetProxyHandshakeCode(
	const xnetproxyhandshake* pHandshake,
	uint32* pCode
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空 | 握手对象 |
| `pCode` | 输出 | 非空 | 接收回复码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 回复码已复制 | — |
| `false` | 尚未收到回复或失败 | `XERR_STATE`（未收到时） |

#### 错误

- `XERR_STATE` — 尚未到达可读回复的阶段

#### 范例

[network/proxy_tour · 回复码](../../examples/network/proxy_tour/main.c) · 观察

```c
		xrtNetProxyHandshakeCode(pHandshake, &iCode) ||
```


## 代理拨号

### `xrtNetProxyDialConfigInit`

初始化 TCP 拨号、64 KiB 协议上限和 30 秒全过程超时。

```c
void xrtNetProxyDialConfigInit(xnetproxydialconfig* pConfig);
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

[network/proxy_dial · 配置](../../examples/network/proxy_dial/main.c) · 观察

```c
	xrtNetProxyDialConfigInit(&DialConfig);
```


### `xrtNetProxyDial`

连接代理端点并完成目标 CONNECT；成功 Stream 引用转移给完成回调。

```c
xnetproxydial* xrtNetProxyDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xnetproxy* pProxy,
	cstr sTargetHost,
	uint16 iTargetPort,
	const xnetproxydialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xnetproxydialproc pDone,
	ptr pDoneData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络引擎 |
| `pResolver` | 输入 | 非空 | 解析器 |
| `pProxy` | 输入 | 非空 | 代理对象 |
| `sTargetHost` | 输入 | 非空 | 目标主机 |
| `iTargetPort` | 输入 | — | 目标端口 |
| `pConfig` | 输入 | 允许空 | 拨号配置 |
| `pStreamEvents` | 输入 | 允许空 | 流事件 |
| `pStreamData` | 输入 | 任意值 | 流用户数据 |
| `pDone` | 输入 | 非空 | 完成回调 |
| `pDoneData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | Dial 对象（终态后 Destroy） | — |
| `NULL` | 提交失败 | `xrt.proxy` 域错误 |

#### 错误

- `xrt.proxy` 域错误 — 提交失败；非 Worker 提交者可能与完成回调并发

#### 范例

[network/proxy_dial · 拨号](../../examples/network/proxy_dial/main.c) · 观察

```c
	pDial = xrtNetProxyDial(
		pEngine,
		pResolver,
		pProxy,
		argv[3],
		iTargetPort,
		&DialConfig,
		&StreamEvents,
		&Example,
		exampleProxyDialDone,
		&Example
	);
```


### `xrtNetProxyDialRef`

增加 Proxy Dial 引用并返回原指针。

```c
xnetproxydial* xrtNetProxyDialRef(xnetproxydial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_tour · 引用](../../examples/network/proxy_tour/main.c) · 观察

```c
	pDialRef = xrtNetProxyDialRef(pDial);
```


### `xrtNetProxyDialDestroy`

释放 Proxy Dial 引用；空指针视为空操作。

```c
void xrtNetProxyDialDestroy(xnetproxydial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[network/proxy_dial · 收尾](../../examples/network/proxy_dial/main.c) · 观察

```c
	xrtNetProxyDialDestroy(pDial);
```


### `xrtNetProxyDialCancel`

协作取消名称解析、TCP 连接或代理握手。

```c
bool xrtNetProxyDialCancel(xnetproxydial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 取消已受理 | — |
| `false` | 已终态或参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_dial · 取消](../../examples/network/proxy_dial/main.c) · 观察

```c
		(void)xrtNetProxyDialCancel(pDial);
```


### `xrtNetProxyDialState`

返回当前拨号阶段或不可变终态。

```c
xnetproxydialstate xrtNetProxyDialState(
	const xnetproxydial* pDial
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 允许空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 阶段/终态枚举 | 解析/连接/握手或终态 | 零值 = 参数非法 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_tour · 状态](../../examples/network/proxy_tour/main.c) · 观察

```c
		xnetproxydialstate State = xrtNetProxyDialState(pDial);
```


### `xrtNetProxyDialError`

失败或取消后借用完整错误原因链。

```c
const xerror* xrtNetProxyDialError(
	const xnetproxydial* pDial
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 允许空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 错误借用（含分层 cause 链） | — |
| `NULL` | 未失败或参数非法 | 纯查询 |

#### 错误

- 无 — 非失败状态返回空是查询结果

#### 范例

[network/proxy_tour · 失败](../../examples/network/proxy_tour/main.c) · 观察

```c
			(xrtNetProxyDialError(pDial) == NULL) ||
```


### `xrtNetProxyDialStats`

复制拨号统计快照（尝试次数等）。

```c
bool xrtNetProxyDialStats(
	const xnetproxydial* pDial,
	xnetproxydialstats* pStats
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | Dial 对象 |
| `pStats` | 输出 | 非空 | 接收拨号统计（尝试次数等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 统计已复制 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[network/proxy_tour · 统计](../../examples/network/proxy_tour/main.c) · 观察

```c
			!xrtNetProxyDialStats(pDial, &DialStats) ||
```



## 扩展库边界

客户端池、重定向、重试、缓存、认证、Cookie、MIME、Multipart、FormData、SSE、
服务器路由和中间件属于独立发布的 `xhttp` 扩展；它们不参与 XRT 核心构建、单头包或
兼容性承诺。

## 模块契约：线程

解析、编码与字段表 API 为无共享状态的纯函数或单拥有者对象，可任意线程并发调用（不同对象间）；同一对象（连接、字段表）由使用它的执行流独占。
