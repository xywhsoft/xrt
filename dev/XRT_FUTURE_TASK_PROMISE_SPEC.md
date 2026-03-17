# XRT Future / Task / Promise 正式化设计稿

版本: `0.1.0`
状态: `active draft`
更新时间: `2026-03-17`



## 1. 目标

本设计稿用于将 `future / task / promise / wait-source` 正式提升为 XRT 的基础运行时能力。

目标不是补若干零散 API，而是一次性建立统一的异步模型，解决以下问题：

- 线程任务结果表达不统一
- 协程等待结果表达不统一
- 网络异步结果表达不统一
- 定时器、event、engine post 等等待能力彼此割裂
- timeout / deadline / cancel 语义在不同模块中重复实现
- AI / Agent 场景下的并发编排缺少统一基础抽象

本方案必须满足：

- 对外简单，保持脚本式易用
- 对内统一，线程/协程/网络/定时器说同一种语言
- 生命周期清晰，不制造悬空对象和隐式泄露
- 性能友好，不强迫所有路径重锁或多次堆分配
- 允许长期扩展到 AI / Agent / subprocess / streaming 场景



## 2. 非目标

本阶段不直接实现以下能力，但设计时必须预留扩展位：

- 多生产者多消费者通道 (`channel/queue`)
- 流式结果抽象 (`streaming future`)
- 完整 Actor 模型
- 完整反应式编程模型
- 完整分布式任务调度
- 厂商绑定的 LLM SDK 语义



## 3. 设计原则

### 3.1 一次性统一语义

未来所有异步结果都必须能统一表达为以下模型之一：

- `xfuture`：一个未来会完成的结果
- `xpromise`：future 的写端
- `xtask`：一个可调度执行的异步任务
- `xwaitsrc`：一个可等待事件源

不得在新模块中继续新增平行“异步结果协议”。



### 3.2 future 是一等公民

`future` 不再只是桥接工具，而是正式基础设施对象。

所有以下能力都必须能落到 `future`：

- 线程任务结果
- 协程任务结果
- engine worker task 结果
- 网络异步结果
- 定时器结果
- subprocess 结果
- Agent tool call 结果
- 组合等待结果



### 3.3 协作式取消优先

XRT 不提供危险的“强制中断栈执行”语义。

取消统一采用：

- 取消请求
- 协作式观察
- 安全点退出
- 完成终态



### 3.4 timeout 和 deadline 必须并存

所有正式等待接口必须同时提供：

- `Wait`
- `WaitTimeout`
- `WaitUntil`

不允许某些模块只支持 timeout、另一些模块只支持 deadline。



### 3.5 线程与协程对称

每一组正式等待语义都必须同时考虑：

- 同步线程等待
- 协程等待

接口命名必须对称，避免 API 体系分裂。



### 3.6 wait-source 是桥，不是终点

底层对象未必天然是 future，但必须可以桥接为 `xwaitsrc`，再进一步桥接为 future。

允许保留性能优化路径，但外层合同必须统一。



### 3.7 生命周期优先于功能扩张

任何新 async 对象必须先明确：

- 谁拥有它
- 谁完成它
- 谁释放它
- cancel 与 destroy 竞争如何处理
- timeout 返回后 waiter 如何撤销

没有生命周期闭环的功能，不得进入 public contract。



## 4. 核心对象

### 4.1 `xfuture`

`xfuture` 代表一个未来会完成的结果。

它负责：

- 完成态管理
- 结果与错误保存
- 等待与唤醒
- continuation 挂接
- cancel request 传播
- timeout / deadline 等待
- 组合 future 聚合

它不负责：

- 真正执行任务
- 自己拥有线程/协程调度器



### 4.2 `xpromise`

`xpromise` 是 `future` 的写端。

它负责：

- `resolve`
- `reject`
- `cancelled`
- `close`

原则：

- 一个 promise 只能使 future 进入一次终态
- 终态后任何再次完成都必须失败
- promise 不暴露等待接口



### 4.3 `xtask`

`xtask` 代表一个可调度执行的异步任务。

它负责：

- 保存任务入口
- 保存执行上下文
- 保存调度目标
- 自动持有 promise
- 将执行结果写入 future

它是“执行模型”和“结果模型”的桥梁。



### 4.4 `xwaitsrc`

`xwaitsrc` 代表一个可等待事件源。

典型来源：

- future
- event
- timer
- stream readable
- stream writable
- stream drain
- stream close
- dgram recv
- process exit

目标：

- 将底层不同对象统一为同一种等待入口
- 避免每种对象各自发明一套 wait API



## 5. 统一状态机

### 5.1 future 状态

```c
typedef enum xfuture_state
{
	XFUTURE_PENDING = 0,
	XFUTURE_RESOLVED,
	XFUTURE_REJECTED,
	XFUTURE_CANCELLED,
	XFUTURE_CLOSED
} xfuture_state;
```

语义：

- `PENDING`：尚未完成
- `RESOLVED`：成功完成
- `REJECTED`：失败完成
- `CANCELLED`：已取消完成
- `CLOSED`：生产端关闭，不再提供结果

约束：

- 只能从 `PENDING` 进入某个终态
- 终态不可逆
- 终态不可重复进入



### 5.2 task 状态

```c
typedef enum xtask_state
{
	XTASK_CREATED = 0,
	XTASK_QUEUED,
	XTASK_RUNNING,
	XTASK_DONE,
	XTASK_CANCELLED,
	XTASK_CLOSED
} xtask_state;
```

说明：

- `task` 状态是执行态
- `future` 状态是结果态
- 两者不可混用



## 6. 统一结果模型

### 6.1 标准结果结构

```c
typedef struct xfuture_result
{
	int32 iStatus;
	ptr pValue;
	str sError;
	uint32 iFlags;
} xfuture_result;
```

建议状态码：

- `X_OK`
- `X_ERR`
- `X_TIMEOUT`
- `X_CANCELLED`
- `X_CLOSED`

`iFlags` 用于表达：

- value 是否由 future 持有
- error 是否由 future 持有
- 是否系统错误
- 是否取消来源
- 是否 deadline 到达



### 6.2 结果所有权

第一版必须支持两种模式：

- borrowed result
- owned result

避免一开始把所有结果都绑死到同一释放策略。



## 7. 生命周期模型

### 7.1 future 引用计数

`future` 必须独立引用计数。

可能持有它的对象包括：

- producer / promise
- consumer
- continuation
- when_all / when_any 聚合 future
- wait-source bridge
- task runtime



### 7.2 promise 独占完成权

`promise` 必须保证：

- 只允许一个写端完成 future
- 终态后自动失效
- 多次 resolve/reject 必须失败



### 7.3 task 自动持有 promise

一旦 task 被提交运行，task runtime 必须自动持有 promise，直到：

- 任务完成并 resolve/reject
- 任务取消完成
- 任务关闭失败

禁止由外部调用者手工维护这条生命周期。



### 7.4 wait-source 注册必须可撤销

所有 wait-source 注册都必须支持：

- 成功完成后自动脱链
- timeout 返回前完成撤销
- deadline 返回前完成撤销
- cancel 返回前完成撤销
- destroy 时拒绝悬空 waiter

这是生产级约束，不得放宽。



## 8. 取消模型

### 8.1 三阶段取消

统一取消流程：

1. `cancel request`
2. `cooperative observe`
3. `cancel complete`



### 8.2 统一语义

调用 `xFutureRequestCancel()` 的含义是：

- 请求相关 producer / task / wait-source 停止继续工作
- 不保证立刻完成
- 必须最终进入某个可解释终态



### 8.3 禁止强制栈中断

不提供：

- 强制终止线程任务
- 强制卸载协程栈
- 不可恢复的异步对象杀死接口

所有正式取消都必须是协作式。



## 9. 统一等待模型

### 9.1 future

```c
bool xFutureWait(xfuture* pFuture);
bool xFutureWaitTimeout(xfuture* pFuture, int64 iTimeoutMs);
bool xFutureWaitUntil(xfuture* pFuture, int64 iDeadlineMs);

bool xFutureWaitCo(xfuture* pFuture);
bool xFutureWaitCoTimeout(xfuture* pFuture, int64 iTimeoutMs);
bool xFutureWaitCoUntil(xfuture* pFuture, int64 iDeadlineMs);
```



### 9.2 wait-source

```c
bool xWaitSourceWait(const xwaitsrc* pSrc);
bool xWaitSourceWaitTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
bool xWaitSourceWaitUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);

bool xWaitSourceWaitCo(const xwaitsrc* pSrc);
bool xWaitSourceWaitCoTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
bool xWaitSourceWaitCoUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
```



### 9.3 value wait

正式支持 value-returning wait：

```c
ptr xFutureWaitValue(xfuture* pFuture);
ptr xFutureWaitValueTimeout(xfuture* pFuture, int64 iTimeoutMs);
ptr xFutureWaitValueUntil(xfuture* pFuture, int64 iDeadlineMs);

ptr xFutureWaitCoValue(xfuture* pFuture);
ptr xFutureWaitCoValueTimeout(xfuture* pFuture, int64 iTimeoutMs);
ptr xFutureWaitCoValueUntil(xfuture* pFuture, int64 iDeadlineMs);
```

`wait-source` 也提供对称的 value 版本。



## 10. continuation 模型

### 10.1 第一版要求

future 必须支持 continuation 挂接。

最低要求：

- `Then`
- `Catch`
- `Finally`



### 10.2 执行目标

continuation 必须允许指定执行目标：

- 当前线程
- 指定 worker/engine
- 指定协程 scheduler
- 立即执行（仅已完成 future）

禁止 continuation 在隐式未知上下文中执行。



## 11. 组合等待

### 11.1 最小组合能力

第一版正式支持：

- `WhenAny`
- `WhenAll`
- `Race`



### 11.2 语义要求

- `WhenAny`：任一完成即可完成
- `WhenAll`：全部完成才完成
- `Race`：任一完成时完成，其余触发 cancel request



### 11.3 后续扩展

后续可以扩展：

- `JoinSet`
- `TaskGroup`
- `Select`



## 12. task 模型

### 12.1 最小任务入口

```c
xfuture* xTaskRunThread(...);
xfuture* xTaskRunCo(...);
xfuture* xTaskRunEngine(...);
xfuture* xTaskRunDelayed(...);
```

说明：

- 返回 future
- task 通过 promise 完成 future
- 对用户暴露统一结果模型



### 12.2 执行结果

task 入口函数允许：

- 直接返回 result
- 返回 status + result
- 手工写 promise

但对外都必须统一成 future。



## 13. wait-source 统一桥接

### 13.1 必须桥接的对象

第一阶段必须正式支持：

- future
- event
- timer
- stream wait kind
- dgram recv

第二阶段扩展：

- listener accept
- process exit
- subprocess pipe ready
- port/socket readiness



### 13.2 现有 xnet 等待面收口方向

现有：

- `future wait`
- `stream readable/writable/drain/close`
- `dgram recv`

必须继续向统一 `xwaitsrc` 收口，不得长期并行维护多套主实现。



## 14. 与线程、协程、xnet 的关系

### 14.1 线程

线程任务应通过 `xtask -> xpromise -> xfuture` 输出结果。

线程等待 future 为同步主路径。



### 14.2 协程

协程不应继续单独定义另一套异步结果协议。

协程负责：

- 运行
- 挂起
- 调度

future 负责：

- 完成态
- 结果
- 错误
- 取消



### 14.3 xnet

`xnet` 必须逐步统一为：

- callback 模式
- future / wait-source 模式

网络模块不应长期维持独立异步结果语义。



## 15. 与 AI / Agent 的关系

这套模型必须天然适用于：

- LLM 请求
- streaming HTTP/SSE
- tool call
- subprocess 执行
- 并行检索
- 多任务竞速
- deadline/cancel 传播

后续 Agent 层应直接构建在：

- `future`
- `task`
- `wait-source`
- `task group`

之上，而不是重新定义另一套异步编排模型。



## 16. 结构化并发

### 16.1 必须预留 task group

后续必须支持：

- 父任务拥有子任务
- 父任务取消传播到子任务
- 子任务失败传播到父任务
- 父任务 wait all / wait any

这对服务请求和 AI Agent 子任务树都很关键。



### 16.2 本阶段不强制实现

第一版不要求完整 task group 落地，但必须保证 future/task 设计不阻塞其实现。



## 17. 可观测性要求

future / task 必须预留这些 metadata：

- 创建时间
- 完成时间
- debug 名称
- 来源模块
- 运行目标
- trace/span id

这是生产调试要求，不是可选增强项。



## 18. API 命名与兼容策略

### 18.1 命名要求

对外统一使用：

- `xFuture*`
- `xPromise*`
- `xTask*`
- `xWaitSource*`

避免继续散落多种风格命名。



### 18.2 历史兼容策略

原则：

- 允许保留薄兼容层
- 不允许历史接口继续成为主实现
- 新架构必须以新正式 API 为中心

这项能力必须一次性正名，避免未来继续积累历史包袱。



## 19. 里程碑

### M1 核心对象定型

- `xfuture` 结构
- `xpromise` 结构
- `xwaitsrc` 正式结构
- future 状态机
- result/status/error 模型

验收：

- 结构体、状态机、引用计数、终态规则全部冻结



### M2 基础等待完成

- `Wait / WaitTimeout / WaitUntil`
- sync/co 对称接口
- value wait
- cancel request 基础路径

验收：

- future 与 wait-source 都有对称等待接口



### M3 task 执行层完成

- thread task
- coroutine task
- engine task
- delayed task

验收：

- 四类任务都能统一返回 future



### M4 continuation 与组合等待

- then/catch/finally
- when_any
- when_all
- race

验收：

- continuation 与组合 future 通过正式回归



### M5 xnet 深度接线

- listener accept
- process exit（若已进入主线）
- stream/dgram 全面收口到 wait-source

验收：

- 网络异步结果不再依赖平行主实现



### M6 文档、bench、主测试流

- 文档
- 压测
- 回归
- 主测试流接入

验收：

- 形成生产可维护能力



## 20. 当前已知开放问题

### O-001 是否允许 future 多等待者 + 多 continuation

建议：

- future 支持多等待者、多 continuation
- 底层 wait-source 可以仍保持单 waiter
- 通过 future bridge 向外暴露统一体验



### O-002 结果 value 是否要绑定 xvalue

建议：

- 第一版不绑定
- 使用 `ptr pValue`
- 高层可另外提供 `xvalue` 友好包装



### O-003 promise 是否公开创建

建议：

- 对外公开
- 但常规用户主路径应优先使用 task/future helper



### O-004 continuation 的默认执行器是什么

建议：

- 不提供隐式默认魔法
- 要么当前线程立即执行（已完成 future）
- 要么显式指定调度目标



## 21. 当前结论

XRT 若要成为“互联网 + AI 时代的 C 语言基础设施库”，就必须拥有统一异步模型。

线程、协程、网络、定时器、event、subprocess、tool call，这些能力不应继续各自发展为不同协议。

本设计稿将 future / task / promise / wait-source 定为统一主干。

后续所有异步能力都必须围绕这套正式模型实现和收口。



## 22. 当前进度

### M1 核心对象定型

状态: `in_progress`

已落地：

- 在 [xrt.h](/D:/git/xrt/xrt.h) 正式引入 `xfuture / xpromise / xwaitsrc`
- 冻结 `xfuture_state`、`xfuture_result`、结果 flags
- 将现有 `xnetfuture/xnetwaitsrc` 提升为正式 public alias
- 在 [xnet_sync.h](/D:/git/xrt/lib/xnet_sync.h) 为 future 增加引用计数、错误槽、结果 flags
- 落地第一版 `xFuture* / xPromise* / xWaitSource*` API 包装
- 落地第一版 `xtask` engine 执行层：`xTaskRunEngine / xTaskRunDelayed`
- 落地第一版 `xtask` thread / coroutine 执行层：`xTaskRunThread / xTaskRunCo`
- 落地第一版显式 continuation：`Then / Catch / Finally`
- 当前已支持的执行目标：`inline(仅已完成 future) / engine / coroutine scheduler`
- 落地第一版组合 future：`WhenAny / WhenAll / Race`
- 落地第一版 `task group`：`Create / AddFuture / Count / Wait / WaitTimeout / WaitUntil / Cancel`
- 落地第一版 group 托管执行入口：`xTaskGroupRunEngine / RunDelayed / RunThread / RunCo`
- 落地第一版 group scope 管理：`Close / ReapCompleted`，且 `Wait*` 成功后自动 reap 已完成子 future
- `xTaskGroupDestroy` 当前合同已收口：destroy 时自动 `Close + Cancel pending children + Release group hold`
- 落地第一版 scope join：`xTaskGroupJoinFuture / Join / JoinTimeout / JoinUntil`
- 落地第一版 current-thread deferred continuation：`ThenCurrent / CatchCurrent / FinallyCurrent + xFuturePumpCurrentContinuations`
- 落地第一版父子取消传播：`xTaskGroupBindParent`，当前支持多 parent bind；任一 parent 进入 `CANCELLED/CLOSED` 时都会向 children 传播 cancel
- 落地第一版 future 元数据：`create/complete time`、`debug name`、`pending continuation count`
- 已通过多 continuation / 多等待者正式回归
- 落地第一版组合 future richer result：`WhenAny / Race / WhenAll(first failure)` 现在暴露 `group source index/source future` 元数据
- 落地第一版 ownership 双轨：`Get* = 持有引用`，`Peek* = 借用引用`
- 落地 `WhenAll(success)` 聚合 value 合同：成功聚合 future 现在返回 `xfuture_all_value`
- 在 [test_xnet2_sync.h](/D:/git/xrt/test/test_xnet2_sync.h) 增加最小正式回归
- 在 [test_future_core.c](/D:/git/xrt/dev/test_future_core.c) 增加独立 core harness，并覆盖 `task group`
- 在 [test_future_core.c](/D:/git/xrt/dev/test_future_core.c) 增加压力回归：
	- `ThenCurrent + auto pump` 重复压力
	- `task group run thread + dynamic join + reap` 批量压力
	- `WhenAny / Race` 并发完成下的 source index/source future 稳定性
	- 多 parent 同时 cancel 的传播压力
	- `task group destroy pending child -> child cancel`
	- `task group close 后拒绝新增 child`
	- `Get/Peek` ownership 生命周期：释放原句柄后 retained ref 仍可用

待完成：

- `join future` 的长期增量追踪语义已落地：当前为 `close-aware dynamic join`
- current-thread deferred continuation 的自动 pump/host loop 集成已落地：同步 future wait 主路径会自动 drain 当前线程 deferred queue
- parent bind 的多父级/嵌套 scope 语义已推进：
	- `xTaskGroupBindParent` 当前支持多 parent bind
	- `xTaskGroupCreateChild` 已落地，child group 会自动绑定 parent scope，并把 child join 纳入 parent scope
	- parent `Close/Cancel/Destroy` 当前会向 nested child 传播 `Close + Cancel`
- `WhenAll(success)` 的聚合 value 深化合同（后续可继续扩成状态/来源并列视图）
