#ifndef XRT_FUTURE_BRIDGE_H
#define XRT_FUTURE_BRIDGE_H

#include <xrt/future.h>



#if defined(XRT_FEATURE_FUTURE_BRIDGE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_FUTURE_BRIDGE requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRT_FEATURE_FUTURE_BRIDGE) && !defined(XRT_FEATURE_THREAD)
	#error "XRT_FEATURE_FUTURE_BRIDGE requires XRT_FEATURE_THREAD"
#endif

#if defined(XRT_FEATURE_FUTURE_BRIDGE) && !defined(XRT_FEATURE_ATOMIC)
	#error "XRT_FEATURE_FUTURE_BRIDGE requires XRT_FEATURE_ATOMIC"
#endif



#if defined(XRT_FEATURE_FUTURE_BRIDGE)

/* Future 桥只保存装配状态、借用的 Promise 与一个取消监听。 */
#define XRT_FUTURE_BRIDGE_STORAGE_SIZE 32u



/* Future 桥的内部状态保持不透明，可直接嵌入异步操作上下文。 */
typedef union xfuturebridge {
	uint64 Alignment;
	uint8 Storage[XRT_FUTURE_BRIDGE_STORAGE_SIZE];
} xfuturebridge;



XRT_EXTERN_C_BEGIN



/* 使用一个已有 Promise 初始化桥；Promise 的所有权仍由调用方持有。 */
XRT_API bool xrtFutureBridgeInit(
	xfuturebridge* pBridge,
	xpromise* pPromise
);



/* 创建 Future/Promise 对并初始化桥；返回的 Future 由调用方持有。 */
XRT_API xfuture* xrtFutureBridgeCreate(
	xfuturebridge* pBridge,
	xcancel* pParent
);



/* 返回桥借用的 Promise；调用方负责按原有所有权契约销毁它。 */
XRT_API xpromise* xrtFutureBridgePromise(
	const xfuturebridge* pBridge
);



/* 把 Future 的协作取消转发给底层异步操作。 */
XRT_API bool xrtFutureBridgeWatch(
	xfuturebridge* pBridge,
	xcancelproc pCancelProc,
	ptr pCancelData
);



/* 发布装配成功，允许底层完成回调向 Promise 写入终态。 */
XRT_API bool xrtFutureBridgeReady(xfuturebridge* pBridge);



/* 发布装配失败，要求底层完成回调只回收结果。 */
XRT_API bool xrtFutureBridgeFail(xfuturebridge* pBridge);



/* 等待极短的装配窗口，并返回底层结果能否写入 Promise。 */
XRT_API bool xrtFutureBridgeWait(const xfuturebridge* pBridge);



/* 注销取消监听，并与正在执行的取消回调汇合。 */
XRT_API void xrtFutureBridgeUnwatch(xfuturebridge* pBridge);



XRT_EXTERN_C_END

#endif

#endif
