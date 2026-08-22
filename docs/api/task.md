# 任务与任务池

任务层把阻塞工作、Future 结果、结构化错误、取消和资源所有权组合成独立于网络的并发契约。`xfuture` 就是任务句柄，不再额外维护一套任务状态对象。

## 裁剪

- `XRT_FEATURE_TASK`：任务过程、显式结果和内部执行生命周期，依赖 Future 与临时内存。
- `XRT_FEATURE_TASK_GROUP`：结构化 Future 作用域、嵌套组与结果统计，只依赖 Future。
- `XRT_FEATURE_TASK_POOL`：有界原生线程池，依赖任务核心与线程。
- `XRT_FEATURE_TASK_GROUP_POOL`：任务组与有界任务池的原子提交便利层，只在两者同时启用时引入。
- `XRT_FEATURE_TASK_COROUTINE`：协程调度任务，依赖任务核心与协程调度器。
- `XRT_FEATURE_TASK_GROUP_COROUTINE`：任务组与协程任务的原子提交便利层。
- `XRT_FEATURE_TASK_NET`：Engine 亲和任务与延迟任务，依赖任务核心和网络 Engine。
- `XRT_FEATURE_TASK_GROUP_NET`：任务组与网络任务的原子提交便利层。

只使用 Future/Promise 时不需要引入任务池；网络引擎、协程调度器和用户执行器也可以复用任务核心，而不依赖线程池。

## 任务结果

`xtaskproc` 接收借用的 `xcancel`、用户数据和预先清零的 `xtaskvalue`，并显式返回 `xtaskoutcome`：

- `XTASK_SUCCESS`：Future 进入 `XFUTURE_RESOLVED`。
- `XTASK_FAILED`：Future 进入 `XFUTURE_FAILED`，保存当前结构化错误；没有错误时运行库使用 `XERR_INTERNAL`。
- `XTASK_CANCELLED`：Future 进入 `XFUTURE_CANCELLED`。

成功值默认是借用值。设置 `xtaskvalue.Destroy` 后，值和析构过程转移给 Future，并在最后一个 Future 引用释放时销毁。失败、取消或完成冲突时，尚未转移的 owned 值由任务运行库销毁。

任务过程返回值是最终结果。取消请求只表示协作意图：过程观察并处理取消后仍可明确返回成功；只有返回 `XTASK_CANCELLED` 或执行前已经取消时，Future 才进入取消终态。

## 执行上下文

每个任务绑定独立的错误上下文和临时 arena：

- 前一个任务遗留的错误不会污染后一个任务。
- `xrtTempCurrent` 和 `xrtTemp` 只在当前任务生命周期内有效。
- 任务返回后，临时内存整体释放，不能作为 Future 结果返回。
- 任务数据析构也在该隔离上下文内执行。

## 数据所有权

`xtaskargs.Destroy` 描述任务数据在受理后的析构方式：

- 任一种提交函数成功后，执行器取得数据所有权；执行、跳过或取消后恰好析构一次。
- 提交因 OOM、队列已满、等待超时、等待取消或任务池关闭而失败时，调用方仍持有数据，析构过程不会执行。
- 数据析构发生在任务过程返回后、Future 终态发布前，因此观察到终态时任务上下文已经完成回收。结果若借用任务数据，不能同时配置数据析构；需要长期保存时应把结果所有权转移给 Future。

`xtaskargs.Cancel` 是可空父令牌。任务使用自己的子令牌，因此任务取消不会反向取消父作用域，而父取消会传播到任务。

## 结构化任务组

`xtaskgroup` 可以跟踪任务 Future、网络 Future 或用户 Promise，不要求源来自某一种执行器。它只为当前活动项保留一个监听节点和源引用，项进入终态后立即摘除并释放，因此内存占用与并发量相关，不随历史任务总数增长。

```c
xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
xrtTaskGroupAdd(pGroup, pFuture);
xrtTaskGroupWait(pGroup);
```

已有 Future 使用 `xrtTaskGroupAdd`。需要先启动异步操作再返回 Future 时，使用 `xrtTaskGroupStart` 和同步启动器 `xtaskgroupstartproc`：运行库先预留活动槽位，预留成功后才调用启动器，随后把返回的 Future 提交到同一槽位。关闭、活动上限、预留 OOM 或启动失败都不会留下已经启动却未登记的操作；关闭和取消发生在启动器执行期间时，组 Done Future 会等待预留完成，取消还会在 Future 提交后补发。

```c
xfuture* pFuture = xrtTaskGroupStart(pGroup, startRequest, pRequest);
```

生命周期语义保持单一口径：

- `xrtTaskGroupClose` 停止接纳新项，让当前项自然结束；重复关闭返回 `false`。
- `xrtTaskGroupCancel` 同时关闭组，并向当前项发出协作取消请求；源生产端仍决定各自最终状态。
- `xrtTaskGroupFuture` 返回稳定的 Done Future。只有组已关闭且活动项归零时，它才成功完成；子项失败不会被冒充为 Done Future 自身失败。
- `xrtTaskGroupWait`、`xrtTaskGroupWaitFor`、`xrtTaskGroupWaitUntil` 和 `xrtTaskGroupWaitUntilCancel` 会先关闭组，再复用 Future 的统一等待语义。
- `xrtTaskGroupCancelToken` 返回组取消令牌的新增引用，调用方负责释放。
- `xrtTaskGroupDestroy` 会关闭并取消仍活动的项，但不伪造完成；活动监听持有内部引用，允许源稍后安全确认终态并完成延迟回收。

`xtaskgroupconfig.Limit` 是活动项硬上限，达到上限时 Add 返回 `XERR_AGAIN`；完成项立即释放槽位。`CancelOn` 可以组合 `XRT_TASK_GROUP_CANCEL_ON_FAILED`、`XRT_TASK_GROUP_CANCEL_ON_CANCELLED` 和 `XRT_TASK_GROUP_CANCEL_ON_CLOSED`，在指定异常出现后关闭组并取消仍活动的兄弟项。`XRT_TASK_GROUP_CANCEL_ON_STOPPED` 表示三者合集。默认值为零，即收集全部结果而不自动取消。

`xrtTaskGroupChild` 创建由父组跟踪的嵌套作用域。父 `Close` 只关闭子组并等待叶任务自然结束；父 `Cancel` 才向子组和叶任务传播取消。这样正常作用域退出与异常停止不会混为同一行为。

`xrtTaskGroupGet` 把活动数、受理数、完成数、成功/失败/取消/关闭数、拒绝数和首个非成功槽位写入 `xtaskgroupstats`。`xrtTaskGroupError` 借用首个失败项的结构化错误。组排空后满足：

```text
Completed = Succeeded + Failed + Cancelled + Closed
Added = Completed
```

## 有界任务池

`xrtTaskPoolCreate` 接受可空配置。全零字段分别表示逻辑处理器数量、`XRT_TASK_POOL_QUEUE_LIMIT_DEFAULT` 指定的默认队列上限 `1024` 和平台默认线程栈。线程数最大值由 `XRT_TASK_POOL_THREAD_LIMIT` 指定，当前为 `256`。

队列上限只统计尚未开始运行的任务。达到上限时提交会先回收已经请求取消的排队任务；仍无可用槽位时，调用方可以选择立即返回或等待背压解除：

- `xrtTaskSubmit`：立即尝试，队列已满时返回空并设置 `XERR_AGAIN`。
- `xrtTaskSubmitWait`：一直等待槽位。
- `xrtTaskSubmitFor`：在相对微秒数内等待槽位。
- `xrtTaskSubmitUntil`：等待到指定单调时钟截止时间。
- `xrtTaskSubmitUntilCancel`：同时等待槽位、截止时间或调用方取消。

槽位已经可用时，受理成功优先于同时到达的超时或等待取消。等待调用尚未受理任务时，超时设置 `XERR_TIMEOUT`，调用方取消设置 `XERR_CANCELLED`，任务池关闭设置 `XERR_CLOSED`。关闭和整体取消都会唤醒全部容量等待者。

`xrtTaskSubmitUntilCancel` 的取消令牌只约束当前容量等待，不会成为任务的父令牌，也不会取消已经受理的任务。任务执行期取消必须通过 `xtaskargs.Cancel` 或 `xrtFutureCancel` 表达。任务池工作线程可以在槽位立即可用时提交到所属池，但队列已满时不得等待自身释放槽位，运行库返回 `XERR_STATE`，从契约上阻止自锁。

生命周期分为：

- 打开：接收任务。
- `xrtTaskPoolClose`：停止接收普通任务，排空已受理任务；工作线程保持休眠，继续服务运行库已经接管资源的 finalizer。
- `xrtTaskPoolCancel`：停止接收普通任务，取消排队任务，并向运行任务发出协作取消请求；资源回收过程不会被取消。
- `xrtTaskPoolDestroy`：等待普通任务和资源回收过程全部结束，再终止工作线程并释放任务池。

`xrtTaskPoolWait`、`xrtTaskPoolWaitFor`、`xrtTaskPoolWaitUntil` 和 `xrtTaskPoolWaitUntilCancel` 只接受已经关闭的池。`xrtTaskPoolWaitUntilCancel` 的令牌只中止调用方等待，不改变池或池内任务状态；池已经排空时完成优先于同时到达的超时或取消。任务池自己的工作线程不能等待或销毁所属池，否则返回 `XERR_STATE`。`xrtTaskPoolDestroy` 默认关闭并自然排空；需要快速停止时先调用 `xrtTaskPoolCancel`。

内部资源回收通道只接收已经由运行库受理的资源，节点嵌入资源对象，不发生投递分配，也不占用 `QueueLimit`。它不是第二条用户任务队列，公共代码不能用它绕过背压。

销毁期间，调用方必须保证没有其他线程继续调用该任务池，并且所有绑定该池的异步资源都已经进入关闭流程。Future 生命周期独立于任务池，池销毁后，调用方已经持有的 Future 仍然有效。

## 任务组与任务池

`XRT_FEATURE_TASK_GROUP_POOL` 提供原子组合入口，避免先调用任务池、再调用 `xrtTaskGroupAdd` 时在两步之间关闭任务组：

- `xrtTaskGroupSubmit`：立即尝试任务池提交。
- `xrtTaskGroupSubmitWait`：永久等待任务池槽位。
- `xrtTaskGroupSubmitFor`：在相对微秒数内等待。
- `xrtTaskGroupSubmitUntil`：等待到单调时钟截止时间。
- `xrtTaskGroupSubmitUntilCancel`：再叠加调用方取消。

可等待版本会自动把组取消合并到容量等待。组在任务尚未受理时取消，提交返回 `XERR_CANCELLED`、组预留回滚且数据仍归调用方；任务已经受理时，Future 正常纳入组并收到协作取消请求。正常 `Close` 不取消容量等待，因为 Close 的含义是停止新增预留并自然等待已经预留的操作。

## 协程任务

`xrtTaskCo` 把同一个 `xtaskproc` 提交到指定协程调度器；调度器为空时使用当前协程所属调度器。显式指定调度器时可以从任意线程提交，内部先进入调度器 FIFO post 队列，再由所属线程创建协程。调用发生在普通执行路径且没有显式调度器时返回 `XERR_STATE`。

协程任务使用分离协程，用户只持有 Future。Future 的取消令牌是协程取消令牌的父级，因此取消 Future 会唤醒正在 park、sleep 或 await 的任务协程。首次调度前取消不会进入任务过程，但协程终结过程仍会把 Future 完成为取消并释放受理后的任务数据。

任务过程的显式结果优先于取消请求：过程从取消等待恢复后仍可返回 `XTASK_SUCCESS`，此时 Future 成功；返回 `XTASK_CANCELLED` 时 Future 才是取消终态。协程栈大小为零时使用协程默认值。

提交成功表示调度器已经受理任务数据和 Future 生命周期，不表示协程已经创建。调度器在处理该投递前关闭时，Future 以 `XERR_CLOSED` 失败并析构受理的数据；关闭后直接提交返回空、设置 `XERR_CLOSED`，数据所有权仍归调用方。

`XRT_FEATURE_TASK_GROUP_COROUTINE` 提供 `xrtTaskGroupCo`，在任务组预留窗口中调用 `xrtTaskCo`。组已经关闭时不会触碰调度器或任务数据；提交与组取消竞争时，成功受理的 Future 会先登记，再收到取消请求。

## 网络 Engine 任务

`XRT_FEATURE_TASK_NET` 是任务体系到网络 Engine 的可选桥，不会让 Future、Task 或
TaskGroup 核心反向依赖网络。`xtasknetproc` 在普通任务参数之前额外借用亲和
`xnetworker`，因此过程可以直接访问 Worker 缓冲池和 Engine 上下文；它仍使用相同的
`xcancel`、`xtaskvalue`、结构化错误、临时 arena 和数据所有权合同。

- `xrtTaskNet`：尽快在指定亲和 Worker 上执行。
- `xrtTaskNetAfter`：在相对微秒数到期后执行。
- `xrtTaskNetUntil`：在单调时钟截止时间到期后执行。

网络任务运行在事件循环线程，不得执行阻塞系统调用或长时间 CPU 工作；这些工作应
提交到 `xtaskpool`。立即任务一旦受理，即使 Engine 随后停止也会在排空阶段执行。
延迟任务在 Engine 到期时执行；Future 取消会请求摘除 Timer，确认后以
`XFUTURE_CANCELLED` 终结。Engine 在截止时间前停止时，任务不进入用户过程，并以
`XERR_CLOSED` 的结构化错误失败。底层 Timer 完成、取消监听注销和任务数据析构全部
先于 Future 终态发布，因此消费者不会观察到仍被 Worker 使用的任务上下文。

`XRT_FEATURE_TASK_GROUP_NET` 提供 `xrtTaskGroupNet`、
`xrtTaskGroupNetAfter` 和 `xrtTaskGroupNetUntil`。这些函数先预留 TaskGroup 活动槽位，
再提交 Engine 操作；组已关闭、达到活动上限或预留 OOM 时不会启动网络任务，也不会
接管用户数据。组取消会传播到任务 Future，并等待 Timer 的真实取消回调完成。

## 统计

`xrtTaskPoolGet` 把一致快照写入 `xtaskpoolstats`：线程数、队列硬上限、排队数、运行数、受理数、完成数、成功数、失败数、取消数、拒绝数，以及关闭和取消状态。

恒等式在池排空后成立：

```text
Completed = Succeeded + Failed + Cancelled
Submitted = Completed
```

拒绝任务不计入 `Submitted`，只计入 `Rejected`。

## 示例

```c
xtaskpoolconfig tConfig = { 4, 1024, 0 };
xtaskpool* pPool = xrtTaskPoolCreate(&tConfig);
xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
xfuture* pFuture = xrtTaskGroupSubmitFor(
	pGroup,
	pPool,
	work,
	pData,
	NULL,
	UINT64_C(2000000)
);

if ( (pFuture != NULL) && (xrtTaskGroupWait(pGroup) == XWAIT_OK) ) {
	ptr pValue = xrtFutureValue(pFuture);
	(void)pValue;
}
xrtFutureDestroy(pFuture);
xrtTaskGroupDestroy(pGroup);
xrtTaskPoolDestroy(pPool);
```

完整示例位于 `examples/concurrency/task_group/main.c`、`examples/concurrency/task_group_scope/main.c`、`examples/concurrency/task_pool/main.c`、`examples/concurrency/task_coroutine/main.c`、`examples/network/task/main.c` 和 `examples/network/task_group/main.c`。`task_group_scope` 明确演示父级取消、子组传播和叶生产端确认终态之间的边界。
