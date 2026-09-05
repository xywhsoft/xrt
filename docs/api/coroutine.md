# Coroutine

`XRT_FEATURE_COROUTINE` 提供不依赖调度器的有栈协程核心。对象、平台上下文和栈布局均不公开；启用该功能会同时启用 `thread`、`wait`、`cancel`、`temp_memory`、`mutex` 和 `cond` 依赖。

## 执行契约

- 协程固定归属于创建它的原生线程，`Resume`、活跃对象的销毁和调度操作只能在该线程执行。
- Windows 使用 `CreateFiberEx` 的可增长栈，仅保留 `StackSize` 指定的虚拟地址空间，并从较小提交量开始按需增长。
- POSIX 原生后端使用匿名映射和不可访问保护页；可写映射由操作系统按首次访问提交物理页面。
- 每个协程拥有独立的临时内存 arena、结构化错误上下文和取消令牌。
- `xrtCoCancelToken` 返回增加引用后的取消令牌，调用方使用完毕后必须调用 `xrtCancelDestroy`。
- 取消是协作式的。`xrtCoYield` 在恢复后返回 `XWAIT_CANCELLED`，未启动的已取消协程不会执行用户过程。
- 取消请求与终态正交：处理请求后正常返回仍是 `XCORO_TERM_RETURNED`；调用 `xrtCoConfirmCancel` 后返回才是 `XCORO_TERM_CANCELLED`。
- 用户过程正常返回时若仍留有未处理错误，终态为 `XCORO_TERM_ERROR`；显式确认取消优先于残留错误。

## 生命周期

`xrtCoCreate` 创建 `XCORO_READY` 对象。`xrtCoResume` 运行到下一次 `xrtCoYield` 或过程返回。只有未启动的 `READY` 对象和 `DONE` 对象可以由 `xrtCoDestroy` 销毁；拒绝销毁活跃栈可避免跳过清理过程或留下悬空上下文。

外部创建的线程在最后一个协程结束后可调用 `xrtCoThreadDetach` 释放惰性线程运行时。`xrtThreadCreate` 创建的线程会在退出时自动执行该步骤；仍有挂起协程时 detach 会失败。

`xrtCoStopping` 用于查询当前协程是否收到取消请求。等待函数返回 `XWAIT_CANCELLED` 后，用户代码可以清理并正常返回结果；确实要把本次执行记为取消时调用 `xrtCoConfirmCancel`。没有取消请求、普通线程路径或清理栈中调用确认函数都会失败并设置 `XERR_STATE`。

### 终结过程

`xcoroargs.Finalize` 是创建时绑定的一次性终结过程。协程进入 `XCORO_DONE` 时，它在清理栈全部执行之后、终态发布之前收到 `xcoroterm`、正常返回值或未处理错误的借用快照。终结过程使用独立错误上下文；它调用 `xrtClearError` 或产生新错误都不会清除、替换协程已经确定的终态错误，也不会把终结过程内部错误泄漏给宿主线程。

首次恢复前已经取消的协程不会进入用户过程，但仍会以 `XCORO_TERM_CANCELLED` 调用终结过程。这一契约用于释放由调度器受理、不能依赖用户过程启动的资源。正常路径中的终结过程运行在协程栈上；首次运行前取消路径运行在所属宿主线程栈上，因此终结过程只能做不可挂起的收尾，不能调用 yield、park、await、resume、清理栈操作或销毁当前协程。这些受限操作会失败并设置 `XERR_STATE`。

销毁一个由 `xrtCoCreate` 直接创建、从未恢复也未进入终态的 `XCORO_READY` 对象不会调用终结过程，创建者仍负责这一放弃路径。调度器成功受理的协程保证执行终结过程；保留句柄在首次运行前被销毁时以 `XCORO_TERM_CANCELLED` 终结，然后才从调度器移除。

## 平台后端

| 平台 | 后端 | 栈策略 |
| --- | --- | --- |
| Windows x86/x64/ARM64 | Fiber | `CreateFiberEx` 保留上限、按需提交 |
| POSIX x86-64 | 原生汇编 | `mmap` 惰性页面和保护页 |
| POSIX ARM64 | AAPCS64 原生汇编 | `mmap` 惰性页面和保护页 |
| POSIX RISC-V 64 | LP64 原生汇编，按浮点 ABI 保存 | `mmap` 惰性页面和保护页 |
| POSIX LoongArch64 | LP64 原生汇编，按浮点 ABI 保存 | `mmap` 惰性页面和保护页 |

所有原生后端保存 ABI 规定的整数和浮点非易失寄存器。当前发布构建必须在对应目标上执行上下文、深栈和浮点寄存器测试；仅在其他平台编译成功不能替代运行验证。

POSIX 手工栈切换在 AddressSanitizer 构建中使用官方 fiber switch 接口登记栈范围和 fake stack，协程永久结束时主动释放其 fake stack；MemorySanitizer 构建使用对应的 fiber switch 接口登记新旧栈边界；ThreadSanitizer 构建为每个协程维护独立 fiber 身份，避免把不同协程的访问历史混为同一执行上下文。Sanitizer 发布门禁必须使用包含对应运行时的插桩工具链实际执行，只有编译或链接结果不能替代运行验证。Linux x86-64 在启用 CET 时同时支持 IBT 和 shadow stack：内部恢复点包含 `ENDBR64`，每个协程按需映射独立 shadow stack 并随协程销毁。其他 x86-64 系统若要求 SHSTK，编译阶段会明确拒绝，不能静默生成不完整上下文。

## 清理栈

清理栈提供两层 API：

- `xrtCoDefer` 是常用路径，由协程分配并管理节点，过程返回后仍可可靠执行。
- `xrtCoCleanupPush` 是无分配路径，节点必须先使用 `XRT_CO_CLEANUP_INIT` 或全零初始化，且存储期必须覆盖协程终态；不能把未弹出的节点放在协程过程的自动局部变量中。

`xrtCoCleanupPop` 可以弹出任一种节点，并可选择立即执行。所有仍在栈上的清理过程按后进先出顺序执行，清理过程中禁止让出协程。

## 调度器

`XRT_FEATURE_COROUTINE_SCHEDULER` 在核心之上增加单线程调度器。调度器、执行队列、timer heap 和 join 链只由所属线程修改；跨线程入口只把工作放入互斥保护的 FIFO 队列，再由所属线程修改执行结构。

- `xrtCoSpawn` 返回保留句柄。协程完成后仍可读取 `Term`、`Result` 和 `Error`，最后由 `xrtCoDestroy` 或调度器销毁。
- `xrtCoGo` 创建分离协程，完成后自动回收，适合不需要结果的后台过程。
- `xrtCoSchedPost` 从任意线程投递借用数据的短过程；`xrtCoSchedPostOwned` 在受理后接管数据，并在过程返回后恰好析构一次。失败时 Owned 数据仍归调用方。
- `xrtCoPark`、`ParkFor`、`ParkUntil` 区分 `OK`、`TIMEOUT` 和 `CANCELLED`；提前 wake 会保留到下一次 park，不会丢失。
- `xrtCoSleep` 和 `SleepUntil` 自然到期返回 `OK`，取消返回 `CANCELLED`。
- `xrtCoJoin` 只接受同一调度器的保留句柄，支持多个等待者、deadline、取消和依赖环检测。
- `xrtCoSchedClose` 停止接收新协程和新投递，并协作取消全部活跃协程；关闭前已经受理的投递仍按 FIFO 顺序排空。

投递过程运行在所属线程的普通调用栈中，不在协程内，不能调用 yield、park 或 await；它应只做创建协程、完成 Promise、更新短状态等不可阻塞操作。每次 `xrtCoSchedStep` 或 `Poll` 至多执行一个通用投递并恢复一个协程。没有活跃协程和待执行投递时返回 `XWAIT_CLOSED`，用户截止时间到达但仍有挂起协程时返回 `XWAIT_TIMEOUT`。

`xrtCoSchedRun` 会排空调用时已经受理的投递和由它们创建的协程。它不是永久驻留的事件循环：当活跃协程和投递同时归零时返回。销毁前必须停止其他线程访问；有尚未执行的投递时，`xrtCoSchedDestroy` 返回 `XERR_STATE`，不会静默丢弃回调或 Owned 数据。

`xrtCoSchedCreate()` 默认限制 1024 个尚未执行的用户投递（`XRT_CO_SCHED_POST_LIMIT_DEFAULT`）。`xrtCoSchedCreateLimit(n)` 可在创建时调整；0 使用默认值，`SIZE_MAX` 显式取消实际限额。队满立即返回 `false / XERR_AGAIN`，不接管 Owned 数据。过程从队列取出时释放名额，因此过程内部也可以继续投递。内部协程唤醒使用独立的无分配链，不受用户队列预算影响；已有批量生产者若一次提交超过 1024 项，需要显式预算或处理背压。

`xrtCoWake` 线程安全，但句柄销毁不是并发操作。调用方必须保证 wake 返回前目标及所属调度器仍然有效；需要跨线程管理寿命时，应由更高层 Future/Task 持有引用，而不是裸传 `xcoro*`。

## 协程事件

`XRT_FEATURE_COROUTINE_EVENT` 提供调度器原生的 `xcoevent`，用于不阻塞所属线程地等待一次跨线程通知。它依赖 `coroutine_scheduler`，但不依赖线程事件的内核等待对象。

- `xrtCoEventInit` / `Unit` 使用调用方固定存储；`Create` / `Destroy` 是对应的堆对象易用层。
- 自动复位事件每次 `Set` 按 FIFO 唤醒一个等待者；没有等待者时保存一个信号。
- 手动复位事件每次 `Set` 唤醒全部等待者并保持信号态，直到 `Reset`。
- `Await`、`TryAwait`、`AwaitFor` 和 `AwaitUntil` 分别返回 `OK`、`TIMEOUT`、`CANCELLED` 或 `ERROR`。
- 每次 Await 使用协程栈上的等待节点，不产生堆分配；跨线程 `Set` 通过调度器等待代际投递，不会丢失注册期间的信号。
- `Unit` / `Destroy` 与其他操作不能并发。只要仍有 Await 尚未返回，即使它已经获得信号，释放也会以 `XERR_STATE` 失败并保持对象有效。

通用 `xrtCoWake` 只能促使 Await 重新检查条件，不能伪造事件信号。终结过程中的 Await 在读取或消费事件信号前失败。

## 公共类型与常量

| 标识符 | 契约 |
| --- | --- |
| `xcoro` | 线程归属且不透明的协程句柄。 |
| `xcoroproc` | 协程入口过程；返回值的所有权由调用方约定。 |
| `xcocleanupproc` | 不可挂起的清理过程。 |
| `xcorostate` | 可恢复状态：`XCORO_READY`、`XCORO_RUNNING`、`XCORO_SUSPENDED`、`XCORO_DONE`。 |
| `xcoroterm` | 终态原因：`XCORO_TERM_NONE`、`XCORO_TERM_RETURNED`、`XCORO_TERM_CANCELLED`、`XCORO_TERM_ERROR`。 |
| `xcorofinalproc` | 一次性终结过程，接收终态、借用结果和借用错误快照。 |
| `xcoroargs` | 栈大小、父取消令牌、终结过程及其数据。未指定字段保持零值。 |
| `xcocleanup` | 调用方存储的无分配清理节点。 |
| `xcosched` | 线程归属且不透明的调度器句柄。 |
| `xcoschedpostproc` | 在调度器所属线程普通调用栈执行的短投递过程。 |
| `xcoevent` | 可嵌入调用方结构的固定存储协程事件。 |
| `XRT_CO_CLEANUP_INIT` | `xcocleanup` 的静态零值初始化器。 |
| `XRT_CORO_STACK_DEFAULT` | 默认栈保留大小：64 位目标 128 KiB，32 位目标 64 KiB。 |
| `XRT_CORO_STACK_MIN` | 最小栈保留大小 32 KiB。 |
| `XRT_CORO_STACK_MAX` | 最大栈保留大小 64 MiB。 |
| `XRT_CO_EVENT_STORAGE_SIZE` | 当前平台的 `xcoevent` 内部存储容量；不能作为跨平台 ABI 尺寸。 |
| `XRT_CO_SCHED_POST_LIMIT_DEFAULT` | 默认外部投递队列上限：1024 项，不限制内部协程唤醒队列。 |

## API 索引

### 协程核心

| API | 说明 |
| --- | --- |
| `xrtCoCreate` | 创建 `READY` 协程；失败返回空指针并设置结构化错误。 |
| `xrtCoDestroy` | 销毁未启动或已完成对象；活跃对象失败且保持有效。 |
| `xrtCoResume` | 在所属线程恢复协程，直到让出或完成。 |
| `xrtCoYield` | 让出当前协程，并在恢复后返回取消状态。 |
| `xrtCoCurrent` | 返回当前协程的借用句柄；普通调用栈返回空指针。 |
| `xrtCoState` | 返回可恢复状态快照。 |
| `xrtCoTerm` | 仅在状态已发布为 `DONE` 后返回终态，否则返回 `NONE`。 |
| `xrtCoResult` | 返回正常终态的借用结果。 |
| `xrtCoError` | 返回错误终态的借用结构化错误。 |
| `xrtCoCancel` | 线程安全且幂等地请求协作取消，并通知可选调度器。 |
| `xrtCoCancelToken` | 返回增加引用后的取消令牌。 |
| `xrtCoStopping` | 查询当前协程是否收到取消请求。 |
| `xrtCoConfirmCancel` | 确认用户过程返回时发布取消终态。 |
| `xrtCoThreadDetach` | 释放当前外部线程的惰性协程运行时。 |
| `xrtCoCleanupPush` | 压入零初始化、调用方存储的无分配清理节点。 |
| `xrtCoDefer` | 分配并压入由协程管理存储期的清理节点。 |
| `xrtCoCleanupPop` | 弹出栈顶节点，并可立即执行清理过程。 |
| `xrtCoBackend` | 返回当前目标的静态后端名称。 |

### 调度器

| API | 说明 |
| --- | --- |
| `xrtCoSchedCreate` | 在当前线程创建调度器。 |
| `xrtCoSchedCreateLimit` | 创建时指定用户投递上限；0 使用默认 1024。 |
| `xrtCoSchedDestroy` | 销毁空闲调度器及其保留的完成句柄。 |
| `xrtCoSchedCurrent` | 返回当前协程所属的借用调度器。 |
| `xrtCoSchedPost` | 从任意线程 FIFO 投递借用数据过程。 |
| `xrtCoSchedPostOwned` | 投递并在成功受理后接管数据，执行后恰好析构一次。 |
| `xrtCoSpawn` | 创建完成后保留句柄的调度协程。 |
| `xrtCoGo` | 创建完成后自动回收的分离协程。 |
| `xrtCoSchedClose` | 停止受理新工作并请求取消全部活跃协程。 |
| `xrtCoSchedStep` | 非阻塞地执行至多一个投递和一个就绪协程。 |
| `xrtCoSchedPollFor` | 在相对微秒期限内等待，并执行至多一个调度步。 |
| `xrtCoSchedPollUntil` | 在绝对截止时间前等待，并执行至多一个调度步。 |
| `xrtCoSchedRun` | 运行到活跃协程和已受理投递全部排空。 |
| `xrtCoSchedAlive` | 在所属线程返回尚未完成的协程数量。 |
| `xrtCoWake` | 线程安全且幂等地唤醒仍然有效的调度协程。 |
| `xrtCoPark` | 挂起当前协程直到 wake 或取消。 |
| `xrtCoParkFor` | 挂起到相对微秒期限、wake 或取消。 |
| `xrtCoParkUntil` | 挂起到绝对截止时间、wake 或取消。 |
| `xrtCoSleep` | 睡眠相对微秒数；自然到期或提前 wake 返回 `OK`。 |
| `xrtCoSleepUntil` | 睡眠到绝对截止时间。 |
| `xrtCoJoin` | 等待同一调度器的保留句柄完成。 |
| `xrtCoJoinFor` | 在相对微秒期限内等待目标完成。 |
| `xrtCoJoinUntil` | 在绝对截止时间前等待目标完成。 |

### 协程事件

| API | 说明 |
| --- | --- |
| `xrtCoEventInit` | 初始化调用方存储的自动或手动复位事件。 |
| `xrtCoEventUnit` | 释放固定存储事件；有活动等待时失败且保持有效。 |
| `xrtCoEventCreate` | 创建堆存储的自动或手动复位事件。 |
| `xrtCoEventDestroy` | 销毁堆事件；有活动等待时失败且不释放。 |
| `xrtCoEventSet` | 置位事件，并按复位模式唤醒一个或全部等待者。 |
| `xrtCoEventReset` | 清除后续等待可见的信号态。 |
| `xrtCoEventAwait` | 等待信号或取消。 |
| `xrtCoEventTryAwait` | 非阻塞检查信号，并消费自动复位信号。 |
| `xrtCoEventAwaitFor` | 在相对微秒期限内等待信号。 |
| `xrtCoEventAwaitUntil` | 在绝对截止时间前等待信号。 |

## 示例

```c
static ptr work(ptr data)
{
	int* value = (int*)data;

	(*value)++;
	if ( xrtCoYield() != XWAIT_OK ) {
		return NULL;
	}
	return value;
}

int value = 1;
xcoro* co = xrtCoCreate(work, &value, NULL);

xrtCoResume(co);
xrtCoResume(co);
xrtCoDestroy(co);
xrtCoThreadDetach();
```

协程事件适合表达一次或广播式就绪通知：

```c
static ptr wait_ready(ptr data)
{
	xcoevent* ready = (xcoevent*)data;

	return xrtCoEventAwait(ready) == XWAIT_OK ? ready : NULL;
}

xcoevent ready;
xcosched* sched;
xcoro* waiter;

xrtCoEventInit(&ready, false, false);
sched = xrtCoSchedCreate();
waiter = xrtCoSpawn(sched, wait_ready, &ready, NULL);
xrtCoSchedStep(sched);
xrtCoEventSet(&ready);
xrtCoSchedRun(sched);
xrtCoDestroy(waiter);
xrtCoSchedDestroy(sched);
xrtCoEventUnit(&ready);
xrtCoThreadDetach();
```

从任意线程投递创建工作时，使用调度器 post，而不是直接跨线程调用 `xrtCoGo`：

```c
static void post_task(xcosched* sched, ptr data)
{
	(void)xrtCoGo(sched, task, data, NULL);
}

xrtCoSchedPost(sched, post_task, data);
xrtCoSchedRun(sched);
```

调度器常用路径：

```c
static ptr task(ptr data)
{
	if ( xrtCoSleep(1000) != XWAIT_OK ) {
		return NULL;
	}
	return data;
}

xcosched* sched = xrtCoSchedCreate();
xcoro* co = xrtCoSpawn(sched, task, data, NULL);

xrtCoSchedRun(sched);
use_result(xrtCoResult(co));
xrtCoDestroy(co);
xrtCoSchedDestroy(sched);
xrtCoThreadDetach();
```

完整生命周期示例位于 `examples/concurrency/coroutine_lifecycle/main.c`，同时展示协作取消、同调度器 Join、调用方存储清理节点、托管清理节点，以及清理完成后收到的终态快照。
