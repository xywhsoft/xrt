#ifndef XRT_THREAD_H
#define XRT_THREAD_H

#include <xrt/core.h>
#include <xrt/wait.h>



#if defined(XRT_FEATURE_ONCE)

/* Once 使用固定存储并允许静态零初始化。 */
#define XRT_ONCE_STORAGE_SIZE 16u



/* Once 对象的字段保持不透明，只能使用 XRT_ONCE_INIT 或全零初始化。 */
typedef union xonce {
	uint64 Alignment;
	uint8 Storage[XRT_ONCE_STORAGE_SIZE];
} xonce;



/* 初始化过程返回 true 时永久完成，返回 false 时允许后续调用重试。 */
typedef bool (*xonceproc)(ptr pData);



/* 静态 Once 对象初始化值。 */
#define XRT_ONCE_INIT { 0 }

#endif



#if defined(XRT_FEATURE_THREAD_KEY)

/* 动态键按原生线程隔离，同一线程上的 Fiber 和协程共享其值。 */
typedef struct xthreadkey xthreadkey;



/* 非空线程局部值在线程退出、显式清理或被替换时交给析构过程。 */
typedef void (*xthreadkeyproc)(ptr pValue);

#endif



#if defined(XRT_FEATURE_THREAD) && !defined(XRT_FEATURE_WAIT)
	#error "XRT_FEATURE_THREAD requires XRT_FEATURE_WAIT"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_ONCE)
/* 并发执行一次初始化；成功后所有调用返回 true，失败允许后续调用重试。 */
XRT_API bool xrtOnce(xonce* pOnce, xonceproc pProc, ptr pData);
#endif



#if defined(XRT_FEATURE_THREAD_KEY)
/* 创建动态原生线程局部键；析构过程可以为空。 */
XRT_API xthreadkey* xrtThreadKeyCreate(xthreadkeyproc pDestroy);



/* 关闭键；其他线程不得再主动访问，已有线程槽会在退出时延迟释放。 */
XRT_API bool xrtThreadKeyDestroy(xthreadkey* pKey);



/* 返回当前线程的借用值；当前线程尚未设置时返回空指针。 */
XRT_API ptr xrtThreadKeyGet(const xthreadkey* pKey);



/* 转移新值的所有权；成功替换后析构旧值，空值等价于清除。 */
XRT_API bool xrtThreadKeySet(xthreadkey* pKey, ptr pValue);



/* 取走当前线程的值但不执行析构；返回值所有权交给调用方。 */
XRT_API ptr xrtThreadKeyTake(xthreadkey* pKey);



/* 析构并清除当前原生线程的全部动态键值；XRT 创建的线程退出时自动调用。 */
XRT_API bool xrtThreadKeysClear(void);
#endif



XRT_EXTERN_C_END



#if defined(XRT_FEATURE_THREAD)

/* 原生线程对象对外保持不透明，并使用引用计数管理生命周期。 */
typedef struct xthread xthread;



/* 线程入口返回稳定的 32 位退出码。 */
typedef int32 (*xthreadproc)(ptr pData);



/* 线程只有运行和完成两种可观测状态，停止请求不伪装成执行状态。 */
typedef enum xthreadstate {
	XTHREAD_RUNNING = 0,
	XTHREAD_FINISHED = 1
} xthreadstate;



XRT_EXTERN_C_BEGIN



/* 创建并立即启动线程；栈大小为零时使用平台默认值。 */
XRT_API xthread* xrtThreadCreate(xthreadproc pProc, ptr pData, size_t iStackSize);



/* 增加线程对象引用并返回原指针。 */
XRT_API xthread* xrtThreadRef(xthread* pThread);



/* 释放线程对象引用；运行线程自持引用，因此允许用它安全分离线程。 */
XRT_API void xrtThreadDestroy(xthread* pThread);



/* 等待线程执行体和 XRT 线程上下文清理完成，允许多个线程同时等待同一个对象。 */
XRT_API xwaitresult xrtThreadWait(xthread* pThread);



/* 在相对微秒数内等待线程执行体和 XRT 线程上下文清理完成。 */
XRT_API xwaitresult xrtThreadWaitFor(xthread* pThread, uint64 iTimeout);



/* 等待线程执行体和 XRT 线程上下文清理完成到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtThreadWaitUntil(xthread* pThread, xdeadline iDeadline);



/* 幂等地请求线程协作停止，不会强制终止执行。 */
XRT_API bool xrtThreadStop(xthread* pThread);



/* 判断指定线程是否收到停止请求。 */
XRT_API bool xrtThreadStopRequested(const xthread* pThread);



/* 判断当前 XRT 线程是否收到停止请求。 */
XRT_API bool xrtThreadStopping(void);



/* 返回线程状态快照。 */
XRT_API xthreadstate xrtThreadState(const xthread* pThread);



/* 返回完成线程的退出码；线程仍运行或参数无效时返回零并设置错误。 */
XRT_API int32 xrtThreadExitCode(const xthread* pThread);



/* 返回线程对象在进程内稳定的非零平台标识；线程结束后该标识可能被平台复用。 */
XRT_API uint64 xrtThreadId(const xthread* pThread);



/* 返回当前线程在进程内稳定的非零平台标识，外部创建的线程同样可用。 */
XRT_API uint64 xrtThreadCurrentId(void);



/* 返回当前 XRT 创建线程的借用对象，外部线程返回空指针。 */
XRT_API xthread* xrtThreadCurrent(void);



/* 主动让出当前线程的处理器时间片。 */
XRT_API void xrtThreadYield(void);



XRT_EXTERN_C_END

#endif

#endif
