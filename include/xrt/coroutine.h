#ifndef XRT_COROUTINE_H
#define XRT_COROUTINE_H

#include <xrt/cancel.h>
#include <xrt/thread.h>



#if defined(XRT_FEATURE_COROUTINE) && !defined(XRT_FEATURE_THREAD)
	#error "XRT_FEATURE_COROUTINE requires XRT_FEATURE_THREAD"
#endif

#if defined(XRT_FEATURE_COROUTINE) && !defined(XRT_FEATURE_CANCEL)
	#error "XRT_FEATURE_COROUTINE requires XRT_FEATURE_CANCEL"
#endif

#if defined(XRT_FEATURE_COROUTINE) && !defined(XRT_FEATURE_TEMP_MEMORY)
	#error "XRT_FEATURE_COROUTINE requires XRT_FEATURE_TEMP_MEMORY"
#endif

#if defined(XRT_FEATURE_COROUTINE_SCHEDULER) && !defined(XRT_FEATURE_COROUTINE)
	#error "XRT_FEATURE_COROUTINE_SCHEDULER requires XRT_FEATURE_COROUTINE"
#endif

#if defined(XRT_FEATURE_COROUTINE_SCHEDULER) && !defined(XRT_FEATURE_MUTEX)
	#error "XRT_FEATURE_COROUTINE_SCHEDULER requires XRT_FEATURE_MUTEX"
#endif

#if defined(XRT_FEATURE_COROUTINE_SCHEDULER) && !defined(XRT_FEATURE_COND)
	#error "XRT_FEATURE_COROUTINE_SCHEDULER requires XRT_FEATURE_COND"
#endif

#if defined(XRT_FEATURE_COROUTINE_EVENT) && \
	!defined(XRT_FEATURE_COROUTINE_SCHEDULER)
	#error "XRT_FEATURE_COROUTINE_EVENT requires XRT_FEATURE_COROUTINE_SCHEDULER"
#endif

#if defined(XRT_FEATURE_COROUTINE_EVENT) && !defined(XRT_FEATURE_MUTEX)
	#error "XRT_FEATURE_COROUTINE_EVENT requires XRT_FEATURE_MUTEX"
#endif



#if defined(XRT_FEATURE_COROUTINE)

/* 协程对象对外保持不透明，并且固定归属于创建它的原生线程。 */
typedef struct xcoro xcoro;



/* 协程过程返回的指针由调用方定义所有权。 */
typedef ptr (*xcoroproc)(ptr pData);



/* 协程退出清理过程在所属协程的执行上下文中运行。 */
typedef void (*xcocleanupproc)(ptr pData);



/* 协程状态只描述可恢复性，退出原因由 xcoroterm 单独表达。 */
typedef enum xcorostate {
	XCORO_READY = 0,
	XCORO_RUNNING = 1,
	XCORO_SUSPENDED = 2,
	XCORO_DONE = 3
} xcorostate;



/* 协程终态区分正常返回、协作取消和未处理错误。 */
typedef enum xcoroterm {
	XCORO_TERM_NONE = 0,
	XCORO_TERM_RETURNED = 1,
	XCORO_TERM_CANCELLED = 2,
	XCORO_TERM_ERROR = 3
} xcoroterm;



/* 终结过程接收最终终态快照，不能让出、恢复或销毁当前协程。 */
typedef void (*xcorofinalproc)(
	xcoroterm Term,
	ptr pResult,
	const xerror* pError,
	ptr pData
);



/* 创建配置只保存会改变核心执行契约的选项。 */
typedef struct xcoroargs {
	size_t StackSize;
	xcancel* Cancel;
	xcorofinalproc Finalize;
	ptr FinalizeData;
} xcoroargs;



/* 调用方提供清理节点存储，避免每次压栈产生堆分配。 */
typedef struct xcocleanup {
	struct xcocleanup* Previous;
	xcoro* Owner;
	xcocleanupproc Proc;
	ptr Data;
	bool Active;
	bool Managed;
} xcocleanup;

#define XRT_CO_CLEANUP_INIT { 0 }



/* 默认栈仅保留虚拟地址空间，Windows Fiber 按需提交实际页面。 */
#if UINTPTR_MAX > UINT32_MAX
	#define XRT_CORO_STACK_DEFAULT (128u * 1024u)
#else
	#define XRT_CORO_STACK_DEFAULT (64u * 1024u)
#endif

#define XRT_CORO_STACK_MIN (32u * 1024u)
#define XRT_CORO_STACK_MAX (64u * 1024u * 1024u)



XRT_EXTERN_C_BEGIN



/* 创建一个尚未运行的协程；配置为空时使用默认栈和独立取消令牌。 */
XRT_API xcoro* xrtCoCreate(xcoroproc pProc, ptr pData, const xcoroargs* pArgs);



/* 销毁未启动或已经结束的协程；活跃协程保持有效并返回 false。 */
XRT_API bool xrtCoDestroy(xcoro* pCo);



/* 在所属线程恢复协程，直到它让出执行权或结束。 */
XRT_API bool xrtCoResume(xcoro* pCo);



/* 让出当前协程；恢复后若已请求取消则返回 CANCELLED。 */
XRT_API xwaitresult xrtCoYield(void);



/* 返回当前正在运行的协程；普通执行路径返回空指针。 */
XRT_API xcoro* xrtCoCurrent(void);



/* 返回协程状态快照。 */
XRT_API xcorostate xrtCoState(const xcoro* pCo);



/* 返回协程终态原因；尚未结束时返回 NONE。 */
XRT_API xcoroterm xrtCoTerm(const xcoro* pCo);



/* 返回正常结束协程的借用结果；其他状态返回空指针。 */
XRT_API ptr xrtCoResult(const xcoro* pCo);



/* 返回失败协程借用的结构化错误；其他状态返回空指针。 */
XRT_API const xerror* xrtCoError(const xcoro* pCo);



/* 幂等地请求协程协作取消，并唤醒调度器中的等待。 */
XRT_API bool xrtCoCancel(xcoro* pCo);



/* 返回增加引用后的取消令牌，调用方使用完毕后必须释放。 */
XRT_API xcancel* xrtCoCancelToken(const xcoro* pCo);



/* 判断当前协程是否收到取消请求。 */
XRT_API bool xrtCoStopping(void);



/* 确认当前协程将以取消终态返回；只能在已收到取消请求时调用。 */
XRT_API bool xrtCoConfirmCancel(void);



/* 释放当前外部线程的惰性协程运行时；XRT 线程退出时自动调用。 */
XRT_API bool xrtCoThreadDetach(void);



/* 将零初始化的调用方清理节点压入当前协程；节点必须存活到弹出或协程终结。 */
XRT_API bool xrtCoCleanupPush(
	xcocleanup* pCleanup,
	xcocleanupproc pProc,
	ptr pData
);



/* 注册由协程管理存储期的清理过程，返回可用于提前弹出的节点。 */
XRT_API xcocleanup* xrtCoDefer(xcocleanupproc pProc, ptr pData);



/* 从当前协程弹出栈顶清理节点，并可选择立即执行。 */
XRT_API bool xrtCoCleanupPop(xcocleanup* pCleanup, bool bRun);



/* 返回当前目标使用的稳定后端名称。 */
XRT_API cstr xrtCoBackend(void);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_COROUTINE_SCHEDULER)

/* 单线程协程调度器对外保持不透明。 */
typedef struct xcosched xcosched;

/* 默认最多保留 1024 个尚未执行的用户投递；内部唤醒不占用此预算。 */
#define XRT_CO_SCHED_POST_LIMIT_DEFAULT 1024u



/* 调度器投递过程运行在所属线程的普通调用栈中，适合短小的调度操作。 */
typedef void (*xcoschedpostproc)(xcosched* pSched, ptr pData);



XRT_EXTERN_C_BEGIN



/* 在当前原生线程创建使用默认投递上限的协程调度器。 */
XRT_API xcosched* xrtCoSchedCreate(void);



/* 指定待执行用户投递上限；0 使用默认值，SIZE_MAX 显式取消实际限额。 */
XRT_API xcosched* xrtCoSchedCreateLimit(size_t iPostLimit);



/* 销毁没有活跃协程和待执行投递的调度器，并回收仍保留的完成句柄。 */
XRT_API bool xrtCoSchedDestroy(xcosched* pSched);



/* 返回当前协程所属的借用调度器；普通执行路径返回空指针。 */
XRT_API xcosched* xrtCoSchedCurrent(void);



/* 从任意线程按 FIFO 顺序投递借用数据过程；队满返回 XERR_AGAIN，不受理过程。 */
XRT_API bool xrtCoSchedPost(
	xcosched* pSched,
	xcoschedpostproc pProc,
	ptr pData
);



/* 从任意线程投递过程并接管数据；失败不接管，受理后在过程返回后恰好析构一次。 */
XRT_API bool xrtCoSchedPostOwned(
	xcosched* pSched,
	xcoschedpostproc pProc,
	ptr pData,
	xcocleanupproc pDestroy
);



/* 创建由调度器管理且在完成后保留句柄的协程。 */
XRT_API xcoro* xrtCoSpawn(
	xcosched* pSched,
	xcoroproc pProc,
	ptr pData,
	const xcoroargs* pArgs
);



/* 创建完成后由调度器自动回收的分离协程。 */
XRT_API bool xrtCoGo(
	xcosched* pSched,
	xcoroproc pProc,
	ptr pData,
	const xcoroargs* pArgs
);



/* 请求取消全部活跃协程并停止接收新协程和新投递。 */
XRT_API bool xrtCoSchedClose(xcosched* pSched);



/* 非阻塞执行至多一个就绪协程。 */
XRT_API xwaitresult xrtCoSchedStep(xcosched* pSched);



/* 在相对微秒数内等待事件并执行至多一个就绪协程。 */
XRT_API xwaitresult xrtCoSchedPollFor(xcosched* pSched, uint64 iTimeout);



/* 等待事件到指定截止时间并执行至多一个就绪协程。 */
XRT_API xwaitresult xrtCoSchedPollUntil(xcosched* pSched, xdeadline iDeadline);



/* 持续运行调度器，直到全部协程结束。 */
XRT_API bool xrtCoSchedRun(xcosched* pSched);



/* 在所属线程返回调度器中尚未结束的协程数量。 */
XRT_API size_t xrtCoSchedAlive(const xcosched* pSched);



/* 线程安全且幂等地唤醒协程；调用期间句柄必须保持有效。 */
XRT_API bool xrtCoWake(xcoro* pCo);



/* 挂起当前调度协程，直到被唤醒或取消。 */
XRT_API xwaitresult xrtCoPark(void);



/* 在相对微秒数内挂起当前调度协程。 */
XRT_API xwaitresult xrtCoParkFor(uint64 iTimeout);



/* 挂起当前调度协程，直到被唤醒、取消或到达截止时间。 */
XRT_API xwaitresult xrtCoParkUntil(xdeadline iDeadline);



/* 睡眠相对微秒数；自然到期或提前唤醒返回 OK。 */
XRT_API xwaitresult xrtCoSleep(uint64 iTimeout);



/* 睡眠到指定截止时间；自然到期或提前唤醒返回 OK。 */
XRT_API xwaitresult xrtCoSleepUntil(xdeadline iDeadline);



/* 在当前调度协程中等待同一调度器的目标结束。 */
XRT_API xwaitresult xrtCoJoin(xcoro* pCo);



/* 在相对微秒数内等待同一调度器的目标结束。 */
XRT_API xwaitresult xrtCoJoinFor(xcoro* pCo, uint64 iTimeout);



/* 等待同一调度器的目标结束到指定截止时间。 */
XRT_API xwaitresult xrtCoJoinUntil(xcoro* pCo, xdeadline iDeadline);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_COROUTINE_EVENT)

/*
 * 协程事件使用固定对齐存储，内部包含互斥锁和等待队列。
 * 平台余量允许内部布局演进，但不承诺跨平台二进制尺寸相同。
 */
#if defined(_WIN32) || defined(_WIN64)
	#define XRT_CO_EVENT_STORAGE_SIZE 64u
#else
	#define XRT_CO_EVENT_STORAGE_SIZE 160u
#endif



/* 协程事件允许嵌入调用方结构，不需要为对象本身分配内存。 */
typedef union xcoevent {
	uint64 Alignment;
	uint8 Storage[XRT_CO_EVENT_STORAGE_SIZE];
} xcoevent;



XRT_EXTERN_C_BEGIN



/* 初始化自动或手动复位协程事件。 */
XRT_API bool xrtCoEventInit(
	xcoevent* pEvent,
	bool bManualReset,
	bool bSignaled
);



/* 释放协程事件；仍有尚未返回的等待者时失败并保持对象有效。 */
XRT_API bool xrtCoEventUnit(xcoevent* pEvent);



/* 创建自动或手动复位协程事件。 */
XRT_API xcoevent* xrtCoEventCreate(
	bool bManualReset,
	bool bSignaled
);



/* 释放 Create 返回的协程事件；仍有等待者时失败且不释放对象。 */
XRT_API bool xrtCoEventDestroy(xcoevent* pEvent);



/* 置位事件；手动复位唤醒全部等待者，自动复位按 FIFO 唤醒一个。 */
XRT_API bool xrtCoEventSet(xcoevent* pEvent);



/* 清除事件的信号态；已经获得信号的等待者不受影响。 */
XRT_API bool xrtCoEventReset(xcoevent* pEvent);



/* 挂起当前调度协程，直到事件置位或协程取消。 */
XRT_API xwaitresult xrtCoEventAwait(xcoevent* pEvent);



/* 非阻塞地检查并消费自动复位事件。 */
XRT_API xwaitresult xrtCoEventTryAwait(xcoevent* pEvent);



/* 在相对微秒数内等待事件置位。 */
XRT_API xwaitresult xrtCoEventAwaitFor(
	xcoevent* pEvent,
	uint64 iTimeout
);



/* 等待事件置位、协程取消或到达截止时间。 */
XRT_API xwaitresult xrtCoEventAwaitUntil(
	xcoevent* pEvent,
	xdeadline iDeadline
);



XRT_EXTERN_C_END

#endif

#endif
