---
num: 55
slug: coroutine-sched
title: 协程（中）：调度器与定时器
volume: 卷六 进程与并发
type: practice
lead: 调度器接管驱动——Post 投递、Sleep 定时、RunUntil 泵循环；每线程一个实例的模型。
api: coroutine
---

## 导读

协程中册把上册的手工三拍（Create/Resume/读终态）升级为**调度器托管**：`xrtCoSchedCreate` 建调度器、`xrtCoGo`（或 `xrtCoSchedPost` 投函数）启动协程、`xrtCoSleep` 协程内定时让出、`xrtCoSchedRun` 驱动泵（跑到全部协程结束）与 `PollFor/PollUntil` 事件等待步进；**定时器**与调度器一体（Sleep 的让出由定时轮唤醒）；**每线程一个调度器实例**是标准模型——协程的线程绑定由调度器天然满足。中册之后，业务代码几乎不再碰上册原语。

## 引入

上册的调度器雏形（三任务交替）留了一个问题：谁调用 Resume？答案若是"主循环手工轮转"，那么"哪个协程先跑、睡几秒的协程谁来叫醒、新协程怎么插队"全要自己写——这个"驱动策略"代码就是调度器。它的三个核心决策：**就绪队列**（Post 进队、Run 出队驱动——FIFO 或按策略）；**定时轮**（Sleep 的协程挂到定时结构、到期回就绪队列——"谁来叫醒"的答案）；**泵循环**（RunUntil 带时限驱动——"什么时候停"的答案：队列空且无定时器时返回，或时限到）。

这套机制组合出的标准形态是**每线程一个调度器 + 泵循环**：线程的主循环就是 `while ( running ) SchedRunUntil(sched, deadline)`——投递进来的协程被驱动、Sleep 的定时唤醒、外部事件（IO 完成回调）Post 新协程。卷七网络引擎的事件循环就是这个形态的工业化：IO 事件 → 唤醒对应连接的协程 → 泵循环继续。

## 概念

### 调度器核心三件

```diagram flow
- 建例：xrtCoSchedCreate() → 调度器（CreateLimit 可限投递量——背压阀）
- 启动：xrtCoGo(调度器, 过程, 数据, 参数)——一步创建并入队；Post 投普通函数
- 定时/挂起：xrtCoSleep(微秒) 定时让出；xrtCoPark 族挂起直到 Wake/取消/时限
- 泵：xrtCoSchedRun 跑到全部结束；Step/PollFor/PollUntil 单步与限时事件等待
```

**`xrtCoGo` 是标准入口**：一步完成创建与入队（多数场景不需要先 Create 再投递）；`xrtCoSchedPost` 投递普通函数（签名 `xcoschedpostproc`——投递即执行的轻事件）；Post 带背压上限（CreateLimit 设的投递量满即失败——第 21 章背压阀的调度器版）。**Sleep 的语义要点**：协程内调用（普通线程未定义）；让出到定时轮、到期由泵唤醒——线程继续驱动别人。**Park/Wake 对**：Park 挂起当前协程直到被 Wake（外部线程安全唤醒）或取消或时限——外部事件（IO 完成）唤醒协程的标准通道。

### 泵循环：Run 与 RunUntil

`xrtCoSchedRun` 驱动到**全部协程结束**（批量任务的守候形态）；`xrtCoSchedStep` 单步（至多驱动一个就绪协程——嵌入既有事件循环的形态）；`PollFor/PollUntil` 在时限内**等待事件并步进**（外部 Post 到来即醒——常驻泵循环的标准节拍：`while(running) PollFor(sched, 10ms)`）。`xrtCoSchedAlive` 查存活协程数（监控指标）、`xrtCoSchedClose` 请求取消全部并停收（停机的调度器入口——第 53 章取消树的上层按钮）。泵循环的两种宿主形态：**常驻线程**——`while(running) PollFor(sched, 10ms)` 循环，外部事件经 Post 唤醒（网络引擎形态）；**嵌入式**——外层事件循环手动调用 Step/PollFor（GUI 框架的空闲时间片——把调度器嵌入既有循环）。

### 定时器族

定时能力以三族呈现：`xrtCoSleep`（协程内睡眠）、`xrtCoParkFor/ParkUntil`（挂起到时限或 Wake——外部事件与定时二选一先到）、投递的延迟执行（Post 的函数在下一泵拍执行——天然"下一帧"语义）。全部走同一个泵——**定时唤醒与协程恢复在同一线程序列化**（单线程调度器的零锁红利）。

### 每线程一个：模型与理由

标准模型是每线程一个调度器实例。理由三层：协程线程绑定（上册硬性纪律）由"协程只在所属线程的调度器里跑"自动满足；单线程内的调度器**零锁**（队列与定时轮只被本线程碰——第 52 章"什么时候不用锁"的最大豁免区）；多线程扩展 = 多个独立调度器 + 跨线程 Post（投递线程安全的——外部事件从任意线程 Post 进正确线程的调度器）。这与第 58 章执行器/任务池的"工作窃取多线程"是两种模型——各有主场（下一节对照）。

### 调度器 vs 任务池：两个模型的选型

| 维度 | 调度器（协程） | 任务池（第 58 章） |
| --- | --- | --- |
| 单元 | 协程（有栈、可让出） | 任务（函数、跑完即止） |
| 等待 | Sleep/让出——挂起几乎免费 | 占线程或配 Future 回调 |
| 线程模型 | 每线程一实例（单线程零锁） | 多线程共享池（工作窃取/队列） |
| 主场 | IO 密集、大量挂起等待 | CPU 密集、少量长任务 |

选型口诀：**等得多用调度器（挂起便宜）、算得多用任务池（吞吐优先）**；混合负载两个都用（任务池里跑的 CPU 任务完成后 Post 回调度器侧的协程——第 57 章 continuation 的跨模型形态）。

## 示例

### 完整程序：三任务交替与泵

来自仓库范例 `examples/concurrency/coroutine_scheduler/main.c`：

```embed path="examples/concurrency/coroutine_scheduler/main.c" title="examples/concurrency/coroutine_scheduler/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine_scheduler/main.c -lws2_32 -liphlpapi
task 1
task 2
task 3
```

**刚才发生了什么。** ① 三个协程 `xrtCoGo` 进调度器——创建并入队。② 泵（Run 或 PollFor 步进）驱动：逐个驱动到暂停点、轮转、驱动到结束——`task 1/2/3` 的输出顺序由队列与让出点共同决定（调度策略的中册版完全体在 tour 范例）。③ 全部结束后 `xrtCoSchedRun` 返回；`Alive` 归零可断言；三个协程的终态可逐一检查。④ 对照上册的"手工三拍"：本例没有显式的 Create/Resume——**启动与驱动分离**是调度器的全部抽象。

### 完整程序：定时与泵循环

来自 `examples/concurrency/coroutine_event/main.c`——协程事件与定时唤醒：

```embed path="examples/concurrency/coroutine_event/main.c" title="examples/concurrency/coroutine_event/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine_event/main.c -lws2_32 -liphlpapi
（事件按定时序到达并唤醒等待协程——输出体现时序）
```

**刚才发生了什么。** ① 协程 `xrtCoPark` 挂起（等待外部唤醒）；定时器到期或 Wake 调用后协程回就绪队列——**定时/事件的唤醒路径**完整走通（Park 的返回值区分自然醒/取消/时限）。② 泵循环在等待期间驱动其他协程（或跑空返回）——单线程上的"并发等待"就是让出 + 唤醒的组合。③ 定时唤醒与协程恢复在同一线程序列化——共享数据在协程间传递不需要锁（单线程调度的红利实证；跨线程只有 Post 与 Wake 两个入口是安全的）。把 coroutine_tour 留作全接口阅读：调度器配置、策略变体、统计各一组断言。

## 契约

- **启动/驱动分离**：Go/Post 只入队、泵只驱动——投递与 Wake 线程安全（跨线程合法）、驱动只属于调度器线程。
- **Sleep/Park 语义**：协程内调用（普通线程未定义）；Sleep 定时轮、Park 等 Wake/取消/时限——都不睡线程。
- **泵形态**：Run 跑到全部结束；PollFor/Until 限时等事件并步进（常驻循环节拍）；Step 单步嵌入。
- **零锁红利**：单线程调度器内队列/定时轮/回调无锁——跨实例协作才需要同步（经 Post 或第 21 章队列）。
- **模型选型**：等得多（IO 密集）调度器、算得多（CPU 密集）任务池——混合负载两模型协作。

### 从示例到工程：调度器的三个宿主

**网络引擎**（卷七主场）：常驻线程 `PollFor(10ms)` 泵 + IO 完成回调 `Wake` 唤醒连接协程——每连接一个协程直线处理读写；引擎的事件层（第 60 章起）与调度器泵共享线程——事件驱动与协程恢复同线程零锁。**异步业务编排**：多步骤流程（查缓存→查库→回写）每步 Park 等外部事件（DB 回调 Wake）——直线代码、异步执行；与 Future 链对照：协程有栈有局部变量，状态不用进闭包。**Actor 模型**：每个 Actor 一个调度器协程、消息经 Post 投递（投递即"发信"）——单协程处理保证 Actor 内无锁。三宿主共同形态：**泵是心脏、Post/Wake 是动脉、协程是细胞**——理解这个循环就理解了调度器的一切用法。

### 停机路径：调度器的收尾

调度器的停机是第 48 章信号章与第 53 章取消章在协程世界的汇合点。完整路径：外部触发（信号或管理指令）→ `xrtCoSchedClose`（请求取消全部协程、停收新投递——第 53 章取消树的大按钮）→ 泵继续跑到 `Alive` 归零（协程各自在 Park/Sleep/让出点感知取消、走清理栈收尾）→ `xrtCoSchedDestroy` 销毁。三条纪律：**Close 后泵要继续跑**（取消是协作式——不泵就没人收尾）；**Alive 归零再 Destroy**（与上册"Destroy 只对已结束成功"同源）；**限时守候**（泵等收尾带 deadline——超时即放弃优雅、记录未收尾数后强退——第 48 章五秒约定的调度器版）。

### 一个观测提醒：调度器的健康指标

调度器自身需要观测（第 49 章管线在并发世界的哨位）：**Alive 数**（存活协程数——稳态上限与泄漏漂移的标尺）、**Post 队列深度**（背压水位——CreateLimit 的满溢次数是过载信号）、**泵周期耗时**（PollFor 一拍的实际用时——周期拉长说明有协程在让出前做重活——坑 1 的运行时指纹）、**Sleep/Park 唤醒延迟**（定时精度漂移——泵节奏被拖的证据）。四指标进结构化日志（第 37 章字段），调度器的行为从黑盒变成曲线——并发的"感觉不对"先看曲线再猜代码。

## 避坑

### 坑 1：泵循环里做长阻塞

症状：调度器上的其他协程集体卡住——某个协程在泵内做了阻塞调用（同步 IO、sleep 线程、抢锁等待），同调度器的所有协程等待。

原因：调度器是协作式——一个协程不让出，别人跑不了；阻塞调用不是让出。

```c bad
static ptr worker(ptr pData)
{
	BlockingRead(Fd, Buf, Size);   /* 阻塞 IO——不是 Yield！ */
	return NULL;                    /* 泵被卡死：其他协程全部等待 */
}
```

```c good
static ptr worker(ptr pData)
{
	/* 阻塞操作移出调度器：投任务池（第 58 章）完成后 Post 回来 */
	xrtTaskSubmit(gPool, blockingTask, pData, NULL);   /* CPU/IO 任务进池 */
	xrtCoPark();   /* 挂起——池完成回调 Wake 唤醒 */
	/* 池完成回调 Post 新协程回本调度器（第 57 章续接形态） */
	return NULL;
}
```

### 坑 2：跨线程直接碰调度器内部

症状：偶发崩溃或队列损坏——外部线程直接操作调度器状态（清队列、遍历协程）。

原因：跨线程只允许 Post（内部同步）；其他操作都假定单线程。

```c bad
/* 线程 B */
/* 直接操作调度器内部状态——与泵循环竞争，未定义 */
```

```c good
/* 线程 B */
xrtCoSchedPost(gSched, shutdownProc, NULL);   /* Post 一个"关停函数" */
/* 关停协程在调度器线程内执行清理——正确的线程做正确的事 */
```

## 练习

### 基础：三拍到泵的改写

把上册练习的生成器从手工三拍（Create/Resume×N）改写成调度器版（CoGo + Run）——输出一致、驱动代码消失。

### 进阶：定时轮实验

三个协程分别 `xrtCoSleep` 100/200/300ms 后打印——验证唤醒顺序与时序；再混入一个不 Sleep 的计算协程，观察它穿插执行。

### 挑战：迷你事件循环

实现单线程事件循环：调度器泵（PollFor 10ms）+ 第 21 章 MPSC 队列收外部事件 + 事件分发到对应协程。验收标准：外部线程投递的事件在下一泵周期内被处理；Sleep 定时与事件唤醒共存；循环空转率低于 50%（PollFor 时限调优有数据支撑）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三件 | CoSchedCreate 建例（Limit 设背压）/ CoGo·Post 启动投递（跨线程安全）/ Run·Poll·Step 泵族 |
| Sleep/Park | Sleep 定时让出、Park 等 Wake（外部线程安全唤醒）/取消/时限——都不睡线程 |
| 泵族 | Run 全结束（守候）/ PollFor·Until 限时事件步进（常驻）/ Step 单步（嵌入）|
| 零锁区 | 单线程调度器内部无锁——跨线程只走 Post |
| 模型对照 | 协程调度器（IO 密集挂起多）vs 任务池（第 58 章，CPU 密集吞吐） |
| 阻塞纪律 | 泵内不做阻塞调用——移任务池完成后 Post 回 |
