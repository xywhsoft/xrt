# 取消令牌

`cancel` 模块提供与网络、任务或协程无关的通用取消状态。令牌可以组成不可变父子链；监听任一子令牌时，父链上的首次取消也会同步触发该监听。

创建/父链保有、引用/销毁、Watch 装配、Request 和 Unwatch 的拥有转换参与
XRT 协作冻结，先取得 mutation 准入再取内部锁。普通借用型回调仍保持完整
mutation 隔离；显式拥有型回调及其引用释放在 API 自己的 scope 外执行。
Request 为每个派发持有真实 Watch 引用，并独立保活令牌；在途派发、调用、
装配和注销转换不提供可采集状态。Unwatch 等待另一线程时不持有本 API 的
mutation scope，不暂停调用者已有的外层 scope。Requested/Triggered 仍是
原子状态观察。任何形式都不授权在 freeze 内调用取消回调或等待它。

## 显式拥有型监听与退役合同

`xcancelwatchownershipv1` 是常驻、不可变的策略，包含精确 `size`、`Notify`、
`Drop` 和实际 Data 节点的 `Ops`。`xrtCancelWatchOwnedV1(cancel, data, policy)`
成功时接管一个已有的真实 Data 引用；失败不接管、不通知、不调用 Drop。
祖先已取消时可在注册返回前同步 Notify，但这不提前归还 Data。Notify 借用
该引用，必须自行协调可变状态与代码存活；最终 Drop 在注销和所有派发/计划
pin 归还后只执行一次。Drop 归还所描述的实际引用，不能绕过 Data 自己需要的
语义准备或在子节点 finalization 后新增语义清理。旧 `xrtCancelWatch` 仍只
借用 Data，不能因为存在 Trace 或相同回调签名而被自动认证。

`xrtCancelWatchOwnershipAdapterV1` 和 `xrtCancelOwnershipAdapterV2` 必须在
调用者建立的整图 freeze 内查询，显式接收允许的策略身份集合，并同时取得
lifecycle 与 `xrtownershippreparationv1`。匹配身份前不解引用策略，不调用
Data 的 Trace；拒绝时 preparation 输出不变。旧 token V1 继续拒绝所有仍有
监听的令牌。新的准入也独立拒绝在途 Request、派发、运行、装配、注销中的
对象及未知/借用监听，Data 及其子节点仍须由收集器独立准入。

`xrtCancelOwnership` 与 `xrtCancelWatchOwnership` 返回上述真实节点的借用视图。
拥有关系是 Watch 的一个 Cancel 引用、一个 Data 引用，以及 Cancel 保留的
父引用。令牌链表只借用 Watch 的注册存储，不能伪造 token 到 Watch 的拥有边。
派发及 API entry 的临时引用按实际引用计数形成外部根；它们不是可忽略的噪声。
一个只从 token 出发、没有触及 Watch 实际拥有者的图不能据此证明 Watch 不可达。

Prepare 不请求取消，也不摘除尚有语义效果的监听。它返回 BUSY，直到生产者
自身按真实完成/取消协议执行 Unwatch，且通知和注销尾部结束。之后才能在
同一 claim 下 Clear，并在 freeze 外 Finish 归还真实 Cancel、父及 Data 槽。
Restore 保留原监听语义；两个 Finish 顺序均由真实 plan pin 保活，重复 Finish
不重复释放。单独取得 lifecycle、单独追踪图或 BUSY 都不构成卸载权限。

Future 的 `xrtFutureCancel` 同样先取得真实 Cancel 临时引用，再在自身转换
scope 外请求取消；请求不伪造 Future 终态。正常 Promise close/cancel/最后
生产端离开沿既有通知路径执行。调用者已经持有的外层 scope 始终不被暂停。

## 模块契约：错误

取消令牌 API 的失败经 `xrtGetError()` 报告：

| 错误 | 触发场景 |
|---|---|
| `XERR_ARGUMENT` | 令牌指针为空 |
| `XERR_STATE` | 令牌已销毁或回调链状态非法 |
| `XERR_MEMORY` | 回调链节点分配失败 |
| `XERR_RANGE` | 溢出保护触发 |

## 裁剪与依赖

| 项目 | 值 |
| --- | --- |
| 裁剪宏 | `XRT_FEATURE_CANCEL` |
| 直接依赖 | `XRT_FEATURE_COND`、`XRT_FEATURE_MUTEX` |
| 头文件 | `<xrt/cancel.h>` 或 `<xrt.h>` |

该模块复用 XRT 的互斥锁和条件变量完成监听生命周期同步，不创建线程，也不依赖网络引擎、任务调度器或协程运行时。

## 类型

### `xcancel`

引用计数取消令牌。取消状态只能从“未请求”变为“已请求”，不能复位。子令牌持有父令牌引用，因此父链在子令牌存活期间保持有效。

### `xcancelwatch`

一次监听句柄。它同时挂接到目标令牌及其全部祖先，任一节点首次取消时把监听标记为已触发。每个监听的回调至多执行一次。

### `xcancelproc`

```c
typedef void (*xcancelproc)(ptr pData);
```

回调由命中取消的线程同步执行。回调不得跨越 C 调用栈跳转，必须正常返回。耗时工作应由回调唤醒其他执行单元处理，避免延长 `xrtCancelRequest`。

## 核心契约

- `xrtCancelRequest` 只对当前令牌的第一次本地请求返回 `true`。父令牌已经取消后，子令牌仍可完成自己的第一次本地请求，但同一监听不会重复回调。
- `xrtCancelRequested` 查询当前令牌和完整父链，因此父取消对子查询立即可见；子取消不会向父传播。
- 父关系在创建后不可变，避免传播期间修改拓扑和额外同步。
- 在已经取消的令牌上注册监听，会在 `xrtCancelWatch` 返回前同步执行一次回调，不会遗漏取消。
- `xrtCancelUnwatch` 从其他线程调用时，会等待已经开始的回调返回；函数返回后该监听不会再执行回调。
- 回调可以注销自己的监听。此时注销不会等待自己，实际内存回收延迟到回调返回后完成；调用方不能再次访问该监听。
- 同一个监听句柄只能完成一次所有权注销。并发发起的注销会被安全串行化，但首次注销完成后再次使用旧指针属于无效访问。

## 函数

### `xrtCancelCreate`

创建独立根令牌并返回一个调用方拥有的引用。

```c
xcancel* xrtCancelCreate(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新令牌（引用计数 1）；`Destroy` 释放 | — |
| `NULL` | 结构分配或内部互斥锁初始化失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_MEMORY` — 令牌结构分配失败或互斥锁初始化失败

#### 范例

[concurrency/cancel · 父子传播](../../examples/concurrency/cancel/main.c) · 根令牌起步

```c
xcancel* pRequest = xrtCancelCreate();
xcancel* pOperation;
xcancelwatch* pWatch;
bool bStopped = false;

if ( pRequest == NULL ) {
	return 1;
}
```

### `xrtCancelChild`

创建子令牌并持有父引用；`pParent` 为空时等价于创建新的根令牌。

```c
xcancel* xrtCancelChild(xcancel* pParent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pParent` | 输入 | 允许空 | 父令牌；存活期由子令牌引用保证 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 子令牌；父链不可变，`Destroy` 子令牌时逐级释放父引用 | — |
| `NULL` | 父引用获取失败或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 父令牌引用计数耗尽（`Ref` 失败）
- `XERR_MEMORY` — 子令牌结构分配失败

#### 范例

[concurrency/cancel · 父子传播](../../examples/concurrency/cancel/main.c) · 父取消同步触发子监听

```c
pOperation = xrtCancelChild(pRequest);
if ( pOperation == NULL ) {
	xrtCancelDestroy(pRequest);
	return 1;
}
```

### `xrtCancelRef`

增加取消令牌引用，供多个持有者共享同一取消源。

```c
xcancel* xrtCancelRef(xcancel* pCancel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCancel` | 输入 | 非空 | 目标令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1；每个成功 `Ref` 须配一次 `Destroy` | — |
| `NULL` | 参数非法或引用计数耗尽 | 令牌不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pCancel` 为空或引用计数已耗尽

#### 范例

[concurrency/cancel · 多持有者](../../examples/concurrency/cancel/main.c) · Ref 后各自 Destroy 一次

```c
/* Ref：增加令牌引用（多持有者共享同一取消源），用完各Destroy一次。 */
xcancel* pExtra = xrtCancelRef(pOperation);
```

### `xrtCancelDestroy`

释放取消令牌引用，并顺着唯一父引用迭代回收。空指针是空操作。

```c
void xrtCancelDestroy(xcancel* pCancel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCancel` | 输入 | 允许空 | 要释放的引用；归零时释放令牌并继续释放父引用 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 每个成功创建或增加的引用必须释放一次 |

#### 范例

[concurrency/cancel · 收尾](../../examples/concurrency/cancel/main.c) · 子先于父销毁（顺序不强制）

```c
xrtCancelDestroy(pOperation);
xrtCancelDestroy(pRequest);
return bStopped ? 0 : 1;
```

### `xrtCancelRequest`

原子完成当前令牌的首次本地取消请求，摘除该节点上的监听后在令牌锁外同步通知。

```c
bool xrtCancelRequest(xcancel* pCancel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCancel` | 输入 | 非空 | 目标令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 本次是第一个本地请求；回调已同步执行完毕 | — |
| `false` | 已请求过（正常结果，不设错）或参数非法 | 重复请求不设置错误；空指针设置 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pCancel` 为空

#### 范例

[concurrency/cancel · 父子传播](../../examples/concurrency/cancel/main.c) · 请求父令牌，子监听立即回调

```c
(void)xrtCancelRequest(pRequest);
printf("operation stopped: %s\n", bStopped ? "yes" : "no");
```

### `xrtCancelRequested`

查询当前令牌或完整祖先链是否已取消。空指针表示没有取消源，返回 `false` 且不设置错误，便于可选取消参数直接使用。

```c
bool xrtCancelRequested(const xcancel* pCancel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCancel` | 输入 | 允许空 | 目标令牌；空指针视为“无取消源” |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 本令牌或任一祖先已请求取消 | — |
| `false` | 全链未取消或无取消源 | 纯查询，不设置错误 |

#### 错误

- 无 — 空指针是合法的“无取消源”查询

#### 范例

[concurrency/task_group_pool · 协作自查](../../examples/concurrency/task_group_pool/main.c) · 任务函数内的取消检查点

```c
if ( xrtCancelRequested(pCancel) ) {
	return XTASK_CANCELLED;
}
```

### `xrtCancelWatch`

注册一次同步回调。监听持有目标令牌引用，并用一次连续分配保存完整父链节点。

```c
xcancelwatch* xrtCancelWatch(
	xcancel* pCancel,
	xcancelproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCancel` | 输入 | 非空 | 目标令牌；监听覆盖其全部祖先 |
| `pProc` | 输入 | 非空 | 取消回调，至多执行一次 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 调用方拥有的监听；`Unwatch` 释放 | — |
| `NULL` | 参数非法、链溢出或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 令牌或回调为空
- `XERR_RANGE` — 父链长度超出可分配的监听节点数
- `XERR_MEMORY` — 监听结构分配失败

#### 范例

[concurrency/cancel · 观察者](../../examples/concurrency/cancel/main.c) · 已取消令牌上注册会同步执行一次回调

```c
pWatch = xrtCancelWatch(pOperation, stopWork, &bStopped);
if ( pWatch == NULL ) {
	xrtCancelDestroy(pOperation);
	xrtCancelDestroy(pRequest);
	return 1;
}
```

### `xrtCancelTriggered`

无锁查询监听是否已经命中取消。

```c
bool xrtCancelTriggered(const xcancelwatch* pWatch);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输入 | 非空 | 目标监听 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已命中取消（回调已执行或正在执行） | — |
| `false` | 未命中或参数非法 | 空指针设置 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pWatch` 为空

#### 范例

[concurrency/cancel · 观察者](../../examples/concurrency/cancel/main.c) · 请求后监听已触发

```c
printf("watch-triggered=%d\n", xrtCancelTriggered(pWatch) ? 1 : 0);
```

### `xrtCancelUnwatch`

注销并释放调用方拥有的监听。空指针是空操作。普通路径会保证返回时没有正在执行或未来可能执行的回调；回调自身注销使用延迟回收规则。

```c
void xrtCancelUnwatch(xcancelwatch* pWatch);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输入 | 允许空 | 要注销的监听；同一句柄只能注销一次 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；从其他线程注销会等待已开始的回调返回 |

#### 范例

[concurrency/cancel · 收尾](../../examples/concurrency/cancel/main.c) · 注销后再销毁令牌

```c
xrtCancelUnwatch(pWatch);
xrtCancelDestroy(pOperation);
xrtCancelDestroy(pRequest);
```

## 取消监听的拥有图

`xrtCancelWatchOwnership(watch)` 返回由 Unwatch 释放的实际监听节点视图，追踪它
保留的 Cancel，并通过 Cancel 视图追踪父链。监听链中的注册节点、Proc / Data
都是借用，不能伪造为额外强引用。回调执行中或监听正在销毁时拒绝图检查。
调用方必须保证整个传递图静止、回调代码驻留；这不是线程停止或模块卸载接口。

## 示例

```c
xcancel* pGroup = xrtCancelCreate();
xcancel* pOperation = xrtCancelChild(pGroup);
xcancelwatch* pWatch = xrtCancelWatch(pOperation, stopWork, pState);

if ( (pGroup == NULL) || (pOperation == NULL) || (pWatch == NULL) ) {
	return false;
}
xrtCancelRequest(pGroup);
xrtCancelUnwatch(pWatch);
xrtCancelDestroy(pOperation);
xrtCancelDestroy(pGroup);
```

完整示例位于 `examples/concurrency/cancel/main.c`。

## 回收器接纳边界

`xrtCancelOwnershipAdapterV1` 仅在调用者已冻结全图且该令牌没有注册观察者时
返回常驻物理适配器。Parent 是真实拥有边、分别接纳；其不可变尾链保持到最终
Drop，不通过估计引用数或提前释放父对象来消环。该接口不认证 CancelWatch。
Future 回收中的无观察者生产端关闭仅修改当前令牌的取消状态，既不取消父令牌，
也不在 freeze 中调用回调或等待原生锁。
