# 内存调试 API

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

## 函数

### `xrtMemDebugEnable`

在没有活动分配时开启或关闭运行时记录。违反生命周期约束返回 `false` 并设置 `XERR_STATE`。

### `xrtMemDebugEnabled`

返回当前运行时记录开关。

### `xrtMemDebugFailAfter`

允许当前线程继续成功执行指定次数的逻辑分配，然后让下一次逻辑分配返回 `NULL` 并设置 `XERR_MEMORY`。故障触发后自动解除；传入零表示下一次分配失败。常规编译器的状态不需要动态内存；TinyCC 首次创建系统 TLS 状态可能失败，此时函数返回 `false` 并保留错误。成功返回 `true` 后可用于确定性扫描 OOM 回滚路径。

### `xrtMemDebugFailClear`

清除当前线程尚未触发的分配故障及触发标志。测试离开故障区间前应显式调用。

### `xrtMemDebugFailTriggered`

返回当前线程最近一次 `xrtMemDebugFailAfter` 是否已经触发。该查询不改变故障状态，也不分配内存。

### `xrtMemDebugReset`

清空统计和事件，并立即释放隔离队列。存在活动分配时失败且不改变现有状态。

### `xrtMemDebugSnapshot`

复制统计快照。输出参数为 `NULL` 时设置 `XERR_ARGUMENT`。

### `xrtMemDebugVisit`

按事件序号访问当前有界历史。访问器返回 `false` 时停止，返回值是已经调用访问器的次数。

### `xrtMemDebugVisitLive`

访问内部锁线性化点捕获的完整活动分配快照。并发增长超过预分配容量时会重新申请并重试，不会静默截断；实现使用底层分配器，不会递归进入 XRT 调试分配路径。

### `xrtMemDebugEventName`

返回稳定的小写事件名称，未知枚举返回 `unknown`，适合日志、报告和测试使用。

### `xrtMemDebugReport`

启用 `XRT_FEATURE_MEMORY_DEBUG_REPORT` 后，把首次输出前分别捕获的统计、完整活动分配和有界事件写为文本或 JSON。写入器接收借用的字节片段，可以直接连接文件、网络、日志系统或调用方缓冲；报告层不替调用方选择存储位置。三组数据各自具有明确的线性化点，但不承诺组成一个跨锁原子的全局事务快照。

报告在第一次调用写入器前完成快照，因此写入器自身产生的 XRT 分配不会进入本次报告。写入器返回 `false` 时报告立即停止；写入器没有设置更具体错误时，XRT 设置 `XERR_STATE`。

### `xrtMallocAt` 等调用点函数

`xrtMallocAt`、`xrtCallocAt`、`xrtReallocAt`、`xrtFreeAt`、`xrtMemDupAt` 是宏重定向的目标，也允许诊断工具直接调用。除调用点外，所有权和错误契约与普通函数一致。

## 范例

完整范例位于 `examples/memory/debug/main.c`，并由 `python tools/build.py --suite memory_debug` 自动编译运行。

流式报告范例位于 `examples/memory/debug_report/main.c`，由 `python tools/build.py --suite memory_debug_report` 验证。

## 旧版资产决策

旧版 canary、512 条有界事件、256 块大对象隔离、分配点宏、文本/JSON 报告和临时 arena 事件均被保留并压实。报告从直接打开路径改为写入器分层，避免内存调试核心强依赖文件系统，也允许直接流向日志或网络。

旧版为每个容器 API 生成一组 `Dbg` 宏、在全局调试器中维护容器对象表的做法不再保留。分配点由统一内存宏覆盖；对象生命周期和错误状态由各对象模块自身的状态机负责，避免内存调试模块反向耦合全部容器。错误分配器释放仍由拥有该地址的分配器边界验证，通用堆把未知地址报告为 `XERR_ARGUMENT`。
