#ifndef XHTTP_TEST_HTTP_CALL_H
#define XHTTP_TEST_HTTP_CALL_H

#include "../../src/internal/xrt_http_client_runtime.h"



/* 初始化栈上模拟客户端的同步状态，业务字段由测试用例填写。 */
static inline void testHttpClientStateInit(xhttpclient* pClient)
{
	xrtAtomic32Init(&pClient->State, XHTTP_CLIENT_RUNNING);
	testRequire(
		xrtSpinInit(&pClient->LifecycleLock),
		"HTTP test Client lifecycle lock initialization failed"
	);

	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		testRequire(
			__xrtHttpPoolInit(pClient),
			"HTTP test Client pool initialization failed"
		);
	#endif
}



/* 释放栈上模拟客户端的同步状态。 */
static inline void testHttpClientStateUnit(xhttpclient* pClient)
{
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolUnit(pClient);
	#endif

	testRequire(
		xrtSpinUnit(&pClient->LifecycleLock),
		"HTTP test Client lifecycle lock cleanup failed"
	);
}



/* 初始化栈上模拟调用的终态发布字段，业务字段由测试用例填写。 */
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
	xrtAtomic32Init(&pCall->Info.Error, XHTTP_CLIENT_ERROR_NONE);
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
	testRequire(
		xrtSpinInit(&pCall->Lock),
		"HTTP test Call lock initialization failed"
	);
	pCall->Done = pDone;
	pCall->Data = pData;
}



/* 释放栈上模拟调用的同步状态。 */
static inline void testHttpCallStateUnit(xhttpcall* pCall)
{
	testRequire(
		xrtSpinUnit(&pCall->Lock),
		"HTTP test Call lock cleanup failed"
	);
}



#endif
