#ifndef XRT_SIGNAL_H
#define XRT_SIGNAL_H

#include <xrt/core.h>
#include <xrt/time.h>



#if defined(XRT_FEATURE_SIGNAL) && \
	(!defined(XRT_FEATURE_ATOMIC) || \
	 !defined(XRT_FEATURE_ONCE) || \
	 !defined(XRT_FEATURE_COND) || \
	 !defined(XRT_FEATURE_THREAD))
	#error "XRT_FEATURE_SIGNAL requires atomic, once, cond and thread"
#endif



#if defined(XRT_FEATURE_SIGNAL)

/* XRT 信号代码跨平台稳定；并非每个平台都支持全部代码。 */
typedef enum xsignal {
	XSIGNAL_NONE = 0,
	XSIGNAL_HUP = 1,
	XSIGNAL_INT = 2,
	XSIGNAL_TERM = 15,
	XSIGNAL_BREAK = 1001,
	XSIGNAL_CLOSE = 1002,
	XSIGNAL_LOGOFF = 1003,
	XSIGNAL_SHUTDOWN = 1004
} xsignal;



/* 信号错误代码在 xrt.signal 错误域内稳定。 */
typedef enum xsignalerror {
	XSIGNAL_ERROR_CODE = 1,
	XSIGNAL_ERROR_UNSUPPORTED,
	XSIGNAL_ERROR_SYSTEM,
	XSIGNAL_ERROR_STATE
} xsignalerror;



/* 一次调度可以合并多个同类原生通知，Count 是本批数量，Total 是清零后的累计数量。 */
typedef struct xsignalevent {
	xsignal Code;
	int32 SystemCode;
	uint32 Count;
	uint64 Total;
	xtime Time;
	cstr Name;
} xsignalevent;



/* 信号监听句柄由 XRT 引用计数管理，对外保持不透明。 */
typedef struct xsignalwatch xsignalwatch;



/* 用户回调始终在 XRT 信号调度线程执行，不在原生信号处理上下文执行。 */
typedef void (*xsignalproc)(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
);



/* Owned 监听句柄最终释放时执行数据析构器。 */
typedef void (*xsignalfreeproc)(ptr pData);



XRT_EXTERN_C_BEGIN



/* 判断当前平台是否支持指定信号代码。 */
XRT_API bool xrtSignalSupported(xsignal Code);



/* 返回稳定信号名称；未知代码返回 "UNKNOWN"。 */
XRT_API cstr xrtSignalName(xsignal Code);



/* 订阅信号；成功后调用方拥有返回句柄，回调可重复执行。 */
XRT_API xsignalwatch* xrtSignalOn(
	xsignal Code,
	xsignalproc pProc,
	ptr pData
);



/* 订阅信号并在句柄最终释放时析构用户数据；失败时数据所有权不转移。 */
XRT_API xsignalwatch* xrtSignalOnOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
);



/* 订阅一次信号；第一次入选调度后先注销，再执行用户回调。 */
XRT_API xsignalwatch* xrtSignalOnce(
	xsignal Code,
	xsignalproc pProc,
	ptr pData
);



/* 订阅一次信号并接管用户数据；失败时数据所有权不转移。 */
XRT_API xsignalwatch* xrtSignalOnceOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
);



/* 增加监听句柄引用并返回原指针。 */
XRT_API xsignalwatch* xrtSignalRef(xsignalwatch* pWatch);



/* 幂等注销监听；从其他线程调用时，返回前保证该句柄回调已经结束。 */
XRT_API bool xrtSignalOff(xsignalwatch* pWatch);



/* 注销监听并释放一个调用方引用；空指针可安全传入。 */
XRT_API void xrtSignalFree(xsignalwatch* pWatch);



/* 判断监听是否仍会进入新的回调。 */
XRT_API bool xrtSignalActive(const xsignalwatch* pWatch);



/* 判断调度后端是否健康；故障时重建前必须先调用 xrtSignalShutdown。 */
XRT_API bool xrtSignalHealthy(void);



/* 返回监听对应的信号代码；空指针返回 XSIGNAL_NONE。 */
XRT_API xsignal xrtSignalCode(const xsignalwatch* pWatch);



/* 忽略指定信号并注销该代码的全部 XRT 监听。 */
XRT_API bool xrtSignalIgnore(xsignal Code);



/* 注销指定代码的全部监听，并恢复 XRT 接管前的原生处理方式。 */
XRT_API bool xrtSignalRestore(xsignal Code);



/* 注销全部监听并恢复全部由 XRT 接管的原生处理方式。 */
XRT_API bool xrtSignalRestoreAll(void);



/* 向当前进程发送原生信号；默认处理方式可能终止进程。 */
XRT_API bool xrtSignalRaise(xsignal Code);



/* 返回指定信号的累计接收数；XSIGNAL_NONE 返回全部信号之和。 */
XRT_API uint64 xrtSignalCount(xsignal Code);



/* 判断指定信号自上次清零后是否至少接收过一次。 */
XRT_API bool xrtSignalReceived(xsignal Code);



/* 清零指定信号的累计数与尚未调度数量；XSIGNAL_NONE 清零全部。 */
XRT_API bool xrtSignalClear(xsignal Code);



/* 停止调度线程、注销全部监听并恢复原生处理方式；回调线程内不可调用。 */
XRT_API bool xrtSignalShutdown(void);



XRT_EXTERN_C_END

#endif

#endif
