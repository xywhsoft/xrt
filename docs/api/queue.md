# Queue 队列

Queue 体系提供有界、无运行时扩容的并发指针队列。公共层只定义容量、结果和排空回调；SPSC、MPSC、MPMC 与等待适配层分别裁剪，调用方只承担实际使用的并发模型成本。

## 裁剪与依赖

| 功能宏 | 能力 | 直接依赖 |
| --- | --- | --- |
| `XRT_FEATURE_QUEUE` | 公共结果、容量规则 | `XRT_FEATURE_ATOMIC` |
| `XRT_FEATURE_QUEUE_SPSC` | 单生产者、单消费者队列 | `XRT_FEATURE_QUEUE` |
| `XRT_FEATURE_QUEUE_MPSC` | 多生产者、单消费者序列槽队列 | `XRT_FEATURE_QUEUE` |
| `XRT_FEATURE_QUEUE_MPMC` | 多生产者、多消费者序列槽队列 | `XRT_FEATURE_QUEUE` |

启用子模块时必须显式启用依赖宏；公共头会在依赖缺失时拒绝编译。SPSC、MPSC 和 MPMC 核心不依赖线程、锁、信号量或任务体系。

## 公共契约

`xrtQueueCapacity` 把非零最小容量向上取整到 2 次幂。支持范围为 `1` 到 `XRT_QUEUE_MAX_CAPACITY`；零容量报告 `XERR_ARGUMENT`，超过上限报告 `XERR_RANGE`。

`xqueueresult` 将并发流控与运行库错误分开：

| 结果 | 含义 |
| --- | --- |
| `XQUEUE_OK` | 操作成功 |
| `XQUEUE_EMPTY` | 当前没有可读元素，但仍可能继续写入 |
| `XQUEUE_FULL` | 当前没有可写槽位 |
| `XQUEUE_CLOSED` | 写入端已经关闭；弹出操作返回该值时队列已经排空 |
| `XQUEUE_ERROR` | 参数、状态、内存或系统错误，详细信息由 `xrtGetError` 取得 |

`xqueuebatchresult` 同时返回状态和实际处理数量。批量操作允许部分成功，此时 `Result` 为 `XQUEUE_OK`，`Count` 小于请求数。请求数为零是无副作用的成功操作，不要求数组指针非空。

所有非空批量指针数组和单元素 Pop 输出都必须按指针对齐，并且不能与队列结构或内部环重叠。实现只检查本次实际访问的批量前缀；违反条件设置 `XERR_ARGUMENT`、返回 `XQUEUE_ERROR`，且不领取槽位、不移动游标、不写输出。公开结构的存储地址、容量、掩码或游标关系损坏时设置 `XERR_STATE`，不会把损坏的容量差用于环访问。

队列保存原始指针值，不拥有指针目标，也不限制空指针元素。空指针元素由 `xqueueresult` 与真正的空队列区分。

等待、期限和取消不是基础队列结果。后续等待包装层使用统一的 `xwaitresult`，不会把同步原语依赖反向引入队列核心。

## SPSC

`xspscqueue` 只允许一个生产者执行入队、批量入队和关闭，只允许一个消费者执行弹出、批量弹出和排空。状态查询可以由其他线程调用。违反角色数量约束属于调用方错误，库无法在不损失热路径性能的情况下检测。

生产者通过 release 发布尾游标，消费者通过 acquire 取得元素；消费者以 release 发布头游标，生产者以 acquire 复用槽位。生产者必须在最后一次入队完成后调用 `xrtSPSCQueueClose`。消费者观察到关闭后会再次确认尾游标，因此不会把关闭发布前的最后一批元素遗漏为已排空。

### 初始化与所有权

`xrtSPSCQueueInit` 创建由队列拥有的内部指针环，容量按公共规则向上取整。`xrtSPSCQueueCreate` 额外创建队列结构，必须由 `xrtSPSCQueueDestroy` 释放。

两个 `Init` 函数都只接受尚未初始化的队列对象；失败时把非空输出对象保持为零状态。已初始化对象必须先执行 `xrtSPSCQueueUnit`，不能用再次初始化代替释放。

`xrtSPSCQueueInitBuffer` 使用调用方提供的指针数组，不进行分配。外部容量必须已经是 2 次幂，数组必须按指针对齐，并且不能与队列结构重叠。调用方必须保证队列存活期间数组地址和长度不变。

`xrtSPSCQueueUnit` 只释放队列拥有的内部环并清零结构，不释放元素目标，也不释放外部数组。初始化与释放时不得存在并发操作。

### 入队与弹出

`xrtSPSCQueueTryPush` 与 `xrtSPSCQueueTryPop` 从不阻塞。满队列返回 `XQUEUE_FULL`，未关闭的空队列返回 `XQUEUE_EMPTY`。关闭后禁止新入队；已经发布的元素仍按 FIFO 顺序弹出，全部取完后返回 `XQUEUE_CLOSED`。

批量输入和输出数组不能与队列结构或内部环重叠。实现只检查本次实际访问的数组区间，因此超出当前可用槽位的请求仍可部分成功。单元素 Pop 使用内联常数时间范围检查，不为热路径引入跨模块调用。

生产者和消费者各自取得的游标快照之差必须不超过固定容量。超过容量只能来自公开状态损坏；单元素、批量和独占 Reset 路径都会报告 `XERR_STATE`，而不是把无符号下溢解释为可用空间。

`xrtSPSCQueueCount` 是并发快照，只适合统计、监控和启发式流控，不能代替一次实际入队或弹出结果。

### 关闭、排空与重置

`xrtSPSCQueueClose` 幂等且由唯一生产者调用。`xrtSPSCQueueIsDrained` 仅在队列已关闭且没有剩余元素时返回真。

`xrtSPSCQueueDrain` 在消费者角色中弹出当前可见的全部元素。回调非空时按 FIFO 接收元素；回调为空时直接丢弃指针值，仍返回实际移除数量。

`xrtSPSCQueueReset` 只允许在调用方独占队列且队列为空时执行。成功后游标归零并重新开放；队列非空时失败并报告 `XERR_AGAIN`。重置不是并发操作，也不承担元素析构。

## MPSC

`xmpscqueue` 允许多个生产者并发执行入队和批量入队，只允许一个消费者执行弹出、批量弹出和排空。状态查询可以由其他线程调用。实现继承旧版 XRT 已验证的有界序列槽环，但用 32 位模运算代替旧版 64 位游标，避免 32 位目标上昂贵的 64 位原子操作。

每个槽位以 release 发布序号和元素，消费者以 acquire 取得；消费者释放槽位后，生产者才可在下一轮复用。容量上限为 `2^30`，因此序号差转换为有符号 32 位值时，即使游标跨越 `UINT32_MAX` 也不会混淆空槽、就绪槽和已越过槽。

MPSC 提供非阻塞的尝试式 API，但不声明形式化的 lock-free 进度保证：生产者取得尾部区间后若被长期挂起，唯一消费者必须等待该 FIFO 前缀发布。需要可等待、期限和取消的调用方，应使用后续同步/任务阶段提供的等待适配层，核心队列不会引入线程和同步原语依赖。

### 初始化与所有权

`xrtMPSCQueueInit` 创建队列拥有的序列槽环，容量按公共规则向上取整，并把最小有效容量规范为 `2`。`xrtMPSCQueueCreate` 额外创建队列结构，必须由 `xrtMPSCQueueDestroy` 释放。

`xrtMPSCQueueInitBuffer` 使用调用方提供的 `xqueueslot` 数组，不进行分配。外部容量必须是大于等于 `2` 的 2 次幂，槽环必须按指针对齐，并且不能与队列结构重叠。初始化会重写全部槽位；`xrtMPSCQueueUnit` 不释放或清理外部槽环。

两个 `Init` 函数都只接受尚未初始化的队列对象；失败时把非空输出对象保持为零状态。初始化、释放和重置时不得存在并发操作。

### 入队与弹出

`xrtMPSCQueueTryPush` 和 `xrtMPSCQueuePushBatch` 可由任意生产者调用。满队列返回 `XQUEUE_FULL`；批量入队会原子预留一个连续区间，允许只取得当前可用前缀。`xrtMPSCQueueTryPop`、`xrtMPSCQueuePopBatch` 和 `xrtMPSCQueueDrain` 只允许由唯一消费者调用。

批量输入和输出数组不能与队列结构或完整序列槽环重叠。实现只验证并访问本次实际处理的前缀。单元素 Pop 同样拒绝覆盖队列元数据或任一槽位，避免先清空输出再破坏待消费元素。`xrtMPSCQueueCount` 是有界近似值，包含已经预留但可能尚未发布的槽位，只适合统计和启发式流控。

### 关闭、排空与重置

MPSC 的关闭合同刻意避免增加每次入队的引用计数成本：调用方必须先阻止新生产者进入，并等待所有生产者调用返回，然后才能调用幂等的 `xrtMPSCQueueClose`。关闭与生产者并发属于合同违例。关闭后消费者仍可排空所有已发布元素，随后弹出返回 `XQUEUE_CLOSED`。

`xrtMPSCQueueIsDrained` 仅在队列已关闭且头尾游标相等时返回真。`xrtMPSCQueueReset` 只允许在调用方独占且队列为空时执行；它会重新初始化全部槽序号并开放写入端。

## MPMC

`xmpmcqueue` 允许多个生产者并发入队，也允许多个消费者并发弹出。生产者热路径与 MPSC 共用同一份内部序列槽实现；消费者通过 CAS 领取头部的一个槽或一个连续批量区间。两套公开 API 分别表达不同角色合同，不要求调用方接触通用基类或函数指针。

MPMC 与 MPSC 使用相同的 32 位模序号、`2^30` 容量上限、release/acquire 发布关系和热游标隔离。x86 目标使用 64 字节隔离跨度，AArch64 与 PowerPC64 目标使用 128 字节隔离跨度；这是布局规则，不承诺等同于运行 CPU 的实际缓存行大小。它同样提供非阻塞尝试式 API，但不声明形式化的 lock-free 进度保证：已经取得头尾区间后暂停的线程可能暂时阻塞 FIFO 前缀或槽位复用。

### 初始化与所有权

`xrtMPMCQueueInit` 和 `xrtMPMCQueueCreate` 创建队列拥有的槽环，最小容量规范为 `2`。`xrtMPMCQueueInitBuffer` 接收调用方拥有的 `xqueueslot` 数组；容量必须是大于等于 `2` 的 2 次幂，数组必须按指针对齐且不能与队列结构重叠。

初始化会重写全部外部槽位。`xrtMPMCQueueUnit` 只释放内部槽环并清零队列，不释放外部槽环和元素目标。初始化、释放和重置都要求调用方独占对象。

### 生产与消费

`xrtMPMCQueueTryPush`、`xrtMPMCQueuePushBatch` 可由任意生产者调用；`xrtMPMCQueueTryPop`、`xrtMPMCQueuePopBatch` 和 `xrtMPMCQueueDrain` 可由任意消费者调用。批量操作通过一次 CAS 领取连续区间，成功时返回实际处理前缀。

同一个消费者调用中的批量结果保持队列顺序；多个消费者领取的区间也按头游标线性化，但各线程何时处理完成不受队列控制。批量输入和输出数组不能与队列结构或完整槽环重叠。尤其不能把单元素 Pop 输出指向槽内 `Item`：实现会在领取槽位前拒绝该别名，避免释放槽位后把旧值写回已被生产者复用的位置。

`xrtMPMCQueueCount` 是流控快照：尾游标包含生产者已经预留但可能尚未发布的槽位，头游标不再包含消费者已经领取但尚未返回的槽位。因此它不能用于证明全部工作已经处理完成。

### 关闭与生命周期

调用方必须先阻止新生产者进入并等待全部生产者调用返回，再执行 `xrtMPMCQueueClose`。消费者可以在关闭前后持续运行；当没有更多可领取元素时，弹出返回 `XQUEUE_CLOSED`。

`xrtMPMCQueueIsDrained` 表示队列已关闭且没有尚未领取的元素，不表示其他消费者已经完成本地处理或返回。销毁、释放和重置前必须另外等待所有消费者退出。`xrtMPMCQueueDrain` 与其他消费者并发时只保证排空当前线程成功领取的元素，回调也可能由多个调用线程并发执行。

`xrtMPMCQueueReset` 只允许在所有生产者和消费者停止、队列为空且调用方独占对象时执行。成功后重新初始化全部槽序号并开放写入端。

## 示例

```c
#include <xrt.h>

xspscqueue Queue;
ptr Storage[256];

if ( xrtSPSCQueueInitBuffer(&Queue, Storage, 256u) == false ) {
	return 1;
}

if ( xrtSPSCQueueTryPush(&Queue, pMessage) == XQUEUE_OK ) {
	/* 消息已经发布。 */
}

xrtSPSCQueueClose(&Queue);
while ( xrtSPSCQueueTryPop(&Queue, &pMessage) == XQUEUE_OK ) {
	consume(pMessage);
}
xrtSPSCQueueUnit(&Queue);
```

可运行范例位于 `examples/containers/queue_spsc/main.c`。

MPSC 的批量范例位于 `examples/containers/queue_mpsc/main.c`。

MPMC 的批量范例位于 `examples/containers/queue_mpmc/main.c`，多线程竞争由 `tests/containers/test_queue_mpmc_threads.c` 完整演示。

## 性能门禁

`dev/bench/queue/bench_queue_pointer.c` 测量 SPSC、MPSC、MPMC 的单元素与批量吞吐量，`dev/bench/queue/bench_queue_latency.c` 测量 MPSC、MPMC 的平均延迟、P50、P95、P99 与消费者公平性。两者直接编译当前 `single/xrt.h`，不依赖网络头或网络链接库。

Windows 与 Linux runner 位于 `dev/bench/run_queue_*`。历史 `20260324`、`20260325` 报告只作为旧版比较证据；XRT 2 的发布结论必须使用固定主机、编译器、矩阵和串行采样重新生成，不能把诊断用 CPU 绑定结果混入默认基线。
