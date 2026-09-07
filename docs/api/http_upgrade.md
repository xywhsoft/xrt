# HTTP Upgrade

`<xrt/http_upgrade.h>` 实现 RFC 9110 `Upgrade` 字段的通用协议层，不绑定
WebSocket、HTTP 客户端、服务器或具体网络传输。

## 类型与常量

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

## 语法与借用

一个 `xhttpupgradeitem` 表示 `protocol-name[/protocol-version]`。`Protocol` 与
`Version` 都借用原字段值；空 `Version` 表示线路中没有斜杠和版本。名称与版本必须是
非空 HTTP token，斜杠两侧不允许空白。

`xrtHttpUpgradeParse` 解析单个元素。`xrtHttpUpgradeValid` 和
`xrtHttpUpgradeCount` 完整检查一个列表字段值。HTTP `#list` 允许空成员，因此前导、
连续和尾随逗号会被忽略；畸形的非空成员会失败。

## 迭代

`xrtHttpUpgradeNext` 在第一次发布条目前完整验证当前字段值。
`xrtHttpUpgradeFieldNext` 把全部重复 `Upgrade` 字段视为一份有序列表，并在第一次
发布前验证所有同名字段。这样后续坏字段不会让调用方先消费半份可信升级列表。

游标必须由对应的 `CursorInit` 函数初始化，输入在迭代结束前保持不变。游标、输出和
字段描述符支持未对齐存储；输出不能覆盖输入或游标。

协议名称按 RFC 使用 ASCII 大小写不敏感比较，可复用 `xrtHttpTokenEqual`；协议版本
是否区分大小写由具体协议定义。HTTP/1 解析器会拒绝任何畸形 `Upgrade` 字段，只有
列表至少包含一个协议时才发布 `XHTTP1_UPGRADE`。

## 写出

启用 `XRT_FEATURE_HTTP_UPGRADE_WRITE` 后：

- `xrtHttpUpgradeWrite` 规范生成 `name, name/version` 列表；
- `xrtHttpUpgradeElementWrite` 写一个元素；
- `xrtHttpUpgradeBuild` 一次分配零结尾文本，返回值由 `xrtFree` 释放。

直接写出支持空输出精确计长；短缓冲返回所需长度且不写部分结果。描述符、借用值、
长度输出和目标缓冲不得重叠。

## 裁剪

- `XRT_FEATURE_HTTP_UPGRADE`：解析、验证和重复字段迭代；依赖 `HTTP`；
- `XRT_FEATURE_HTTP_UPGRADE_WRITE`：直接写出与 Build；依赖 `HTTP_UPGRADE`。

示例位于 `examples/http/upgrade/main.c`。

## API

### 单值解析

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
|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
		xrtHttpUpgradeCursorInit(&UpCursor);
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · Upgrade](../../examples/http/small_fields/main.c) · 观察

```c
		while ( xrtHttpUpgradeNext(SV("websocket, h2c"),
				&UpCursor, &Upgrade) == XHTTP_NEXT_ITEM ) {
```


### 跨字段与写出

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
|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/upgrade · 字段游标](../../examples/http/upgrade/main.c) · 观察

```c
	xrtHttpUpgradeFieldCursorInit(&Cursor);
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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

## 模块契约：错误

失败经 `xrtGetError()` 报告：

| 域/种类 | 触发场景 |
|---|---|
| `XERR_ARGUMENT` / `XERR_RANGE` | 参数与字段数组容量 |
| `XERR_MEMORY` | 输出缓冲分配失败 |
| `xrt.http` 域错误 | 升级字段缺失、Accept 不匹配等协议错误 |

## 模块契约：线程

升级协商 API 均为无共享状态的纯函数，可任意线程并发调用。
