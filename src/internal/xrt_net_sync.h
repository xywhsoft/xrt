#ifndef XRT_INTERNAL_NET_SYNC_H
#define XRT_INTERNAL_NET_SYNC_H

#include <xrt/future.h>
#include <xrt/net.h>



#if defined(XRT_FEATURE_NET_SYNC)

/*
	等待一个网络 Future，并把超时、取消、关闭和失败映射到当前错误上下文。
	成功结果只在 Future 引用仍然存活时借用。
*/
bool __xrtNetSyncWait(
	xfuture* pFuture,
	const xnetworker* pWorker,
	xdeadline iDeadline,
	xcancel* pCancel,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	xfutureresult* pResult
);

#endif

#endif
