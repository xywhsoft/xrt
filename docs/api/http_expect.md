# HTTP Expect

`<xrt/http_expect.h>` 实现 RFC 9110 `Expect` 字段的传输无关协议层。它不绑定
HTTP 客户端、服务器或网络对象，直接路径不分配内存。

## 元素

`xhttpexpectation` 借用完整 `Element`、大小写不敏感的 `Name`、可选线路 `Value`
以及从首个分号开始的原始 `Parameters`。`Value` 保留 token 或包含双引号的
quoted-string，`XHTTP_EXPECT_VALUE_QUOTED` 明确区分两者；调用方可以原样转发，或在
启用 HTTP 参数层时用 quoted-string API 解码。

解析接受 `expectation = token [ "=" ( token / quoted-string ) parameters ]`。
等号两侧不允许空白；参数允许 RFC 定义的空分号成员，quoted-string 内的逗号、分号和
quoted-pair 不会被误当作外层分隔符。

## 列表与重复字段

`xrtHttpExpectNext` 迭代一个字段值，`xrtHttpExpectFieldNext` 按线路顺序迭代全部重复
`Expect` 字段。两种游标都必须由初始化函数建立，并在发布第一项前完成输入全集校验；
畸形后缀不会留下已消费的半份结果。输入在迭代期间必须保持不变。

`xrtHttpExpectValid` 和 `xrtHttpExpectCount` 验证单字段列表。HTTP `#list` 的空成员被
忽略，因此空值及首尾、连续逗号可形成零项或较短列表。描述符、游标和输出均支持未对齐
存储，输出不能覆盖输入或游标。

## 四态分类

`xrtHttpExpectFields` 完整验证全部重复字段并返回：

- `XHTTP_EXPECT_NONE`：没有非空 expectation；
- `XHTTP_EXPECT_CONTINUE`：全部元素都是无值、无参数的 `100-continue`；
- `XHTTP_EXPECT_UNSUPPORTED`：语法有效，但至少含一个扩展 expectation；
- `XHTTP_EXPECT_ERROR`：字段或语法错误。

服务器可把 `UNSUPPORTED` 映射为 417，同时仍允许应用在更低层迭代并实现扩展；客户端可
明确拒绝自己不会执行的 expectation。HTTP/1.0 服务器按 RFC 忽略标准
`100-continue`；HTTP/1.1 服务器只有在正文计划为非零定长或 chunked 时才发布
继续握手事实，零正文不会触发 100 响应。

## 范例

参见 `examples/http/expect/main.c`。

## API

### 单值解析

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


### 跨字段与分类

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

## 模块契约：线程

Expect 语义 API 为无共享状态的纯函数，可任意线程并发调用。
