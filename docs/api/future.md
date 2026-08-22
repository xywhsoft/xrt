# Future 与 Promise

`future` 提供与网络无关的一次性异步结果。`xfuture` 是可共享的只读消费端，`xpromise` 是可引用的生产端；两者由一次分配共同创建，Future 的终态一旦写入便不可改变。

## 裁剪与分层

| 层 | 裁剪宏 | 依赖 | 能力 |
| --- | --- | --- | --- |
| Future 核心 | `XRT_FEATURE_FUTURE` | `cancel` | 结果、所有权、同步等待、协作取消 |
| Future 适配桥 | `XRT_FEATURE_FUTURE_BRIDGE` | `future`、`thread`、`atomic` | 异步操作、Promise 与取消监听的一次性装配 |
| Future 延续 | `XRT_FEATURE_FUTURE_CONTINUE` | `future` | Continue、Then、Catch、Finally 与安全结果透传 |
| Future 组合器 | `XRT_FEATURE_FUTURE_COMBINE` | `future` | Any、All、Race 与源结果保活 |
| 协程等待桥 | `XRT_FEATURE_FUTURE_COROUTINE` | `future`、`coroutine_scheduler` | 不阻塞调度线程的 await |

延续链、线程池执行器和结构化任务组属于后续独立层，不会增加只需要 Future 核心的程序体积。

## 统一等待边界

当前契约不再提供 `xwaitsrc` 或网络专用 `xnetwaitsrc`。异步操作直接返回同一种
`xfuture`：Future 同时携带终态、成功值、结构化错误、取消入口和所有权，因此线程
等待、协程 Await、延续、组合器和 TaskGroup 不需要先把它包装成另一个等待对象。

TCP 可读、可写、Drain、Accept 等只表示条件就绪的场景仍通过对应的
`xrtNet*WaitAsync` 返回 Future；真正接收数据或建立连接的操作返回拥有结果的 Future。
这种分层保留“只观察条件”和“取得结果”两类能力，但不复制 bool、`xnet_result` 和
Future 终态三套互相转换的状态模型。

需要组合不同对象类型时，上层只保存 `xfuture*`，通过 Any、All、Race 或 TaskGroup
协调；完成后按创建该 Future 的操作契约解释值。底层事件端口的 readiness/completion
观察仍属于网络引擎内部原语，不作为跨模块类型擦除接口泄漏到业务层。

## 状态

`xfuturestate` 只有五个稳定状态：

- `XFUTURE_PENDING`：生产端尚未写入结果。
- `XFUTURE_RESOLVED`：成功，可读取 `Value`。
- `XFUTURE_FAILED`：失败，可读取不可变 `xerror`。
- `XFUTURE_CANCELLED`：生产过程确认以取消结束。
- `XFUTURE_CLOSED`：最后一个生产端在未完成时离开，或显式关闭。

等待一个失败、取消或关闭的 Future 仍返回 `XWAIT_OK`，因为等待动作已经完成。调用方随后通过 `xrtFutureState` 或 `xrtFutureResult` 判断操作结果。`XWAIT_TIMEOUT` 和 `XWAIT_CANCELLED` 只描述等待者自身提前停止等待。

## 创建与生命周期

```c
xfuture* pFuture;
xpromise* pPromise = xrtPromiseCreate(&pFuture, pParentCancel);
```

创建成功后，调用方各拥有一个 Promise 和 Future 引用。`pParentCancel` 可以为空；非空时，新 Future 的取消令牌继承父链。`xrtPromiseRef`、`xrtFutureRef` 分别增加端点引用，`xrtPromiseDestroy`、`xrtFutureDestroy` 分别释放。

最后一个 Promise 引用在 Pending 状态释放时，Future 自动进入 `XFUTURE_CLOSED` 并请求取消，避免消费者永久等待。已经完成的 Future 不会因 Promise 释放而改变状态或取消令牌。

Future 的内部完成监听按注册顺序执行。嵌套完成会追加到当前 Fiber 或线程的无分配派发队列，由最外层完成调用迭代排空；因此组合器和后续延续链的级联深度不会转化为 C 调用栈深度。

## 无分配终态监听

`xfuturewatch` 是调用方提供存储的底层终态监听节点，使用
`XRT_FUTURE_WATCH_STORAGE_SIZE` 固定不透明空间，不为每次监听分配内存。
`xfuturewatchproc` 接收通知上下文；可选的 `xfuturewatchreleaseproc` 在节点被
Future 接管后，于完成通知或成功摘除时执行一次。

```c
bool xrtFutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
);
xfuturewatchresult xrtFutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
);
bool xrtFutureWatchDetach(xfuture* pFuture, xfuturewatch* pWatch);
void xrtFutureWatchRemove(xfuture* pFuture, xfuturewatch* pWatch);
```

`xrtFutureWatchAdd` 返回 `XFUTURE_WATCH_PENDING` 时，Future 已接管节点；返回
`XFUTURE_WATCH_READY` 时源已经完成，节点仍归调用方且不会执行 Release；
`XFUTURE_WATCH_ERROR` 表示参数或状态无效。`xrtFutureWatchDetach` 只尝试摘除尚未
开始通知的节点，成功时同步执行 Release；`xrtFutureWatchRemove` 还会等待已经开始的
通知结束，不能从该 Watch 自己的通知中调用。`xfuturewatchresult` 只描述注册结果，
不替代 Future 的终态。

## 结果与所有权

```c
bool xrtPromiseResolve(xpromise* pPromise, ptr pValue);
bool xrtPromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
bool xrtPromiseReject(xpromise* pPromise, const xerror* pError);
bool xrtPromiseCancel(xpromise* pPromise);
bool xrtPromiseClose(xpromise* pPromise);
```

- `xrtPromiseResolve` 只借用值。调用方必须保证值至少活到全部 Future 引用释放之后。
- `xrtPromiseResolveOwned` 仅在成功完成时接管值；重复完成失败时，值仍归调用方。
- `xrtPromiseReject` 增加不可变错误引用，调用方仍释放自己的错误引用。
- 只有第一个终态写入成功。重复写入返回 `false` 并设置 `XERR_STATE`。
- Future 最后释放时执行 owned 值析构；析构过程在 Future 内部锁外运行。

`xrtFutureDone` 和 `xrtPromiseDone` 判断对应端点是否已经进入终态。`xrtFutureResult` 返回借用的 `xfutureresult`，`xrtFutureError` 直接返回失败终态中的借用错误。结果中的值和错误只在调用方继续持有 Future 引用时有效。Pending 返回 `false` 并设置 `XERR_AGAIN`。`xrtFutureValue` 是常见成功路径助手；对失败、取消或关闭状态调用时，会把对应结构化错误设置到当前执行上下文。

`xrtPromiseForward` 把一个已经进入终态的源 Future 安全透传到目标 Promise。失败错误会增加引用；成功值保持借用语义，同时由目标 Future 保留源引用。透传所有者链使用迭代释放，因此深延续链的销毁不会递归耗尽 C 调用栈。源仍为 Pending、目标已经完成或发生自透传时返回 `false`。

## 取消

```c
bool xrtFutureCancel(xfuture* pFuture);
xcancel* xrtFutureCancelToken(const xfuture* pFuture);
xcancel* xrtPromiseCancelToken(const xpromise* pPromise);
```

`xrtFutureCancel` 只发出协作取消请求，Future 保持 Pending，直到生产端完成、确认取消或关闭。这样资源回收、内核 I/O 取消和任务退出不会被一个伪造终态掩盖。令牌访问函数返回新增引用，使用完毕后调用 `xrtCancelDestroy`。

生产过程可以处理取消后正常返回值；取消请求与最终状态是两个正交事实。只有 `xrtPromiseCancel` 才写入 `XFUTURE_CANCELLED`。

`xrtPromiseCancel`、`xrtPromiseClose` 和最后一个 Promise 引用的隐式关闭使用两阶段发布：先独占终态写入权并完成取消令牌通知，再发布 `CANCELLED`/`CLOSED`、唤醒等待者和执行 Future 回调。因此任何已经观察到取消或关闭终态的线程都必然也能观察到令牌已经请求；令牌监听执行期间 Future 仍显示为 Pending，但其他生产者已经不能抢占终态。取消监听不得等待同一个 Future 进入终态，否则会形成生产端自等待。

## 同步等待

```c
xwaitresult xrtFutureWait(xfuture* pFuture);
xwaitresult xrtFutureWaitFor(xfuture* pFuture, uint64 iTimeout);
xwaitresult xrtFutureWaitUntil(xfuture* pFuture, xdeadline iDeadline);
xwaitresult xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel
);
```

时间单位统一为微秒，截止时间使用单调时钟。等待支持任意数量线程，并对虚假唤醒、完成与超时竞争、取消监听注销竞争进行循环检查。Future 终态与外部取消回调在同一把 Future 锁下线性化：终态先取得锁时返回 `XWAIT_OK`；取消先取得锁时，本次等待固定返回 `XWAIT_CANCELLED`，即使 Future 在等待线程恢复前已经完成也不会覆盖该结果。截止时间在相同循环边界检查，Future 尚未终结时到期返回 `XWAIT_TIMEOUT`。调用方取消只停止当前等待，不取消 Future；需要同时取消生产过程时另行调用 `xrtFutureCancel`。

## 协程等待

```c
xwaitresult xrtFutureAwait(xfuture* pFuture);
xwaitresult xrtFutureAwaitFor(xfuture* pFuture, uint64 iTimeout);
xwaitresult xrtFutureAwaitUntil(xfuture* pFuture, xdeadline iDeadline);
```

这些函数只能在 `xcosched` 管理的协程中调用。等待节点由当前协程栈保存，不产生每次 await 堆分配；Future 可由任意线程完成，完成通知通过内部代际令牌投递回所属调度器。通知早于真正 park 时不会丢失，等待退出后也不会污染下一次 park；独立的公共 `xrtCoWake` 不会被 Await 清理过程误消费。提前唤醒、超时和协程取消都会安全摘除等待节点。

协程取消使 `Await` 返回 `XWAIT_CANCELLED`，但不改变被等待 Future 的状态，也不自动决定当前协程的终态。用户过程可以完成清理后正常返回；只有显式调用 `xrtCoConfirmCancel`，当前协程才以 `XCORO_TERM_CANCELLED` 终结。对于一次调用者独占的网络 Future，放弃操作时应先调用 `xrtFutureCancel`，让 TCP/UDP 等待节点及时摘除；共享 Future 则由拥有生产过程取消权的一层决定。

## 延续

```c
xfuture* xrtFutureContinue(xfuture* pSource, xfuturecontinueproc pProc, ptr pData);
xfuture* xrtFutureThen(xfuture* pSource, xfuturecontinueproc pProc, ptr pData);
xfuture* xrtFutureCatch(xfuture* pSource, xfuturecontinueproc pProc, ptr pData);
xfuture* xrtFutureFinally(xfuture* pSource, xfuturefinallyproc pProc, ptr pData);
```

延续返回一个独立输出 Future。`Continue` 对全部终态执行；`Then` 只处理 `RESOLVED`；`Catch` 只处理 `FAILED`，不会吞掉 `CANCELLED` 或 `CLOSED`；`Finally` 观察任意终态并自动透传原结果。未命中的条件延续也自动透传。

尚未完成的源在完成 Promise 的线程或 Fiber 中执行短回调；已经完成的源在注册延续的执行上下文中同步执行。内部按注册顺序派发，深链迭代排空，不需要 current-thread pump。回调属于完成路径，不得阻塞、挂起协程或执行长时间 CPU 工作；这些工作应显式提交到任务池、协程调度器或上层网络 worker。

`xfuturecontinueproc` 借用输入结果和输出 Promise。回调必须在返回前完成输出 Promise，或者先调用 `xrtPromiseRef` 保留它并转交异步路径；如果两者都不做，运行库释放最后一个生产端后，输出 Future 进入 `CLOSED`。回调不得释放借用的 Promise 引用。

每个延续都有对应的 `xrtFutureContinueOwned`、`xrtFutureThenOwned`、`xrtFutureCatchOwned` 或 `xrtFutureFinallyOwned` 入口。调用成功后，运行库接管 `pData`，并在回调执行、条件跳过或输出取消后调用一次析构过程；调用失败时所有权仍属于调用方。析构在回调返回后执行，因此异步带走 Promise 时，回调也必须自行转移异步工作所需的数据。

输出 Future 的取消请求会使尚未开始的 `Continue`、`Then` 或 `Catch` 跳过用户回调，并在源进入终态后确认输出取消。`Finally` 仍观察源终态，但输出结果改为取消。取消输出不会伪造或强制改变共享源 Future 的终态。

`xrtFutureContinueOwnedCancelSource` 与 `xrtFutureThenOwnedCancelSource` 用于调用方明确拥有完整生产链的组合层。它们保持对应延续的选择和透传语义，但输出被取消时也向源发出协作取消请求，使文件读取、网络操作或任务能够尽早停止。普通共享源不得使用这两个入口，否则一个消费者会意外取消其他消费者仍需等待的工作。

## 组合器

```c
xfuture* xrtFutureAny(xfuture* const* pFutures, size_t iCount);
xfuture* xrtFutureAll(xfuture* const* pFutures, size_t iCount);
xfuture* xrtFutureRace(xfuture* const* pFutures, size_t iCount);
```

组合器只协调多个 Future 的生命周期，不复制、不接管源值，也不把某个源的失败冒充为组合器自身失败：

- `Any` 在第一个源进入任意终态时，以 `xfuturepick` 成功完成，不改变其余源。
- `Race` 与 `Any` 的选择结果相同，随后向其余源发出协作取消请求；源生产端仍决定各自最终状态。
- `All` 等全部输入槽位进入终态后，以保序的 `xfutureall` 成功完成。重复源按重复槽位计数；空集合立即成功完成。
- 输入数量同时受连续分配大小和内部引用计数上限约束，超出任一边界都会在读取输入数组前以 `XERR_RANGE` 失败。
- `xfuturepick` 和 `xfutureall` 都是借用视图，只在持有组合 Future 引用时有效。组合器内部保留全部源引用，因此通过视图读取源结果不会悬空。
- 取消尚未完成的组合 Future 会确认组合器自身为 `XFUTURE_CANCELLED`、摘除全部源监听，并向源发出取消请求；源 Future 不会被伪造为已取消。

这种契约把“多个操作何时达到协调点”和“每个操作以什么结果结束”分开。调用方可以直接检查 `pPick->Future` 或 `pAll->Futures[i]` 的状态、值和结构化错误，也可以在上层任务组中实现失败即取消、收集全部错误等策略。

## 示例

```c
xfuture* pFuture;
xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
int iValue = 42;

if ( (pPromise == NULL) || !xrtPromiseResolve(pPromise, &iValue) ) {
	return false;
}
if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
	return false;
}
printf("%d\n", *(int*)xrtFutureValue(pFuture));
xrtPromiseDestroy(pPromise);
xrtFutureDestroy(pFuture);
```

网络适配层直接返回同一种 `xfuture`。TCP、UDP、TLS Stream、HTTP 和 WebSocket
的 Future 都可以交给上述同步等待、组合器、延续和协程 Await；协议层只定义
成功值、取消点和终态映射，不复制 Future 或协程状态机。

标准库和扩展库的异步适配器共用公开的 `xfuturebridge`；对象使用
`XRT_FUTURE_BRIDGE_STORAGE_SIZE` 字节不透明存储，可直接嵌入操作上下文。底层操作可以
在监听安装期间并发完成，但只有装配进入 READY 后才能发布 Future；监听分配
失败会进入 FAILED，底层晚到结果只执行对应资源回收，不会留下回调。这个底层
契约由 `tests/concurrency/test_future_bridge_oom.c` 独立验证，DNS、TCP、TLS 与
HTTP 适配器不再分别复制这一段易错生命周期代码。

```c
xfuturebridge bridge;
xfuture* future = xrtFutureBridgeCreate(&bridge, parentCancel);
xpromise* promise = xrtFutureBridgePromise(&bridge);

if ( !xrtFutureBridgeWatch(&bridge, cancelOperation, operation) ) {
	(void)xrtFutureBridgeFail(&bridge);
	cancelUnderlyingOperation(operation);
	return NULL;
}
(void)xrtFutureBridgeReady(&bridge);
```

`xrtFutureBridgeCreate` 创建 Future/Promise 对；`xrtFutureBridgeInit` 则借用一个
已有 Promise，两种路径都不接管 Promise 所有权。适配器最终仍须调用
`xrtPromiseDestroy(xrtFutureBridgePromise(&bridge))`。`xrtFutureBridgeReady` 与
`xrtFutureBridgeFail` 只能发布一次，重复发布以状态错误失败。完成回调先调用
`xrtFutureBridgeWait` 跨过极短装配窗口，再调用 `xrtFutureBridgeUnwatch` 与正在
执行的取消回调汇合。Wait 返回 `false` 时只回收底层结果，不写入 Promise。

适配器发布终态前必须先固定成功值或错误引用、注销取消监听、销毁底层操作，
并归还等待节点、缓存预算、网络对象引用和临时 Engine 租约。因而观察到网络
Future 终态时，适配器上下文不会再被迟到回调访问，也没有只为该操作保留的
Engine 活动对象。成功 Future 明确拥有的 Stream、Packet、响应或连接是公开结果，
其生命周期继续由 Future 引用保护，不属于内部临时资源。协作取消同样遵守该屏障，
不会先伪造 `CANCELLED` 再在后台回收操作。

完整示例位于 `examples/concurrency/future/main.c`、
`examples/concurrency/future_combine/main.c`、
`examples/concurrency/future_coroutine/main.c` 与
`examples/tls/stream_future/main.c`、`examples/tls/dial_future/main.c`。
其中 `future_combine` 同时演示 Any、All、Race、胜出源索引和 Race 的协作取消语义。
