# 临时内存 API

## 分层模型

临时内存提供两层 API：`xtemparena` 是可显式放入对象、请求或协程上下文的原语；`xrtTemp`、`xrtTempCurrent` 和 `xrtTempClear` 使用当前执行上下文的默认 arena，覆盖常见的一行式临时分配。

每个原生线程通过 Windows FLS 或 POSIX pthread TLS 拥有独立默认 arena，并在线程退出时自动释放。协程和任务调度器通过内部上下文切换绑定自己的 arena，因此临时指针可以跨 yield 保持，但不会污染宿主线程。

## 分配策略

默认常规块为 4096 字节，请求对齐到 16 字节。对齐后大于 2048 字节的请求使用独立 spill 块；spill 在作用域结束或 reset 时立即释放。常规块供后续复用，reset 后默认最多保留 65536 字节，避免偶发峰值永久滞留。

`xrtTempAlloc(0)` 返回可释放但不可解引用的零尺寸临时地址。所有临时地址在所属作用域结束、arena reset、trim、unit 或执行上下文退出后失效。

显式 arena 不包含并发锁，同一时间只能由一个执行上下文操作。不同 arena 可以并发使用。

## 类型

### `xtempconfig`

`BlockSize` 是新常规块容量，`SpillLimit` 是进入独立大块的阈值，`RetainLimit` 是 reset 后常规块保留上限。前两项必须非零，`RetainLimit` 可以为零。

### `xtemparena`

可栈上分配的 arena 状态。显式调用 `xrtTempInit` 前无需预置字段；使用惰性 API 时必须零初始化。结束使用后调用 `xrtTempUnit`。

### `xtempmark`

保存作用域回退位置和不可复用的作用域标识。作用域必须在同一 arena 上严格后进先出结束。复制 mark 不会复制结束权；旧 mark 即使遇到相同嵌套深度也不能结束后续作用域。重复结束已经成功结束的同一个 mark 是幂等操作。

### `xtempinfo`

包含常规块数、spill 数、保留字节、当前/峰值用量、reset 次数和作用域深度。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_TEMP_BLOCK_SIZE_DEFAULT` | `4096u` | 阻塞策略尺寸默认值 |
| `XRT_TEMP_SPILL_LIMIT_DEFAULT` | `2048u` | SPILL超限默认值 |
| `XRT_TEMP_RETAIN_LIMIT_DEFAULT` | `65536u` | RETAIN超限默认值 |

### `xtempblock`

```c
typedef struct xtempblock xtempblock;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

## 函数

### `xrtTempInit`

使用默认或指定配置初始化一个空 arena。

```c
bool xrtTempInit(xtemparena* pArena, const xtempconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输出 | 非空 | 接收 arena |
| `pConfig` | 输入 | 允许空 | arena 配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数或配置非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[temp](../../examples/memory/temp/main.c) · 初始化

```c
	if ( !xrtTempInit(&tArena, &tConfig) ) {
```

### `xrtTempUnit`

释放 arena 持有的全部常规块和 spill 块。

```c
void xrtTempUnit(xtemparena* pArena)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 释放

```c
		xrtTempUnit(&tArena);
```

### `xrtTempSecureUnit`

安全擦除 arena 持有的全部用户区，再释放所有内存。

```c
void xrtTempSecureUnit(xtemparena* pArena)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已擦除并释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 安全释放

```c
	xrtTempSecureUnit(&tArena);
```

### `xrtTempAlloc`

从指定 arena 分配一段 16 字节对齐的临时内存。

```c
ptr xrtTempAlloc(xtemparena* pArena, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空 | 目标 arena |
| `iSize` | 输入 | — | 请求字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 16 字节对齐临时内存，下次重置前有效 | — |
| `NULL` | 分配失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 尺寸计算溢出
- `XERR_MEMORY` — 新块分配失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 分配

```c
	if ( xrtTempAlloc(&tArena, 64) == NULL ) {
```

### `xrtTempDup`

把二进制数据复制到指定 arena。

```c
ptr xrtTempDup(xtemparena* pArena, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空 | 目标 arena |
| `pData` | 输入 | 非空 | 源数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 16 字节对齐临时内存，下次重置前有效 | — |
| `NULL` | 分配失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 尺寸计算溢出
- `XERR_MEMORY` — 新块分配失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 复制数据

```c
	pData = xrtTempDup(&tArena, arrData, sizeof(arrData));
```

### `xrtTempStr`

把字符串视图复制为指定 arena 中的零结尾字符串。

```c
str xrtTempStr(xtemparena* pArena, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空 | 目标 arena |
| `Text` | 输入 | 借用 | 源文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾字符串借用，下次重置前有效 | — |
| `NULL` | 分配失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 尺寸计算溢出
- `XERR_MEMORY` — 新块分配失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 复制字符串

```c
	sInner = xrtTempStr(pArena, XRT_STR_LITERAL("promoted"));
```

### `xrtTempReset`

回收全部临时分配并保留配置允许的常规块。

```c
bool xrtTempReset(xtemparena* pArena)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空 | 目标 arena |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已重置 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 存在未结束的作用域

#### 范例

[temp](../../examples/memory/temp/main.c) · 重置

```c
	if ( !xrtTempReset(&tArena) || !xrtTempTrim(&tArena, 0) ) {
```

### `xrtTempSecureReset`

安全擦除 arena 持有的全部用户区，再执行普通重置。

```c
bool xrtTempSecureReset(xtemparena* pArena)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空 | 目标 arena |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已擦除并重置 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 存在未结束的作用域

#### 范例

[temp](../../examples/memory/temp/main.c) · 安全重置

```c
	if ( !xrtTempSecureReset(&tArena) ) {
```

### `xrtTempTrim`

在 arena 空闲时将常规块缩减到指定保留字节数。

```c
bool xrtTempTrim(xtemparena* pArena, size_t iRetainBytes)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空、空闲 | 目标 arena |
| `iRetainBytes` | 输入 | — | 保留字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已裁剪 | — |
| `false` | 忙碌或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — arena 存在活动分配或作用域

#### 范例

[temp](../../examples/memory/temp/main.c) · 裁剪

```c
	if ( !xrtTempReset(&tArena) || !xrtTempTrim(&tArena, 0) ) {
```

### `xrtTempGet`

获取 arena 当前状态。

```c
void xrtTempGet(const xtemparena* pArena, xtempinfo* pInfo)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空 | 目标 arena |
| `pInfo` | 输出 | 非空 | 接收状态快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 快照已写出 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[temp](../../examples/memory/temp/main.c) · 状态查询

```c
	xrtTempGet(&tArena, &tInfo);
```

### `xrtTempBegin`

建立一个必须后进先出结束的临时作用域。

```c
xtempmark xrtTempBegin(xtemparena* pArena)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArena` | 输入 | 非空 | 目标 arena |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 作用域标记 | 用于 `xrtTempEnd*` 回退 | — |
| 零值 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[temp](../../examples/memory/temp/main.c) · 建立作用域

```c
	tScope = xrtTempBegin(pArena);
```

### `xrtTempEnd`

回退作用域内产生的临时分配。

```c
bool xrtTempEnd(xtempmark* pMark)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMark` | 输入/输出 | 非空、后进先出 | 目标标记 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已回退 | — |
| `false` | 顺序违反或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 违反后进先出顺序

#### 范例

[temp](../../examples/memory/temp/main.c) · 结束作用域

```c
	if ( !xrtTempEnd(&tScope) ) {
```

### `xrtTempEndDup`

结束作用域并把二进制结果复制到父作用域。

```c
ptr xrtTempEndDup(xtempmark* pMark, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMark` | 输入/输出 | 非空、后进先出 | 目标标记 |
| `pData` | 输入 | 非空 | 源数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 16 字节对齐父作用域临时内存，下次重置前有效 | — |
| `NULL` | 分配失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 违反后进先出顺序
- `XERR_OVERFLOW` — 尺寸计算溢出
- `XERR_MEMORY` — 新块分配失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 结束并复制数据

```c
	pData = xrtTempEndDup(&tScope, arrData, sizeof(arrData));
```

### `xrtTempEndStr`

结束作用域并把字符串结果复制到父作用域。

```c
str xrtTempEndStr(xtempmark* pMark, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMark` | 输入/输出 | 非空、后进先出 | 目标标记 |
| `Text` | 输入 | 借用 | 源文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 父作用域零结尾字符串借用 | — |
| `NULL` | 复制失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 违反后进先出顺序
- `XERR_OVERFLOW` — 尺寸计算溢出
- `XERR_MEMORY` — 新块分配失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 结束并复制字符串

```c
	sPromoted = xrtTempEndStr(&tScope, (xstrview){ sInner, 8 });
```

## 当前上下文便捷层

### `xrtTempCurrent`

返回当前原生线程或协程绑定的默认 arena。

```c
xtemparena* xrtTempCurrent(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 默认 arena 借用 | — |
| `NULL` | 无绑定上下文 | 不设错误 |

#### 错误

- 无错误 — 外部线程首次使用前返回 `NULL` 且不设置错误

#### 范例

[temp](../../examples/memory/temp/main.c) · 当前 arena

```c
	xtemparena* pArena = xrtTempCurrent();   /* 线程默认 arena */
```

### `xrtTemp`

从当前执行上下文的默认 arena 分配临时内存。

```c
ptr xrtTemp(size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSize` | 输入 | — | 请求字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 16 字节对齐临时内存，本上下文下次重置前有效 | — |
| `NULL` | 分配失败 | 见错误 |

#### 错误

- 无默认 arena 时按需建立
- `XERR_OVERFLOW` — 尺寸计算溢出
- `XERR_MEMORY` — 新块分配失败

#### 范例

[temp](../../examples/memory/temp/main.c) · 上下文分配

```c
	sOuter = (char*)xrtTemp(32);
```

### `xrtTempClear`

重置当前执行上下文的默认 arena。

```c
bool xrtTempClear(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已重置 | — |
| `false` | 无绑定上下文或状态非法 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 当前上下文没有默认 arena 或存在未结束作用域

#### 范例

[temp](../../examples/memory/temp/main.c) · 上下文重置

```c
	if ( !xrtTempClear() ) {
```

## 旧版资产决策

旧版 arena 的常规块复用、大请求 spill、嵌套作用域、字符串结果提升、线程隔离、协程跨 yield 保留以及调试事件全部保留。新版把这些能力从线程运行时私有结构中提取为可公开嵌入的 `xtemparena`，默认上下文只作为上层便捷入口。

新版补充了有界保留、显式 `trim`、二进制与字符串复制、通用结果提升、完整诊断信息、算术溢出检查和不可复用的作用域标识。旧 mark 只校验嵌套深度，复制后可能错误结束未来同深度作用域；新版使用单调作用域编号消除该状态漏洞。

旧范例中关于固定槽位回绕和地址复用的展示不再保留。地址是否复用不是公共契约，而且会诱导调用方在 reset 后继续观察失效指针；新版范例只展示明确的生命周期、结果提升和主动归还容量。

## 范例

完整范例位于 `examples/memory/temp/main.c`，并由 `python tools/build.py --suite temp_memory` 自动编译运行。
