#ifndef XMAIL_TEST_TLS_H
#define XMAIL_TEST_TLS_H

#include "../../../tests/fixtures/tls_server.h"



typedef struct testmailtlsupgrade {
	xnetstream* Tcp;
	const xtlsserverconfig* Server;
	const xtlsstreamconfig* Stream;
	xtlsstream* Tls;
	xpromise* Promise;
} testmailtlsupgrade;



/* 等待 TLS Future 在截止时间内成功完成。 */
static inline bool testMailTlsFuture(
	xfuture* pFuture,
	xdeadline iDeadline
)
{
	return (pFuture != NULL) &&
		(xrtFutureWaitUntil(pFuture, iDeadline) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 在 TCP Stream 所属 Worker 上附加服务端 TLS session。 */
static void testMailTlsUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	testmailtlsupgrade* pUpgrade = (testmailtlsupgrade*)pData;
	xtlssession* pSession;

	(void)pWorker;
	pSession = xrtTlsServerCreate(pUpgrade->Server, NULL);
	if ( (pSession != NULL) && xrtTlsStreamAttach(
		pUpgrade->Tcp,
		pSession,
		pUpgrade->Stream,
		NULL,
		NULL,
		&pUpgrade->Tls
	) ) {
		(void)xrtPromiseResolve(pUpgrade->Promise, NULL);
	} else if ( xrtGetError() != NULL ) {
		xrtTlsSessionDestroy(pSession);
		(void)xrtPromiseReject(pUpgrade->Promise, xrtGetError());
	} else {
		xrtTlsSessionDestroy(pSession);
		(void)xrtPromiseClose(pUpgrade->Promise);
	}
	xrtPromiseDestroy(pUpgrade->Promise);
}



/* 把公开的 TCP Stream 原位升级为服务端 TLS。 */
static inline xtlsstream* testMailTlsUpgrade(
	xnetstream** ppTcp,
	const xtlsserverconfig* pServer,
	xdeadline iDeadline
)
{
	testmailtlsupgrade Upgrade;
	xtlsstreamconfig Stream;
	xnetstream* pTcp = ppTcp != NULL ? *ppTcp : NULL;
	xnetworker* pWorker = xrtNetStreamWorker(pTcp);
	xfuture* pFuture;
	xfuture* pOpen;

	if ( pWorker == NULL ) {
		return NULL;
	}
	xrtTlsStreamConfigInit(&Stream);
	memset(&Upgrade, 0, sizeof(Upgrade));
	Upgrade.Tcp = pTcp;
	Upgrade.Server = pServer;
	Upgrade.Stream = &Stream;
	Upgrade.Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( Upgrade.Promise == NULL ) {
		return NULL;
	}
	if ( !xrtNetEnginePost(
		xrtNetWorkerEngine(pWorker),
		xrtNetWorkerIndex(pWorker),
		testMailTlsUpgradeTask,
		&Upgrade
	) ) {
		xrtPromiseDestroy(Upgrade.Promise);
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	if ( (xrtFutureWaitUntil(pFuture, iDeadline) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ||
		(Upgrade.Tls == NULL) ) {
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	xrtFutureDestroy(pFuture);
	*ppTcp = NULL;
	pOpen = xrtTlsStreamWaitAsync(Upgrade.Tls, XTLS_STREAM_WAIT_OPEN);
	if ( !testMailTlsFuture(pOpen, iDeadline) ) {
		xrtFutureDestroy(pOpen);
		(void)xrtTlsStreamAbort(Upgrade.Tls);
		xrtTlsStreamDestroy(Upgrade.Tls);
		return NULL;
	}
	xrtFutureDestroy(pOpen);
	return Upgrade.Tls;
}



/* 完整发送一段明文协议数据。 */
static inline bool testMailTcpSend(
	xnetstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, iSize);

		if ( Result == XNET_RESULT_OK ) {
			return true;
		}
		if ( (Result != XNET_RESULT_AGAIN) || !xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_WRITE,
			iDeadline,
			NULL
		) ) {
			return false;
		}
	}
}



/* 精确接收并比较一段明文协议数据。 */
static inline bool testMailTcpReceive(
	xnetstream* pStream,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	size_t iReceived = 0;

	while ( iReceived < iExpected ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream,
			iExpected - iReceived,
			iDeadline,
			NULL
		);
		xbytesview Bytes;

		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0) ||
			(memcmp(Bytes.Data, sExpected + iReceived, Bytes.Size) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}



/* 完整发送一段 TLS 协议数据。 */
static inline bool testMailTlsSend(
	xtlsstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	xfuture* pFuture = xrtTlsStreamSendAsync(pStream, sText, iSize);
	bool bSuccess = testMailTlsFuture(pFuture, iDeadline);

	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 精确接收并比较一段 TLS 协议数据。 */
static inline bool testMailTlsReceive(
	xtlsstream* pStream,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	size_t iReceived = 0;

	while ( iReceived < iExpected ) {
		xfuture* pFuture = xrtTlsStreamRecvAsync(
			pStream,
			iExpected - iReceived
		);
		xnetbytes* pBytes;
		xbytesview Bytes;

		if ( !testMailTlsFuture(pFuture, iDeadline) ) {
			xrtFutureDestroy(pFuture);
			return false;
		}
		pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
		xrtFutureDestroy(pFuture);
		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0) ||
			(memcmp(Bytes.Data, sExpected + iReceived, Bytes.Size) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}

#endif
