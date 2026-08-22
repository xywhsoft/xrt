#ifndef XRT_BENCH_NETWORK_COMMON_H
#define XRT_BENCH_NETWORK_COMMON_H



/* 输出当前线程最近一次结构化错误，供协议栈 benchmark 共用。 */
static void xbenchPrintCurrentError(void)
{
	const xerror* pError = xrtGetError();
	cstr sDomain;
	cstr sOperation;
	cstr sMessage;

	if ( pError == NULL ) {
		return;
	}
	sDomain = xrtErrorDomain(pError);
	sOperation = xrtErrorOperation(pError);
	sMessage = xrtErrorMessage(pError);
	fprintf(
		stderr,
		": kind=%d domain=%s code=%" PRId32
		" system=%" PRId32 " operation=%s message=%s",
		(int)xrtErrorKind(pError),
		(sDomain != NULL) ? sDomain : "",
		xrtErrorCode(pError),
		xrtErrorSystemCode(pError),
		(sOperation != NULL) ? sOperation : "",
		(sMessage != NULL) ? sMessage : ""
	);
}



/* 输出 Engine 第一个 Worker 的实际后端事实。 */
static bool xbenchPrintNetworkBackend(xnetengine* pEngine)
{
	xnetworker* pWorker;
	xnetport* pPort;
	cstr sBackend;

	if ( pEngine == NULL ) {
		return false;
	}
	pWorker = xrtNetEngineWorker(pEngine, 0);
	if ( pWorker == NULL ) {
		return false;
	}
	pPort = xrtNetWorkerPort(pWorker);
	if ( pPort == NULL ) {
		return false;
	}
	sBackend = xrtNetPortName(pPort);
	if ( sBackend == NULL ) {
		return false;
	}
	printf("network_backend=%s\n", sBackend);
	return true;
}



#if defined(XRT_BENCH_NETWORK_DESTROY_HELPERS)

/* 关闭并释放 Stream；异常路径使用 Abort 保证基准不遗留对象。 */
static bool xbenchNetworkStreamDestroy(xnetstream* pStream, bool bAbort)
{
	bool bResult = true;

	if ( pStream == NULL ) {
		return true;
	}
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		if ( bAbort ) {
			bResult = xrtNetStreamAbort(pStream);
		} else {
			bResult = xrtNetStreamClose(pStream);
		}
		if (
			bResult &&
			!xrtNetStreamWait(
				pStream,
				XNET_STREAM_WAIT_CLOSE,
				xrtDeadlineAfter(UINT64_C(5000000)),
				NULL
			)
		) {
			bResult = false;
		}
	}
	xrtNetStreamDestroy(pStream);
	return bResult;
}



/* 关闭 Listener 并等待全部预投递 Accept 退出。 */
static bool xbenchNetworkListenerDestroy(xnetlistener* pListener)
{
	xdeadline iDeadline;
	bool bResult = true;

	if ( pListener == NULL ) {
		return true;
	}
	if ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		bResult = xrtNetListenerClose(pListener);
		iDeadline = xrtDeadlineAfter(UINT64_C(5000000));
		while (
			bResult &&
			(xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED)
		) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				bResult = false;
				break;
			}
			xrtThreadYield();
		}
	}
	xrtNetListenerDestroy(pListener);
	return bResult;
}

#endif



#endif
