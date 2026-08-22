# TaskGroup 与结构化作用域

TaskGroup 不是执行器，也不是 Future 数组。它把一批属于同一逻辑作用域的异步操作
登记在一起，明确何时停止新增、何时自然收口、何时请求取消，以及父子作用域怎样
传播生命周期。

单个函数的精确返回值、统计字段和线程规则见 [Task API](../api/task.md)。并发层整体
选型见[并发、协程与任务选择指南](concurrency.md)。

## 三层职责

| 层 | 回答的问题 | XRT 对象 |
| --- | --- | --- |
| 执行器 | 工作在哪里运行 | TaskPool、Coroutine Scheduler、Network Engine、外部执行器 |
| 结果 | 一个操作怎样结束 | Future/Promise |
| 作用域 | 这一批操作何时停止增长并全部结束 | TaskGroup |

TaskGroup 可以同时跟踪不同执行器产生的 Future。它不要求子项都是线程任务，也不会
为了统一作用域而让 Future、协程或网络核心反向依赖线程池。

## 当前生命周期

### Open

新建组处于 Open 状态，可以通过 `xrtTaskGroupAdd()` 登记已有 Future，或通过
`xrtTaskGroupStart()` 先预留槽位再启动操作。TaskPool、Coroutine 和 Network 的
组合入口使用同一套预留协议。

每个活动项只占一个监听节点和一个源引用。项进入终态后，TaskGroup 立即统计结果、
摘除监听并释放源引用；内存与当前并发量相关，不随历史完成数量增长。因此当前 API
不需要 `ReapCompleted`。

### Close

`xrtTaskGroupClose()` 停止接纳新项，已经登记的项继续自然运行。Close 不发送取消，
也不会把成功运行中的操作改写为取消。重复 Close 返回 `false`，但不会破坏已有状态。

`xrtTaskGroupWait*()` 会先 Close，再等待活动项归零。当前契约不再区分“只等当前快照”
和“最终 Join”两套入口，避免 Open 组在等待期间继续长出新项而形成模糊屏障。

### Cancel

`xrtTaskGroupCancel()` 同时 Close，并向活动 Future 发出协作取消请求。请求取消不是伪造
终态：线程任务、协程、网络操作或外部生产端仍然负责确认成功、失败、取消或关闭。
组的 Done Future 只有在全部活动项发布真实终态后才完成。

### Destroy

`xrtTaskGroupDestroy()` 会关闭并取消仍活动的项，然后释放调用方引用。活动监听持有
内部引用，所以生产端可以稍后安全完成；Destroy 不是“假定后台已经停止”的同步 free。
若任务数据的生命周期短于生产端，应先等待 Done Future，再释放数据。

## Done Future

`xrtTaskGroupFuture()` 返回增加引用后的稳定 Done Future。它只表达“组已关闭且活动项
归零”，不会把第一个子项失败冒充成自身失败。子项结果通过各自 Future 或
`xrtTaskGroupGet()`、`xrtTaskGroupError()` 读取。

Done Future 与 `xrtFutureAll()` 的区别是：

- Future All 在调用时接收一个固定数组；
- TaskGroup 在 Open 期间可以继续受理操作；
- TaskGroup 有 Close、Cancel、活动上限、统计和嵌套作用域；
- TaskGroup 完成后不保留全部历史 Future，只保留累计统计和首个错误。

在线程中可以直接调用 `xrtTaskGroupWait*()`。在调度协程中，先取得 Done Future，
Close 后调用 `xrtFutureAwait*()`，不会阻塞调度器所属线程。

## 原子启动

以下写法存在竞态：

```c
pFuture = startOperation(pData);
xrtTaskGroupAdd(pGroup, pFuture);
```

操作可能在 Add 前已经启动，而组可能同时关闭、取消、达到上限或发生 OOM。此时会
留下已经运行却没有进入作用域的工作。

已有启动器应使用 `xrtTaskGroupStart()`。TaskPool 使用 `xrtTaskGroupSubmit*()`，
协程使用 `xrtTaskGroupCo()`，网络 Engine 使用 `xrtTaskGroupNet*()`。这些入口先预留
组槽位，成功后才启动；启动失败会完整回滚，不留下半登记操作。

`xtaskgroupconfig.Limit` 是活动项硬上限。达到上限时不会调用启动器，也不会接管任务
数据。对于服务端请求、批处理扇出和会话子任务，应设置符合资源预算的上限，而不是
依赖内存耗尽作为背压。

## 父子作用域

`xrtTaskGroupChild()` 创建由父组跟踪的子组。父组只看见子组的 Done Future，不需要
知道叶操作来自哪种执行器。

- 父 Close 会关闭子组，并等待叶操作自然结束。
- 父 Cancel 才会向子组和叶操作传播取消。
- 子取消不会反向取消父组，除非父组的 `CancelOn` 策略把该终态定义为停止条件。
- 子组和父组各自持有自己的统计、首个错误与活动上限。

这种语义区分正常作用域退出和异常停止。不能用 Close 代替 Cancel，也不能把父 Close
后仍在完成的叶操作当作泄漏。

## 失败策略

默认 `CancelOn == 0`，TaskGroup 收集全部结果。需要 fail-fast 时，可以组合：

- `XRT_TASK_GROUP_CANCEL_ON_FAILED`；
- `XRT_TASK_GROUP_CANCEL_ON_CANCELLED`；
- `XRT_TASK_GROUP_CANCEL_ON_CLOSED`；
- 或三者合集 `XRT_TASK_GROUP_CANCEL_ON_STOPPED`。

触发策略后，组关闭并请求取消兄弟项，但统计仍等待所有项确认终态。排空后始终满足：

```text
Completed = Succeeded + Failed + Cancelled + Closed
Added = Completed
```

## 典型流程

### 固定批次

创建 TaskGroup，使用原子组合入口提交全部子项，然后调用 `xrtTaskGroupWait*()`。需要
逐项结果时保留各 Future 引用；只关心成功/失败计数时读取组统计即可。

### 协程编排线程池

TaskPool 执行阻塞或 CPU 工作，TaskGroup 限定批次作用域，协程 Await 组的 Done
Future。等待超时后先 Cancel，再继续 Await Done Future，确认任务不再使用输入数据后
才离开作用域。

### 请求或连接作用域

为一个请求、连接或会话创建父组；数据库、文件、上游 HTTP 和定时操作进入子组。
正常结束使用 Close，连接中断或上层取消使用 Cancel。网络 Worker 上不能同步等待
TaskPool，也不能执行长时间 CPU 工作。

## 旧契约迁移结论

旧版 `JoinFuture` 被当前稳定的 `xrtTaskGroupFuture()` 承接；旧版 Wait/Join 双轨合并为
“Wait 自动 Close”这一套口径。旧版 `ReapCompleted` 因完成项即时释放而退役，
`BindParent` 的常见用途由 `xrtTaskGroupChild()` 和父取消令牌替代。公共
`wait-source` 不再存在，线程直接 Wait Future，协程直接 Await Future。

这些是契约替换，不提供旧名称别名或过渡 API。

## 范例

- `examples/concurrency/task_group`：登记已有 Future 并读取累计统计。
- `examples/concurrency/task_group_scope`：父取消、子组和叶 Future 的真实终态。
- `examples/concurrency/task_group_pool`：TaskPool 原子提交与同步等待。
- `examples/concurrency/task_group_coroutine`：协程任务进入结构化作用域。
- `examples/concurrency/report`：TaskPool 执行、Future 结果、TaskGroup 收口和 Coroutine
  编排的完整组合。
- `examples/network/task_group`：网络 Engine 任务进入同一作用域。

全部入口均由模块清单登记，并参与模块化、单头和裁剪回归。
