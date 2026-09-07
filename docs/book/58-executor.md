---
num: 58
slug: executor
title: 执行器与任务
volume: 卷六 进程与并发
type: practice
lead: 执行资源抽象——提交即忘与 Future 任务双形态、批量接口、Close+Wait 停机协议。
api: executor, future
---

## 导读

executor 是**执行资源的抽象**：任务提交给它而不是直接开线程——池化复用、工作窃取都在这层实现；业务只见**提交与完成**。双形态任务：**提交即忘**（`xrtExecutorSubmit`——跑完就完，无返回值；executor 范例的 1000 个任务计数即此形态）与 **Future 任务**（`xrtTaskSubmit` 返回 Future——第 57 章的货币）；**批量接口**（SubmitBatch——摊还同步成本）；**停机协议**（`xrtExecutorClose` 停收 + `Wait` 排空——与第 21 章队列的关闭停机同构）。第 47 章文件异步的任务池（xtaskpool）是执行器家族的专用形态——本章讲通用版。

## 引入

"来个任务就开个线程"的模型在三处破产。**量**：一万个并发任务 = 一万个线程 = 内存爆炸（每线程 1MB 栈）——池化复用几十个线程跑一万个任务。**开销**：线程创建是毫秒级系统调用——高频短任务的开销超过任务本身；池的提交是微秒级（无锁队列入队）。**管理**：一千个裸线程的停止、等待、错误收集各自为政；执行器给这批线程一套统一的 Close+Wait 停机协议与统计口径。

所以"执行器"的价值公式是：**线程复用（量）+ 轻量提交（开销）+ 统一停机（管理）**。业务侧的变化只是把"开线程"换成"提交任务"——第 47 章你已经这么用过（文件异步的任务池）；本章把执行器的完整接口面与停机协议讲清。

## 概念

### 双形态任务

```diagram flow
- 即忘任务：xrtExecutorSubmit(执行器, 函数, 数据)——无返回值
- Future 任务：xrtTaskSubmit(任务池, 函数, 数据, 取消令牌) → Future
- 批量：SubmitBatch / TaskSubmit 批量变体——一次入队一组
- 选择：要结果/要取消 → Future 形态；纯副作用（计数、通知、清理）→ 即忘
```

**即忘与 Future 不是等级之分**——是需求的分岔：纯副作用的任务（日志落盘、缓存刷新、统计上报）拿 Future 是浪费（没人等）；要结果的（计算、查询）必须 Future（第 57 章的交付模型）。取消令牌参数在 Future 形态天然携带（第 53 章树穿任务池）。

### 提交与完成的最小闭环

`xrtExecutorCreate(配置)` 建例（线程数、队列深度可配——深度是背压阀，满提交失败而不是无限堆）；任务函数签名与协程过程同族（`ptr(ptr)` 或 void 形态按入口）；`xrtExecutorWait` 等全部完成（`WaitFor/Until` 限时变体）。executor 范例的闭环：提交 1000 个即忘任务（各自原子加计数）→ Wait → `completed: 1000`——**提交、执行、汇合**的最简形态。

### 停机协议：Close + Wait

| 步骤 | 语义 |
| --- | --- |
| `xrtExecutorClose` | 停收新任务（再提交失败）；已入队任务**继续执行** |
| `Wait` / `WaitFor` | 等队列排空与在途任务完成——存量不丢 |
| Destroy | 全部结束后销毁资源 |

与第 21 章队列的 Close 段落（第 47 章停机协议）完全同构——**关闭是"停收"不是"作废"**：存量任务执行完、Future 任务的结果照常可取。executor_tour 范例验证：`close+wait closed=1 queued=0`——Close 后 Wait 到排空、队列归零的机器证言。

### 任务池 vs 调度器：终局对照

第 55 章的对照表在批尾升级为**终局版**（两章都已学完）：任务池跑**函数**（无栈——跑完即止，不能中途让出）；调度器跑**协程**（有栈——Park/Sleep 让出后恢复）。**组合形态**是工程标准答案：协程里遇到的阻塞/CPU 工作 Post 给任务池、池完成后 Future/延续把结果送回调度器侧协程（第 57 章延续的跨模型形态）——**协程管等待的骨架、任务池管计算的肌肉**。卷七网络引擎正是这个组合的完全体。

## 示例

### 完整程序：千任务闭环

来自仓库范例 `examples/concurrency/executor/main.c`：

```embed path="examples/concurrency/executor/main.c" title="examples/concurrency/executor/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/executor/main.c -lws2_32 -liphlpapi
completed: 1000
```

**刚才发生了什么。** ① `xrtExecutorCreate(NULL)`——NULL 用默认配置（线程数与队列深度按平台默认；要定制传配置——深度即背压）。② 1000 个即忘任务提交——每个原子加计数（第 10 章原子——任务间唯一共享是这个计数器，无锁完成汇合统计）。③ `Wait` 等全部完成——`completed: 1000` 证明 1000 个任务在池线程上全部执行、无一丢失。④ 注意**没有为任何任务开线程**——池复用了固定线程；提交开销是入队微秒级。这个 62 行闭环是执行器的最小完备用法。

### 完整程序：批量与停机

来自 `examples/concurrency/executor_tour/main.c`——批量接口与 Close+Wait 协议：

```embed path="examples/concurrency/executor_tour/main.c" title="examples/concurrency/executor_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/executor_tour/main.c -lws2_32 -liphlpapi
executor: batch=3 stats(submitted>=3 completed=3) ok
executor: close+wait closed=1 queued=0 ok
```

**刚才发生了什么。** ① `SubmitBatch` 一次入队三个任务——批量摊还同步成本（第 21 章队列批量接口的执行器版）；统计口径（submitted/completed）随批核对。② Close 后再提交失败（停收验证）、已入队继续跑；Wait 到排空——`queued=0` 是队列归零的证言。③ 停机协议两步的顺序不可换：Close 先（否则 Wait 可能在旧任务与新提交之间永远等不完）、Wait 后（存量执行完才 Destroy）。④ Future 形态的任务（带取消令牌的 `xrtTaskSubmit`）在 future/executor 系范例展开——第 57 章已学、本章组合。

## 契约

- **双形态**：即忘（副作用）/ Future（结果+取消）——按需求分岔，不是等级。
- **背压内建**：队列深度配置即上限——满提交失败而不是无限堆积（第 21 章纪律）。
- **停机协议**：Close 停收（存量继续）→ Wait 排空 → Destroy——顺序不可换；与队列关闭同构。
- **批量接口**：组任务一次入队——摊还同步；统计口径随批可核对。
- **任务纪律**：任务函数不做无限循环（除非响应取消令牌）；长任务接受令牌并设检查点（第 53 章）。
- **模型组合**：调度器管等待骨架、任务池管计算肌肉——跨模型经 Future/Post 衔接。

### 从示例到工程：执行器的三个宿主

**并行计算**（Future 形态主场）：批处理、图像处理、数据分析——数据分片提交 Future 任务、`future_combine` 或逐个 Wait 汇合；池深度 = CPU 核数是起点（算力边界），超额提交靠队列排队（背压）。**异步 IO 底座**（第 47 章形态）：文件异步、DNS 解析、外部进程等待——阻塞 IO 包成任务进池，调用方拿 Future 不等；池深度按 IO 并发需求（IO 密集可超核数——等待不占 CPU）。**调度器的计算外挂**（组合形态）：协程遇到 CPU 段或阻塞段 → Post 池 → 池完成 Future/延续 → 回调度器协程——卷七网络引擎的标准分工。三宿主的池深度配置各不相同：CPU 边界用核数、IO 边界用并发预算、混合按测量调优——**深度是性能参数**（第 136 章基准方法的对象）。

### 执行器家族全景

XRT 的"执行资源"不止一个实现，一张全景图防混淆。**xexecutor**（本章）：通用执行器——即忘+批量+Close/Wait 协议，任务模型是"函数跑完即止"。**xtaskpool**（第 47 章）：任务池——`xrtTaskSubmit` 返回 Future、取消令牌一等参数；文件异步的专用底座；与 executor 的关系是"同族不同侧重"——taskpool 的 Future/取消集成更深、executor 的批量与统计更全。**xcosched**（第 55 章）：协程调度器——不是任务池（跑协程不跑函数），但泵循环承担同位的"执行资源"角色。三者共享同一套底座词汇（队列、背压、停机），选择看任务形态：函数→executor/taskpool、协程→sched、混合→组合。

### 一个运维观：池的容量经济学

池深度的配置不是拍脑袋——有经济学。**过小**：并行不足（CPU 核闲置）或 IO 并发不足（吞吐上限被池卡住）——表现为队列深度常满、任务等待时间长。**过大**：线程内存开销（每线程栈）、上下文切换（CPU 型超额无益——核就那么多）、下游压力（IO 型超额打垮数据库连接数）——表现为内存涨、切换高、下游报错。**测量调优**：队列深度曲线（水位常满→加或扩容下游）、任务耗时分布（第 6 章统计或第 41 章计时）、系统指标（CPU/内存/切换）三组数据合看。起点值：CPU 型 = 核数、IO 型 = 并发预算（下游容量 80%）、混合从 CPU 型起步按测量加。**深度不是越大越好**——它是"下游容量的守门员"。

## 避坑

### 坑 1：任务里死循环不停机

症状：Close+Wait 永远不返回——一个任务无限循环，排空遥遥无期；停机超时后强杀留下未收尾状态。

原因：任务不接受取消——执行器的停机依赖任务会结束；无检查点的死循环把协议堵死。

```c bad
static void taskForever(ptr pData)
{
	while ( true ) {
		Poll();   /* 死循环——不看取消令牌 */
	}
}
```

```c good
static void taskCancellable(ptr pData, xcancel* pCancel)   /* Future 形态带令牌 */
{
	while ( !xrtCancelRequested(pCancel) ) {   /* 检查点（第 53 章） */
		Poll();
	}
	/* 取消命中——任务返回，排空推进 */
}
```

### 坑 2：即忘任务里分配 Future 泄漏

症状：即忘任务内部创建了 Future/资源却无人释放——每任务泄一份；统计的活跃对象阶梯上涨。

原因：即忘形态"没人等"不等于"没人负责"——任务内部创建的拥有式产物必须在任务内收尾。

```c bad
static void taskLeak(ptr pData)
{
	xvalue* pTmp = xrtValueInt(42);   /* 创建即弃——无人释放 */
	Report(pTmp);
	/* 少 Release——即忘任务的泄漏窗口 */
}
```

```c good
static void taskClean(ptr pData)
{
	xvalue* pTmp = xrtValueInt(42);
	Report(pTmp);
	xrtValueRelease(pTmp);   /* 任务内创建、任务内收尾 */
}
```

## 练习

### 基础：双形态对照

同一个计算（平方）分别用即忘+原子结果 与 Future+Value 跑十个——代码量、汇合方式、结果读取三方面各写一句对照结论。

### 进阶：批量提交基准

1000 个任务：逐个提交 vs 十批×100——对比总提交耗时与吞吐（第 6 章统计或计时）；批量收益的量级写进结论。

### 挑战：混合负载执行器

配置定制的执行器：CPU 任务（Future 形态、可取消）+ IO 模拟任务（即忘、限时）混合提交；停机协议全走（Close→限时 Wait→统计未完成）。验收标准：负载比例可配；取消树贯穿 Future 任务（停机时在途任务 2 秒内收尾）；未完成统计与日志对账一致。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 双形态 | ExecutorSubmit 即忘 / TaskSubmit→Future（结果+取消） |
| 背压 | 队列深度即上限——满提交失败不堆积 |
| 停机协议 | Close 停收（存量走）→ Wait 排空 → Destroy——顺序铁律 |
| 批量 | SubmitBatch 摊还同步；统计随批核对 |
| 任务纪律 | 无死循环（或带令牌检查点）；任务内产物任务内收尾 |
| 组合模型 | 调度器管等待、池管计算——Future/Post 跨模型衔接 |
