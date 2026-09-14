---
num: 60
slug: task-cancel
title: 取消与结构化并发
volume: 卷六 进程与并发
type: practice
lead: 任务组：一组任务一个等待点、终态统计一次拿全、父子作用域的取消树——结构化并发的完全体。
api: task, future, cancel
---

## 导读

任务组（`xtaskgroup`）是结构化并发的完全体：**一组任务一个等待点**（`xrtTaskGroupWait`——不再逐个等 Future）、**终态统计一次拿全**（`xtaskgroupstats`——成功/失败/取消/关闭/拒绝的完整口径）、**父子作用域**（`xrtTaskGroupChild`——取消树的结构化形态：取消父组=整棵子树收到请求，叶的终态仍由生产端确认）。第 54 章的令牌、第 58 章的 Future、第 59 章的池在此汇合——本章把散装的并发工具装配成有**生命周期边界**的程序单元。

## 引入

没有任务组时的"等一批"代码：把 20 个 Future 存进数组、逐个 Wait 或写循环计数、手工统计成功几个失败几个、有任务失败要取消其余的还得自己建令牌转发——四十行样板代码，每次写并发汇合都重写一遍，每次漏掉一个分支（取消的算失败吗？）就是一个统计 bug。

任务组把这四十行收进一个概念。更重要的价值是**结构化**：并发操作的Scope有边界——作用域开始时组创建、内部任务全部属于组、作用域出口时组等待收尾（"要么全部完成、要么全部取消"）。这与结构化编程的块边界同构：**goto（裸线程/裸 Future）被块（作用域组）取代**——泄漏的并发操作（忘了等的 Future）在结构化下不可能发生，因为作用域出口就是汇合点。父子组把这个结构嵌套化：请求处理组是连接组的子组、连接组是服务组的子组——服务停止取消服务组、整棵树响应。

## 概念

### 任务组核心五件

```diagram flow
- 创建：xrtTaskGroupCreate(配置)——组即作用域
- 登记：Add(已有 Future) / Start(启动过程返回 Future)——先预留槽位再启动
- 关闭：Close(停收、自然结束) / Cancel(停收+协作取消请求)
- 汇合：Wait / For / Until / UntilCancel——关闭并等待
- 统计：xtaskgroupstats——终态口径一次拿全
```

**登记的两种方式**：`Add` 跟踪已有 Future（组保留引用到终态——引用配平由组管）；`Start` 先预留槽位再调用启动过程（过程返回 Future——失败时不留未登记项，登记与启动的原子性）。**关闭的两档**：`Close` 自然收尾（存量跑完）、`Cancel` 协作取消（第 54 章取消树的大按钮——向当前项及子组发请求）。**汇合的四形态**：Wait（无限等）、For/Until（限时）、WaitCancel（可被调用方取消——嵌套等待的传递形态，xrtTaskGroupWaitUntilCancel）。

### 终态统计：一次拿全的口径

| 字段 | 语义 |
| --- | --- |
| `Succeeded` | Resolved 的任务数 |
| `Failed` | FAILED 的任务数 |
| `Cancelled` | CANCELLED 的任务数 |
| `Closed` | CLOSED 的任务数 |
| `Rejected` | 组已停收时被拒的提交数 |
| `Active` / `Added` / `Completed` | 活跃/累计/完成 |

第 58 章"取消≠失败"的统计落地：三类终态分开计数——发布报表（第 39 章日志）不用再手工分类。`Rejected` 值得注意：停收后的提交尝试也被统计（过载证据——第 50 章观测的现成字段）。

### 父子作用域：取消树的结构化

`xrtTaskGroupChild(父, 配置)` 派生子组——父 Cancel 时取消传播到子组与叶 Future（第 54 章树的自动形态）；**叶的终态由生产端确认**——取消请求到达后，生产端（Promise 持有者）选择 Reject/Resolve/Cancel 之一（scope 范例验证 `parent completed = 1, cancelled = 1`：一个叶正常完成、一个叶确认为取消——同一父取消下的两种叶终态）。子组的 Wait 只等子组（局部汇合）；父组的 Wait 等全树（全局汇合）——**等待的粒度与作用域的粒度对齐**。

### 组的 Done Future：异步汇合

`xrtTaskGroupFuture` 返回组的 Done Future（组关闭且活动归零时成功）——汇合本身异步化：组的完成可以是延续链上的一环（第 58 章）、可以被 Watch（第 58 章回调）、可以进另一个组（组中组——Done Future 用 Add 挂进上层组）。**等待点从阻塞调用变成可组合的一等值**——结构化并发与 Future 体系的合流点。

### 任务组与协程、池的配合

task 系范例展开三个配合形态。**组+池**（task_group_pool）：任务经池执行、Future 挂进组——池管执行、组管汇合；**组+协程**（task_coroutine / task_group_coroutine）：协程过程返回 Future 挂组——协程内的多步异步整体作为一个组项；**组+Promise**（task_group 主例）：手工 Promise 也可挂组——组不关心 Future 从哪来。三种来源统一汇合——**组是终态的汇合层，与执行层（池/调度器）正交**。

## 示例

### 完整程序：一组一个等待点

来自仓库范例 `examples/concurrency/task_group/main.c`——两组员、一次汇合、终态统计：

```embed path="examples/concurrency/task_group/main.c" title="examples/concurrency/task_group/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/task_group/main.c -lws2_32 -liphlpapi
completed: 2, succeeded: 2
```

**刚才发生了什么。** ① `xrtTaskGroupCreate(NULL)` 建组（NULL 用默认配置）；两个 Promise/Future 对创建（手工生产端——模拟异步操作）。② 两个 Future `Add` 进组——组的引用接管（Future 的后续终态由 Promise 端发布）。③ Promise 端发布两个值——两个 Future 终态 Resolved。④ `xrtTaskGroupWait` **一次等待两个任务**——替代逐个 Wait 的循环；`xtaskgroupstats` 一次拿全：`completed: 2, succeeded: 2`。这就是"一组一个等待点、终态一次拿全"的最小实证——对照引言里的四十行手写版，全部消失。

### 完整程序：父子作用域取消

来自 `examples/concurrency/task_group_scope/main.c`——取消树的结构化形态：

```embed path="examples/concurrency/task_group_scope/main.c" title="examples/concurrency/task_group_scope/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/task_group_scope/main.c -lws2_32 -liphlpapi
parent completed = 1, cancelled = 1
```

**刚才发生了什么。** ① 父组建立、子组经 `xrtTaskGroupChild` 派生、叶 Future 挂进子组——**三层作用域树**。② 父组 `Cancel`——取消请求沿树传播到子组与全部叶（第 54 章树的自动版：不需要手工给每个叶转发令牌）。③ 叶生产端的**自主终态确认**：一个叶在取消前已完成（completed=1）、另一个叶响应取消确认为 Cancelled（cancelled=1）——**取消请求统一发出、终态各自确认**（协作式取消的完整语义：请求≠结果，生产端拥有终态决定权）。④ 父组 Wait 后统计拿到混合终态——`completed = 1, cancelled = 1` 的输出是这条纪律的机器证言。task_tour 范例是全接口巡礼（submit/组/池/取消/限时各一组 ok）。

## 契约

- **登记原子**：Start 先预留槽位再启动——失败不留未登记项；Add 后引用归组管。
- **关闭两档**：Close 自然收尾（存量跑完）/ Cancel 协作取消（树形传播）——按停机语义选。
- **终态自主**：取消请求统一发、终态由生产端确认——取消≠失败≠关闭的三分统计。
- **等待粒度**：子组等子组、父组等全树——等待与作用域对齐；Wait 族四形态（无限/For·Until 限时/UntilCancel 可取消）。
- **Done Future**：汇合异步化——组完成事件可挂上层组、可延续、可 Watch。
- **来源正交**：Future 从池/协程/Promise 来都行——组是汇合层，执行层自由组合。

### 从示例到工程：任务组的三个宿主

**请求处理**（最常见）：每个请求一个子组——请求内的并发操作（缓存/DB/下游）全部挂子组、应答或超时时子组汇合或取消、子组销毁即请求结束——请求生命周期与作用域严格对齐（挑战练习的完全体）。**服务编排**：服务级父组持有全部连接组/请求组——第 49 章停止信号触发父组 Cancel、全树响应、父组 Wait 后安全退出——优雅停机的结构化实现（对照第 54 章"停机路径"的手工版——任务组把它规范化）。**批处理作业**：一批任务一个组——组 Wait 即批完成、stats 即批报表（成功/失败/取消分列）、部分失败的重试策略按 Failed 清单驱动。三宿主的共同形状：**作用域边界=生命周期边界=汇合点**——这就是"结构化"的工程含义。

### 结构化并发的前世今生

结构化并发不是新发明——是旧教训的形式化。**非结构化的痛**：裸线程/裸 Future 像 goto——控制流"跳出去就回不来"（忘了等、没法取消、泄漏无人管）；2016 年前后的"回调地狱"讨论本质就是非结构化并发的可读性灾难。**形式化的源流**：nurseries 概念（Trio 作者 Nathaniel Smith，2017）首次系统化"作用域内必汇合"；Kotlin structured concurrency（2019）把它带进主流；Swift TaskGroup（2022）给出 API 形态参考。XRT 的任务组是这个谱系的 C 实现——**Child 派生、Close/Cancel 两档、终态三分、组 Future**四个设计点都能在谱系里找到对应。理解这条源流的价值：结构化不是"又一个 API 风格"——是并发正确性的形式保障（作用域出口的汇合保证让"泄漏的并发操作"从"靠纪律避免"变成"结构不可能"）。

### 与第 50 章观测的合流：终态统计即观测

任务组的 stats 结构是第 50 章观测管线在并发终点的哨位。**汇报节奏**：批处理组在 Wait 后一次性汇报（第 38 章日志的结构化字段——Succeeded/Failed/Cancelled 三列进 JSON）；服务级父组按周期采样 Active（活跃数曲线——第 56 章调度器健康指标的任务版）。**告警分诊**：Failed>0 报警、Cancelled 大增查上游（谁在发取消）、Rejected>0 查过载（组停收的原因是背压还是停机）。**对账**：请求子组的 stats 与连接父组的累加一致（嵌套统计的自洽性——挑战练习的验收项）。终态统计从"手工数数"变成"结构化产出的观测字段"——卷六的取消主线在第 50 章的观测管线里找到最终落点。

## 避坑

### 坑 1：忘了 Wait/终态就销毁

症状：组销毁后任务仍在跑——悬挂执行、统计丢失、偶发崩溃（组资源已释放而任务终态要写统计）。

原因：作用域的出口是汇合点——销毁前必须 Wait（或 WaitCancel）；这是结构化并发的"花括号"。

```c bad
xtaskgroup* Group = xrtTaskGroupCreate(NULL);
xrtTaskGroupStart(Group, launchQuery, NULL);   /* 异步任务在跑 */
xrtTaskGroupDestroy(Group);                     /* 直接销毁——任务悬挂 */
```

```c good
xtaskgroup* Group = xrtTaskGroupCreate(NULL);
xrtTaskGroupStart(Group, launchQuery, NULL);
if ( xrtTaskGroupWait(Group) != XWAIT_OK ) {   /* 出口即汇合（结构化的花括号） */
	xrtTaskGroupCancel(Group);                   /* 或 Cancel 后限时等 */
}
xrtTaskGroupDestroy(Group);                     /* 汇合后销毁 */
```

### 坑 2：把 Cancelled 当 Failed 处理

症状：正常停机（主动取消在途任务）被报为故障——监控误报、告警噪音；回滚与失败的处置路径混淆。

原因：终态三分（Succeeded/Failed/Cancelled）没进决策——Cancel 后的 Cancelled 是**预期结果**不是错误。

```c bad
if ( Stats.Failed + Stats.Cancelled > 0 ) {
	Alert("tasks failed");   /* 取消也报警——停机=事故的假象 */
}
```

```c good
if ( Stats.Failed > 0 ) { Alert("tasks failed"); }          /* 真失败报警 */
if ( Stats.Cancelled > 0 ) { LogInfo("tasks cancelled"); }  /* 取消记录 */
/* 发布停机 vs 故障停机分开上报（第 54 章纪律的统计面） */
```

## 练习

### 基础：三终态制造机

一个组三个任务：一个 Resolve、一个 Reject、一个被 Cancel 后确认 Cancelled——Wait 后打印 stats 的完整终态行，逐项核对。

### 进阶：嵌套作用域

父组→两个子组（各三任务）——父 Cancel 后观察传播；再对比只 Cancel 一个子组（父与兄弟无感）——两个方向各验证一次，输出各层统计。

### 挑战：结构化请求处理

完整请求组：请求到达→子组创建→三路并发（查缓存/查库/超时竞争）挂子组→子组汇合（First 语义手动实现）→应答→子组销毁。父组（连接级）持有全部请求子组，连接断开时父 Cancel。验收标准：请求子组生命周期与请求严格对齐（零泄漏——第 6 章统计验证）；断开时在途请求全部收尾（终态分类正确）；三层统计对账一致。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 五件 | Create 建组 / Add·Start 登记 / Close·Cancel 两档关闭 / Wait 族汇合 / stats 统计 |
| 终态口径 | Succeeded/Failed/Cancelled/Closed/Rejected 分开计——取消≠失败 |
| 父子作用域 | Child 派生子组；父 Cancel 树形传播；叶终态生产端自主确认 |
| Done Future | GroupFuture 返回组完成 Future——汇合异步化、可挂上层组 |
| 登记原子 | Start 先留槽再启动——失败不留未登记项 |
| 出口即汇合 | 销毁前必 Wait/Cancel——结构化的"花括号" |
| 来源正交 | 池/协程/Promise 的 Future 皆可挂——汇合层与执行层解耦 |
