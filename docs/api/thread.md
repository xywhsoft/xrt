# Thread

`thread` 提供可等待、可共享、可安全分离的原生线程对象。启用宏为
`XRT_FEATURE_THREAD`，依赖 `XRT_FEATURE_WAIT`。

## 类型

```c
typedef struct xthread xthread;
typedef int32 (*xthreadproc)(ptr pData);
```

`xthread` 是不透明对象。线程入口接收创建时的 `pData`，返回稳定的 32 位退出码。

```c
typedef enum xthreadstate {
	XTHREAD_RUNNING = 0,
	XTHREAD_FINISHED = 1
} xthreadstate;
```

状态只表达“运行”和“完成”。协作停止请求不是第三种执行状态。

## 所有权

- `xrtThreadCreate()` 返回一个拥有引用，运行线程另外持有一个内部引用。
- 调用方可以立即 `xrtThreadDestroy()`，让线程在没有外部句柄时安全运行到结束。
- 跨所有权边界共享对象时调用 `xrtThreadRef()`；每个拥有引用最终释放一次。
- `xrtThreadCurrent()` 返回借用对象，只在线程入口执行期间有效，不得释放或保存到入口之外。
- 等待者必须在整个等待期间持有对象引用；多个等待者可以并发等待同一对象。

## 函数

| 函数 | 说明 |
|---|---|
| `xrtThreadCreate(pProc, pData, iStackSize)` | 创建并立即启动线程；栈大小为 `0` 时使用平台默认值。失败返回 `NULL`。 |
| `xrtThreadRef(pThread)` | 增加拥有引用并返回原指针；无效对象返回 `NULL`。 |
| `xrtThreadDestroy(pThread)` | 释放一个拥有引用；传入 `NULL` 无操作，不等待也不强制停止线程。 |
| `xrtThreadWait(pThread)` | 无限等待线程执行体和 XRT 线程上下文清理完成，返回 `xwaitresult`。 |
| `xrtThreadWaitFor(pThread, iTimeout)` | 最多等待相对微秒数。`0` 是非阻塞状态检查。 |
| `xrtThreadWaitUntil(pThread, iDeadline)` | 等待线程执行体和 XRT 线程上下文清理完成到单调时钟 deadline。 |
| `xrtThreadStop(pThread)` | 幂等地发布协作停止请求。 |
| `xrtThreadStopRequested(pThread)` | 查询指定线程是否收到停止请求。 |
| `xrtThreadStopping()` | 在线程入口内查询当前线程的停止请求；外部线程返回 `false`。 |
| `xrtThreadState(pThread)` | 返回线程状态快照。 |
| `xrtThreadExitCode(pThread)` | 线程完成后返回退出码；仍运行时返回 `0` 并设置状态错误。 |
| `xrtThreadId(pThread)` | 返回对象记录的进程内非零线程标识。 |
| `xrtThreadCurrentId()` | 返回调用线程的进程内非零线程标识，外部创建的线程也可使用。 |
| `xrtThreadCurrent()` | 返回当前 XRT 线程的借用对象；外部线程返回 `NULL`。 |
| `xrtThreadYield()` | 主动让出当前处理器时间片。 |

`xrtThreadWait()`、`xrtThreadWaitFor()` 和 `xrtThreadWaitUntil()` 可以重复调用，
超时不会改变对象状态，也不会消费后续等待机会。线程等待自己会返回 `XWAIT_ERROR`
并设置状态错误。成功返回时，线程入口已经返回，线程键、协程及当前错误等 XRT
线程上下文已经清理，内部运行引用也已经释放。

线程标识只用于同一进程生命周期内的相等性比较，不是可持久化编号。线程结束后，
操作系统可以把相同标识分配给后续线程。`xrtThreadExitCode()` 在线程仍运行时返回零并
设置 `XERR_STATE`，因此调用方应先等待完成，不能仅凭返回的零判断真实退出码。

## 创建和等待

```c
static int32 worker(ptr pData)
{
	return *(int*)pData;
}

int iValue = 42;
xthread* pThread = xrtThreadCreate(worker, &iValue, 0);

if ( pThread == NULL ) {
	return -1;
}
if ( xrtThreadWaitFor(pThread, UINT64_C(2000000)) != XWAIT_OK ) {
	xrtThreadDestroy(pThread);
	return -1;
}
printf("%d\n", xrtThreadExitCode(pThread));
xrtThreadDestroy(pThread);
```

## 共享与分离

```c
xthread* pShared = xrtThreadRef(pThread);

xrtThreadDestroy(pThread);       /* 释放原引用 */
consumeThread(pShared);          /* 接收方拥有 pShared */

xthread* pDetached = xrtThreadCreate(backgroundWorker, NULL, 0);
xrtThreadDestroy(pDetached);     /* 不等待，线程仍可安全结束 */
```

## 协作停止

```c
static int32 serviceLoop(ptr pData)
{
	(void)pData;
	while ( !xrtThreadStopping() ) {
		processOneBatch();
	}
	return 0;
}

xthread* pThread = xrtThreadCreate(serviceLoop, NULL, 0);
xrtThreadStop(pThread);
xrtThreadWait(pThread);
xrtThreadDestroy(pThread);
```

库不提供强制终止、挂起或恢复线程。这些操作会破坏资源清理顺序，且跨平台语义不一致。

## 线程安全与错误

状态、退出码、停止请求和等待都可并发访问。引用计数只保护已经合法持有的引用，
不能让一个线程在另一线程释放最后一个外部引用的同时从裸指针获取新引用。
API 返回失败时，当前线程可通过结构化错误接口读取 `xrt.thread` 错误域信息。

可运行示例见 `examples/concurrency/thread/main.c`；生命周期、detach、多等待者、
自等待和 OOM 边界见 `tests/concurrency/test_thread.c` 与
`tests/concurrency/test_thread_oom.c`。
