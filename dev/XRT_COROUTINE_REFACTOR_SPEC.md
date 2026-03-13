# XRT Coroutine Refactor Specification

## 1. 文档控制

- 文档路径: `dev/XRT_COROUTINE_REFACTOR_SPEC.md`
- 状态: Active draft
- 版本: `0.1.0`
- 最后更新: `2026-03-13`
- 范围: XRT 协程运行时、协程调度器、上下文后端、与 runtime/threading/xnet 的集成边界
- 相关文档:
- `dev/XRT_RUNTIME_PHASE1_SPEC.md`
- `dev/XRT_RUNTIME_PHASE2_SPEC.md`
- `dev/XRT_RUNTIME_THREADING_SUMMARY.md`
- `dev/XNET_V2_SPEC.md`

### 1.1 目的

本文件用于冻结协程重构的目标状态，防止后续实现过程中发生设计漂移。

它记录：

- 协程在 `xrt` 中的产品定位
- 与线程 runtime 的关系
- 生命周期和所有权规则
- 后端选择策略
- 调度器模型
- API 分层
- 测试、验收和里程碑

如果后续工作在新的对话或较长暂停后继续，应先恢复本文件上下文，再进入实现。

### 1.2 更新规则

以下规则是强制的：

1. 任何实质性的协程设计变化都必须同步更新本文件。
2. 任何完成的里程碑都必须更新进度区。
3. 任何影响范围、兼容性、性能或后端策略的决策都必须记录到决策日志。
4. 每次协程相关工作结束后，都应在工作日志追加简短记录。
5. 在达到本文件的验收标准前，协程模块不视为稳定。

### 1.3 状态图例

- `todo`: 未开始
- `in_progress`: 正在进行
- `blocked`: 被依赖项、设计问题或 bug 阻塞
- `done`: 已完成并通过验收
- `deferred`: 明确推迟到更后阶段


## 2. 重构摘要

现有协程实现可以通过基础 happy-path 测试，但它仍属于“原型级 stackful coroutine”：

- 当前协程状态和主上下文是进程级静态变量，而不是线程级状态
- Windows 后端依赖 Fiber，且当前实现对基础设施库场景过于侵入
- 生命周期契约不完整，销毁、调度器引用、错线程使用都没有严格保护
- 调度器只有 `yield + sleep` 级别能力，没有 event loop、I/O、cancellation、deadline、join 语义
- 部分汇编后端缺少完整 ABI 审计
- 测试默认未接入主测试流，也没有覆盖高风险场景

如果 `xrt` 要成为“互联网 + AI 时代的 C 语言基础设施库”，协程必须达到以下级别：

- 与线程 runtime 模型完全一致
- 明确的线程绑定和 ownership 语义
- 明确的生命周期和取消语义
- 可与网络事件循环和 future/task 层整合
- 后端可审计、可测试、可在生产中解释


## 3. 目标

### 3.1 产品目标

协程系统必须满足以下产品目标：

- 对常见使用者保持简单
- 对基础设施层保持严格、可预测、可审计
- 默认适合单线程 event-loop 内的高并发任务编排
- 能与线程并存，而不是和线程模型相互冲突
- 能作为未来 xnet/xserver/AI Agent 执行链路的调度基础

### 3.2 技术目标

- 将协程运行时状态纳入线程状态，而不是进程全局静态变量
- 明确“协程严格绑定一个线程和一个调度器”的规则
- 重写协程生命周期，避免 destroy/UAF/错线程 resume 等不确定行为
- 将当前 demo 级调度器升级为 ready-queue + timer + waiter 的真正 runtime
- 将 `sleep` 从忙轮询方案升级为 deadline/timer 驱动方案
- 设计标准化的 I/O 等待接口，以便后续和 xnet/xhttp2/xws2 等模块整合
- 提供严格的 backend 策略，不再默认信任弱兼容后端
- 将协程测试提升为正式回归测试的一部分

### 3.3 成功标准

协程重构只有在满足以下条件时才算成功：

1. 每个线程都有独立的协程 runtime 状态
2. 跨线程误用会被明确拒绝，而不是静默工作
3. destroy / cancel / join / scheduler 拥有清晰的生命周期契约
4. 所有受支持 backend 都经过 ABI 级别验证
5. `sleep`、timer、scheduler 不再依赖 1ms 空转轮询
6. 协程能和 xnet/event-loop 的 wait source 整合
7. 默认测试流能覆盖高风险场景


## 4. 非目标

本轮协程重构不包含以下目标：

- 不做语言级 `async/await` 语法系统
- 不做抢占式协程
- 不把协程设计成跨线程可迁移对象
- 不在第一阶段把所有网络 API 都改写成协程接口
- 不直接引入 GC、异常系统或脚本 VM


## 5. 设计约束

- 必须兼容现有 runtime thread model
- 不能重新引入进程级可变共享状态作为核心调度状态
- local fast path 不能因协程引入无谓锁开销
- public API 要尽量保留 xrt 一贯的直接、轻量风格
- 但对于不安全或歧义过大的旧接口，允许行为收紧
- 协程后端必须优先选择可审计、可控、对宿主副作用小的方案


## 6. 设计原则

1. 线程绑定优先于“看起来灵活”
2. 生命周期清晰优先于“用起来省一步”
3. backend 可审计优先于“任何平台都偷偷 fallback”
4. scheduler 应该是 runtime，不是 demo helper
5. shared wait / wake 必须通过 queue/post 机制，不允许外线程直接 resume чужой coroutine
6. 兼容层可以保留，但规范和主实现必须先正确


## 7. 产品定位

新的协程系统应被定位为：

- 单线程内的 stackful task runtime
- 线程绑定的 cooperative scheduler
- future / timer / I/O wait 的宿主执行层
- 与线程互补，而不是线程替代品

线程和协程的标准分工如下：

- 线程负责 CPU 并行、worker 隔离、阻塞边界
- 协程负责单线程内高并发任务编排、等待、取消、超时和 structured work

换句话说，协程不应该被设计成“比线程轻一点的另一个线程”，而应该是“绑定在线程上的任务执行层”。


## 8. 线程与所有权模型

### 8.1 基本规则

新的协程系统采用以下硬规则：

- 一个 `xcoro` 只属于一个线程
- 一个 `xcosched` 只属于一个线程
- 一个协程创建后不得迁移到其他线程
- 外线程只能通过 `post/wake` 投递事件，不能直接 `resume` 另一个线程的协程

### 8.2 线程级协程运行时

协程运行时状态必须放入 `xrtThreadData`，而不是使用进程级静态变量。

建议的数据结构如下：

```c
typedef struct xco_runtime {
	xthread pOwnerThread;
	uint64 iOwnerTid;
	xcoro pCurrent;
	xcoro pRoot;
	xcosched* pDefaultSched;
	uint32 iBackendFlags;
	ptr pBackendMain;
	ptr pBackendAux;
} xco_runtime;
```

其中：

- `pCurrent` 表示当前线程正在运行的协程
- `pRoot` 表示当前线程的根协程
- `pDefaultSched` 表示当前线程默认调度器
- `pBackendMain` / `pBackendAux` 用于保存主上下文或 backend 私有句柄

### 8.3 attach 规则

协程 API 属于 runtime-bound API。

要求如下：

- 当前线程必须已 attach 到 xrt runtime
- 如果线程未 attach，协程 create/resume/scheduler API 应明确报错
- `xrtThreadCreate()` 创建的线程可直接使用协程 API
- 外部线程必须先 attach，再使用协程系统


## 9. 协程数据模型

### 9.1 `xcoro`

建议的目标结构如下：

```c
typedef struct xcoro_struct {
	uint32 iState;
	uint32 iFlags;
	uint64 iId;
	uint64 iOwnerTid;
	xcosched* pSched;
	xcoro pCaller;
	xcoro pNextReady;
	xcoro pPrevAll;
	xcoro pNextAll;
	xco_entry pfnEntry;
	ptr pParam;
	ptr pUserData;
	ptr pResult;
	int64 iExitCode;
	int64 iDeadline;
	xco_stack tStack;
	xco_context tCtx;
	xco_wait tWait;
	xco_cancel tCancel;
	xco_cleanup* pCleanupTop;
} xcoro_struct, *xcoro;
```

关键点：

- `iOwnerTid` 固定线程归属
- `pSched` 固定所属调度器
- `pCaller` 支持调用链或 scheduler 恢复链
- `tWait` 描述当前等待源
- `tCancel` 支持取消状态和传播
- `pCleanupTop` 支持协程退出清理

### 9.2 `xcosched`

建议的目标结构如下：

```c
typedef struct xcosched {
	xrtThreadData* pThreadData;
	uint64 iOwnerTid;
	xcoro pCurrent;
	xcoro pRoot;
	xcoro pReadyHead;
	xcoro pReadyTail;
	xco_timer_heap tTimers;
	xco_wait_table tWaiters;
	xco_post_queue tPosts;
	xcoro pAllHead;
	uint32 iRunnable;
	uint32 iAlive;
	uint32 iFlags;
} xcosched;
```

关键点：

- scheduler 是线程私有对象
- ready queue、timer heap、post queue 是核心三件套
- `pAllHead` 便于统一遍历、cancel、destroy、debug dump

### 9.3 协程状态

建议的新状态集合：

- `XRT_CO_NEW`
- `XRT_CO_READY`
- `XRT_CO_RUNNING`
- `XRT_CO_WAIT_TIMER`
- `XRT_CO_WAIT_IO`
- `XRT_CO_WAIT_EVENT`
- `XRT_CO_WAIT_JOIN`
- `XRT_CO_DONE`
- `XRT_CO_CANCELLED`
- `XRT_CO_CLOSED`

说明：

- 现有 `SUSPENDED` 过于模糊，无法区分“主动 yield”与“等待某种外部事件”
- 新状态必须能直接表达 runtime 调度原因


## 10. 生命周期模型

### 10.1 创建

- 协程创建时绑定当前线程和目标调度器
- 协程初始状态为 `NEW` 或 `READY`
- 创建后不可更换 owner thread

### 10.2 运行

- 只有所属调度器可将协程从 `READY` 切换到 `RUNNING`
- 协程运行中可主动 `yield`
- `yield` 后必须进入明确的等待/就绪状态，而不是一个语义模糊的总状态

### 10.3 等待

等待必须通过显式 wait source 表达：

- timer
- I/O
- event
- join
- post queue

### 10.4 结束

- 协程函数自然返回后进入 `DONE`
- 取消后进入 `CANCELLED`
- 资源最终释放时进入 `CLOSED`

### 10.5 销毁

`Destroy` 不应再作为“任何时候都能 free”的野蛮接口。

建议规则：

- `RUNNING` 协程禁止销毁
- 有所属 scheduler 的协程，销毁权归 scheduler
- 外部用户只能 `cancel/close/join`，不能直接 free 挂在 scheduler 上的活协程
- `Destroy` 只处理已经脱离 scheduler 且已终态的协程


## 11. 调度模型

### 11.1 ready queue

调度器必须有显式 ready queue。

要求：

- `yield` 后重新入 ready queue 或进入 wait state
- 外线程唤醒通过 `post` 入队，不直接上下文切换
- 不再每轮线性扫描全部协程决定谁可运行

### 11.2 timer

timer 必须改为基于 deadline 的有序结构。

建议：

- 使用 min-heap 或 timer wheel
- `xrtCoSleep()` 变为 `sleep_until(deadline)` 的语义包装
- `SchedRun` 在没有 runnable coroutine 时阻塞到最近 deadline 或外部 post

### 11.3 wait source

引入标准 wait source 抽象：

- `timer wait`
- `fd/socket wait`
- `future wait`
- `event wait`
- `join wait`

wait source 需要可统一取消、超时和唤醒。

### 11.4 外线程唤醒

外线程不能直接 `resume` 某个协程。

正确方式是：

- 将 wake request 投递到目标 scheduler 的 post queue
- 由 owner thread 在安全点处理并将协程入 ready queue


## 12. backend 策略

### 12.1 总原则

backend 选择必须遵循：

- 优先使用可审计、可控、宿主副作用小的后端
- 不默默 fallback 到高风险后端
- 不为了“什么都能编”牺牲基础设施级正确性

### 12.2 生产级首选后端

生产级首选方案：

- x86_64: 自定义汇编后端
- ARM64: 自定义汇编后端
- RISC-V 64: 自定义汇编后端
- LoongArch64: 自定义汇编后端

要求：

- 按 ABI 完整保存必须的 callee-saved 状态
- 进行浮点/向量寄存器专项验证
- 进行 `yield` 前后浮点计算正确性测试

### 12.3 Windows 策略

Windows 不应再把 Fiber 作为默认标准后端。

建议：

- x64/ARM64 优先使用自定义上下文切换后端
- Fiber 仅保留为显式 legacy/compat backend
- 如果启用 Fiber backend，必须按线程维护 main fiber 状态，并正确处理 already-fiber 场景

### 12.4 POSIX fallback 策略

`ucontext` 只能作为显式兼容 fallback。

要求：

- 默认不自动启用
- 需要显式宏开关才允许使用
- 文档中标注为 compatibility-only backend

### 12.5 TCC 策略

TCC 支持应区分两个层次：

- 开发验证层: 可以使用兼容后端
- 生产标准层: 必须使用通过 ABI 审计的正式后端

不应为了照顾 TCC 而让整个 coroutine runtime 降级成 Fiber/ucontext-only 产品。

### 12.6 backend 分层与编译开关

为兼顾渐进落地和长期标准，backend 必须分层管理：

- `production backend`
	- 自有上下文切换实现
	- 通过 ABI 回归与平台验证
	- 允许作为默认 backend
- `compat backend`
	- Fiber / `ucontext`
	- 仅用于兼容、过渡或开发验证
	- 不得被表述为生产级标准能力

建议固定以下编译开关：

- `XRT_CO_ENABLE_COMPAT_BACKEND`
	- 默认 `1`
	- 为 `0` 时，禁止选择 Fiber / `ucontext`
- `XRT_CO_REQUIRE_PRODUCTION_BACKEND`
	- 默认 `0`
	- 为 `1` 时，如果当前目标平台没有 production backend，直接编译报错

阶段性策略如下：

- 当前阶段允许 compat backend 继续存在，以保证源码树、测试和历史用法可继续演进
- 但新 backend 框架必须先支持“显式拒绝 compat fallback”
- 随着测试设备和 CI 覆盖增加，再逐步把更多平台提升为 production backend
- 当 Windows / Linux 主流目标都具备 production backend 后，再评估是否改变默认策略

### 12.7 单头文件与内联汇编策略

`xrt` 的分发形态以单头文件为第一优先级约束，因此协程 production backend 应优先遵循以下原则：

- 在可行的平台和编译器上，优先使用单头文件内联汇编实现上下文切换
- 不把外部 `.S` / `.asm` 文件作为 primary distribution 的默认前提
- backend 的设计维度优先按 “ABI + 编译器家族” 划分，而不是简单按操作系统划分

这意味着：

- Linux / macOS / iOS 上，只要 ABI 和编译器家族一致，通常可以共享同一套 inline-asm backend 设计
- Windows 的真正难点不在 OS 本身，而在 `MSVC x64/ARM64` 对 inline asm 的限制
- 如果某个平台未来必须依赖外部汇编文件才能得到 production backend，那么它应被视为单头文件原则下的特例，而不是新的默认规则

### 12.8 分阶段支持矩阵

当前建议的阶段性支持矩阵如下：

- `production / inline-asm first`
	- `x86_64 Win64` + `GCC/Clang`
	- `x86_64 SysV` + `GCC/Clang`
	- `ARM64 AAPCS64/Darwin` + `GCC/Clang/Apple Clang`
	- `RV64` + `GCC/Clang`
	- `LA64` + `GCC` 优先，其他编译器后续补齐
- `compat only`
	- Windows 非 `x86_64 GCC/Clang` 目标当前仍使用 Fiber
	- 其他未完成 production backend 的目标使用 `ucontext`
- `deferred / later`
	- `MSVC x64/ARM64` production backend
	- arm64e / pointer-authentication 相关专项验证

矩阵的提升原则如下：

- 只有在真实设备、真实编译器和 ABI 测试通过后，目标平台才能从 compat 提升为 production
- 未验证的平台可以继续存在，但状态必须明确标注为 compat 或 deferred
- 后续每增加一类测试设备，都应同步更新本矩阵


## 13. 栈模型

协程栈必须从简单 `malloc` 升级为正式 stack allocator。

建议：

- 使用 `VirtualAlloc` / `mmap` 保留栈区
- 在低地址侧放 guard page
- 支持 reserve/commit 分离
- debug 模式下支持 canary / high-water mark 统计
- 栈大小配置分为 tiny/small/default/large 几档

最低要求：

- 不能继续只依赖普通 heap buffer 充当“栈”

当前阶段实现状态：

- 非 Fiber backend 已切换到正式 stack allocator
- Windows 使用 `VirtualAlloc + PAGE_NOACCESS guard page`
- POSIX 使用 `mmap + mprotect(PROT_NONE) guard page`
- reserve/commit 分离与 high-water mark 统计继续留到后续阶段


## 14. API 设计方向

### 14.1 低层兼容 API

现有 API 可以保留，但应降级为 low-level compatibility tier：

- `xrtCoCreate`
- `xrtCoDestroy`
- `xrtCoResume`
- `xrtCoYield`
- `xrtCoGetState`
- `xrtCoGetCurrent`

这些 API 的语义必须收紧，并在文档中明确：

- 线程绑定
- destroy 约束
- scheduler 约束
- 禁止错线程操作

### 14.2 新的标准 runtime API

建议新增并逐步推广以下 API：

- `xrtCoCreateEx`
- `xrtCoClose`
- `xrtCoCancel`
- `xrtCoJoin`
- `xrtCoExit`
- `xrtCoSleepUntil`
- `xrtCoSchedCurrent`
- `xrtCoSchedPost`
- `xrtCoSchedPollOnce`
- `xrtCoWaitEvent`
- `xrtCoWaitFuture`
- `xrtCoWaitDeadline`

另外建议保留一组 backend 自检接口，供设备 bring-up、CI 和 ABI 验证使用：

- `xrtCoGetBackendName`
- `xrtCoGetBackendTier`
- `xrtCoGetBackendStyle`

### 14.3 structured concurrency

如果追求“最高标准”，建议从设计层预留 task group / scope 模型。

建议预留：

- `xco_group`
- 父任务对子任务的取消传播
- `wait all` / `fail fast`
- deadline 继承

第一版可以先不完整公开，但数据模型和 scheduler 设计应提前兼容。


## 15. 与 xnet / event loop 的集成方向

协程的真正价值不在 `yield + sleep`，而在和事件循环结合。

设计方向如下：

- scheduler 需要能接收 xnet future/command/wakeup
- `xrtCoWaitFuture()` 应能等待 xnet future
- socket / listener / stream 的可读可写事件应能映射为 coroutine wait source
- timeout、cancel、deadline 应与网络层统一

长期目标不是“单独存在的协程库”，而是“runtime task layer”。


## 16. 兼容策略

### 16.1 保留项

以下内容尽量保留：

- `xrtCo*` 命名体系
- 简单创建/恢复/挂起使用方式
- user data 接口
- 简单 scheduler 入口名称

### 16.2 行为收紧项

以下行为允许变为硬错误：

- 错线程 `resume`
- 错线程 `destroy`
- destroy 活协程
- destroy 属于 scheduler 的活协程
- 未 attach 线程调用协程 runtime API
- 在不支持的 backend 组合下偷偷 fallback


## 17. 测试与基准计划

### 17.1 单元测试

必须覆盖：

- create/resume/yield/join/destroy 正常路径
- destroy 非法状态
- scheduler 管理下的 close/cancel
- cross-thread misuse rejection
- timer/deadline 正确性
- post queue 唤醒
- current coroutine / current scheduler 正确性

### 17.2 ABI 测试

必须覆盖：

- 浮点寄存器跨 yield 保持
- 深栈调用跨 yield 保持
- 不同优化级别下的上下文切换正确性
- Windows / Linux 分平台后端测试

### 17.3 集成测试

必须覆盖：

- 与 xrt 线程 runtime 共存
- 与 future / timer / xnet wait 集成
- 多 scheduler、多线程并行场景
- already-fiber / host runtime 嵌入场景

### 17.4 压测与基准

至少提供以下指标：

- 单次 context switch 成本
- 1000 / 10000 coroutine 创建与销毁成本
- timer 唤醒吞吐
- post queue 跨线程唤醒成本
- 与 thread-per-task 模式对比


## 18. 验收标准

协程重构只有在以下条件全部成立时才可视为完成：

1. 每线程 coroutine runtime 状态独立
2. Windows 和 Linux 主流后端通过正式回归
3. ARM64 等非 x86 后端完成 ABI 审计
4. destroy/cancel/join 契约无 UAF 漏洞
5. sleep/timer 不再依赖 1ms 空转轮询
6. cross-thread misuse 明确拒绝
7. 协程测试已纳入默认测试流
8. 与 xnet/event-loop 至少有一条正式 wait 集成链路


## 19. 里程碑与进度

| 里程碑 | 状态 | 目标 |
|---|---|---|
| M0 Spec freeze | done | 冻结协程重构目标、边界、后端策略 |
| M1 Runtime integration | done | 协程 runtime 状态迁入 `xrtThreadData` |
| M2 Lifecycle hardening | done | destroy/cancel/join/thread affinity 契约落地 |
| M3 Scheduler core | done | ready queue + timer heap + post queue |
| M4 Backend rewrite/audit | in_progress | 生产级 backend 完成并通过 ABI 测试 |
| M5 Public API compat layer | in_progress | 兼容旧 `xrtCo*`，引入新标准 runtime API |
| M6 xnet integration | in_progress | future/timer/I/O wait 正式接线 |
| M7 Tests, docs, benchmarks | todo | 主测试流、文档、基准齐备 |


## 20. 风险与缓解

- 风险: 试图一步到位把协程做成“万能 async 系统”，导致 scope 失控
- 缓解: 先把 runtime、生命周期和 backend 做正确，再做 xnet 集成

- 风险: 为了兼容所有编译器，继续默认使用 Fiber/ucontext
- 缓解: 生产级后端与兼容级后端分层，弱后端必须显式 opt-in

- 风险: 新 API 过重，破坏 xrt 一贯易用性
- 缓解: 保留 low-level `xrtCo*` 兼容层，标准路径则引导到 scheduler/task API

- 风险: 协程和线程 runtime 再次各自发展出两套状态模型
- 缓解: 协程 runtime 强制挂在 `xrtThreadData` 上


## 21. 开放问题

1. `xco_entry` 第一版是否保持 `void (*)(ptr)`，还是升级为可返回状态/结果的签名？
2. task result 第一版是否仅支持 `exit code + user data`，还是直接引入 `future/value` 结果对象？
3. structured concurrency 第一版是否公开，还是先只在内部保留数据模型？
4. TCC 的协程支持是否应明确降级为“开发验证级能力”？


## 22. 初始决策日志

1. `2026-03-13`: 协程在 XRT 中被定义为线程绑定的 cooperative task runtime，而不是跨线程迁移对象。
2. `2026-03-13`: 协程运行时状态必须纳入 `xrtThreadData`，禁止继续使用进程级静态主上下文和当前协程指针。
3. `2026-03-13`: Windows Fiber 不再作为生产级默认标准后端，只能作为显式兼容后端存在。
4. `2026-03-13`: `ucontext` 只能作为 compatibility-only fallback，不允许再作为静默默认后端。
5. `2026-03-13`: destroy/cancel/join 必须重做，scheduler 管理下的活协程不能再被外部任意 free。
6. `2026-03-13`: 协程调度器必须升级为 ready queue + timer + waiter 的 runtime，不再接受 busy-sleep 轮询方案作为目标设计。
7. `2026-03-13`: 新协程系统必须为 xnet/event-loop 集成预留正式 wait source 抽象。
8. `2026-03-13`: 第一批实现优先完成 runtime 线程态迁移和 thread-affinity/destroy 收口，legacy scheduler 的线性扫描与 `sleep(1ms)` 暂时保留到 M3 再重写。
9. `2026-03-13`: backend 最终方向是“自有 backend 为主，Fiber/ucontext 为 compat 层”，但在 Windows 等尚未落地 production backend 的目标上，兼容后端可以暂时保留，以支撑增量迁移和设备逐步补齐。
10. `2026-03-13`: backend 选择必须支持 strict 模式，即由 `XRT_CO_REQUIRE_PRODUCTION_BACKEND` 直接拒绝 compat fallback。
11. `2026-03-13`: 单头文件分发仍是优先级很高的产品约束，因此 production backend 应优先选择内联汇编，而不是把外部汇编文件作为默认实现形式。
12. `2026-03-13`: 平台支持应按“ABI + 编译器 + 实测设备”逐步扩展，未验证目标保持 compat 或 deferred 状态，不做超前承诺。
13. `2026-03-13`: `x86_64 Win64 + GCC/Clang` 可以作为首个 Windows production backend 路径，前提是补齐 Win64 ABI 的非易失 GPR、XMM 寄存器和 shadow space 约束。
14. `2026-03-13`: ARM64 production backend 的最低要求不是只保存 `x19-x30/sp`，还必须保存协程切换点上的 SIMD 非易失状态；当前实现按更保守的方式保存 `q8-q15` 全寄存器。
15. `2026-03-13`: 第一版 `cancel` 明确采用协作式语义，不引入不安全的强制栈卸载；`READY` 协程可直接取消为终态，已运行协程通过取消请求 + 安全点观察完成退出。
16. `2026-03-13`: 第一版 `join` 仅支持单等待者，并在 scheduler 管理协程上引入 join pin / 延迟 reap，避免 join 期间句柄被 scheduler 提前回收。
17. `2026-03-13`: `M3` 第一阶段先将 legacy scheduler 替换为 `arrCoros(所有权) + ready queue + timer min-heap` 结构；跨线程 post queue 继续留在同里程碑后半段，不为了赶进度把唤醒语义做成半成品。
18. `2026-03-13`: `M3` 完成态必须包含 `xrtCoSchedCurrent()`、跨线程 `xrtCoSchedPost()` 和 `xrtCoSchedPollOnce()`；owner thread 需要在 `poll/run` 安全点统一 drain post queue，并以条件变量/事件唤醒替代 legacy 的 `sleep(1ms)` 轮询等待。
19. `2026-03-13`: 非 Fiber backend 的协程栈必须停止使用普通 heap buffer；第一版正式 stack allocator 采用“保留区 + guard page”模型，Windows 走 `VirtualAlloc`，POSIX 走 `mmap/mprotect`，reserve/commit 分离与高水位统计暂缓。
20. `2026-03-13`: `RV64/LA64` production backend 的 ABI 审计不能只停留在整数寄存器；硬浮点 ABI 下必须同时保存对应的非易失浮点寄存器集合，且要区分 single/double float ABI，真机验证不足时只能把“代码已补齐、设备回归待补”明确写清。
21. `2026-03-13`: Fiber compat backend 也必须具备宿主纤程语义；如果当前线程本来就是 fiber，runtime 应采用当前 host fiber，而不是再次 `ConvertThreadToFiber()` 或在 detach 时误执行 `ConvertFiberToThread()`。
22. `2026-03-13`: 协程 backend 需要公开最小自检信息（name/tier/style），这样未来上 ARM64/RV64/LA64 设备时可以先确认 backend 选择是否符合预期，再进入 ABI smoke test。
23. `2026-03-13`: `M5` 第一批标准 runtime API 优先落低风险接口：`xrtCoExit()`、`xrtCoGetExitCode()`、`xrtCoSleepUntil()`；其中 `xrtCoSleep()` 退化为 `sleep_until(deadline)` 的语义包装。
24. `2026-03-13`: `xrtCoCreateEx()` 采用轻量配置结构而不是复杂 builder：当前只开放 `stack size / initial user data / reserved flags`，未知 flags 一律拒绝，保证后续扩展时语义仍然可控。
25. `2026-03-13`: `xrtCoWaitDeadline()` 的第一版语义收敛为“等待到 deadline 或被取消唤醒”；返回值只表达是否在等待期间收到取消请求，不提前引入更复杂的 wake reason 枚举。
26. `2026-03-13`: `M5` 的第一条 wait source 选择 `xcoevent`，且第一版只支持单 waiter；跨线程 `set` 统一通过 scheduler post 唤醒，manual-reset 与 auto-reset 语义都在事件对象内收口，不把复杂 wake reason 提前扩散到公共 API。
27. `2026-03-13`: `M5` 需要补齐协程级 cleanup 栈，保证 return / exit / cancel 终态都能按 LIFO 统一释放协程局部资源；第一版公开 `xrtCoPushCleanup/xrtCoPopCleanup`，并明确 cleanup 回调不得执行 `yield/wait` 这类重新进入调度器的操作。
28. `2026-03-13`: `M5` 的任务结果第一版先收敛为“原始结果指针 + exit code”模型：公开 `xrtCoSetResult/xrtCoGetResult`，不提前把 `future/value` 结果对象强绑进协程核心。
29. `2026-03-13`: `M6` 的第一条 xnet 接线选择 `future wait`，且先收敛为 `xnet_sync.h` 内部 bridge：公开 `xrtNetFutureWaitCo()`，复用协程 `xcoevent` 做完成唤醒；第一版只支持单 coroutine waiter，但仍保留线程侧 condvar wait 语义。
30. `2026-03-13`: `xcoevent` 需要补 timed wait 变体；第一版公开 `xrtCoWaitEventTimeout()` 与 `xrtCoWaitEventUntil()`，内部复用 scheduler timer + event waiter 脱链，返回值仍保持简单的 bool 语义，不提前引入通用 wake reason 枚举。
31. `2026-03-13`: `future wait` 的协程桥接也应同时提供 timeout 与 deadline 两种形态；第一版在 `xnet_sync.h` 内部补 `xrtNetFutureWaitCoTimeout()` 与 `xrtNetFutureWaitCoUntil()`，继续维持单 waiter 和简单返回语义。
32. `2026-03-13`: `M6` 不能只停在“已有 future 的等待”；还需要一条最小的 `task/post -> future -> coroutine wake` 链路。第一版在 `xnet_sync.h` 内部补 `xrtNetEnginePostFuture()` 与 `xrtNetEnginePostDelayedFuture()`，继续维持单次任务完成后自动 resolve 的简单模型。
33. `2026-03-13`: `posted future` 不能把 future 生命周期完全交给调用者；一旦任务已进入 engine 队列，future 必须持有内部 async-hold，直到 worker 真实 resolve/reject 后再释放。destroy 在 async-hold 存在期间必须明确拒绝，防止 queued task 继续访问已释放 future。
34. `2026-03-13`: `M6` 的下一条低风险 wait-source 选择 `stream drain/close -> future -> coroutine wake`；第一版继续经由 `xnet_sync.h` 内部 future bridge 实现，不把 `xcoevent` 直接嵌入 stream 层。stream 需要显式 async-hold，destroy 在 waiter 仍挂着时必须拒绝，待 drain/close 事件真正触发后再允许最终销毁。
35. `2026-03-13`: `stream readable` 的第一版 bridge 必须收紧合同，避免与现有 `OnRecv` 回调消费路径产生竞态；因此只支持 paused-read 模式下的 buffered readable wait。`xrtNetStreamReadableFuture()` 会在 `bReadPaused` 为真时等待 `tRxChain` 非空或 close，未 pause read 时直接拒绝创建。
36. `2026-03-13`: `stream writable` 的第一版 bridge 不提供原始 socket-ready 语义，而是收敛为“send queue 从 high-water 压力回落到 low-water 以下”的 backpressure-relief wait；实现继续复用 `xnet_stream.h` 的统一 wait-slot 表，并通过 `xrtNetStreamWritableFuture()` 暴露为 one-shot future。
37. `2026-03-13`: 在已有 `stream -> future -> coroutine wake` bridge 之上，需要再补一层直接 coroutine helper，降低调用方样板代码；但第一版只公开 indefinite 形式的 `xrtNetStreamWait*Co()`，不提供 timeout/deadline 版本，因为 stream waiter 目前还没有独立的取消/脱链协议，不能用包装层偷偷制造悬空 future。
38. `2026-03-13`: `xrtNetStreamWait*Co()` 的 timed 形态必须先在 owner worker 上显式撤销 pending stream waiter，再允许 timeout/deadline 返回；实现继续复用 one-shot future bridge，但 direct helper 自身要负责 `cancel/unregister -> release async-hold -> destroy internal future` 的完整收口。
39. `2026-03-13`: `stream` 等待面需要有一层统一抽象，避免调用方和后续 `socket/readiness` 扩展继续复制 `Readable/Writable/Drain/Close` 四套 API；第一版通过 `xrtNetStreamFutureEx()` 与 `xrtNetStreamWaitCo*Ex()` 提供按 wait-kind 驱动的统一入口，而专名 helper 继续作为薄封装保留。
40. `2026-03-13`: 统一 `stream wait` 抽象不能只覆盖协程；线程侧同步等待也必须能复用同一 wait-kind 模型，并在 timeout/deadline 返回前执行同样的 pending waiter 撤销与 future 清理，以保证 sync/coroutine 两条路径共享一致的生命周期合同。
41. `2026-03-13`: 在更广义的 socket/port wait-source 模型落地前，允许先引入最小 `xnetwaitsrc` 对象统一 `future` 与 `stream wait-kind` 两类等待面；协程和线程侧统一经由 `xrtNetWaitSourceWait*()` / `xrtNetWaitSourceWaitCo*()` 进入既有等待语义，而非法 kind 继续按普通错误路径明确拒绝。
42. `2026-03-13`: 统一等待源不能只返回状态；第一版应补齐“带值等待”语义，使 `future` 的 resolved value 与 `stream` wait 的事件值都能通过 `xrtNetWaitSourceWaitValue*()` / `xrtNetWaitSourceWaitCoValue*()` 透传出来，为后续 `dgram packet`、更广义结果对象和上层组合等待预留稳定合同。
43. `2026-03-13`: 统一等待源的下一跳选择 `dgram recv`，并明确采用 one-shot packet 合同：`xnet_sync.h` 通过 `future/wait source` 返回独立的 `xnetdgrampkt`，第一版只支持单 waiter，且与 `OnRecv` 回调互斥，不引入 datagram backlog 或多 waiter 竞争语义。
44. `2026-03-13`: `listener accept` 暂不进入统一等待源；在 `xnet-v2` listener 具备真实的 owner-worker、async-hold、accept waiter 槽位，以及通过 `xnet_port` 提交/回收 accept 的完整生命周期前，不允许为了赶进度引入基于轮询 `accept()` 的伪 future 或伪 coroutine wait。


## 23. 工作日志

- `2026-03-13`: 阅读现有 `lib/coroutine.h`、`test/test_coroutine.h` 和主测试接线，确认当前实现属于原型级 stackful coroutine，存在进程级静态协程状态、Fiber 侵入性、destroy 生命周期不严、scheduler demo 化、后端 ABI 审计不足等问题。基于现有 runtime/threading 重构成果，建立本协程重构 spec，明确线程绑定、生命周期、backend、scheduler、xnet 集成和验收标准。
- `2026-03-13`: 完成 M1 和第一批 M2 收口：协程 runtime 状态已迁入 `xrtThreadData.tCoro`，移除了进程级 `main context/current coroutine` 静态状态；Windows Fiber / 汇编 / `ucontext` 后端都改为按线程持有主上下文；`xrtCoCreate/Resume/Destroy/Sched*` 已接入 attach 校验和 owner-thread 约束；`xrtCoDestroy` 现在拒绝销毁非终态或仍属于 scheduler 的协程；补充了 cross-thread misuse 和 scheduler-owned destroy 的回归测试，并用最小 harness 实际执行通过。
- `2026-03-13`: 根据新的 backend 共识补充分层策略：明确 production/compat backend 二层模型，加入 `XRT_CO_ENABLE_COMPAT_BACKEND` 与 `XRT_CO_REQUIRE_PRODUCTION_BACKEND` 这组编译开关约束，允许当前阶段保留 Fiber / `ucontext` 作为兼容后端，同时为未来 strict production-only 构建准备硬边界。
- `2026-03-13`: 进一步冻结 backend 形态：明确单头文件约束下 production backend 应优先走 inline asm；新增按 ABI/编译器划分的阶段性支持矩阵，并约定平台支持随测试设备与 CI 覆盖逐步提升，不做未验证平台的超前承诺。
- `2026-03-13`: 开始推进第一个 Windows production backend：将 `x86_64 Win64 + GCC/Clang` 从 Fiber compat 提升为 inline-asm production 目标，要求保存 Win64 非易失 GPR/XMM 状态并正确预留初始栈的 32 字节 shadow space；同时补充最小 XMM 非易失寄存器保持测试。
- `2026-03-13`: `x86_64 Win64 + GCC` production backend 已在当前环境完成基础验证：默认构建可编译并通过 `Test_Coroutine` 全部用例；打开 `XRT_CO_REQUIRE_PRODUCTION_BACKEND=1` 且关闭 compat backend 后，`xrt.c` 与 `test.c` 仍可编译通过，说明当前目标已不再依赖 Fiber fallback。
- `2026-03-13`: 收紧 ARM64 backend 的 ABI 保存集合：AArch64 inline-asm 上下文切换现在除 `x19-x30/sp` 外，还保存 `q8-q15`，为后续真实 ARM64 设备上的浮点/向量回归测试做准备；当前阶段仅完成代码与当前平台编译回归，运行验证仍待 ARM64 设备补齐。
- `2026-03-13`: 完成 M2 剩余生命周期收口：新增 `xrtCoCancel/xrtCoJoin/xrtCoClose/xrtCoIsCancelRequested/xrtCoWasCancelled` 第一版 API；协程终态原因、join waiter / join target 元数据已接入；scheduler 管理协程新增 join pin 与延迟 reap 保护，避免 join 期间句柄悬空；补充了 unscheduled join、scheduler 内 join、协作式 cancel + close 回归测试。
- `2026-03-13`: 开始推进 M3 scheduler core：legacy 的“扫描 `arrCoros` + `sleep(1ms)`”已被替换为“`arrCoros` 仅做所有权、`ready queue` 负责调度顺序、`timer min-heap` 负责 sleep/deadline 唤醒”的结构；`yield` 现在回到 ready queue，`xrtCoSleep()` 走 timer heap，`join/cancel` 也已经接入新的 scheduler bookkeeping。
- `2026-03-13`: 完成 M3 scheduler core 收口：新增 `xrtCoSchedCurrent()`、`xrtCoSchedPost()`、`xrtCoSchedPollOnce()`，scheduler 现在具备跨线程 post queue、条件变量唤醒和单步 poll 能力；`xrtCoSchedStep()`/`xrtCoSchedRun()` 已切到统一的 poll/drain 路径，补充了“post queue 提前唤醒 + current scheduler”正式回归，`Test_Coroutine` 当前共 15 项全部通过。
- `2026-03-13`: 收口协程栈模型的第一版正式实现：非 Fiber backend 已从 `malloc` 栈切到 `VirtualAlloc/mmap` 保留区，低地址侧 guard page 已启用，`ucontext` 与自有汇编 backend 统一复用这套栈分配器；当前平台重新跑完 `Test_Coroutine` 15 项，未引入新的行为回归。
- `2026-03-13`: 继续推进 M4 backend ABI 审计：`RV64` backend 现在会在硬浮点 ABI 下保存 `fs0-fs11`，并区分 single/double float ABI；`LA64` backend 现在会在硬浮点 ABI 下保存 `fs0-fs7`，同样区分 single/double float ABI。当前平台已重新编译并跑完 `Test_Coroutine` 15 项，未引入回归；`RV64/LA64` 真机运行验证仍待设备补齐。
- `2026-03-13`: 收口 Fiber compat 的宿主场景：runtime 现在会区分“由 xrt 转成 fiber”和“线程本来就是 host fiber”；already-fiber 线程会直接 adopt `GetCurrentFiber()`，不会再重复 `ConvertThreadToFiber()`，detach 时也不会误调用 `ConvertFiberToThread()`。当前平台重新编译并跑完 `Test_Coroutine` 15 项，未引入回归。
- `2026-03-13`: 补充 ABI 深栈回归：新增“深栈跨 yield 保持”正式测试，验证深调用链局部栈帧在 `yield/resume` 前后保持稳定，并检查第一次恢复后确实处于 `SUSPENDED` 状态。当前平台 `Test_Coroutine` 已扩到 16 项并全部通过。
- `2026-03-13`: 新增 backend introspection API：`xrtCoGetBackendName/Tier/Style` 已公开，并接入 `Test_Coroutine` 的启动自检输出。当前环境可明确看到命中的 backend 为 `asm-x64-win64`、tier 为 `production`、style 为 `inline-asm`；当前平台重新跑完测试后，编号已扩到 `Test 0` 至 `Test 16` 共 17 项并全部通过。
- `2026-03-13`: 开始推进 M5 Public API compat layer：已公开 `xrtCoExit()`、`xrtCoGetExitCode()`、`xrtCoSleepUntil()`，并把 `xrtCoSleep()` 收口成 `sleep_until(deadline)` 包装；补充了“主动退出 + 退出码”和“deadline 睡眠”两组正式回归。当前平台 `Test_Coroutine` 已扩到 `Test 0` 至 `Test 18` 共 19 项并全部通过。
- `2026-03-13`: 继续推进 M5：已公开 `xrtCoCreateEx()` 和轻量 `xco_create_args`，允许在创建时设置栈大小与初始 `UserData`；当前阶段未知 flags 会直接报错拒绝。补充了 `CreateEx 默认路径 + 初始 UserData + 未知 flags 拒绝` 正式回归；当前平台 `Test_Coroutine` 继续保持 `Test 0` 至 `Test 19` 共 20 个检查点全部通过。
- `2026-03-13`: 继续推进 M5：已公开 `xrtCoWaitDeadline()`，并用“同 scheduler 协程在 50ms 后发取消”的正式回归验证其 cancel 语义；当前实现下 `WaitDeadline` 会在被取消唤醒时返回 `false`，目标协程终态也会记录为取消。当前平台 `Test_Coroutine` 继续保持 `Test 0` 至 `Test 20` 共 21 个检查点全部通过。
- `2026-03-13`: 继续推进 M5 wait source：新增最小 `xcoevent`，公开 `xrtCoEventCreate/Destroy/Set/Reset/WaitEvent`；第一版仅支持单 waiter，协程取消时会自动从 event waiter 槽位脱链，跨线程 `set` 通过 scheduler post 唤醒等待协程。补充了“auto-reset event 跨线程 set 唤醒”和“manual-reset 预置位立即返回”两组正式回归。
- `2026-03-13`: 继续推进 M5 cleanup：新增 `xrtCoPushCleanup/xrtCoPopCleanup`，cleanup 节点直接挂在 `xcoro` 上并在 `return/exit/cancel` 终态统一按 LIFO 执行；同时阻止 cleanup 回调里再次 `yield`。补充了“cleanup push/pop 顺序”和“cancel 路径 cleanup 执行”两组正式回归。
- `2026-03-13`: 继续推进 M5 result：在 `xcoro` 上新增 `pResult`，公开 `xrtCoSetResult/xrtCoGetResult`，第一版结果模型先保持为“原始结果指针 + exit code”。补充了“result 指针记录 + exit code 协同”正式回归。
- `2026-03-13`: 开始推进 M6 xnet integration：在 `xnet_sync.h` 为 `xnetfuture` 增加最小协程桥接，新增 `xrtNetFutureWaitCo()`；future resolve 时会通过协程 event 唤醒 waiter，协程侧可以不阻塞线程地等待网络 future 完成。第一版只支持单 coroutine waiter，并补了 core/runtime 条件下的 `future coroutine wait` 正式回归。
- `2026-03-13`: 继续推进 wait source 与 M6 桥接：新增 `xrtCoWaitEventTimeout()` 与 `xrtCoWaitEventUntil()`，timed wait 由 scheduler timer heap 驱动并在超时或 deadline 到达时自动从 event waiter 槽位脱链；在此基础上 `xrtNetFutureWaitCoTimeout()` 已落地，补充了“event timeout / deadline”和“future coroutine timeout”正式回归。
- `2026-03-13`: 继续推进 M6 future wait：在 `xnet_sync.h` 补 `xrtNetFutureWaitCoUntil()`，并让 `xrtNetFutureWaitCo()` / `xrtNetFutureWaitCoTimeout()` 统一建立在 `event until` 语义之上；补充了 `future coroutine deadline timeout` 正式回归，当前 future 等待桥接已具备 indefinite / timeout / deadline 三种形态。
- `2026-03-13`: 继续推进 M6 task/post 接线：在 `xnet_sync.h` 补 `xrtNetEnginePostFuture()` 与 `xrtNetEnginePostDelayedFuture()`，把 engine worker task 结果自动封装成 future，并通过既有 coroutine future-wait 路径唤醒 owner scheduler；补充了“post future”和“delayed post future”两组正式回归。
- `2026-03-13`: 收紧 `M6` posted-future 生命周期：`xnetfuture` 现在带 `iAsyncHoldCount`，queued/delayed task 在入队时先 pin future，worker 完成 resolve 后再 release；`xrtNetFutureDestroy()` 会在 async-hold 仍存在时直接拒绝，防止 posted task 对已释放 future 产生 UAF。补充了“early destroy rejected, future still resolves, final destroy succeeds”正式回归。
- `2026-03-13`: 继续推进 `M6` wait-source：在 `xnet_sync.h` 补 `xrtNetStreamDrainFuture()` 与 `xrtNetStreamCloseFuture()`，把 stream 的 drain/close 事件桥接成可被协程等待的 future；`xnet_stream.h` 同时新增 stream async-hold 与单 waiter 的 drain/close 注册槽，destroy 会在 async-hold 未释放时直接拒绝。补充了“stream drain future rejects early stream destroy”和“stream close future coroutine wait”正式回归。
- `2026-03-13`: 继续推进 `M6` wait-source：在 `xnet_sync.h` 补 `xrtNetStreamReadableFuture()`，并在 `xnet_stream.h` 增加单 waiter 的 readable 注册槽；第一版明确要求 `xrtNetStreamPauseRead()` 已开启，只在 buffered `tRxChain` 可读或 close 时 resolve，对未 pause read 的调用直接拒绝。补充了“stream readable future coroutine wait”和“readable future rejects unpaused read”正式回归。
- `2026-03-13`: 继续推进 `M6` wait-source：把 `xnet_stream.h` 的 drain/close/readable waiter 收口到统一 wait-slot 表，并在 `xnet_sync.h` 补 `xrtNetStreamWritableFuture()`；第一版 writable 合同收敛为“send queue 从 high-water 压力回落到 low-water 以下时 resolve”，不假装提供原始 socket-ready 语义。补充了“stream writable future coroutine wait”“writable future rejects early stream destroy”和“pressure relieved to low water”正式回归。
- `2026-03-13`: 继续推进 `M6` 的使用层收口：在 `xnet_sync.h` 补 `xrtNetStreamWaitReadableCo/WaitWritableCo/WaitDrainCo/WaitCloseCo`，把 `future create + coroutine wait + future destroy` 的样板代码收进内部 helper；第一版仅支持 indefinite wait，并补了“readable coroutine helper”和“writable coroutine helper”正式回归。
- `2026-03-13`: 继续推进 `M6` 的 timed direct wait：在 `xnet_stream.h` 补 `__xnetStreamCancelSyncWait()`，在 `xnet_sync.h` 给 stream-future waiter 增加状态机、cancel-complete event 和 pending-cleanup hook，进而公开 `xrtNetStreamWait*CoTimeout()` 与 `xrtNetStreamWait*CoUntil()`；补了“readable timeout 后可重新注册”和“writable deadline 后可重新注册”两组正式回归，明确 timed helper 已不会留下悬空 stream waiter。
- `2026-03-13`: 继续补齐 `M6` 的 timed direct helper 覆盖：新增“drain timeout 后可重新注册”和“close deadline 后可重新注册”两组正式回归，把 `readable/writable/drain/close` 四条 stream wait surface 的 timed direct wait 合同全部覆盖到默认的 `xnet sync` 专项测试里。
- `2026-03-13`: 继续推进 `M6` 的 wait-source 抽象：在 `xnet_sync.h` 新增 `xrtNetStreamFutureEx()` 与 `xrtNetStreamWaitCo*Ex()`，同时把 stream future 注册路径收口成 wait-kind -> callback/error 表驱动；补了“generic future invalid kind rejected”和“generic coroutine wait helper readable”正式回归，为后续 socket/readiness 扩展预留统一入口。
- `2026-03-13`: 继续推进 `M6` 的 wait-source 抽象到线程侧：在 `xnet_sync.h` 新增 `xrtNetStreamWaitEx/WaitTimeoutEx/WaitUntilEx`，让同步线程也能直接走 wait-kind 驱动的统一入口；补了“generic sync wait readable”和“generic sync timeout 后重新注册成功”正式回归，确认 sync/coroutine 两条路径共享同一套 waiter 撤销语义。
- `2026-03-13`: 继续推进 `M6` 的统一 wait-source：在 `xnet_sync.h` 新增最小 `xnetwaitsrc` 对象、`xrtNetWaitSourceFuture()` / `xrtNetWaitSourceStream()` 构造器，以及 `xrtNetWaitSourceWait*()` / `xrtNetWaitSourceWaitCo*()` 通用入口；当前版本先统一 `future` 与 `stream wait-kind` 两类等待面，并补了“future sync wait source”“future coroutine wait source”和“invalid wait source kind rejected”正式回归。当前平台重新完成 `xrt.c`、`test.c`、`dev/test_xnet2_stage.c` 编译检查，并用专用 `xnet sync` harness 实际运行通过。
- `2026-03-13`: 继续补齐 `M6` 的统一 wait-source 回归：把 `xnetwaitsrc` 的 timed 路径也纳入正式覆盖，新增“future coroutine timeout via wait source”“stream sync timeout via wait source”“stream coroutine timeout via wait source”三组测试，验证统一包装层在 `timeout/deadline` 返回前同样会完成 waiter 脱链，不会留下悬空 pending wait。
- `2026-03-13`: 继续补齐 `M6` 的统一 wait-source 覆盖面：新增 `wait source` 驱动的 `stream drain timeout`、`stream close deadline`、`stream writable deadline` 三组协程回归，并验证超时/到期后对同一 surface 的重新注册仍能成功完成；当前 `xnetwaitsrc` 对四类 `stream wait-kind` 的 timed waiter 脱链合同都已经有实际运行覆盖。
- `2026-03-13`: 继续补齐 `M6` 的统一 wait-source 对齐度：新增 `future sync timeout/deadline`、`stream drain sync timeout`、`stream close sync deadline`、`stream writable sync deadline` 这组线程侧回归，确认 `xnetwaitsrc` 在 sync/coroutine 两条入口上都能复用同一套 timed wait 语义，而不会把 waiter 生命周期分叉成两种实现。
- `2026-03-13`: 继续推进 `M6` 的实现收口：`xnet_sync.h` 里的专名协程 `stream wait` wrapper 现在统一降成 `xrtNetStreamWaitCoEx/TimeoutEx/UntilEx` 的薄封装，generic wait-kind 分发成为实际主路径，而不再只是测试层抽象；当前平台重新完成 `xrt.c`、`test.c`、`dev/test_xnet2_stage.c` 编译检查，并用专用 `xnet sync` harness 实际运行通过。
- `2026-03-13`: 继续推进 `M6` 的 dispatcher 收口：`xnet_sync.h` 新增 sync/coroutine 两侧的内部 `wait source` dispatcher，`xrtNetStreamWaitEx/WaitTimeoutEx/WaitUntilEx` 与 `xrtNetStreamWaitCoEx/WaitCoTimeoutEx/WaitCoUntilEx` 现在都通过 `xnetwaitsrc` 统一进入同一套 core 分发，而不再和 `xrtNetWaitSourceWait*` 维持两条平行 dispatch 路径。
- `2026-03-13`: 继续推进 `M6` 的 wait-source 结果模型：`xnet_sync.h` 现已补齐 `xrtNetWaitSourceWaitValue*()` 与 `xrtNetWaitSourceWaitCoValue*()`，sync/coroutine 两侧都可以通过统一等待源直接拿到 `future` 的 resolved value 或 `stream` wait 产生的 `pStream` 值；正式回归已覆盖 `future` 与 `stream readable` 的 sync/coroutine value path，以及 timed wait 返回后 value 维持 `NULL` 的合同。
- `2026-03-13`: 继续推进 `M6` 的统一 wait-source 到 datagram：`xnet_dgram.h` 现已引入独立 `xnetdgrampkt`，`xnet_sync.h` 新增 `xrtNetWaitSourceDgramRecv()`、`xrtNetDgramRecvFuture()`、`xrtNetDgramRecv()/RecvTimeout()/RecvUntil()` 与协程版 `xrtNetDgramRecvCo*()`；第一版采用 one-shot packet、单 waiter、与 `OnRecv` 回调互斥的合同，并补了“sync recv helper”“wait-source timeout 后重试”“future 活跃时 early destroy rejected”“coroutine recv helper”四组正式回归。当前平台重新完成 `xrt.c`、`test.c`、`dev/test_xnet2_stage.c` 编译检查，并用专用 `xnet sync` harness 实际运行通过。
- `2026-03-13`: 继续硬化 `M6` 的 datagram 合同并明确 accept defer：补了 `dgram recv` 的 `OnRecv` 冲突拒绝、第二 waiter 冲突拒绝、sync `RecvUntil()` deadline、coroutine `RecvCoUntil()` deadline 这组正式回归；同时把“`listener accept` 必须等待真实 port/worker 生命周期接入后再进入 wait-source”固定成长期决策，避免退回到轮询式伪 future。
