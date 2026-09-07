# HTTP Connection

`xrt/http_connection.h` 提供 HTTP `Connection` 字段的解析和 HTTP/1 持久性判断。
协议层不依赖客户端、服务器、代理或网络对象，可直接处理借用的 `xhttpfield` 数组。

## 类型与常量

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
| `XHTTP_CONNECTION_CLOSE` | 协商了 close |

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
| `XHTTP_CONNECTION_RESPONSE` | 响应保留连接 |
| `XHTTP_CONNECTION_PROXY` | 代理保留连接 |

## 裁剪

- 模块：`http_connection`
- 功能宏：`XRT_FEATURE_HTTP_CONNECTION`
- 直接依赖：`http`

## 选项迭代

`Connection` 使用 RFC 9110 的 `#connection-option` 语法。空字段、仅含 OWS 的字段和仅含空
列表成员的字段都表示零个选项；非空成员必须是 HTTP token。

`xrtHttpConnectionCursorInit` 初始化通用的 `xhttpfieldtokencursor`，
`xrtHttpConnectionNext` 按线路顺序遍历全部重复字段。第一次发布选项前会验证所有
`Connection` 字段。游标会绑定原字段数组、字段数量和字段名称，不能在迭代中切换输入；
输入存储在游标结束前必须保持有效且不变。

返回的 `xstrview` 借用原字段值。迭代不分配内存，返回值为：

- `XHTTP_NEXT_ITEM`：发布一个选项；
- `XHTTP_NEXT_END`：正常结束，同时清空输出视图；
- `XHTTP_NEXT_ERROR`：参数、游标或字段语法错误。

`xrtHttpConnectionCount` 完整验证并统计选项。
`xrtHttpConnectionFind` 完整验证后执行大小写不敏感的单项查询。
底层通用能力分别是 `xrtHttpFieldTokenCount`、`xrtHttpFieldTokenFind` 和
`xrtHttpFieldTokenNext`。

```c
static const xhttpfield fields[] = {
	{ XRT_STR_INIT("Connection"), XRT_STR_INIT("keep-alive") },
	{ XRT_STR_INIT("Connection"), XRT_STR_INIT("TE") }
};
size_t count;

if ( !xrtHttpConnectionCount(fields, 2u, &count) ) {
	return false;
}
if ( xrtHttpConnectionFind(
	fields, 2u, XRT_STR_LITERAL("te")
) == XHTTP_NEXT_ITEM ) {
	/* 对端声明了 TE 逐跳选项。 */
}
```

## 持久连接

`xrtHttpConnectionPersistence` 实现 RFC 9112 的 HTTP/1 持久性判断：

- `close` 始终返回 `XHTTP_CONNECTION_CLOSE`；
- 没有 `close` 的 HTTP/1.1 默认返回 `XHTTP_CONNECTION_PERSIST`；
- HTTP/1.0 默认关闭；
- HTTP/1.0 只有设置 `XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE`、消息包含
  `keep-alive`，且接收方不是代理或消息是响应时才持久；
- 非法版本、未知标志或非法字段返回 `XHTTP_CONNECTION_ERROR`。

消息为响应时设置 `XHTTP_CONNECTION_RESPONSE`，接收方为代理时设置
`XHTTP_CONNECTION_PROXY`。允许 HTTP/1.0 Keep-Alive 是显式本地策略，库不会默认开启。

```c
xhttpconnectionstatus status = xrtHttpConnectionPersistence(
	XHTTP_VERSION_1_1, fields, 2u, 0);

if ( status == XHTTP_CONNECTION_ERROR ) {
	return false;
}
if ( status == XHTTP_CONNECTION_CLOSE ) {
	/* 完成当前响应后优雅关闭。 */
}
```

连接持久还要求消息具有自描述长度并被完整消费；该函数只判断版本和 `Connection` 字段，
不会替代 HTTP/1 消息分帧或传输状态检查。

## API

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



## 分层复用

- 规范写出选项数组使用 `xrtHttpTokenListWrite` 或 `xrtHttpTokenListBuild`；
- 逐跳字段移除策略使用 `xrtHttpHopField`；
- `TE`、`Upgrade` 等具体协议模块负责校验对应选项与字段是否配套；
- 传输层负责半关闭、排空和最终关闭，不由本协议层操作套接字。

## 内存与错误

- 字段数组、字段值、游标和输出描述符支持未对齐存储；
- 游标和输出不得覆盖字段描述符或任一借用视图；
- Count 失败时输出为零，Next 失败时不推进游标并清空输出视图；
- 所有解析、统计、查找和持久性判断都不分配内存；
- 详细错误由线程错误槽提供。

协议依据：[RFC 9110 Section 7.6.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.6.1)
和 [RFC 9112 Section 9.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3)。

## 验证资产

- `tests/http/test_http_connection.c`
- `tests/http/test_http_connection_noalloc.c`
- `tests/single/test_single_http_connection.c`
- `examples/http/connection/main.c`
