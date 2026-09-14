---
num: 58
slug: future
title: Future / Promise 与组合子
volume: 卷六 进程与并发
type: practice
lead: 写端读端分离的一次性异步结果——完成/失败/取消三态、then 式延续、组合子与等待族；收尾引入 future_bridge 装配桥。
api: future, cancel, future_bridge
---

## 导读

Future 是**一次性异步结果**的标准抽象（第 48 章文件异步、第 59 章执行器、卷七网络 IO 共同的结果货币）：`xrtPromiseCreate` 同时创建一对角色——**Promise 写端**（生产者发布值或失败）与 **Future 读端**（消费者等待并读取）；终局四态（`XFUTURE_RESOLVED` 成功 / `XFUTURE_FAILED` 失败 / `XFUTURE_CANCELLED` 取消 / `XFUTURE_CLOSED` 无值收尾）；**延续**（then 式——源 Future 的结果经回调变换、经输出 Promise 发布新值——异步管道的 C 形态）；**组合子**（future_combine 范例的 First 族——多个 Future 中先完成者胜）；等待族（Wait/WaitFor/WaitUntil + `xrtFutureWatchInit`/`WatchAdd` 回调）。取消令牌（第 54 章）从两端都可取——Future 是取消树的一等公民。

## 引入

第 48 章异步文件已经见过 Future 的用法面（拿到 Future、Wait、Value）——本章补全它的**完整模型**。三个问题开路。问题一：异步操作的结果谁来交付——操作方持有 Promise（写端），调用方持有 Future（读端）：**一对分离的句柄天然匹配"生产者/消费者"两个角色**，写端不会误等待自己、读端无法伪造结果。问题二：多个异步步骤怎么串——查缓存（miss）→查库→回写→返回，四步异步：延续让每步的输出成为下一步的输入（`result: 105` 的 100→+5 就是两次变换），主线只等最终结果。问题三：三个数据源谁快用谁——组合子（First）把三个 Future 合成一个"先到者"的 Future。

三个问题对应 Future 的三层能力：**交付模型**（Promise/Future 对）、**组合**（延续与组合子）、**等待**（族 + Watch）。第 48 章的文件异步、第 59 章的执行器任务、卷七的网络 IO——全部以 Future 为结果货币，本章学的是这门货币的发行规则。

## 概念

### 一对角色与三态终局

```diagram flow
- 创建：xrtPromiseCreate(&Future, 父取消令牌) → 写端+读端
- 发布：xrtPromiseResolve（值）/ Reject（失败+错误）/ Close（无值收尾）
- 取消：任一端的 Cancel——Cancelled 终局（第 54 章令牌联动）
- 读取：State 查态 / Wait 族等待 / Value 借用值 / Error 借用错误
```

**一次性**是 Future 的身份：终局只发生一次——重复发布被拒绝、终局不可改（Resolved 不会变 Rejected）。**终局语义**对应第 54 章"取消≠失败"的完整落地：Resolved（成功交付值）、FAILED（失败——`xrtPromiseReject` 携带第 4 章错误对象）、CANCELLED（控制流放弃）、CLOSED（写端无值收尾——写方销毁前没有结果时的礼貌关闭）。`xfutureresult` 结构体统一携带三态 + 值/错误——读取侧一个结构看全结局。

### 延续：then 的 C 形态

延续回调的签名（future_continue 范例）：`(源结果, 输出Promise, 数据)`——读源、算新值、经输出 Promise 发布。**链式组合**：每环一个回调 + 一个输出——四步异步管道就是四环；主线只在链尾等。**与回调地狱的对照**：嵌套回调把"步骤"藏在缩进里；延续链把步骤排成一条线——错误沿链传播（某环 Rejected 则下游同态）、取消沿链生效（令牌穿链）。**执行时机**：延续在哪个线程跑取决于注册处（调度器侧注册则在调度器线程、池侧在池线程）——组合的确定性来自注册的确定性。

### 组合子：First 与 friends

`future_combine` 范例（`winner[1] = 22` 的输出）：多个 Future 组合成"先完成者"——赢家的索引与值同时可得。First 的经典场景：多副本读（哪个副本快用哪个）、超时竞争（数据 vs 定时器——先到者胜，超时版即 WaitFor 的组合形态）、冗余请求（三路并发取首个成功）。All 族（等全部）在任务组（第 60 章）的汇合等待里呈现——本章聚焦 First。

### 等待族与 Watch

| 形态 | 入口 | 适用 |
| --- | --- | --- |
| 阻塞等待 | `xrtFutureWait` | 简单汇合（第 48 章用法） |
| 限时等待 | WaitFor / WaitUntil | 超时控制（deadline 族） |
| 回调通知 | `xrtFutureWatchInit`/`WatchAdd` | 事件循环（不阻塞——完成时回调） |
| 协程等待 | 调度器协程内 await（第 56 章 Park/Wake） | 协程直线代码 |

Watch 的纪律与全库回调一致：轻活（第 38/48 章）——重处理延续到唤醒后的上下文。**引用配平**：Future/Promise 各自 Ref/Destroy；把手里的引用数交给存它的结构管理（第 5 章纪律）。

### 取消的三向联动

Promise 侧 CancelToken、Future 侧 CancelToken 都可取（`xrtPromiseCancelToken`/`xrtFutureCancelToken`）——取消树穿过整对；`xrtFutureCancel` 是直达按钮。联动形态：父令牌（第 54 章树）→ 创建时的 pParentCancel 参数——父取消则 Future 终局 Cancelled（不需要手动转发）；读端 Cancel 反向通知写端（操作方在 CancelToken 的 Watch 里中止工作——第 54 章检查点纪律）。

### 桥接：把异步完成接回 Future（future_bridge）

异步库的经典竞态：**操作入口要立刻返回一个 Future，而结果要稍后才到**——中间这段"装配窗口"里，底层完成回调可能先到、Future 的终态却还不能写（调用方可能还没拿到 Promise 的引用、取消监听还没挂上）。`xfuturebridge` 就是这段窗口的闸门：`xrtFutureBridgeCreate` 创建 Future/Promise 对并初始化桥（或 `xrtFutureBridgeInit` 桥接调用方自己持有的 Promise——**桥借用不持有**）；`xrtFutureBridgeWatch` 把 Future 的协作取消转发给底层操作；装配完成时二选一发布——`xrtFutureBridgeReady` 放行（此后完成回调可以写 Promise 终态）、`xrtFutureBridgeFail` 失败（回调只回收结果不写入）；底层线程用 `xrtFutureBridgeWait` 等这个发布决定，`xrtFutureBridgeUnwatch` 注销取消监听并与正在执行的取消回调汇合。桥本身是 32 字节固定存储——直接嵌进异步操作的上下文结构，零额外分配。

它的价值在"顺序自由"：装配方与完成方谁先到都不出错——Ready 之前的终态写入被挂起，Ready/Fail 决定放行或回收。第 59 章执行器、第 61 章调度骨架、第 99 章网络库的异步入口，底层都是这座桥——读它们的实现时认出这八个函数，竞态处理就不再神秘。

## 示例

### 完整程序：一次异步交付

来自仓库范例 `examples/concurrency/future/main.c`：

```embed path="examples/concurrency/future/main.c" title="examples/concurrency/future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/future/main.c -lws2_32 -liphlpapi
future value: 42
```

**刚才发生了什么。** ① `xrtPromiseCreate(&pFuture, NULL)`——一次创建写读两端（NULL 即不挂父令牌——挂了就是第 54 章的树形联动）。② Promise 端发布值 42——一次性终局 Resolved。③ Future 端 Wait 等待完成、`xrtFutureValue` **借用读取**值（时效到下一次操作——要持有自己 Ref/拷贝）。④ `future value: 42` 打印——发布→等待→读取的完整交付链。**这个 43 行的闭环是全部 Future 用法的原子形态**——延续、组合、池任务的结果通道全是它的组合化。

### 完整程序：延续链

来自 `examples/concurrency/future_continue/main.c`——then 式两环：

```embed path="examples/concurrency/future_continue/main.c" title="examples/concurrency/future_continue/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/future_continue/main.c -lws2_32 -liphlpapi
result: 105
```

**刚才发生了什么。** ① 延续回调 `addFive(源结果, 输出Promise, 数据)`——读源 Future 的值、加五、经输出 Promise 发布：**输入与输出都是异步的**，变换本身也是异步链上的一环。② 链尾等待得到 `result: 105`（100→+5）——主线没有嵌套、没有中间等待，一条直线等最终值。③ 失败传播：源若 Rejected，延续按约定不发布（或转发错误）——错误沿链到尾（写延续时显式处理源失败——坑区有实例）。④ future_combine 范例是组合子版：多个源中 `winner[1]`（索引 1 的 22）先到——先完成者胜、索引与值齐得。

### 完整程序：bridge——装配窗口的三步演示

来自仓库范例 `examples/concurrency/bridge_tour/main.c`：

```embed path="examples/concurrency/bridge_tour/main.c" title="examples/concurrency/bridge_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/bridge_tour/main.c -lws2_32 -liphlpapi
bridge: create + promise borrow ok
bridge: watch + ready -> resolve -> future value ok
bridge: init on own promise + fail -> wait false ok
```

**刚才发生了什么。** ① `Create` 一步拿到 Future 与桥，`xrtFutureBridgePromise` 借出写端——所有权仍在调用方，销毁责任不变。② Watch（挂取消转发）→ Ready（放行）→ Promise 写终态 → Future 读到值——**顺序刻意摆成"装配先完成"**；真实异步里这三步可能以任何顺序交错，桥都吃得下。③ 第二座桥演示失败路径：Init 桥接自己的 Promise、发布 Fail——此后 `Wait` 返回假，完成回调只回收不写入。取消回调（`exampleCancel`）在 Unwatch 汇合前被转发的事实也一并演示。

## 契约

- **一次性**：终局恰好一次、不可改；重复发布被拒。
- **三态终局**：Resolved/Rejected/Cancelled——`xfutureresult` 统一携带；取消≠失败（第 54 章）。
- **借用读取**：Value/Error 是借用——立即用或自己持有；Destroy 前用完。
- **延续纪律**：显式处理源三态（失败转发或降级——不吞不崩）；输出必发布（否则下游悬挂）。
- **引用配平**：两端各自 Ref/Destroy；存进结构的引用由结构管（第 5 章所有权纪律）。
- **取消联动**：创建挂父令牌则自动联动；两端 CancelToken 各自可取；操作方在令牌 Watch 里中止工作。
- **桥契约**：Ready 前终态写入挂起、Fail 后只回收；桥借用 Promise 不持有；Unwatch 与取消回调汇合后桥才可弃置。

## 避坑

### 坑 1：延续吞掉源失败

症状：链尾永远等不到结果（悬挂）或拿到零值当成功——上游失败被延续静默丢弃。

原因：延续回调只写了成功分支——源的 Rejected/CANCELLED 没有转发给输出 Promise。

```c bad
static void step(const xfutureresult* pIn, xpromise* pOut, ptr pData)
{
	if ( pIn->State == XFUTURE_RESOLVED ) {
		xrtPromiseResolve(pOut, Transform(pIn));   /* 只处理成功 */
	}
	/* 失败分支缺失——pOut 永远不发布——下游悬挂 */
}
```

```c good
static void step(const xfutureresult* pIn, xpromise* pOut, ptr pData)
{
	if ( pIn->State == XFUTURE_RESOLVED ) {
		xrtPromiseResolve(pOut, Transform(pIn));
	} else {
		ForwardTerminal(pOut, pIn);   /* 按源 State 调 Reject 或 Cancel——链不断 */
	}
}
```

### 坑 2：Value 读到旧值或悬空

症状：读到上一次的值、或完成前的垃圾值；偶发崩溃（值指向已释放内存）。

原因：没有终局就读值（一次性结果未就绪）、或把借用值存过了 Future 的 Destroy/下一次操作。

```c bad
ptr Value = xrtFutureValue(pFuture);   /* 未等待——终局前读值是未定义 */
Save(Value);                            /* 借用指针存起来——悬空窗口 */
```

```c good
if ( xrtFutureWait(pFuture) != XWAIT_OK ) { return false; }
ptr Value = xrtFutureValue(pFuture);   /* 终局后借用 */
Use(Value);                             /* 立即用——或拷贝后持有 */
```

## 练习

### 基础：三态制造机

Promise 分别 Resolve 值 / Reject 失败 / Cancel 取消 / Close 收尾四种终局——读端 Wait 后打印 State 与值/错误——四态各验证一次（读端 State 与值/错误逐项打印）。

### 进阶：四环管道

查缓存→miss 查库→变换→回写：四环延续链（每环 mock 一个异步延迟）——链尾等最终值；中间一环注入失败验证传播到尾。

### 挑战：多副本读取器

三个"副本"Future（各自 mock 不同延迟）+ First 组合——取首个返回者；败者取消（令牌联动）。验收标准：成功路径拿到最快副本值、失败路径（全败）得到聚合错误；败者资源收尾（第 6 章统计验证零泄漏）；取消树传播可审计。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 一对角色 | Promise 写端（发布）/ Future 读端（等待读取）——Create 一次成对 |
| 三态终局 | Resolved/Rejected/CANCELLED；xfutureresult 统一携带；一次不可改 |
| 延续 | 回调(源结果,输出Promise,数据)——显式转发失败、输出必发布 |
| 组合子 | First 先到者胜（赢家索引+值同得）；All 在第 60 章任务组 |
| 等待族 | Wait/For/Until 限时 + Watch 无分配回调 + 协程 await（第 56 章） |
| 取消联动 | 创建挂父令牌自动联动；两端各可取 CancelToken |
| 借用纪律 | Value/Error 是借用——立即用或拷贝持有，Destroy 前用完 |
| 装配桥 | Ready 放行/Fail 回收；Watch 转发取消；32 字节可嵌入上下文 |
