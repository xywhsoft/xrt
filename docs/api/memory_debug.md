# 内存调试 API

Memory Debug 在运行时跟踪每一次 XRT 分配：调用点登记、活动分配快照、双写/越界检测与流式报告；仅在诊断构建中启用。

## 启用与裁剪

定义 `XRT_FEATURE_MEMORY_DEBUG` 后，`xrtMalloc`、`xrtCalloc`、`xrtRealloc`、`xrtFree` 和 `xrtMemDup` 自动记录 `__FILE__`、`__LINE__`。未定义该宏时，调试块头、canary、事件、活动链表和隔离队列全部从构建中裁掉。

`XRT_MODULE_ALL` 默认包含内存调试。完整生产构建可以在首次包含 XRT 头之前定义
`XRT_EXCLUDE_MEMORY_DEBUG`，同时裁掉调试核心和报告层而保留 `memory_stats`。该排除
只影响 `XRT_MODULE_ALL` 的隐式选择；显式选择内存调试模块仍然优先。

文本和 JSON 报告由单独的 `XRT_FEATURE_MEMORY_DEBUG_REPORT` 控制。它依赖调试核心，但不依赖文件、字符串构建器或 JSON 模块；不需要报告时不会带入格式化代码。

调试构建默认开启运行时记录。运行时关闭只用于暂时停止统计，不会改变编译期内存布局或移除调试分配路径，不能代替 `XRT_EXCLUDE_MEMORY_DEBUG`。存在任何活动 XRT 分配时不能切换开关或重置状态。

故障注入同样只在调试构建存在。它按 `xrtMalloc`、`xrtCalloc`、非零 `xrtRealloc` 和 `xrtMemDup` 的逻辑调用计数，不受小对象池、线程缓存或 backing allocator 复用影响；状态属于当前线程，不会让并行任务随机失败。

## 检测范围

- 前后 canary 检测缓冲区下溢和溢出；释放时发现损坏会记录事件并设置 `XERR_STATE`。
- 池化块释放后填充 `0xDD`，复用前检测释放后写入。
- 大块释放后进入有界隔离队列，延迟归还底层分配器。
- 活动分配链表提供泄漏位置和大小。
- 临时 arena 记录分配、作用域回退、reset、当前字节和峰值字节。
- 非 XRT 地址先经过所有权查询，不盲目读取未知地址之前的内存。
- 事件历史固定保留最近 512 条，不因长时间运行无限增长。

并发调用是安全的。访问器在内部锁之外执行，可以正常输出日志；访问器收到的结构只在当前回调期间借用。

## 类型

### `xmemdebugeventkind`

事件包括分配、释放、重分配、重复释放、非法释放、上溢、下溢和释放后写入。

`XRT_MEMDEBUG_EVENT_LIMIT` 是固定事件历史容量，当前为 512。

### `xmemdebugevent`

`Sequence` 是严格递增序号；`Address`、`Size`、`File`、`Line` 描述事件现场。调用点字符串由 XRT 借用，直接调用 `At` API 时必须保证字符串在相关分配释放前有效。

### `xmemdebugallocation`

描述调用开始时仍然活动的一项分配，字段为地址、请求大小和分配位置。

### `xmemdebugsnapshot`

包含当前/峰值活动分配、隔离队列、各操作计数、各错误计数和当前事件数量。快照是同一锁临界区内的一致副本。

### `xmemdebugreportformat`

报告格式与具体输出目标解耦。

```c
typedef enum xmemdebugreportformat {
	XMEMDEBUG_REPORT_TEXT = 1,
	XMEMDEBUG_REPORT_JSON
} xmemdebugreportformat;
```

| 值 | 语义 |
|---|---|
| `XMEMDEBUG_REPORT_TEXT` | XMEMDEBUGREPORT文本 |
| `XMEMDEBUG_REPORT_JSON` | （见枚举语义） |

### `xmemdebugvisitor`

事件访问器返回 false 时停止遍历。

```c
typedef bool (*xmemdebugvisitor)(const xmemdebugevent* pEvent, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xmemdebugallocationvisitor`

活动分配访问器返回 false 时停止遍历。

```c
typedef bool (*xmemdebugallocationvisitor)(const xmemdebugallocation* pAllocation, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xmemdebugwriteproc`

报告写入器成功消费全部数据时返回 true。

```c
typedef bool (*xmemdebugwriteproc)(xbytesview Data, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 函数

### `xrtMemDebugEnable`

在没有活动分配时开启或关闭运行时内存调试记录。

```c
bool xrtMemDebugEnable(bool bEnable)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `bEnable` | 输入 | — | 目标开关状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已切换 | — |
| `false` | 存在活动分配 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 仍存在活动分配或临时内存字节

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 开关

```c
	(void)xrtMemDebugEnable(true);
```

### `xrtMemDebugEnabled`

原子读取运行时内存调试开关。

```c
bool xrtMemDebugEnabled(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 当前开关状态 | 不设错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 开关读取

```c
	printf("enabled=%d\n", xrtMemDebugEnabled() ? 1 : 0);
```

### `xrtMemDebugFailAfter`

当前线程允许指定次数成功分配后，让下一次逻辑分配失败一次。

```c
bool xrtMemDebugFailAfter(uint64 iSuccessfulAllocations)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSuccessfulAllocations` | 输入 | — | 放行的成功分配次数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 故障已武装 | — |
| `false` | 线程故障状态分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 线程局部故障状态惰性分配失败

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 故障注入

```c
	(void)xrtMemDebugFailAfter(1u);
```

### `xrtMemDebugFailClear`

清除当前线程尚未触发的分配故障。

```c
void xrtMemDebugFailClear(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清除 | — |

#### 错误

- 无 — 清除不失败

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 清除注入

```c
	xrtMemDebugFailClear();
```

### `xrtMemDebugFailTriggered`

返回当前线程最近配置的分配故障是否已经触发。

```c
bool xrtMemDebugFailTriggered(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否已触发 | 不设错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 触发查询

```c
		xrtMemDebugFailTriggered() ? 1 : 0);
```

### `xrtMemDebugReset`

在没有活动分配时清空统计、事件和隔离队列。

```c
bool xrtMemDebugReset(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已清空 | — |
| `false` | 存在活动分配 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 仍存在活动分配或临时内存字节

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 清空记录

```c
	(void)xrtMemDebugReset();
```

### `xrtMemDebugSnapshot`

获取字段相互一致的内存调试统计快照。

```c
void xrtMemDebugSnapshot(xmemdebugsnapshot* pSnapshot)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSnapshot` | 输出 | 非空 | 接收快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 快照已写出 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[debug](../../examples/memory/debug/main.c) · 统计快照

```c
	xrtMemDebugSnapshot(&tSnapshot);
```

### `xrtMemDebugVisit`

按时间顺序访问当前保留的有界调试事件。

```c
size_t xrtMemDebugVisit(xmemdebugvisitor pVisitor, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pVisitor` | 输入 | 非空 | 事件回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际访问的事件数；0 = 无事件或失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` / `XERR_OVERFLOW` — 事件快照捕获失败
- `XERR_STATE` — 快照在遍历中途失效（调试状态被关闭或清空）

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 事件遍历

```c
	(void)xrtMemDebugVisit(printEvent, &iEvents);
```

### `xrtMemDebugVisitLive`

访问内部锁线性化点捕获的完整活动分配快照。

```c
size_t xrtMemDebugVisitLive(xmemdebugallocationvisitor pVisitor, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pVisitor` | 输入 | 非空 | 分配回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际访问的分配数；0 = 无分配或失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` / `XERR_OVERFLOW` — 活动分配快照捕获失败

#### 范例

[debug](../../examples/memory/debug/main.c) · 活动分配遍历

```c
	(void)xrtMemDebugVisitLive(printAllocation, NULL);
```

### `xrtMemDebugEventName`

返回调试事件种类的稳定小写名称。

```c
cstr xrtMemDebugEventName(xmemdebugeventkind Kind)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | — | 事件种类 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | `"alloc"`、`"free"`、`"realloc"` 等静态名称 | — |
| `"unknown"` | 种类非法 | 不设错 |

#### 错误

- 无 — 非法种类返回 `"unknown"` 且不设置错误

#### 范例

[fail_inject](../../examples/memory/fail_inject/main.c) · 事件名称

```c
			xrtMemDebugEventName(pEvent->Kind));
```

### `xrtMemDebugReport`

把首次输出前分别捕获的统计、完整活动分配和有界事件流式写为文本或 JSON；写入器接收借用字节片段。

```c
bool xrtMemDebugReport(
	xmemdebugreportformat Format,
	xmemdebugwriteproc pWriter,
	ptr pUserData
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Format` | 输入 | `TEXT` 或 `JSON` | 报告格式 |
| `pWriter` | 输入 | 非空 | 字节写入回调 |
| `pUserData` | 输入 | 任意值 | 写入器数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完整写出 | — |
| `false` | 参数非法、捕获或写入失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` / `XERR_OVERFLOW` — 活动分配捕获失败
- `XERR_STATE` — 写入器返回失败但未设置更具体错误

#### 范例

[debug_report](../../examples/memory/debug_report/main.c) · 流式报告

```c
	bResult = xrtMemDebugReport(XMEMDEBUG_REPORT_JSON, writeReport, stdout);
```

调用点函数：`xrtMallocAt`、`xrtCallocAt`、`xrtReallocAt`、`xrtFreeAt`、`xrtMemDupAt` 是宏重定向的目标，也允许诊断工具直接调用；除调用点外，所有权和错误契约与普通函数一致（见 [memory.md](memory.md)）。

## 范例

完整范例位于 `examples/memory/debug/main.c`，并由 `python tools/build.py --suite memory_debug` 自动编译运行。

流式报告范例位于 `examples/memory/debug_report/main.c`，由 `python tools/build.py --suite memory_debug_report` 验证。

## 旧版资产决策

旧版 canary、512 条有界事件、256 块大对象隔离、分配点宏、文本/JSON 报告和临时 arena 事件均被保留并压实。报告从直接打开路径改为写入器分层，避免内存调试核心强依赖文件系统，也允许直接流向日志或网络。

旧版为每个容器 API 生成一组 `Dbg` 宏、在全局调试器中维护容器对象表的做法不再保留。分配点由统一内存宏覆盖；对象生命周期和错误状态由各对象模块自身的状态机负责，避免内存调试模块反向耦合全部容器。错误分配器释放仍由拥有该地址的分配器边界验证，通用堆把未知地址报告为 `XERR_ARGUMENT`。
