#ifndef XRT_INTERNAL_FUTURE_BRIDGE_H
#define XRT_INTERNAL_FUTURE_BRIDGE_H

#include <xrt/atomic.h>
#include <xrt/future_bridge.h>



#if defined(XRT_FEATURE_FUTURE_BRIDGE)

/* 装配状态只在创建线程与唯一完成回调之间发布。 */
typedef enum xrt_future_bridge_setup {
	XRT_FUTURE_BRIDGE_INSTALLING = 0,
	XRT_FUTURE_BRIDGE_READY,
	XRT_FUTURE_BRIDGE_FAILED
} xrt_future_bridge_setup;



#define XRT_FUTURE_BRIDGE_MAGIC 0x46544252u



/* 公开对象的实际布局集中在这里，避免调用方依赖生命周期字段。 */
typedef struct xrt_future_bridge_impl {
	xatomic32 Setup;
	xpromise* Promise;
	xcancelwatch* Watch;
	uint32 Magic;
} xrt_future_bridge_impl;



typedef char xrt_future_bridge_storage_check[
	(sizeof(xrt_future_bridge_impl) <= XRT_FUTURE_BRIDGE_STORAGE_SIZE) ? 1 : -1
];



/* 获取可写内部布局。 */
static inline xrt_future_bridge_impl* __xrtFutureBridgeImpl(
	xfuturebridge* pBridge
)
{
	return (xrt_future_bridge_impl*)pBridge;
}



/* 获取只读内部布局。 */
static inline const xrt_future_bridge_impl* __xrtFutureBridgeConstImpl(
	const xfuturebridge* pBridge
)
{
	return (const xrt_future_bridge_impl*)pBridge;
}

#endif

#endif
