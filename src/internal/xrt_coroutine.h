#ifndef XRT_INTERNAL_COROUTINE_H
#define XRT_INTERNAL_COROUTINE_H

#include "xrt_temp.h"

#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
	typedef uint32 xrt_co_float_environment;
#else
	#include <fenv.h>
	typedef fenv_t xrt_co_float_environment;
#endif

#if !defined(_WIN32) && !defined(_WIN64)
	#if defined(__has_feature)
		#if __has_feature(address_sanitizer)
			#define XRT_CO_ADDRESS_SANITIZER
		#endif
		#if __has_feature(thread_sanitizer)
			#define XRT_CO_THREAD_SANITIZER
		#endif
		#if __has_feature(memory_sanitizer)
			#define XRT_CO_MEMORY_SANITIZER
		#endif
		#if __has_feature(shadow_call_stack)
			#define XRT_CO_SHADOW_CALL_STACK
		#endif
	#endif
	#if defined(__SANITIZE_ADDRESS__) && !defined(XRT_CO_ADDRESS_SANITIZER)
		#define XRT_CO_ADDRESS_SANITIZER
	#endif
	#if defined(__SANITIZE_THREAD__) && !defined(XRT_CO_THREAD_SANITIZER)
		#define XRT_CO_THREAD_SANITIZER
	#endif
	#if defined(__linux__) && defined(__x86_64__) && \
		defined(__CET__) && ((__CET__ & 0x2) != 0)
		#define XRT_CO_SHADOW_STACK
	#endif
#endif

#if defined(XRT_CO_ADDRESS_SANITIZER) && \
	(defined(__GNUC__) || defined(__clang__))
	#define XRT_CO_NO_ADDRESS_SANITIZER \
		__attribute__((no_sanitize_address))
#else
	#define XRT_CO_NO_ADDRESS_SANITIZER
#endif

#if defined(XRT_CO_THREAD_SANITIZER) && \
	(defined(__GNUC__) || defined(__clang__))
	#define XRT_CO_NO_THREAD_SANITIZER \
		__attribute__((no_sanitize_thread))
#else
	#define XRT_CO_NO_THREAD_SANITIZER
#endif

#if defined(XRT_CO_MEMORY_SANITIZER) && defined(__clang__)
	#define XRT_CO_NO_MEMORY_SANITIZER \
		__attribute__((no_sanitize_memory))
#else
	#define XRT_CO_NO_MEMORY_SANITIZER
#endif

#if defined(XRT_CO_SHADOW_CALL_STACK) && defined(__clang__)
	#define XRT_CO_NO_SHADOW_CALL_STACK \
		__attribute__((no_sanitize("shadow-call-stack")))
#else
	#define XRT_CO_NO_SHADOW_CALL_STACK
#endif



#if defined(XRT_FEATURE_COROUTINE)

/* 非 Windows 后端保存各 ABI 要求的非易失寄存器。 */
#if !defined(_WIN32) && !defined(_WIN64)
typedef struct xrt_co_context {
	ptr Registers[40];
	xrt_co_float_environment FloatEnvironment;
	#if defined(XRT_CO_ADDRESS_SANITIZER) || \
		defined(XRT_CO_MEMORY_SANITIZER)
		const void* SanitizerStackBottom;
		size_t SanitizerStackSize;
	#endif
	#if defined(XRT_CO_ADDRESS_SANITIZER)
		ptr SanitizerFakeStack;
	#endif
	#if defined(XRT_CO_THREAD_SANITIZER)
		ptr ThreadSanitizerFiber;
	#endif
} xrt_co_context;
#endif



/* 每个使用协程的原生线程保存宿主和当前协程上下文。 */
typedef struct xrt_co_runtime {
	xcoro* Current;
	uint64 OwnerThreadId;
	size_t LiveCount;
	#if defined(_WIN32) || defined(_WIN64)
		ptr MainFiber;
		ptr CallerFiber;
		bool Converted;
	#else
		xrt_co_context MainContext;
	#endif
} xrt_co_runtime;



#if defined(XRT_FEATURE_COROUTINE_SCHEDULER)
typedef struct xrt_co_post xrt_co_post;



/* 调度等待类型决定协程让出后进入哪一条队列。 */
typedef enum xrt_co_wait_kind {
	XRT_CO_WAIT_NONE = 0,
	XRT_CO_WAIT_PARK = 1,
	XRT_CO_WAIT_JOIN = 2
} xrt_co_wait_kind;



/* 资源适配器用代际令牌限定一次协程等待的通知边界。 */
typedef struct xrt_co_wait {
	xcoro* Coroutine;
	uint32 Token;
} xrt_co_wait;



/* 调度器仅在所属线程操作执行队列，投递链由独立锁保护。 */
struct xcosched {
	uint64 OwnerThreadId;
	xmutex PostLock;
	xcond PostReady;
	xcoro* ActiveHead;
	xcoro* ActiveTail;
	xcoro* CompleteHead;
	xcoro* CompleteTail;
	xcoro* ReadyHead;
	xcoro* ReadyTail;
	xcoro* PostHead;
	xcoro* PostTail;
	xrt_co_post* WorkHead;
	xrt_co_post* WorkTail;
	xcoro** Timers;
	size_t TimerCount;
	size_t TimerCapacity;
	size_t WorkCount;
	size_t Alive;
	bool Closed;
	bool Running;
};
#endif



/* 协程内部布局只在核心与调度器实现之间共享。 */
struct xcoro {
	volatile int32 State;
	volatile int32 Term;
	uint64 OwnerThreadId;
	xcoroproc Proc;
	ptr Data;
	ptr Result;
	xcorofinalproc Finalize;
	ptr FinalizeData;
	xcancel* Cancel;
	xcocleanup* CleanupTop;
	bool Started;
	bool InCleanup;
	bool InFinalize;
	bool Finalized;
	bool CancelConfirmed;
	xrt_error_context ErrorContext;
	xtemparena Temp;
	xrt_co_runtime* Runtime;
	#if defined(_WIN32) || defined(_WIN64)
		ptr Fiber;
		xrt_co_float_environment FloatEnvironment;
	#else
		ptr StackMap;
		size_t StackMapSize;
		#if defined(XRT_CO_SHADOW_STACK) || \
			defined(XRT_CO_SHADOW_CALL_STACK)
			ptr ShadowStackMap;
			size_t ShadowStackMapSize;
		#endif
		xrt_co_context Context;
	#endif
	#if defined(XRT_FEATURE_COROUTINE_SCHEDULER)
		xcosched* Scheduler;
		struct xcoro* ActivePrevious;
		struct xcoro* ActiveNext;
		struct xcoro* CompletePrevious;
		struct xcoro* CompleteNext;
		struct xcoro* ReadyPrevious;
		struct xcoro* ReadyNext;
		struct xcoro* PostNext;
		struct xcoro* JoinTarget;
		struct xcoro* JoinPrevious;
		struct xcoro* JoinNext;
		struct xcoro* JoinHead;
		struct xcoro* JoinTail;
		xcancelwatch* CancelWatch;
		xdeadline Deadline;
		size_t TimerIndex;
		volatile int32 WakePending;
		volatile int32 PostQueued;
		uint32 WaitTokenNext;
		uint32 WaitTokenArmed;
		uint32 WaitTokenPending;
		xwaitresult WaitResult;
		xrt_co_wait_kind WaitKind;
		bool Detached;
		bool ActiveQueued;
		bool CompleteQueued;
		bool ReadyQueued;
	#endif
};



/* 获取当前线程协程运行时，可按需创建。 */
xrt_co_runtime* __xrtCoRuntimeGet(bool bCreate);



/* 释放当前线程的协程运行时。 */
bool __xrtCoRuntimeDetach(void);



/* 创建平台协程栈和初始上下文。 */
bool __xrtCoBackendCreate(xcoro* pCo, size_t iStackSize);



/* 销毁平台协程栈和上下文。 */
void __xrtCoBackendDestroy(xcoro* pCo);



/* 切换到目标协程。 */
bool __xrtCoBackendResume(xrt_co_runtime* pRuntime, xcoro* pCo);



/* 从当前协程切换回本次恢复它的宿主。 */
void __xrtCoBackendYield(xrt_co_runtime* pRuntime, xcoro* pCo);



#if !defined(_WIN32) && !defined(_WIN64)
/* 保存当前 ABI 的非易失寄存器并恢复目标上下文。 */
void __xrtCoContextSwap(xrt_co_context* pFrom, xrt_co_context* pTo);
#endif



/* 平台后端首次进入协程时调用统一过程包装。 */
void __xrtCoEntry(xcoro* pCo);



/* 完成协程并执行全部清理过程。 */
void __xrtCoFinish(xcoro* pCo, xcoroterm Term);



/* 在所属宿主线程栈上完成尚未进入用户过程的协程。 */
void __xrtCoFinishHost(xcoro* pCo, xcoroterm Term);



/* 释放已经从全部外部所有者和调度结构中脱离的协程。 */
void __xrtCoFree(xcoro* pCo);



#if defined(XRT_FEATURE_COROUTINE_SCHEDULER)
/* 判断当前线程是否为调度器所属线程，不设置错误。 */
bool __xrtCoSchedIsOwner(const xcosched* pSched);



/* 为当前协程开启一次资源等待，后续通知只属于返回的代际。 */
bool __xrtCoWaitOpen(xcoro* pCo, xrt_co_wait* pWait);



/* 线程安全地通知一项仍处于当前代际的资源等待。 */
void __xrtCoWaitWake(ptr pData);



/* 挂起当前资源等待到通知、通用唤醒、取消或截止时间。 */
xwaitresult __xrtCoWaitParkUntil(ptr pData, xdeadline iDeadline);



/* 关闭资源等待并清除仅属于该代际的残留投递。 */
void __xrtCoWaitClose(xrt_co_wait* pWait);



/* 从完成链移除并销毁一个调度器保留的协程。 */
bool __xrtCoSchedDestroyCoroutine(xcoro* pCo);



/* 将取消或外部完成通知投递到协程调度器。 */
bool __xrtCoSchedWake(xcoro* pCo);
#endif

#endif

#endif
