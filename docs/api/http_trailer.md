# HTTP Trailer

`http_trailer` 是独立于 HTTP/1 分帧器的 Trailer 协议层。它只依赖基础 HTTP
字段能力，供底层解析器、客户端、服务器、代理和应用直接复用。

## 字段声明

`xrtHttpTrailerCount` 完整验证全部重复 `Trailer` 字段行，并返回声明名称总数。
字段值使用 token-list；按 RFC 列表扩展规则，空值表示零个声明。每个名称还必须
通过通用 trailer 策略。输出在失败时保持为零，后置畸形字段不会造成部分发布。

`xrtHttpTrailerFind` 完整验证全部声明后返回三态结果：

- `XHTTP_NEXT_ITEM`：名称已经声明；
- `XHTTP_NEXT_END`：声明有效但不包含该名称；
- `XHTTP_NEXT_ERROR`：查询名称或任一 `Trailer` 字段无效。

需要逐项处理时，可直接使用基础层 `xrtHttpFieldTokenNext`，字段名传入
`Trailer`；再用 `xrtHttpTrailerNameValid` 应用发送策略。

## 实际尾字段

`xrtHttpTrailerSectionValid` 校验实际 trailer section。它同时验证每个字段名是否
允许在线路末尾发送，以及字段值是否满足通用 HTTP 字段值语法。客户端、服务器、
HTTP/1 chunked 写出器和声明构建器共用这一入口，不再分别维护名称循环。

Header section 与 trailer section 必须保持分离。`Trailer` 声明只表示可能出现的
名称，声明缺少实际字段不是协议错误，实际字段未被提前声明也不应由协议解析层直接
判为错误；需要更严格对应关系的应用可用 `xrtHttpTrailerFind` 实施自己的策略。

## 发送策略

`xrtHttpTrailerNameValid` 拒绝会改变消息分帧、路由、请求条件、响应控制、认证或
正文解释的字段。返回 `true` 仍不代表某个扩展字段的定义允许它出现在 trailer；
发送方必须同时遵守该字段自己的规范。

`xrtHttpTrailerNamesWrite` 从实际 `xhttpfield[]` 生成声明值。它按 ASCII
大小写不敏感规则去重，保留第一次出现的名称与线路顺序；空输出可查询精确长度，
写入路径不分配内存，也不接受与输入描述符或借用文本重叠的输出。实际字段名称和值
会先经过 `xrtHttpTrailerSectionValid`，因此声明构建成功后不会在 chunk-end 写出阶段
才发现无效字段值。

`xrtHttpTrailerNamesBuild` 是一次分配便利层，返回零结尾文本并通过 `pSize` 返回
不含终止零的长度。返回值由 `xrtFree` 释放。

```c
#include <xrt/http_trailer.h>
#include <xrt/memory.h>

static const xhttpfield Trailers[] = {
	{ XRT_STR_INIT("Content-Digest"), XRT_STR_INIT("sha-256=:...:") },
	{ XRT_STR_INIT("X-Result"), XRT_STR_INIT("complete") }
};

size_t iSize;
str sNames = xrtHttpTrailerNamesBuild(Trailers, 2u, &iSize);
```

HTTP/1 的 chunked 解析和末块写出仍由 `http1_body` 负责；客户端与服务器准备层
使用本模块生成 `Trailer` Header，不再各自实现拼接逻辑。

## 裁剪与边界

- 宏：`XRT_MODULE_HTTP_TRAILER`；
- 功能：`XRT_FEATURE_HTTP_TRAILER`；
- 直接依赖：`http`；
- section 校验、声明统计、查询和缓冲写入零分配；
- 测试覆盖重复字段、后置错误、禁止名称、大小写去重、未对齐描述符、重叠输出、
  短缓冲、OOM、单头发布和超过 1 KiB 的动态声明。

协议依据：[RFC 9110 Trailer Fields](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.5)、
[RFC 9110 Trailer](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.6.2)、
[RFC 9112 Chunked Trailer Section](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1.2)
以及 [RFC 9530 Digest Fields](https://www.rfc-editor.org/rfc/rfc9530.html)。

## API

### 名称校验

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
|---|---|---|
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
|---|---|---|
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


### 声明生成与查询

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
|---|---|---|
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
|---|---|---|
| `XHTTP_NEXT_ITEM` | 已产出一项 | — |
| `XHTTP_NEXT_END` | 遍历结束 | — |
| `XHTTP_NEXT_ERROR` | 失败 | — |

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
|---|---|---|
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
|---|---|---|
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

## 模块契约：线程

Trailer 解析 API 为无共享状态的纯函数，可任意线程并发调用。
