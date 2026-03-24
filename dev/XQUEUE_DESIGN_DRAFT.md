# XQUEUE 设计草案

版本: `0.1`
状态: `draft`
更新时间: `2026-03-24`



## 1. 背景

当前 `xrt` 已具备：

- 线程创建与等待
- 互斥锁、条件变量、信号量、读写锁
- 基础原子操作与少量自旋能力
- 协程调度与跨线程唤醒
- `future / task / xnet` 等现代并发主线

但仍缺少一套正式、统一、可复用的通用队列基础设施。

这导致当前主线存在几个问题：

- `xnet` 内部跨线程投递已经有实际队列需求，但仍使用互斥链表临时实现
- `future / task / worker` 类场景缺少正式的高性能 `FIFO` 基础组件
- 对外没有统一的队列生命周期、关闭、排空、等待合同
- 后续如果要做线程池、执行器、异步调度器，会重复发明队列

因此需要在 `xrt` 中补齐一条正式的通用并发队列主线。



## 2. 目标

- 提供正式的通用 `FIFO` 并发队列能力
- 优先覆盖高并发、多线程、低争用路径
- 明确区分 `SPSC / MPSC / MPMC` 三类模型
- 以固定容量、无扩容、可预测延迟为第一优先级
- 保持 `Windows / Linux` 主线一致
- 为 `xnet / future / task / thread pool` 等模块提供统一底座
- 生命周期、关闭语义、等待语义从一开始收口
- 保持 API 简洁，不把业务语义塞进基础队列层



## 3. 非目标

- 不在 `xrt` 中实现 AI session 滑动窗口
- 不在 V1 中实现优先级队列
- 不在 V1 中实现延迟队列、时间轮队列
- 不在 V1 中实现无界无锁队列
- 不在 V1 中实现覆盖写环形缓冲
- 不在 V1 中内建对象析构、引用计数、GC、hazard pointer、epoch reclaim
- 不在 V1 中把阻塞等待揉进无锁核心算法本体



## 4. 总体边界

`XQUEUE` 在 `xrt` 中只负责一件事：

> 在明确并发模型下，稳定、高效地完成元素的先进先出投递与提取。

它不负责：

- 业务层的淘汰策略
- 任务优先级
- 任务取消传播
- AI token 预算
- 自动内存所有权转移策略

这些能力如果需要，应由上层模块单独封装。



## 5. 设计原则

### 5.1 明确并发模型优先于“万能队列”

不同队列的性能上限来自并发约束本身。

因此不建议只暴露一个“看起来通用”的 `xqueue`，再靠配置切换语义；而应显式区分：

- `SPSC`
- `MPSC`
- `MPMC`

原因：

- 调用方更容易知道自己是否用对
- 实现可以针对模型做最优路径
- 测试矩阵更清晰
- 性能特征更可预测


### 5.2 固定容量优先

V1 全部采用固定容量、2 的幂、环形数组模型。

原因：

- 容量边界明确
- 内存布局简单
- 无锁实现更稳定
- 延迟和抖动更可控
- 更适合高并发服务基础设施


### 5.3 无锁核心与等待包装分层

核心队列只负责：

- `TryPush`
- `TryPop`
- `PushBatch`
- `PopBatch`
- `Close`
- `Drain`

如果调用方需要：

- 等待非空
- 等待非满
- 超时等待

则通过外层 `waitable queue` 包装完成，不把平台等待原语硬塞进无锁核心。


### 5.4 API 稳定性优先于一次做全

第一版宁可少做，也不要把后续难以兼容演进的语义过早锁死。

尤其要避免：

- 用一个结构体同时表达三种并发模型
- 在 API 中暴露过多内部统计字段
- 让“长度精确值”成为强合同



## 6. 模块划分

建议拆成以下几层：

- `xatomic`
	- 统一原子基础设施
	- 提供明确的 acquire/release/full fence 语义
- `xqueue_common`
	- 公共状态码、flags、辅助工具、cache line 宏
- `xqueue_spsc`
	- 单生产者单消费者有界无锁环
- `xqueue_mpsc`
	- 多生产者单消费者有界无锁环
- `xqueue_mpmc`
	- 多生产者多消费者有界无锁环
- `xqueue_wait`
	- 阻塞等待包装层
- `xqueue_bench`
	- 基准测试与回归用例

对外导出建议统一收口到：

```c
typedef struct xspscq_struct* xspscq;
typedef struct xmpscq_struct* xmpscq;
typedef struct xmpmcq_struct* xmpmcq;
```

也可以采用嵌入式 `Init / Unit` 结构体风格。V1 建议同时支持：

- `Create / Destroy`
- `Init / Unit`

原因：

- `xrt` 当前大量模块同时存在这两种用法
- 队列既可能做独立句柄，也可能嵌入更大对象



## 7. 原子层前置要求

当前 `xrt` 已有：

- `compare_exchange_32`
- `exchange_32`
- `add_fetch_32`
- `add_fetch_64`

但对于正式无锁队列主线，还需要补齐：

- `load32/load64`
- `store32/store64`
- `cas64`
- `exchange64`
- `thread fence`
- `cpu pause / yield` 轻量自旋提示

建议新增统一接口：

```c
typedef enum xrt_memory_order
{
	XRT_MEM_RELAXED = 0,
	XRT_MEM_ACQUIRE = 1,
	XRT_MEM_RELEASE = 2,
	XRT_MEM_ACQ_REL = 3,
	XRT_MEM_SEQ_CST = 4
} xrt_memory_order;

XXAPI uint32 xrtAtomicLoad32( volatile uint32* pValue, xrt_memory_order iOrder );
XXAPI void xrtAtomicStore32( volatile uint32* pValue, uint32 iValue, xrt_memory_order iOrder );
XXAPI uint64 xrtAtomicLoad64( volatile uint64* pValue, xrt_memory_order iOrder );
XXAPI void xrtAtomicStore64( volatile uint64* pValue, uint64 iValue, xrt_memory_order iOrder );
XXAPI bool xrtAtomicCAS64( volatile uint64* pValue, uint64* pExpected, uint64 iDesired, xrt_memory_order iSuccess, xrt_memory_order iFail );
XXAPI uint64 xrtAtomicExchange64( volatile uint64* pValue, uint64 iValue, xrt_memory_order iOrder );
XXAPI void xrtAtomicFence( xrt_memory_order iOrder );
XXAPI void xrtCpuRelax( void );
```

说明：

- `V1` 即使内部先不完全对外开放 memory-order 全矩阵，也应把内部抽象先立住
- `x86 + TCC` 路径如果 64 位原子不稳定，允许内部降级实现，但 public contract 不变



## 8. 数据模型

### 8.1 元素承载方式

V1 建议优先支持两种承载方式：

- 指针队列
- 固定元素尺寸队列

推荐 API 分层：

- `pointer queue`
	- 最适合任务节点、对象指针、stream 命令、future continuation
- `blob queue`
	- 适合固定大小值对象

但从实现成本和主线价值看，建议第一落地顺序是：

1. 先做指针队列
2. 再根据需要补固定元素尺寸队列

原因：

- `xnet / task / executor` 更常见的是传递指针
- 固定尺寸拷贝会放大 cache 压力
- 指针队列更容易形成统一上层封装


### 8.2 容量规则

统一采用：

- 容量必须大于 0
- 实际容量向上对齐到 2 的幂
- 使用 `mask = capacity - 1`
- 队列满时 `TryPush` 返回失败，不自动扩容


### 8.3 长度语义

建议区分：

- `ExactCount`
	- 只允许在 `SPSC` 中作为强语义
- `ApproxCount`
	- `MPSC / MPMC` 仅提供近似值

因此 `MPSC / MPMC` 不建议对外承诺“任意时刻精确长度”。



## 9. 生命周期合同

建议统一以下动作：

- `Init`
- `Unit`
- `Create`
- `Destroy`
- `Close`
- `Drain`
- `Reset`

语义建议如下：

- `Init`
	- 初始化嵌入式对象
- `Unit`
	- 释放内部资源，要求调用方已停止并发访问
- `Create`
	- 分配并初始化独立句柄
- `Destroy`
	- 等价于 `Close + Unit + free(handle)`，但不负责等待其他线程退出
- `Close`
	- 禁止后续新的 `Push`
	- 允许继续 `Pop` 直到排空
- `Drain`
	- 从当前剩余元素中不断 `Pop` 并交给回调清理
- `Reset`
	- 仅在无并发访问且已排空时允许

必须明确：

- `Close` 不是 `Destroy`
- `Destroy` 不应偷偷自旋等待其他线程“自己结束”
- 并发访问期间调用 `Unit/Destroy` 属于调用方错误



## 10. 状态码合同

建议统一返回值而不是大量布尔值：

```c
typedef enum xqueue_result
{
	XQUEUE_OK = 0,
	XQUEUE_EMPTY = 1,
	XQUEUE_FULL = 2,
	XQUEUE_CLOSED = 3,
	XQUEUE_TIMEOUT = 4,
	XQUEUE_ERROR = -1
} xqueue_result;
```

建议：

- `TryPush`
	- 成功返回 `XQUEUE_OK`
	- 满返回 `XQUEUE_FULL`
	- 已关闭返回 `XQUEUE_CLOSED`
- `TryPop`
	- 成功返回 `XQUEUE_OK`
	- 空返回 `XQUEUE_EMPTY`
	- 关闭且空，也仍返回 `XQUEUE_CLOSED`

这样上层更容易区分：

- 只是暂时空
- 还是生命周期已经结束



## 11. SPSC 设计

### 11.1 使用场景

- 单线程生产，单线程消费
- 单 worker 内部的轻量流水线
- 网络读写线程配对传递
- 协程调度器内部 ready transfer


### 11.2 数据结构

建议采用标准 ring buffer：

```c
typedef struct xspscq_struct
{
	uint32 iCapacity;
	uint32 iMask;
	volatile uint32 iHead;
	volatile uint32 iTail;
	ptr* arrItems;
	uint8 _pad0[64];
} xspscq_struct, *xspscq;
```

实际实现中应把：

- producer 热字段
- consumer 热字段

分离到不同 cache line，避免伪共享。


### 11.3 算法特点

- 无需 `CAS`
- 只需要 acquire/release 读写序号
- 理论上是 V1 最简单、最稳、性能最高的基础模型


### 11.4 V1 价值

虽然 `SPSC` 不是当前主需求，但它有三个价值：

- 作为原子层和 cache line 设计的校验基线
- 作为性能上限参考
- 为后续 `MPSC / MPMC` 提供对照回归



## 12. MPSC 设计

### 12.1 使用场景

- 多线程向单 worker 投递任务
- `xnet` worker post queue
- 多 producer 向单消费线程发送命令
- 典型 reactor/worker 模型


### 12.2 为什么 `MPSC` 应该优先落地

相较于 `MPMC`，`MPSC` 具有更高的工程收益：

- 与当前 `xnet` 真实需求直接匹配
- 比 `MPMC` 更容易做稳定
- 争用点更少
- 更容易先跑通性能收益

因此建议：

> `MPSC` 作为第一优先级主实现，`SPSC` 作为先导校验，`MPMC` 放在第三阶段。


### 12.3 实现方向

`MPSC` 有两条常见路线：

- 链表节点 + 原子交换尾指针
- 有界 ring + 原子 claim tail

V1 建议优先选择：

> 有界 ring + 原子 claim tail

原因：

- 容量固定，契合本方案整体边界
- 更好的 cache 局部性
- 不引入节点级动态分配
- 更容易与 `SPSC / MPMC` 统一风格


### 12.4 核心思路

- producer 通过 `fetch_add/cas` 竞争保留一个写槽位
- consumer 单线程推进 `head`
- 每个槽位需要一个发布状态，防止 consumer 读到未发布元素

V1 建议使用：

- `tail_claim`
- `head_consume`
- `slot sequence / slot state`

而不是仅依赖全局 head/tail 差值。


### 12.5 数据结构建议

```c
typedef struct xmpscq_slot
{
	volatile uint64 iSeq;
	ptr pItem;
} xmpscq_slot;

typedef struct xmpscq_struct
{
	uint32 iCapacity;
	uint32 iMask;
	volatile uint32 bClosed;
	uint8 _pad0[64];
	volatile uint64 iHead;
	uint8 _pad1[64];
	volatile uint64 iTail;
	uint8 _pad2[64];
	xmpscq_slot* arrSlots;
} xmpscq_struct, *xmpscq;
```

说明：

- `iHead` 只由 consumer 推进
- `iTail` 由多 producer 竞争推进
- `slot.iSeq` 用于区分当前槽位是否可写、可读



## 13. MPMC 设计

### 13.1 使用场景

- 通用线程池任务队列
- 多 worker 间任务转发
- 通用高并发生产消费场景


### 13.2 实现方向

V1 建议直接采用业界成熟路线：

> 有界 ring + 每槽位 sequence number

原因：

- 不依赖动态节点分配
- 容量固定
- 结构相对简单
- 已被大量成熟实现验证


### 13.3 数据结构建议

```c
typedef struct xmpmcq_slot
{
	volatile uint64 iSeq;
	ptr pItem;
} xmpmcq_slot;

typedef struct xmpmcq_struct
{
	uint32 iCapacity;
	uint32 iMask;
	volatile uint32 bClosed;
	uint8 _pad0[64];
	volatile uint64 iHead;
	uint8 _pad1[64];
	volatile uint64 iTail;
	uint8 _pad2[64];
	xmpmcq_slot* arrSlots;
} xmpmcq_struct, *xmpmcq;
```

与 `MPSC` 看起来相似，但在算法上：

- producer 竞争 `tail`
- consumer 竞争 `head`
- 双边都需要基于 `slot sequence` 判断是否成功


### 13.4 为什么 `MPMC` 不应先做

- 实现最复杂
- 测试矩阵最大
- 伪共享、活锁、边界序号问题更容易出错
- 如果原子层不完整，最容易在这里暴露平台兼容问题

因此建议把它放到 `MPSC` 稳定之后。



## 14. 关闭与排空语义

这是队列模块必须从 V1 就定清的部分。

建议合同：

- `Close`
	- 原子设置关闭标志
	- 之后所有 `Push` 返回 `XQUEUE_CLOSED`
	- 已入队元素仍可继续 `Pop`
- `IsClosed`
	- 返回关闭标志
- `IsDrained`
	- 返回“已关闭且当前无元素”

建议增加辅助判断：

```c
XXAPI bool xrtQueueIsClosed( const xqueuebase* pQueue );
XXAPI bool xrtQueueIsDrained( const xqueuebase* pQueue );
```

`IsDrained` 很适合：

- worker 退出判定
- 线程池关闭收尾
- 生产者结束后的消费者清理



## 15. 阻塞等待包装层

### 15.1 目标

无锁核心不负责阻塞等待，但上层经常需要：

- 空时等待非空
- 满时等待非满
- 支持超时
- 支持关闭唤醒

因此建议单独提供 `waitable queue wrapper`。


### 15.2 包装思路

`xqueue_wait` 内部维护：

- 一个核心无锁队列
- 一个 `xsem` 或 `xcond`
- 可选的 `not_empty`
- 可选的 `not_full`

建议优先路线：

- `pop wait`
	- 用 semaphore 最简单
- `push wait`
	- 先不做通用阻塞版，或仅在需要时做条件变量包装

原因：

- 大多数高并发任务系统更在意“等到有任务可取”
- “等队列非满” 更容易引入复杂性和惊群


### 15.3 注意事项

等待层必须是包装，不应改变核心队列的无锁性质。

也就是说：

- 无锁核心仍可单独使用
- 等待层只是额外同步辅助
- 不允许把 `WaitForSingleObject/pthread_cond_wait` 直接绑死在核心 `TryPush/TryPop` 上



## 16. 对外 API 草案

### 16.1 指针队列优先 API

建议先定义 pointer queue 版本：

```c
typedef struct xqueue_config
{
	uint32 iCapacity;
	uint32 iFlags;
} xqueue_config;

XXAPI bool xrtSPSCQInit( xspscq pQueue, const xqueue_config* pCfg );
XXAPI void xrtSPSCQUnit( xspscq pQueue );
XXAPI xspscq xrtSPSCQCreate( const xqueue_config* pCfg );
XXAPI void xrtSPSCQDestroy( xspscq pQueue );
XXAPI xqueue_result xrtSPSCQTryPush( xspscq pQueue, ptr pItem );
XXAPI xqueue_result xrtSPSCQTryPop( xspscq pQueue, ptr* ppItem );
XXAPI uint32 xrtSPSCQApproxCount( xspscq pQueue );
XXAPI void xrtSPSCQClose( xspscq pQueue );

XXAPI bool xrtMPSCQInit( xmpscq pQueue, const xqueue_config* pCfg );
XXAPI void xrtMPSCQUnit( xmpscq pQueue );
XXAPI xmpscq xrtMPSCQCreate( const xqueue_config* pCfg );
XXAPI void xrtMPSCQDestroy( xmpscq pQueue );
XXAPI xqueue_result xrtMPSCQTryPush( xmpscq pQueue, ptr pItem );
XXAPI xqueue_result xrtMPSCQTryPop( xmpscq pQueue, ptr* ppItem );
XXAPI uint32 xrtMPSCQApproxCount( xmpscq pQueue );
XXAPI void xrtMPSCQClose( xmpscq pQueue );

XXAPI bool xrtMPMCQInit( xmpmcq pQueue, const xqueue_config* pCfg );
XXAPI void xrtMPMCQUnit( xmpmcq pQueue );
XXAPI xmpmcq xrtMPMCQCreate( const xqueue_config* pCfg );
XXAPI void xrtMPMCQDestroy( xmpmcq pQueue );
XXAPI xqueue_result xrtMPMCQTryPush( xmpmcq pQueue, ptr pItem );
XXAPI xqueue_result xrtMPMCQTryPop( xmpmcq pQueue, ptr* ppItem );
XXAPI uint32 xrtMPMCQApproxCount( xmpmcq pQueue );
XXAPI void xrtMPMCQClose( xmpmcq pQueue );
```


### 16.2 批量 API

建议 V1 就预留批量接口：

```c
XXAPI uint32 xrtMPSCQPushBatch( xmpscq pQueue, ptr* arrItems, uint32 iCount );
XXAPI uint32 xrtMPSCQPopBatch( xmpscq pQueue, ptr* arrItems, uint32 iCap );

XXAPI uint32 xrtMPMCQPushBatch( xmpmcq pQueue, ptr* arrItems, uint32 iCount );
XXAPI uint32 xrtMPMCQPopBatch( xmpmcq pQueue, ptr* arrItems, uint32 iCap );
```

原因：

- 对高并发系统，批量收割能明显减少原子争用
- `xnet`、线程池、调度器都可能直接受益


### 16.3 Drain API

建议加统一清理入口：

```c
typedef void (*xqueue_drain_fn)( ptr pItem, ptr pUserData );

XXAPI uint32 xrtMPSCQDrain( xmpscq pQueue, xqueue_drain_fn procDrain, ptr pUserData );
XXAPI uint32 xrtMPMCQDrain( xmpmcq pQueue, xqueue_drain_fn procDrain, ptr pUserData );
```

它很适合：

- 关闭收尾
- 异常回滚
- 单测清理



## 17. 命名建议

推荐模块名：

- `xqueue`

推荐类型名：

- `xspscq`
- `xmpscq`
- `xmpmcq`

推荐文件划分：

- `lib/atomic.h`
- `lib/queue_common.h`
- `lib/queue_spsc.h`
- `lib/queue_mpsc.h`
- `lib/queue_mpmc.h`
- `lib/queue_wait.h`

如果你希望保持更强的“模块前缀可读性”，也可以使用：

- `xrtSPSCQueue`
- `xrtMPSCQueue`
- `xrtMPMCQueue`

但从 `xrt` 现有整体风格看，短命名更协调。



## 18. 内存与缓存局部性约束

队列是极易受 cache line 影响的模块，建议从一开始明确：

- `head` 与 `tail` 必须分离到不同 cache line
- `closed flag` 不应频繁和热路径序号挤在同一行
- `slot` 建议尽量紧凑，避免过大
- 如果需要调试字段，应在 debug 模式单独放置，避免污染热路径

建议统一 cache line 常量：

```c
#ifndef XRT_CACHELINE_SIZE
	#define XRT_CACHELINE_SIZE 64u
#endif
```



## 19. 平台兼容性策略

### 19.1 主目标平台

V1 主要保证：

- Windows x64
- Linux x64


### 19.2 次目标平台

以下路径建议保持 API 兼容，但实现允许保守处理：

- Windows x86
- Linux x86
- `TCC` 某些 64 位原子能力不足路径


### 19.3 降级策略

如果目标平台无法稳定支持某个无锁实现所需原子操作，允许：

- 保持同名 API
- 内部降级为轻锁实现
- 在文档中明确标注该平台为 fallback backend

这样能保持：

- 上层代码不分叉
- 功能完整性不丢
- 平台覆盖面不被无锁实现卡死



## 20. 与现有模块的集成建议

### 20.1 `xnet`

第一优先级集成点就是当前 worker command queue。

建议路线：

- 先保留现有互斥链表实现
- 新增 `xmpscq` 后做并行替换分支
- 用 `xnetEnginePost` 压测验证收益

这样可以把 `queue` 的第一轮价值直接落到真实主线，而不是只停留在 demo。


### 20.2 `future / task`

后续如果补线程池或 executor，可直接复用：

- `MPMC` 作为共享任务队列
- `MPSC` 作为局部 worker inbox


### 20.3 `coroutine`

协程主线不一定直接需要通用无锁队列。

因为很多协程结构仍以单调度线程为主，更多是：

- ready list
- wait queue
- timer heap

因此不建议为了“统一”而强行把协程内部队列改成 `MPMC`。



## 21. 实施顺序

建议按以下顺序推进：

### 阶段 1

- 补齐 `xatomic`
- 增加 cache line 与 pause/yield 辅助设施
- 写 `SPSC` 实现与基准


### 阶段 2

- 实现 `MPSC`
- 补单测、压力测、长时测
- 做 `xnet post queue` 替换试验


### 阶段 3

- 实现 `MPMC`
- 补批量接口
- 增加更大并发矩阵与吞吐/延迟基准


### 阶段 4

- 增加 `waitable wrapper`
- 视需要接入线程池 / executor 主线



## 22. 测试矩阵

建议测试至少覆盖以下类别：

### 22.1 功能正确性

- 初始化/销毁
- 容量向上对齐
- 空/满边界
- 关闭后 push 失败
- 关闭后剩余元素可继续 pop
- drain 行为正确
- reset 限制正确


### 22.2 并发正确性

- `SPSC` 顺序不乱
- `MPSC` 多生产者无丢失、无重复、无越序消费
- `MPMC` 无丢失、无重复
- 长时间序号推进无 wrap 逻辑错误


### 22.3 压力与稳定性

- 1 分钟持续压测
- 10 分钟持续压测
- 高争用小容量
- 低争用大容量
- 批量 push/pop


### 22.4 平台矩阵

- Windows GCC
- Windows TCC
- Linux GCC


### 22.5 调试与诊断

- 非法销毁时错误提示
- 关闭后状态可见
- debug 模式下可选导出统计信息



## 23. Bench 指标建议

建议至少记录：

- 单线程吞吐
- 双线程 `SPSC` 吞吐
- `MPSC` 多 producer 吞吐
- `MPMC` 综合吞吐
- p50 / p95 / p99 延迟
- 不同容量下的吞吐曲线
- 不同 producer/consumer 数量下的扩展曲线

并与以下基线对比：

- `xmutex + list`
- `xmutex + ring`
- 新 `MPSC`
- 新 `MPMC`



## 24. 风险点

需要提前注意的风险主要有：

- 原子语义不足导致跨平台隐藏 bug
- `x86/TCC` 的 64 位原子支持不稳定
- 伪共享导致实测性能远低于预期
- 过早承诺精确长度等强语义
- 把等待层和无锁层耦合，导致实现复杂度失控
- 过早实现无界队列，把问题推向内存回收



## 25. 建议冻结的 V1 范围

如果按本方案推进，建议第一轮正式冻结的只是：

- `pointer queue`
- `SPSC / MPSC / MPMC`
- `TryPush / TryPop / Close / ApproxCount / Drain`
- 固定容量、2 的幂、无扩容合同

暂不冻结：

- 阻塞等待包装细节
- 固定元素尺寸队列
- 调试统计字段
- 批量接口的最终命名细节



## 26. 最终建议

`XQUEUE` 应作为 `xrt` 的正式并发基础模块引入，但必须保持边界克制。

推荐结论如下：

- `xrt` 只做通用并发队列，不做 AI session 窗口
- V1 采用固定容量无锁环形模型
- 实现顺序为 `SPSC -> MPSC -> MPMC`
- 真实主价值优先落在 `xnet worker post queue`
- 原子层必须先补完整，再推进 `MPMC`
- 阻塞等待作为包装层单独设计

如果后续实现阶段只允许先落一条主线，那么最优先应当是：

> `xatomic` 收口 + `xmpscq` 落地 + `xnet` 试接入验证

这是当前收益最大、风险最低、最容易形成闭环的一步。
