---
num: 54
slug: cancel-system
title: 取消体系：令牌、传播与终态
volume: 卷六 进程与并发
type: practice
lead: 取消令牌的请求-观察-传播三件套、父子树与引用计数、可取消等待的统一姿势——优雅停止的地基。
api: cancel, channel
---

## 导读

取消要解决的问题是：**怎么让"正在等/正在跑"的工作停下来**。XRT 的答案是**取消令牌**（`xcancel`）：`xrtCancelRequest` 发起取消（仅首次返回 true 并触发监听）、`xrtCancelRequested` 查询令牌或任一祖先是否命中、`xrtCancelWatch` 注册观察者（回调式唤醒——至多同步执行一次，`xrtCancelTriggered` 查询监听命中）；`xrtCancelChild` 派生子令牌形成**取消树**（父取消传播到全部后代）；`xrtCancelRef` 引用计数让多持有者共享一个令牌。本章把它定位为"取消体系专章"——令牌是地基，第 57~60 章的 Channel/Future/任务组的取消接口全部建立在它之上。

## 引入

服务收到停止信号（第 49 章）后，主循环停了——但五个工作线程还在各自的 Channel 接收上死等、三个 Future 还没完成、两个连接还在收尾。没有取消体系的解法：给每个等待加超时轮询（延迟高）、全局标志位+各处检查（侵入式改写每个等待点）、或干脆 exit 整个进程（在途工作全部丢失）。三种都见过，三种都痛。

取消令牌的解法：创建一个根令牌挂在停止信号上；每个工作持有它（或其子令牌）；**可取消等待**（如 `xrtChannelRecvCancel`）把"等数据"与"等取消"合成一次等待——先到者胜。停止信号触发根令牌 → 全树传播 → 所有死等同时被唤醒并收到 `XWAIT_CANCELLED` → 各自走收尾路径。**一次请求、全树苏醒、各自收尾**——这是取消体系的全部承诺。

## 概念

### 令牌三件套：请求、观察、传播

```diagram flow
- 创建：xrtCancelCreate() → 令牌；Ref/Destroy 引用计数共享
- 请求：xrtCancelRequest(令牌) → 标记命中（仅首次 true；单向不可撤销）
- 观察：Requested 查令牌/祖先链 / Watch 回调（至多同步一次）+ Triggered 查监听
- 传播：CancelChild(父) → 子令牌——父命中自动传播子；子不影响父
```

三个设计要点。**取消单向不可逆**——发起即生效，没有"取消的取消"；这个简化换来了无锁快路径（Triggered 检查是一次原子读）。**请求与观察分离**——Request 只是标记，谁在什么等待上被唤醒取决于谁注册了观察；"发起取消"和"响应取消"的代码可以完全不知道对方。**监听一次性**——Watch 的回调至多同步执行一次（回调里做标记/唤醒，再次检查用 Triggered 轮询）——回调不会重复打扰，也不必写防重入。

### 取消树：结构化传播

`xrtCancelChild(父)` 派生子令牌：父命中 → 子全部命中（传播方向单一）；子取消 → 父与兄弟不受影响（局部放弃）。这棵树的形状对应**调用的形状**：一个请求处理派生自己的子令牌——请求超时只取消这一个请求的工作（局部）；服务停止取消根（全局）。第 60 章结构化并发的"作用域取消"正是这棵树的规范化用法——作用域进、令牌生、作用域出、子树全收。

### 可取消等待：统一的等待姿势

XRT 并发体系的等待接口遵循统一形态：**等待对象 + 可选令牌**。`xrtChannelRecvCancel(通道, ..., 令牌)` 等数据或取消；Future 的等待族同样接受令牌；任务组的汇合等待亦然。返回的 `xwaitresult` 统一报告等待结局——`XWAIT_OK`（等到）、`XWAIT_CANCELLED`（取消命中）、`XWAIT_TIMEOUT`（超时）。**一个返回码枚举贯穿全部等待**——第 42 章埋的种子在并发体系全面发芽。写自己的可取消等待（自定义阻塞结构对接令牌）用 Watch 注册唤醒回调——cancel 范例的 `stopWork` 就是形态模板。

### 终态语义：取消不是失败

`XWAIT_CANCELLED` 与错误（第 4 章）是两个维度：取消是**控制流决策**（"我们决定不做了"），错误是**操作失败**（"做了但没成"）。混淆的代价：把取消当错误上报（监控误报故障率）、把错误当取消吞掉（真失败被静默）。第 60 章任务组会把两者合成完整的终态分类（成功/失败/取消），本章先立"取消≠失败"的观念。

## 示例

### 完整程序：传播与观察者

来自仓库范例 `examples/concurrency/cancel/main.c`：

```embed path="examples/concurrency/cancel/main.c" title="examples/concurrency/cancel/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/cancel/main.c -lws2_32 -liphlpapi
operation stopped: yes
watch-triggered=1
```

**刚才发生了什么。** ① `xrtCancelCreate` 建令牌、`xrtCancelChild(父)` 派生子——观察侧的 Requested 沿祖先链查询（子令牌上问"你或你的祖先被取消了吗"），传播无需显式通知。② 工作侧两种观察姿势并用：轮询 `Requested`（`operation stopped: yes` 的判定——含祖先链）与 `xrtCancelWatch` 注册回调（`stopWork` 置停止标志——**观察者回调只做唤醒或标记**，与本卷"回调轻活"纪律一致；回调至多一次，之后由 Triggered 轮询接管）；`watch-triggered=1` 断言监听命中。③ `xrtCancelRef/Destroy` 的引用配平——多持有者共享令牌时各自 Ref、各自 Destroy。④ 停止路径走完：Request → 传播 → 观察命中 → 工作收尾——取消体系的四拍节奏。

### 完整程序：可取消的通道接收

来自 `examples/concurrency/channel_cancel/main.c`——死等通道的标准解法：

```embed path="examples/concurrency/channel_cancel/main.c" title="examples/concurrency/channel_cancel/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/channel_cancel/main.c -lws2_32 -liphlpapi
cancelled: yes
```

**刚才发生了什么。** ① 接收线程在 `xrtChannelRecvCancel` 上等待——**一次等待同时盯着两个事件**（数据到达 / 取消命中），先到者胜；这不是轮询——是内核级的多路等待。② 主线程发令 Request 后，接收立即以 `XWAIT_CANCELLED` 返回——`cancelled: yes` 的判定来源；没有数据丢失风险（取消先到就当没等到）。③ 对比"超时轮询方案"：RecvCancel 无延迟（取消即醒）、无空转（不烧 CPU）、无侵入（等待代码一处改）。deadline 范例是同一思想的姊妹篇——`xrtDeadlineAfter/Expired` 与取消令牌组合成"等数据、或取消、或超时"的三路等待（并发等待的标准完全体）。

## 契约

- **单向不可逆**：Request 即生效（仅首次返回 true），无撤销；Requested 含祖先链查询——传播即查询。
- **传播方向**：父→子单向；子取消不影响父与兄弟——局部放弃的语义基础。
- **观察轻活**：Watch 回调至多同步一次、只做唤醒/标记；此后 Triggered 轮询接管——重活在等待侧收尾。
- **统一等待**：等待族接口 = 等待对象 + 可选令牌；结局经 `xwaitresult`（OK/CANCELLED/TIMEOUT）。
- **取消≠失败**：控制流决策与操作失败是两个维度——上报与统计分开（第 60 章终态分类）。
- **引用配平**：多持有者各自 Ref/Destroy；令牌树随持有者生命周期收尾。

### 从示例到工程：取消的三个宿主

**服务停机**（第 49 章信号章的下半场）：根令牌挂在停止信号上 → 各工作线程的等待全部改可取消形态（RecvCancel/Future 带 token）→ 停止信号 Request 根 → 全树苏醒 → 各自收尾 → 主线程汇合（Wait 全部线程）后退出。停机的"等存量"限时（第 49 章五秒约定）在令牌体系里自然实现：Request 后 WaitFor 汇合带 deadline，超时即强退。**请求超时**：每个请求处理派生子令牌（`CancelChild(根)`）挂在请求 deadline 上——单个请求超时只取消自己的工作树，根与兄弟无感；第 65 章 TCP 的 AcceptWait deadline 是它的网络版。**用户取消**：CLI 的 Ctrl+C、界面按钮——Watch 回调把 UI 事件翻译成 Request；"取消按钮"的全部实现就是一行 Request。

### 取消树与作用域：结构化并发的预演

取消树最有力的用法是**作用域绑定**：一段并发工作开始时派生令牌、结束时销毁——令牌的生命周期与代码块重合，"块内所有工作要么全部完成、要么全部取消"的语义由树结构保证。第 60 章结构化并发把这个用法规范化（任务组、作用域取消、终态分类），但它依赖的原语全部在本章——**先会种树（Child），再学剪枝（作用域）**。两个预备纪律：子令牌随工作创建与销毁（Ref 配平——泄漏的令牌让 Triggered 永假）；作用域出口前必须等到全部子工作落地（"取消已发"不等于"工作已停"——汇合是收尾的一半）。

### 一个设计观：取消体系的"检查点经济学"

取消的及时性取决于检查点密度——密度是成本（每次 Requested 是原子读，便宜但累积；逻辑被检查点切割可读性受损）。经济学平衡术：**层级检查**——粗粒度段间检查（毫秒级响应）、关键路径步进检查（循环内）、纯计算密集段用 Watch 异步打断（如果平台支持）或分段自检；**调用方决定密度**——库函数暴露可取消版本（RecvCancel 形态）而不是内部偷偷检查——调用方知道延迟预算，库不知道；**测量验证**——取消延迟（Request 到工作实际停止的时间）作为可观测指标进日志（第 50 章字段），漂移立现。三招合起来："多快能停"从玄学变成可设计、可测量、可回归的工程属性。

## 避坑

### 坑 1：取消检查只放在循环头

症状：取消后工作"还要跑完当前这一大段"才停——延迟数十秒；或者根本停不下来（长操作内部无检查点）。

原因：取消是**协作式**的——它只标记与唤醒，"停下来"要靠工作代码在检查点主动看 Triggered；检查点太稀疏等于没检查。

```c bad
while ( running ) {
	ProcessOneBigChunk();   /* 一大段 30 秒——取消要等它跑完 */
	if ( xrtCancelRequested(pToken) ) break;   /* 检查点只在循环头 */
}
```

```c good
while ( running ) {
	for ( int i = 0; i < ChunkSteps; ++i ) {
		ProcessStep(i);
		if ( xrtCancelRequested(pToken) ) {   /* 步进级检查点 */
			goto Cleanup;
		}
	}
}
Cleanup:
	ReleasePartialWork();   /* 部分工作的收尾 */
```

### 坑 2：把取消当错误上报

症状：每次发布（正常取消一批在途工作）监控就报"错误率飙升"；回滚与故障混在同一个告警通道。

原因：`XWAIT_CANCELLED` 与错误共用上报路径——控制流事件被统计成故障。

```c bad
if ( xrtChannelRecvCancel(&Ch, ..., pToken) != XWAIT_OK ) {
	ReportError("recv failed");   /* 取消也进错误——误报 */
}
```

```c good
xwaitresult R = xrtChannelRecvCancel(&Ch, ..., pToken);
if ( R == XWAIT_OK ) { Handle(item); }
else if ( R == XWAIT_CANCELLED ) { LogInfo("recv cancelled"); /* 控制流 */ }
else { ReportError("recv failed"); /* 真失败 */ }
```

## 练习

### 基础：传播树验证

建三层令牌树（根→中→叶），对根 Request，断言三层 Requested 全命中；再对中 Request，断言只有中与叶命中、根不受影响——传播方向的两面各验证一次。

### 进阶：三路等待

数据、取消、超时的三路合成：RecvCancel 带 deadline 的等待（或 Watch+DeadlineAfter 组合）——三个结局各触发一次并断言返回码正确。

### 挑战：可停止的批处理

长批处理任务：步进级检查点 + 部分进度保存（中断点记录）+ 取消后可从断点续跑。验收标准：任意时刻取消后 10 秒内停下；进度不丢（重跑跳过已完成段）；取消原因（哪个令牌、什么原因）进结构化日志。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三件套 | Request 请求（仅首次 true）/ Requested 轮询（含祖先）/ Watch 监听（至多一次） |
| 传播 | CancelChild 父→子单向；子取消是局部放弃 |
| 引用 | Ref/Destroy 多持有者配平——与值树同款纪律 |
| 统一等待 | 等待对象 + 可选令牌 → xwaitresult（OK/CANCELLED/TIMEOUT） |
| 检查点 | 协作式——步进级 Requested 检查；长操作内嵌检查点 |
| 语义 | 取消=控制流决策 ≠ 错误=操作失败——上报分开 |
| 原因 | Request 携带原因 → 观察侧可读——诊断字段随树传播 |
