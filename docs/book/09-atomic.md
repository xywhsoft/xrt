---
num: 9
slug: atomic
title: 原子操作、自旋锁与等待
volume: 卷一 起步与核心
type: concept
lead: 单变量的无锁读写、最短临界区的自旋锁、绝对截止的等待数学——全书并发设施的公共底座，卷一收官。
api: atomic, spin, wait
---

## 导读

卷一到此收官。前面八章打的是"单线程世界的地基"：类型与视图（第 3 章）、错误模型（第 4 章）、内存管理（第 5、6 章）、临时区（第 7 章）、版本与裁剪（第 8 章）。本章补上最后一块：**多个执行流同时触碰同一份数据时，最底层靠什么不乱**。它由三层递进组成：**原子操作**（`xatomic32/64/ptr`——单变量的读、写、读改写无交错）；**自旋锁**（`xspinlock`——几条指令长度的临界区保护）；**等待原语**（`xdeadline` 与 `xwaitresult`——超时的数学与等待结果的统一口径）。

这一章是典型的"地基章"：第 8 章说过，原子操作的头文件是特性宏编译期分派的首批消费者之一——同一份头文件在无原子指令的平台上走内部实现，在支持的平台上走内联路径，靠的就是这里的特性闭包。往后看，第 53 章的线程同步、第 54 章的取消体系、第 139–140 章配置中心的"原子替换全局指针"，全部站在本章原语之上；本章只教**原语层**——单机、单变量、看得见摸得着的小件，体系化的并发设计留给卷六。读完你应当能回答三个问题：什么场景**不需要锁**（单变量就够时）；什么时候锁也**不该太重**（自旋与互斥的数量级账）；以及"等待一个东西直到超时"为什么必须用**绝对截止**而不是每轮重设的相对超时。

## 引入

三段真实事故开路。**第一段：计数器丢了更新。** 统计服务用 `count++` 累计请求数，八个线程各自加了一百万次，总数停在七百二十万附近且每次运行都不同。`count++` 是"读—改—写"三步，两个线程的三步可以交错：都读到 41，各加一，都写回 42——一次更新无声消失。Mutex 能修，但为了一个整数的自增付出"睡眠—唤醒"的代价，杀鸡用了牛刀。**第二段：标志位说了谎。** 生产线程先填好结构体，再用一个标志变量宣布"数据好了"；消费线程偶尔看到标志为真，读到的却是半新的数据。没有重排限制的写入之间没有顺序承诺——标志的可见不保证数据的可见。这不是概率玄学，是内存模型问题。**第三段：重试循环的总时长失控。** "每轮等 100 毫秒再试，最多试十秒"的代码，实际跑了二十八秒——每一轮都重新等 100 毫秒，等待预算被重置了十几次；系统需要的不是"每次等多久"，而是"**到什么时候为止**"。

三个问题对应三件工具：原子读改写让 `count++` 一步完成；内存顺序让"发布—获取"成为可依赖的配对；deadline 数学把超时从相对值变成绝对时刻。它们合起来不到三十个函数，却是全书从队列（第 21 章）到取消令牌（第 54 章）所有并发设施的公共底座。

## 概念

### 三种宽度与读改写家族

原子模块提供三种宽度：`xatomic32`、`xatomic64`、`xatomicptr`——32/64 位整数与一个指针宽度。每种宽度的家族形态一致：`Init` 运行期初始化（静态对象用 `XRT_ATOMIC32_INIT(0u)` 这类宏，保证所有平台起始状态一致）、`Load` 读、`Store` 写、`Exchange` 整体换、`FetchAdd/FetchSub/FetchAnd/FetchOr/FetchXor` 读改写、`CompareExchange` 条件换。"Fetch"前缀的语义统一是**返回旧值**：`xrtAtomic64FetchAdd(&Counter, 1u, ...)` 返回加之前的值，计数器本身已经加好——想要新值就用旧值加一，不要回头再 Load 一次（多一次原子操作还多一次时序假设）。

一个直觉性的判断标准：**共享的形状决定工具**。只是一个计数器、一个标志位、一个"当前指向谁"的指针——单变量，原子足够，无锁且便宜；"两个字段必须一起变"这类不变式，原子做不了（两次原子之间仍然可以被打断），那是锁的地盘（第 53 章）；线程之间传的是指针所有权，那是队列的地盘（第 21 章）。层级用错的典型症状：给队列外面包 Mutex（队列本来线程安全）、给计数器上锁（原子便宜一个数量级）。

### 比较交换：期望值的回写契约

`xrtAtomic64CompareExchange(&Counter, &iExpected, 10u, 成功序, 失败序)` 是整个家族里最值得单独讲的一个：当前值等于 `*iExpected` 时换成 `10u` 返回真；**不相等时返回假，并把实际观察到的当前值回写进 `*iExpected`**。这个回写是刻意的契约设计——CAS 循环的标准形态是"拿期望值去试，失败了用回写的实际值修正后重试"，不需要再 Load 一次，也不会在"读期望"与"再试"之间引入额外的窗口。失败序因此必须具备获取语义（回写值要被读到），且不能强于成功序、不能包含 RELEASE——这些约束由参数校验兜底，非法组合设置 `XERR_ARGUMENT` 且对象保持不变。

CAS 的经典用途是"改结构体里的一个字段，但不想锁整个结构体"：读出旧值、在本地算出新值、CAS 写回；失败说明有人抢先，用回写值重来。循环的每一步都是原子的，整个"读—算—写"却不是——这正是它的适用边界：**冲突稀疏时几乎一次成功，冲突密集时循环本身成为热点**。后面的无锁队列（第 21 章）内部就是这个模式的高度工程化。

### 内存顺序：五个等级与一条默认纪律

每个原子操作都带一个 `xmemoryorder` 参数：`XMEMORY_RELAXED`（只保证本次操作原子，不建立任何跨变量顺序——纯计数够用）；`XMEMORY_ACQUIRE`（读取端——本次读之后的访问不许重排到它前面，用于"获取"别人发布的数据）；`XMEMORY_RELEASE`（写入端——本次写之前的访问不许重排到它后面，用于"发布"已写好的数据）；`XMEMORY_ACQ_REL`（读改写两者兼备——CAS 常用）；`XMEMORY_SEQ_CST`（全顺序一致，最直观也最贵）。合法性矩阵很简单：Load 只接受 RELAXED/ACQUIRE/SEQ_CST，Store 只接受 RELAXED/RELEASE/SEQ_CST，违反即 `XERR_ARGUMENT`。

工程纪律只有一条：**没有证明可以放宽之前，用 SEQ_CST**。内存序的坑在于"错了也大概率能跑"——重排与可见性问题在特定 CPU、特定编译器优化级别下才显形，测试绿不等于正确。正确的工作流是：先用 SEQ_CST 写对，性能数据（第 136 章的方法）指出某个热点计数器或发布点后，再**有依据地**降级到 RELAXED 或 ACQUIRE/RELEASE 配对，并在注释里写明为什么安全。平台的承诺是单向的：只提供全栅栏的平台会采用更强的顺序实现，公开契约绝不被弱化——你可以依赖文档里的下限。

"发布—获取"是最常用的一对：生产侧 `Store(标志, 1, RELEASE)`（此前写的数据全部就绪后才宣布），消费侧 `Load(标志, ACQUIRE)`（看到标志后读到的数据必然是宣布前的版本）。配对任一侧用了 RELAXED，承诺即告失效——这是本章避坑第一坑的主角。

### 栅栏、Pause 与无锁判定

三个辅助件补齐家族：`xrtAtomicThreadFence(序)` 是独立栅栏（不附着于任何变量，给"多个普通写入 + 一个原子标志"这类混合形态兜底）；`xrtAtomicSignalFence` 只约束编译器重排不约束 CPU（信号处理器与线程的边界场景）；`xrtAtomicPause()` 是自旋间隙的提示指令（x86 上是 PAUSE，ARM 上是 YIELD）——告诉超流水线"这个循环在等别人，别把你猜测执行的资源全砸在这里"。`xrtAtomicIsLockFree(sizeof(uint64))` 回答"这个宽度在你的平台上是不是真无锁"——绝大多数量级问题在选型阶段问它一次就够了。

### 自旋锁：几条指令的临界区

`xspinlock` 建立在原子 CAS 之上，是全库**最轻的锁**：拿不到就忙等（自旋），不睡眠。它的适用面可以用数量级账说清：临界区只有几条指令时，自旋的代价是一次 CAS 失败加几个 Pause；互斥的代价是潜在的系统调用与线程切换，通常贵一个数量级。反过来，临界区一旦包含 IO、分配、超过几十条指令的计算，自旋就从"便宜"变成"烧 CPU"——忙等的线程不但不干活，还在抢持锁线程的核。所以选型判据是**临界区长度**，不是"自旋听起来高级"。

生命周期三种形态与第 53 章四件套完全同构：栈上 `xrtSpinInit/xrtSpinUnit`、静态 `XRT_SPIN_INIT`、堆上 `xrtSpinCreate/xrtSpinDestroy`；`xrtSpinTryLock` 是非阻塞变体（已持有时返回假）。一条硬契约：**释放仍被持有的锁失败并设置 `XERR_STATE`**——这不是清理建议，是防泄漏的强制检查。

```diagram flow
- 单变量共享（计数/标志/指针）→ 原子操作：无锁，单步交错安全
- 多变量不变式（字段必须一起变）→ 锁：自旋（临界区几条指令）或互斥（更长）
- 线程间传递指针所有权 → 队列（第 21 章）：内部已无锁，外层不加锁
- 等待带期限 → xdeadline 绝对截止 + xwaitresult 统一结果（第 53-54 章体系化）
```

### 等待原语：deadline 数学与五态结果

等待模块只有三个函数和一个枚举，却是全库超时语义的统一出口。`xdeadline` 是 `uint64` 的绝对时刻（单调时钟微秒刻度，第 42 章）：`xrtDeadlineAfter(相对微秒)` 从当前时刻构造截止（溢出返回 `XRT_DEADLINE_NEVER`，即永不超时）；`xrtDeadlineExpired` 判断是否到达；`xrtDeadlineRemaining` 返回剩余微秒。它解决的是引入里的第三段事故：**一处构造、多处传递、绝不重置**——重试循环每轮用 `Remaining` 等待，总预算就是最初那个数。

`xwaitresult` 把"等待的结果"拆成五个互斥的值：`XWAIT_ERROR`（真正失败）、`XWAIT_OK`（成功）、`XWAIT_TIMEOUT`（到时）、`XWAIT_CANCELLED`（被取消——第 54 章的取消令牌会走到这）、`XWAIT_CLOSED`（等待的对象已关闭）。设计意图是**把正常控制流与错误分开**：超时与取消是调用方应当处理的预期分支，不是塞进错误链的"异常"。从第 53 章的 `xrtThreadWait` 到网络库的连接等待，返回的都是这个枚举——本章先把语义记住，体系化的等待与取消在卷六展开。

## 示例

### 完整程序一：原子计数与无歧义的 CAS

```embed path="examples/core/atomic/main.c" title="examples/core/atomic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/atomic/main.c -lws2_32 -liphlpapi
counter=10
```

**刚才发生了什么。** ① `XRT_ATOMIC64_INIT(0u)` 宏初始化——静态与栈上对象统一用它，跨平台起始状态一致；② `FetchAdd(1, RELAXED)` 演示纯计数的最弱序足够——不发布别的数据就不需要顺序承诺；③ CAS 期望值设为 1（与 FetchAdd 后的实际值一致），成功换成 10——**成功路径 `*iExpected` 不动**；④ 成功序 ACQ_REL、失败序 ACQUIRE 的搭配是 CAS 的标准姿势：失败序必须能读回写值且不得强于成功序。把 `iExpected` 改成 2 再运行：CAS 失败，回写把 1 写回去——那个值就是重试的起点。

### 完整程序二：RMW 全家巡检

```embed path="examples/core/atomic_tour/main.c" title="examples/core/atomic_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/atomic_tour/main.c -lws2_32 -liphlpapi
atomic: 32-bit exchange=0F and/or/xor = E/C/5
atomic: cas ok=1 fail-rewrites=A
atomic: 64-bit init/exchange/sub/and/or/xor = 7
atomic: ptr exchange/cas = ok
atomic: lockfree(4/8)=1 fence+pause ok
```

**刚才发生了什么。** ① 三种宽度各走一遍 Exchange 与位运算族——"Fetch 返回旧值"在每个断言里都是直接证据（`FetchAnd` 返回 0x3C，新值 0x0C 是它和掩码的与）；② 第二段输出专门演示 CAS 失败回写：期望 0x99 不匹配，回写实际值 0x0A；③ 指针宽度用静态缓冲区地址做交换与 CAS——"当前指向谁"的发布场景（无锁栈顶、实例切换）就是它的实战位；④ 收尾的 `IsLockFree + ThreadFence + SignalFence + Pause` 把辅助件一次点亮。这个程序也是很好的"迁移自检"：换平台先跑它，五个输出行全对说明原子层行为一致。

### 完整程序三：自旋锁的两种生命周期

```embed path="examples/concurrency/spin/main.c" title="examples/concurrency/spin/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/spin/main.c -lws2_32 -liphlpapi
counter=1
heap try=0/1 destroy ok
```

**刚才发生了什么。** ① 栈形态走 `Init → Lock → 临界区（一条自增）→ Unlock → Unit` 全周期——临界区短到只有一条指令，这正是自旋的合法区间；② 堆形态补齐 `Create/TryLock/Destroy`：持有状态下 `TryLock` 必失败、空闲后必成功——两态各验一次；③ 例子里锁保护的是演示性的单变量——真实场景若真只是计数器，答案是原子而非锁，示例锁的是"展示生命周期"这件事本身。第 53 章会把自旋放进"自旋 vs 互斥 vs 无锁队列"的三方对照里计时。

### 完整程序四：deadline 的剩余量与过期判定

```embed path="examples/concurrency/deadline/main.c" title="examples/concurrency/deadline/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/deadline/main.c -lws2_32 -liphlpapi
remaining: 50000 us
expired: yes
```

**刚才发生了什么。** ① `xrtDeadlineAfter(50000)` 构造 50 毫秒后的绝对截止，`Remaining` 立即读回约 50000 微秒（单调时钟微秒刻度）；② `xrtSleepUntil`（第 42 章时间模块）睡到截止，随后 `Expired` 返回真——三个函数合演"构造—等待—判定"的闭环；③ 注意 `SleepUntil` 吃的也是这套 deadline 数学：全库等待类 API 的超时参数统一从这里换算，一处构造、处处传递。

## 契约

- **原子性边界**：原子操作保证单变量单步无交错；两次原子操作之间**没有**整体原子性——跨变量的不变式归锁（第 53 章），所有权传递归队列（第 21 章）。
- **Fetch 语义**：一律返回操作前的旧值；新值由调用方自行推导，禁止"回头再 Load"的惯性写法。
- **CAS 回写**：成功不动 `*pExpected`；失败把实际观察值写入 `*pExpected`——重试循环据此修正，无需补读。
- **内存序合法性**：Load ∈ {RELAXED, ACQUIRE, SEQ_CST}；Store ∈ {RELAXED, RELEASE, SEQ_CST}；CAS 失败序不含 RELEASE 且不得强于成功序。非法组合 `XERR_ARGUMENT`，对象保持不变。
- **顺序下限承诺**：实现只允许强于文档顺序、不允许弱化——依赖文档写作，不为具体平台写作。
- **默认纪律**：未证明可放宽前用 SEQ_CST；降级须有性能依据（第 136 章方法）并注释理由。
- **自旋锁生命周期**：栈 Init/Unit、静态 `XRT_SPIN_INIT`、堆 Create/Destroy；**释放仍被持有的锁失败并置 `XERR_STATE`**；TryLock 非阻塞、已持有返回假。
- **deadline 纯值**：无所有权、按值传递；`After` 溢出返回 `XRT_DEADLINE_NEVER`；`NEVER` 永不过期、`Remaining` 返回 `UINT64_MAX`。
- **xwaitresult 五态互斥**：ERROR/OK/TIMEOUT/CANCELLED/CLOSED；超时与取消是控制流不是错误，不进错误链。
- **线程规则**：原子对象无部分更新状态（任何观测要么旧要么新）；栅栏与 Pause 对所有平台可移植。

## 避坑

### 坑 1：发布数据不带内存序（标志说了谎）

症状：消费线程看到"数据就绪"标志后读到的仍是旧数据；问题概率性出现，换 CPU 或编译器选项后频率变化。

原因：数据写入与标志写入都用了 RELAXED（或根本是普通写）——两者之间没有顺序承诺，标志可以先于数据可见。"发布—获取"配对缺了一侧，承诺就不存在。

```c bad
Data = Compute();                                   /* 普通写 */
xrtAtomic32Store(&Ready, 1u, XMEMORY_RELAXED);      /* 弱序发布：数据未必已可见 */
```

```c good
Data = Compute();                                   /* 写数据 */
xrtAtomic32Store(&Ready, 1u, XMEMORY_RELEASE);      /* 发布：此前写入全部就绪 */
/* 消费侧配对：xrtAtomic32Load(&Ready, XMEMORY_ACQUIRE) 之后读 Data 必为发布版 */
```

### 坑 2：自旋锁里做长活

症状：负载一上来 CPU 占满、吞吐反而下降；性能剖析显示热点在 `xrtSpinLock` 的忙等循环上。

原因：临界区里放了 IO、内存分配或长计算——忙等的线程不干活还抢核，持锁线程被拖慢，等待变长，恶性循环。自旋的前提是"临界区只有几条指令"。

```c bad
xrtSpinLock(&Lock);
SaveToDisk(&Record);        /* 临界区含 IO：毫秒级——自旋者烧掉整个核 */
xrtSpinUnlock(&Lock);
```

```c good
xrtSpinLock(&Lock);
iCount++;                   /* 几条指令：自旋的合法区间 */
xrtSpinUnlock(&Lock);
/* 长活用互斥（第 53 章 mutex）——睡眠等待不烧 CPU；或先算好再进短临界区提交 */
```

### 坑 3：把相对超时放进重试循环

症状：写着"最多重试十秒"的循环实际跑了二十几秒；每轮等待预算都被重置，总时长随轮数发散。

原因：把"每轮等多久"当成了"总共等多久"。相对超时只约束单轮，循环把它放大成 N 倍；系统真正能承诺的是**绝对截止时刻**。

```c bad
for ( i = 0; i < 100; i++ ) {
	if ( TryOnce() ) { break; }
	Sleep(100000);        /* 每轮重置预算：100 轮可达 10 秒以上 */
}
```

```c good
xdeadline D = xrtDeadlineAfter(UINT64_C(10000000));   /* 循环外一次构造 */
while ( !TryOnce() ) {
	if ( xrtDeadlineExpired(D) ) { return XWAIT_TIMEOUT; }
	WaitOnce(xrtDeadlineRemaining(D));                 /* 剩余量递减，绝不重置 */
}
return XWAIT_OK;
```

## 练习

### 基础：四线程无锁计数

用 `xrtAtomic64FetchAdd`（RELAXED）让四个线程各累加一百万次，主线程汇合后 Load 校验总数恰为四百万。验收标准：总数恒定（每次运行一致）；把原子换成普通 `++` 重跑，观察丢更新。

### 进阶：CAS 循环改字段

结构体含 `Done` 与 `Value` 两个字段；用 `xrtAtomic32CompareExchange` 实现"仅当 Done 为 0 时置 1 并写 Value"的无锁迁移——失败时利用期望值回写修正重试。验收标准：多线程竞争下恰好一个线程成功；全程无锁。

### 挑战：自旋 vs 互斥的三方计时

同一计数任务三版实现：原子版、自旋锁版、mutex 版（第 53 章 API），各计时并对比；再分别把临界区拉长（加一段计算）复测，观察自旋版优势何时反转。验收标准：得出"几条指令内自旋占优、变长后互斥占优"的量化拐点；结论与第 53 章分工一节互证。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 定位 | 卷一收官：原子原语 + 自旋锁 + 等待数学——全书并发的公共底座 |
| 三宽度 | `xatomic32/64/ptr`；Init 宏（静态）与函数（运行期） |
| Fetch 语义 | 返回旧值；新值自行推导，不回头 Load |
| CAS 回写 | 成功不动期望；失败回写实际值——重试的起点 |
| 内存序 | RELAXED/ACQUIRE/RELEASE/ACQ_REL/SEQ_CST；默认 SEQ_CST，降级要有据 |
| 发布—获取 | 写侧 RELEASE、读侧 ACQUIRE 配对；任一 RELAXED 即失效 |
| 辅助件 | ThreadFence/SignalFence/Pause/IsLockFree |
| 自旋锁 | 几条指令临界区专用；释放持锁 → `XERR_STATE`；TryLock 非阻塞 |
| deadline | 绝对截止（单调微秒）；循环外 After 一次、循环内 Remaining |
| xwaitresult | ERROR/OK/TIMEOUT/CANCELLED/CLOSED 五态；超时取消是控制流 |
| 分工线 | 单变量→原子；不变式→锁；传指针→队列（第 21 章） |
