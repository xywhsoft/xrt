---
num: 55
slug: coroutine
title: 协程（上）：创建、切换与清理栈
volume: 卷六 进程与并发
type: practice
lead: 有栈协程的原语层——Resume 驱动到暂停点、四态三终态、协作取消与清理栈、独立错误槽。
api: coroutine
---

## 导读

协程三章的上册讲**原语层**：`xrtCoCreate` 创建（有栈——切换回来从让出点继续）、`xrtCoResume` 驱动到下一个暂停点、`xrtCoYield` 让出；四态（READY/RUNNING/SUSPENDED/DONE）+ 三终态（RETURNED/CANCELLED/ERROR）刻画完整生命周期；**清理栈**（CleanupPush/Pop 与 Defer）保证任意退出路径的逆序收尾；协程自带**独立错误槽**与内建 arena（第 7 章）。后端自动选择 Windows Fiber / POSIX 手写上下文切换（四架构支持）。多数业务代码用中册的调度器而非本章原语——但原语是理解一切上层的地基。

## 引入

回到第 53 章的"消费者等待"问题：线程在 Condition 上睡——一个等待占一个线程（1MB 栈的系统成本）；一千个并发等待就是一千个线程，内存与调度双双爆炸。协程换一个模型：**等待时不是睡在原地，而是把执行权交出去、整个"任务"挂起**——代价只是几十 KB 的协程栈；一千个挂起的协程只占几十 MB。唤醒时恢复上下文从暂停点继续——逻辑上像"睡"，物理上是"让"。

这个模型的工程红利在 IO 密集场景：网络服务里成千上万的连接各自"等数据"——线程模型要么线程爆炸、要么 select 轮询复杂化；协程模型每个连接一个协程、等数据时让出、数据到时恢复——**写同步风格的代码，得异步的伸缩性**。卷七的网络引擎正是这么用它的。

## 概念

### 生命周期：四态三终态

```diagram flow
- 创建：xrtCoCreate(过程, 数据, 配置) → READY（未启动）
- 驱动：xrtCoResume → 运行到 Yield（SUSPENDED）或结束（DONE）
- 让出：xrtCoYield 协作点——下次 Resume 从让出点继续
- 终态：RETURNED（正常返回）/ CANCELLED（取消）/ ERROR（过程出错）
- 收尾：Destroy 只对未启动或已结束的协程成功——活跃协程先 Cancel
```

**Resume 驱动到下一个暂停点**是理解协程的关键句：一次 Resume 推动协程跑到下一个 Yield 或结束——不是"执行完"而是"执行到"。金标准章（ch4 错误模型）演示过三段式：创建后三次 Resume（第二次后 SUSPENDED、第三次 DONE+RETURNED）。协程过程签名 `ptr(ptr)`——返回值经 `xrtCoResult` 借用读取。

### 让出与协作取消

`xrtCoYield()` 返回 `xwaitresult`——正常让出返回 OK、**取消请求时返回 CANCELLED**：协程在让出点感知取消、清理后返回（终态 CANCELLED）。`xrtCoCancel` 只是标记（与第 54 章令牌同款协作语义）——真正的终止发生在协程自己的下一个让出点；`xrtCoCurrent` 判断"我在不在协程里"（普通代码调 Yield 是未定义——先判再让）。取消令牌族（第 54 章）经调度器接入协程体系——中册展开。

### 清理栈：任意退出路径的保证

协程内的资源（打开的句柄、借用的缓冲）用**清理栈**托管：`xrtCoCleanupPush/Pop` 手动管理（栈上节点零开销）或 `xrtCoDefer` 自动管理；协程无论**正常返回、取消还是出错**——清理栈都**逆序执行**（与 Go defer 同款保证）。这与第 7 章 arena 的"整段回收"互补：arena 管内存、清理栈管动作（关句柄、发通知这类非内存收尾）。

### 独立错误槽与内建 arena

协程**切换自己的错误槽**（第 4 章的并发版完全体）：协程 A 的错误不会污染承载线程或其他协程——迁移协程不丢错误上下文；协程内照常用 `xrtGetError`/`xrtTakeError` 操作的是自己的槽。内建 arena：协程创建即带独立 temp arena，结束自动回收——第 7 章"请求级临时内存"的协程版（协程就是"请求"）。

### 线程绑定：协程属于创建线程

协程**固定归属创建它的线程**——别的线程 Resume 是未定义。跨线程协作走调度器 Post（中册）或消息传递；这条纪律与有栈协程的上下文实现（TLS/栈守卫）绑定，不是建议是硬性。

## 示例

### 完整程序：两段协程与让出

来自仓库范例 `examples/concurrency/coroutine/main.c`——59 行走完创建/让出/终态：

```embed path="examples/concurrency/coroutine/main.c" title="examples/concurrency/coroutine/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine/main.c -lws2_32 -liphlpapi
after yield: 21
result: 42
```

**刚才发生了什么。** ① 协程过程分两段：前半 `*pValue += 1`（20→21）、Yield 让出、后半 `*pValue *= 2`。② 第一次 Resume：跑到 Yield 暂停（SUSPENDED）——此时 `after yield: 21` 打印（21 是前半的成果——**让出点即观察点**，调用方在此看到中间状态）。③ 第二次 Resume：从让出点继续、跑完返回（DONE + RETURNED）——`result: 42` 是 `xrtCoResult` 借用读取的返回值（21×2）。④ 三次交互（Resume/Resume/读结果）的节奏就是协程原语的全部手感——调度器（中册）把这三拍自动化。

### 完整程序：调度器雏形

来自 `examples/concurrency/coroutine_scheduler/main.c`——多协程交替执行：

```embed path="examples/concurrency/coroutine_scheduler/main.c" title="examples/concurrency/coroutine_scheduler/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine_scheduler/main.c -lws2_32 -liphlpapi
task 1
task 2
task 3
```

**刚才发生了什么。** ① 三个协程提交进调度器——`task 1/2/3` 依次输出展示**交替执行**：不是顺序跑完一个再跑下一个，而是各跑到暂停点轮转（具体节奏取决于调度策略——中册展开）。② 这就是"协作式多任务"的最小样：单线程、零锁、三个"并发"任务——协程的并发不需要多核，只需要交替点。③ 调度器把本章的 Create/Resume 手工节奏封装成 Post/Run 自动节奏——业务代码写"过程"、调度器管"驱动"。coroutine_tour 与 coroutine_event 范例是全接口与协程事件的扩展阅读。

## 契约

- **驱动语义**：Resume 驱动到下一个暂停点（Yield）或终态；不是执行完。
- **终态三分类**：RETURNED/CANCELLED/ERROR；Result/Error 是借用——终态后立即读，Destroy 前用完。
- **线程绑定**：固定归属创建线程；跨线程走调度器 Post 或消息传递——硬性纪律。
- **协作取消**：Cancel 标记、Yield 感知、清理后 CANCELLED 终态；递归进入自身返回 XERR_STATE。
- **清理栈**：逆序执行覆盖全部退出路径；arena 管内存、清理栈管动作——互补不互代。
- **独立错误槽**：协程错误不污染承载线程；内建 arena 随协程回收。
- **Destroy 时机**：只对未启动或已结束的协程成功——活跃协程先 Cancel 驱动到终态。

### 协程与线程的对照表

协程与线程是并发工具箱的两极，一张对照表定分寸。**成本**：线程 1MB 栈 + 内核调度对象；协程几十 KB（可配）+ 用户态切换（纳秒级）。**抢占**：线程可被抢占（调度器说了算）；协程只能协作让出（代码说了算——所以不存在协程级的"执行到一半被撕开"）。**并行**：线程真并行（多核同时跑）；协程单线程内交替（并行要靠"多线程 × 每线程多协程"的组合——中册的调度器模型）。**适用**：CPU 密集并行用线程（或任务池）；IO 密集高并发用协程（等得多跑得少）；两者常组合——每线程一个调度器、每调度器千百协程。**心智模型**：线程共享内存要同步（第 53 章）；协程交替执行在同一线程内——协程之间的共享在很多场景天然互斥（让出点就是边界），但跨线程的调度器实例之间仍要同步。

### 从示例到工程：协程的三个宿主

**网络连接处理**（卷七主场）：每连接一个协程——读到 EOF 为止的直线逻辑（读、解析、应答循环），等待时让出、数据到时恢复；一千连接 = 一千协程 + 一两个线程——这是协程存在的第一理由。**异步流程编排**：多步骤异步流程（先查缓存、miss 查库、回写、响应）写成直线协程——每步 await 让出，恢复后接着写；与 Future 链（第 58 章）相比，协程版有局部变量与栈——状态不用显式存在闭包里。**生成器与惰性序列**：读取大文件的分块处理——每 Yield 产出一块，消费方控制节奏（背压的天然形态）。三宿主的共同判据：**"等待多、逻辑想写成直线"——协程的手感优势就在直线二字**。

### 有栈与无栈：一个工程视角

协程实现的两大门派值得一个工程视角的对照。**有栈**（XRT 选型）：每个协程独立栈——任意深度的调用链里都能让出（框架代码里 Yield 也行）、局部变量天然保留、调试器能看到真栈；代价是每协程的栈内存（几十 KB 起）与上下文切换的栈交换。**无栈**（C++20 coroutine / 状态机编译）：编译器把函数拆成状态机——零栈内存、切换近乎免费；代价是只有协程函数本体能让出（调用链里的函数不能）、局部变量放状态机（生命周期受限）、调试看到的是拆解后的机器。XRT 选有栈的理由：C 语言没有编译器魔法可用，手写状态机的复杂度转嫁给每个用户不可接受——有栈的"任意深度让出 + 真栈调试"是 C 工程的务实解。这个视角也解释了为什么协程栈大小可配（第 52 章线程栈的同款考量）。

## 避坑

### 坑 1：跨线程 Resume

症状：偶发崩溃或状态错乱——协程在别的线程被驱动；复现依赖线程时序。

原因：协程固定归属创建线程（上下文实现绑定 TLS 与栈守卫）——跨线程驱动是未定义行为。

```c bad
/* 线程 A 创建 */          /* 线程 B 驱动 */
xcoro* Co = xrtCoCreate(proc, Data, &Config);
xrtCoResume(Co);            /* B 驱动 A 的协程——未定义 */
```

```c good
/* 跨线程协作走消息传递：A 的协程在 A 线程被驱动 */
/* 线程 B 想触发 → 向 A 的队列/通道发消息（第 21/57 章） */
/* 或用调度器的 Post 把"驱动责任"留在正确的线程（中册） */
```

### 坑 2：清理栈逆序被打破

症状：资源释放顺序错乱——先关了句柄又有人用它；依赖顺序的收尾偶发失效。

原因：手动 CleanupPop 乱序弹出，或混用 Defer 与手动 Pop 导致执行顺序偏离入栈逆序。

```c bad
xrtCoCleanupPush(&Node1, closeHandle, H1);   /* 先入栈：应最后执行 */
xrtCoCleanupPush(&Node2, releaseBuffer, B);  /* 后入栈：应先执行 */
xrtCoCleanupPop(&Node1);   /* 手动先弹了 Node1——逆序被打破 */
```

```c good
xrtCoCleanupPush(&Node1, closeHandle, H1);
xrtCoCleanupPush(&Node2, releaseBuffer, B);
xrtCoCleanupPop(&Node2);   /* 后入先出——逆序保持 */
xrtCoCleanupPop(&Node1);
/* 或全用 Defer：让协程机制保证逆序（推荐——少一个手滑维度） */
```

## 练习

### 基础：生成器

用 Resume/Yield 实现整数序列生成器：每次 Resume 产出一个值（共享变量或 Result），第十次后 DONE——金标准章练习的协程原语版。

### 进阶：生产者消费者（单线程版）

两个协程 + 一个缓冲变量：生产者 Yield 交出时留数据、消费者 Yield 交回时取走——零锁的协作交替；体会"并发不需要多核"。

### 挑战：可取消的流水线协程

三级协程流水线（读/算/写），清理栈托管各级资源；取消令牌命中后三级全部干净收尾（终态 CANCELLED、清理栈全执行、arena 回收）。验收标准：任意时刻取消后资源零泄漏（第 6 章统计）；终态与清理执行顺序进结构化日志可审计。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三拍节奏 | Create（READY）→ Resume×N（跑到 Yield 或 DONE）→ 终态读 Result |
| 四态三终态 | READY/RUNNING/SUSPENDED/DONE；RETURNED/CANCELLED/ERROR |
| Yield 语义 | 让出点继续；返回 CANCELLED 表示取消命中——清理后返回 |
| 清理栈 | Push/Pop 手动 或 Defer 自动；**逆序执行覆盖全部退出路径** |
| 独立设施 | 错误槽隔离（迁移不污染）+ 内建 arena（随协程回收） |
| 线程绑定 | 固归属创建线程——跨线程走调度器 Post 或消息（硬性） |
| Destroy 门槛 | 只对未启动或已结束成功——活跃先 Cancel 到终态 |
