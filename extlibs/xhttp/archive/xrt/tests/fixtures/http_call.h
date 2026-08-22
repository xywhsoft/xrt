#ifndef XRT_TEST_HTTP_CALL_H
#define XRT_TEST_HTTP_CALL_H

#include "../../src/internal/xrt_http_client_runtime.h"



/*
	初始化栈上模拟 Client 的组合功能同步状态。
	调用方应先清零 Client 并填写测试需要的业务字段。
*/
static inline void testHttpClientStateInit(xhttpclient* pClient)
{
	xrtAtomic32Init(&pClient->State, XHTTP_CLIENT_RUNNING);
	__xrtSpinInit(&pClient->LifecycleLock);
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		testRequire(
			__xrtHttpPoolInit(pClient),
			"HTTP test Client pool initialization failed"
		);
	#endif
}



/* 释放栈上模拟 Client 的组合功能同步状态。 */
static inline void testHttpClientStateUnit(xhttpclient* pClient)
{
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolUnit(pClient);
	#endif
	__xrtSpinUnit(&pClient->LifecycleLock);
}



/*
	初始化栈上模拟 Call 的终态发布字段。
	调用方先清零并填写业务字段。
*/
static inline void testHttpCallStateInit(
	xhttpcall* pCall,
	xhttpcallproc pDone,
	ptr pData
)
{
	xrtAtomic32Init(&pCall->State, XHTTP_CALL_QUEUED);
	xrtAtomic32Init(&pCall->CancelGate, 0);
	xrtAtomic32Init(&pCall->FinishGate, 0);
	xrtAtomic32Init(&pCall->TimeoutCause, 0);
	xrtAtomic32Init(&pCall->TotalTimerDone, 0);
	xrtAtomic32Init(&pCall->IdleTimerDone, 0);
	xrtAtomic64Init(&pCall->TotalTimer, 0);
	xrtAtomic64Init(&pCall->IdleTimer, 0);
	xrtAtomic64Init(&pCall->IdleDeadline, 0);
	xrtAtomic32Init(
		&pCall->Info.Phase,
		XHTTP_CALL_PHASE_RESPONSE_HEADERS
	);
	xrtAtomic32Init(
		&pCall->Info.Result,
		(uint32)(int32)XNET_RESULT_AGAIN
	);
	xrtAtomic32Init(
		&pCall->Info.Error,
		XHTTP_CLIENT_ERROR_NONE
	);
	xrtAtomic32Init(&pCall->Info.Reused, 0);
	xrtAtomic32Init(&pCall->Info.Secure, 0);
	xrtAtomic64Init(&pCall->Info.Submitted, xrtClock());
	xrtAtomic64Init(&pCall->Info.Started, 0);
	xrtAtomic64Init(&pCall->Info.TransportReady, 0);
	xrtAtomic64Init(&pCall->Info.RequestSent, 0);
	xrtAtomic64Init(&pCall->Info.FirstByte, 0);
	xrtAtomic64Init(&pCall->Info.Headers, 0);
	xrtAtomic64Init(&pCall->Info.LastProgress, 0);
	xrtAtomic64Init(&pCall->Info.Completed, 0);
	xrtAtomic64Init(&pCall->Info.RequestWireBytes, 0);
	xrtAtomic64Init(&pCall->Info.ResponseWireBytes, 0);
	xrtAtomic64Init(&pCall->Info.ResponseBodyBytes, 0);
	xrtAtomic64Init(&pCall->Info.Redirects, 0);
	__xrtSpinInit(&pCall->Lock);
	pCall->Done = pDone;
	pCall->Data = pData;
}



/*
	释放栈上模拟 Call 的同步原语。
*/
static inline void testHttpCallStateUnit(xhttpcall* pCall)
{
	__xrtSpinUnit(&pCall->Lock);
}



#endif
