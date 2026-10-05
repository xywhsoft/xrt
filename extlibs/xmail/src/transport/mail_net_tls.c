#include <xrt/detail/wait.h>
#include "../internal/xrt_mail_net.h"



#if defined(XMAIL_FEATURE_MAIL_NET_TLS)

/* 排队期间由 Worker 持有；接管开始后调用线程必须等到交接完成。 */
typedef struct __xmailtlsupgrade {
	xatomic32 Gate; /* 0: queued, 1: running, 2: cancelled before start */
	xnetstream* Tcp;
	const xtlsclientconfig* Client;
	const xtlsstreamconfig* Stream;
	xtlsstream* Tls;
	xpromise* Promise;
} __xmailtlsupgrade;

/* 判断拨号主机是否是无需 SNI 的数字 IP。 */
static bool __xrtMailNetTlsHostIsIp(cstr sHost)
{
	xnetaddr Address;
	xerror* pSaved = xrtTakeError();
	bool bIp = xrtNetAddrParse(&Address, sHost, 0);
	xerror* pProbe = xrtTakeError();

	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
	} else {
		xrtClearError();
	}
	xrtErrorFree(pSaved);
	xrtErrorFree(pProbe);
	return bIp;
}



/* 等待 TLS Future；只有关闭时的接收允许认证 EOF 的 CLOSED 终态。 */
static bool __xrtMailNetTlsFutureResult(
	xfuture* pFuture,
	double iDeadline,
	xcancel* pCancel,
	bool bAllowClosed
)
{
	xwaitresult Wait = __xrtFutureWaitUntilCancel(
		pFuture,
		iDeadline,
		pCancel
	);
	xfuturestate State;

	if ( Wait != XWAIT_OK ) {
		(void)xrtFutureCancel(pFuture);
		if ( Wait == XWAIT_TIMEOUT ) {
			__xrtMailError(
				XERR_TIMEOUT,
				XMAIL_ERROR_PROTOCOL,
				"mail TLS operation timed out"
			);
		} else if ( Wait == XWAIT_CANCELLED ) {
			__xrtMailError(
				XERR_CANCELLED,
				XMAIL_ERROR_PROTOCOL,
				"mail TLS operation was cancelled"
			);
		}
		return false;
	}
	State = xrtFutureState(pFuture);
	if ( (State == XFUTURE_RESOLVED) ||
		(bAllowClosed && (State == XFUTURE_CLOSED)) ) {
		return true;
	}
	if ( State == XFUTURE_FAILED ) {
		xrtSetError(xrtFutureError(pFuture));
	} else if ( State == XFUTURE_CANCELLED ) {
		__xrtMailError(
			XERR_CANCELLED,
			XMAIL_ERROR_PROTOCOL,
			"mail TLS operation was cancelled"
		);
	} else {
		__xrtMailError(
			XERR_CLOSED,
			XMAIL_ERROR_PROTOCOL,
			"mail TLS operation closed without a result"
		);
	}
	return false;
}



/* 一般 TLS 操作必须交付成功结果，不能把 EOF 当作成功。 */
static bool __xrtMailNetTlsFuture(
	xfuture* pFuture,
	double iDeadline,
	xcancel* pCancel
)
{
	return __xrtMailNetTlsFutureResult(
		pFuture,
		iDeadline,
		pCancel,
		false
	);
}



/* 为 TLS 拨号补齐默认验证名称，并避免向数字 IP 发送 SNI。 */
static void __xrtMailNetTlsClient(
	const xmailnetconfig* pConfig,
	xtlsclientconfig* pTls
)
{
	xstrview Host;

	*pTls = pConfig->Tls;
	Host.Data = pConfig->Host;
	Host.Size = strlen(pConfig->Host);
	if ( pTls->VerifyName.Size == 0 ) {
		pTls->VerifyName = Host;
	}
	if ( (pTls->ServerName.Size == 0) &&
		!__xrtMailNetTlsHostIsIp(pConfig->Host) ) {
		pTls->ServerName = Host;
	}
}



/* 在 TCP 所属 Worker 上原子接管传输引用。 */
static void __xrtMailNetTlsUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	__xmailtlsupgrade* pUpgrade = (__xmailtlsupgrade*)pData;
	xpromise* pPromise = pUpgrade->Promise;
	uint32 iExpected = 0;

	(void)pWorker;
	if ( !xrtAtomic32CompareExchange(
		&pUpgrade->Gate, &iExpected, 1,
		XMEMORY_ACQ_REL, XMEMORY_ACQUIRE
	) ) {
		/* 调用方已返回；不得再触碰其 TCP 或借用的 TLS 配置。 */
		xrtPromiseDestroy(pPromise);
		xrtFree(pUpgrade);
		return;
	}
	if ( xrtTlsStreamClient(
		pUpgrade->Tcp,
		pUpgrade->Client,
		pUpgrade->Stream,
		NULL,
		NULL,
		&pUpgrade->Tls
	) ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( xrtGetError() != NULL ) {
		(void)xrtPromiseReject(pPromise, xrtGetError());
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	/* Resolve/Reject 可立即唤醒调用方；此后不再访问 pUpgrade。 */
	xrtPromiseDestroy(pPromise);
}



/* 完成隐式 TLS 拨号。 */
bool __xrtMailTransportTlsOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
	xtlsclientconfig Tls;
	xtlsdialconfig Dial;
	xfuture* pFuture;

	__xrtMailNetTlsClient(pConfig, &Tls);
	xrtTlsDialConfigInit(&Dial);
	Dial.Transport = pConfig->Dial;
	Dial.Stream = pConfig->TlsStream;
	Dial.Timeout = pConfig->TlsTimeout;
	Dial.ServerNameFromHost = false;
	pFuture = xrtTlsDialAsync(
		pConfig->Engine,
		pConfig->Resolver,
		pConfig->Host,
		pConfig->Port,
		&Tls,
		&Dial,
		NULL,
		NULL
	);
	if ( pFuture == NULL ) {
		return false;
	}
	if ( !__xrtMailNetTlsFuture(pFuture, iDeadline, pCancel) ) {
		xrtFutureDestroy(pFuture);
		return false;
	}
	pTransport->Tls = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
	xrtFutureDestroy(pFuture);
	return pTransport->Tls != NULL;
}



/* 把已经完成协议协商的明文 TCP Stream 接管为 TLS Stream。 */
bool __xrtMailTransportStartTls(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
	__xmailtlsupgrade* pUpgrade;
	xtlsclientconfig Client;
	xnetworker* pWorker;
	xfuture* pFuture;
	xfuture* pOpen;
	xwaitresult Wait;
	uint32 iExpected;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMailNetConfigValid(pConfig) ||
		(pConfig->Security != XMAIL_SECURITY_STARTTLS) ||
		(pTransport->Tcp == NULL) || (pTransport->Tls != NULL) ||
		(pTransport->PendingConsumed > pTransport->PendingSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pTransport->PendingSize - pTransport->PendingConsumed) != 0 ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"STARTTLS response left unconsumed plaintext"
		);
		return false;
	}
	if ( __xrtWaitExpired(iDeadline) || xrtCancelRequested(pCancel) ) {
		__xrtMailError(
			__xrtWaitExpired(iDeadline) ? XERR_TIMEOUT : XERR_CANCELLED,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS was not started"
		);
		return false;
	}
	pWorker = xrtNetStreamWorker(pTransport->Tcp);
	if ( (pWorker == NULL) || xrtNetWorkerIsCurrent(pWorker) ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS cannot block its transport worker"
		);
		return false;
	}
	__xrtMailNetTlsClient(pConfig, &Client);
	pUpgrade = (__xmailtlsupgrade*)xrtCalloc(1, sizeof(*pUpgrade));
	if ( pUpgrade == NULL ) {
		return false;
	}
	xrtAtomic32Init(&pUpgrade->Gate, 0);
	pUpgrade->Tcp = pTransport->Tcp;
	pUpgrade->Client = &Client;
	pUpgrade->Stream = &pConfig->TlsStream;
	pUpgrade->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pUpgrade->Promise == NULL ) {
		xrtFree(pUpgrade);
		return false;
	}
	if ( !xrtNetEnginePost(
		xrtNetWorkerEngine(pWorker),
		xrtNetWorkerIndex(pWorker),
		__xrtMailNetTlsUpgradeTask,
		pUpgrade
	) ) {
		xrtPromiseDestroy(pUpgrade->Promise);
		xrtFutureDestroy(pFuture);
		xrtFree(pUpgrade);
		return false;
	}
	Wait = __xrtFutureWaitUntilCancel(pFuture, iDeadline, pCancel);
	if ( Wait != XWAIT_OK ) {
		iExpected = 0;
		if ( xrtAtomic32CompareExchange(
			&pUpgrade->Gate, &iExpected, 2,
			XMEMORY_ACQ_REL, XMEMORY_ACQUIRE
		) ) {
			/* Worker 尚未开始接管，TCP 仍由调用方持有。 */
			xrtFutureDestroy(pFuture);
			__xrtMailError(
				Wait == XWAIT_TIMEOUT ? XERR_TIMEOUT :
					Wait == XWAIT_CANCELLED ? XERR_CANCELLED : XERR_IO,
				XMAIL_ERROR_PROTOCOL,
				"mail STARTTLS worker handoff interrupted"
			);
			return false;
		}
		/* Worker 已开始：先收回所有权，避免 TLS/TCP 引用悬空。 */
		(void)xrtFutureWait(pFuture);
		if ( pUpgrade->Tls != NULL ) {
			pTransport->Tcp = NULL;
			(void)xrtTlsStreamAbort(pUpgrade->Tls);
			xrtTlsStreamDestroy(pUpgrade->Tls);
		}
		xrtFutureDestroy(pFuture);
		xrtFree(pUpgrade);
		__xrtMailError(
			Wait == XWAIT_TIMEOUT ? XERR_TIMEOUT :
				Wait == XWAIT_CANCELLED ? XERR_CANCELLED : XERR_IO,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS worker handoff interrupted"
		);
		return false;
	}
	if ( (Wait != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ||
		(pUpgrade->Tls == NULL) ) {
		if ( xrtFutureState(pFuture) == XFUTURE_FAILED ) {
			xrtSetError(xrtFutureError(pFuture));
		} else if ( Wait != XWAIT_OK ) {
			__xrtMailError(
				XERR_IO,
				XMAIL_ERROR_PROTOCOL,
				"mail STARTTLS worker handoff failed"
			);
		}
		xrtFutureDestroy(pFuture);
		xrtFree(pUpgrade);
		return false;
	}
	xrtFutureDestroy(pFuture);
	pTransport->Tcp = NULL;
	pTransport->Tls = pUpgrade->Tls;
	xrtFree(pUpgrade);
	pTransport->PendingSize = 0;
	pTransport->PendingConsumed = 0;
	pOpen = xrtTlsStreamWaitAsync(
		pTransport->Tls,
		XTLS_STREAM_WAIT_OPEN
	);
	if ( pOpen == NULL ) {
		(void)xrtTlsStreamAbort(pTransport->Tls);
		xrtTlsStreamDestroy(pTransport->Tls);
		pTransport->Tls = NULL;
		return false;
	}
	if ( !__xrtMailNetTlsFuture(pOpen, iDeadline, pCancel) ) {
		xrtFutureDestroy(pOpen);
		(void)xrtTlsStreamAbort(pTransport->Tls);
		xrtTlsStreamDestroy(pTransport->Tls);
		pTransport->Tls = NULL;
		return false;
	}
	xrtFutureDestroy(pOpen);
	pTransport->Security = XMAIL_SECURITY_TLS;
	return true;
}



/* 发送一个完整 TLS 明文分片。 */
bool __xrtMailTransportTlsSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture = xrtTlsStreamSendAsync(
		pTransport->Tls,
		pData,
		iSize
	);
	bool bSuccess;

	if ( pFuture == NULL ) {
		return false;
	}
	bSuccess = __xrtMailNetTlsFuture(pFuture, iDeadline, pCancel);
	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 取得一块拥有型 TLS 明文。 */
xnetbytes* __xrtMailTransportTlsRecv(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture = xrtTlsStreamRecvAsync(
		pTransport->Tls,
		pTransport->ReadChunk
	);
	xnetbytes* pBytes;

	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( !__xrtMailNetTlsFuture(pFuture, iDeadline, pCancel) ) {
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	return pBytes;
}



/* 按有界块消费剩余 TLS 明文，使读取背压不会挡住 close_notify。 */
static bool __xrtMailTransportTlsDrainClose(
	__xmailtransport* pTransport,
	double iDeadline
)
{
	for ( ;; ) {
		xfuture* pFuture = xrtTlsStreamRecvAsync(
			pTransport->Tls,
			pTransport->ReadChunk
		);
		bool bSuccess;
		bool bEnd;

		if ( pFuture == NULL ) {
			return false;
		}
		bSuccess = __xrtMailNetTlsFutureResult(
			pFuture,
			iDeadline,
			NULL,
			true
		);
		bEnd = xrtFutureState(pFuture) == XFUTURE_CLOSED;
		xrtFutureDestroy(pFuture);
		if ( !bSuccess || bEnd ) {
			return bSuccess;
		}
	}
}



/* 请求认证关闭，消费协议结束后的残留明文并等待 Stream 关闭终态。 */
bool __xrtMailTransportTlsClose(
	__xmailtransport* pTransport,
	double iDeadline
)
{
	xfuture* pFuture;
	bool bSuccess;

	if ( !xrtTlsStreamClose(pTransport->Tls) ||
		!__xrtMailTransportTlsDrainClose(pTransport, iDeadline) ) {
		return false;
	}
	pFuture = xrtTlsStreamWaitAsync(
		pTransport->Tls,
		XTLS_STREAM_WAIT_CLOSE
	);
	if ( pFuture == NULL ) {
		return false;
	}
	bSuccess = __xrtMailNetTlsFuture(pFuture, iDeadline, NULL);
	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 异常中止并释放 TLS Stream 引用。 */
void __xrtMailTransportTlsDestroy(__xmailtransport* pTransport)
{
	if ( (xrtTlsStreamState(pTransport->Tls) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pTransport->Tls) != XTLS_STREAM_FAILED) ) {
		(void)xrtTlsStreamAbort(pTransport->Tls);
	}
	xrtTlsStreamDestroy(pTransport->Tls);
	pTransport->Tls = NULL;
}

#endif
