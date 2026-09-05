#include "../internal/xrt_tls_session.h"



#if defined(XRT_FEATURE_TLS_SESSION)

#define XTLS_WAIT_MASK ((uint32)(XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT | \
	XTLS_WAIT_APPLICATION | XTLS_WAIT_IDENTITY | XTLS_WAIT_VERIFY))
#define XTLS_SESSION_KEY_MAX_SIZE 32u
#define XTLS_SESSION_IV_MAX_SIZE 12u



/* 设置会话错误并返回协议错误结果。 */
static xtlsresult __xrtTlsSessionError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(Kind, Code, sOperation, sMessage, SIZE_MAX);
	return XTLS_ERROR;
}



/* 包装底层分配或缓冲失败，同时保留原始原因链。 */
static xtlsresult __xrtTlsSessionCause(
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_INTERNAL;

	__xrtTlsErrorCause(
		Kind, XTLS_ERROR_INTERNAL,
		sOperation, sMessage, SIZE_MAX, pCause
	);
	return XTLS_ERROR;
}



/* 验证需要实际会话对象的公共入口。 */
static bool __xrtTlsSessionValid(
	const xtlssession* pSession,
	cstr sOperation
)
{
	if ( pSession != NULL ) {
		return true;
	}
	__xrtTlsError(
		XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
		sOperation, "TLS session is null", SIZE_MAX
	);
	return false;
}



/* 判断一次追加是否仍位于队列硬上限内，不修改线程错误。 */
static bool __xrtTlsSessionFits(
	const xnetbuf* pBuffer,
	size_t iLimit,
	size_t iSize
)
{
	size_t iCurrent = xrtNetBufSize(pBuffer);

	return (iCurrent <= iLimit) && (iSize <= (iLimit - iCurrent));
}



/* 根据双向 close_notify 与待发密文统一发布关闭状态和等待方向。 */
static void __xrtTlsSessionCloseUpdate(xtlssession* pSession)
{
	uint32 iWait = XTLS_WAIT_NONE;

	if ( (pSession->State == XTLS_STATE_CLOSED) ||
		(pSession->State == XTLS_STATE_FAILED) ) {
		return;
	}
	if ( (pSession->State == XTLS_STATE_READY) &&
		(pSession->CloseSent || pSession->CloseReceived) ) {
		if ( !__xrtTlsSessionSetState(
			pSession, XTLS_STATE_CLOSING
		) ) {
			return;
		}
	}
	if ( (pSession->State == XTLS_STATE_CLOSING) &&
		pSession->CloseSent && pSession->CloseReceived &&
		xrtNetBufEmpty(&pSession->Send) ) {
		(void)__xrtTlsSessionSetState(pSession, XTLS_STATE_CLOSED);
		return;
	}
	if ( pSession->State != XTLS_STATE_CLOSING ) {
		return;
	}
	if ( !xrtNetBufEmpty(&pSession->Send) ) {
		iWait |= XTLS_WAIT_OUTPUT;
	}
	if ( !pSession->CloseReceived && !pSession->TransportEof ) {
		iWait |= XTLS_WAIT_INPUT;
	}
	(void)__xrtTlsSessionSetWait(pSession, iWait);
}



/* 验证复制式追加参数；零长度是无需分配的成功操作。 */
static bool __xrtTlsSessionCopyValid(
	const xtlssession* pSession,
	const void* pData,
	size_t iSize,
	cstr sOperation
)
{
	if ( !__xrtTlsSessionValid(pSession, sOperation) ) {
		return false;
	}
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS session data is null", SIZE_MAX
		);
		return false;
	}
	return true;
}



/* 验证所有权式追加参数；空引用不产生含糊的所有权转移。 */
static bool __xrtTlsSessionRefValid(
	const xtlssession* pSession,
	const void* pData,
	size_t iSize,
	cstr sOperation
)
{
	if ( !__xrtTlsSessionValid(pSession, sOperation) ) {
		return false;
	}
	if ( (pData == NULL) || (iSize == 0) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS session reference is empty", SIZE_MAX
		);
		return false;
	}
	return true;
}



/* 把网络缓冲失败转换为 TLS cause，追加成功后不触碰错误槽。 */
static xtlsresult __xrtTlsSessionAppend(
	xnetbuf* pBuffer,
	const void* pData,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( iSize == 0 ) {
		return XTLS_OK;
	}
	if ( !xrtNetBufAppend(pBuffer, pData, iSize) ) {
		return __xrtTlsSessionCause(sOperation, sMessage);
	}
	return XTLS_OK;
}



/* 为内部编码器预留受限空间，并把实际视图截断到剩余预算。 */
static xtlsresult __xrtTlsSessionReserve(
	xnetbuf* pBuffer,
	size_t iLimit,
	size_t iMinimum,
	xnetwspan* pSpan,
	cstr sOperation,
	cstr sMessage
)
{
	size_t iCurrent;
	size_t iAvailable;

	if ( (pSpan == NULL) || (iMinimum == 0) ) {
		return __xrtTlsSessionError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation, "TLS session reservation is invalid"
		);
	}
	if ( pBuffer->Reserved != NULL ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			sOperation, "TLS session queue already has a reservation"
		);
	}
	iCurrent = xrtNetBufSize(pBuffer);
	if ( (iCurrent > iLimit) || (iMinimum > (iLimit - iCurrent)) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufReserve(pBuffer, iMinimum, pSpan) ) {
		return __xrtTlsSessionCause(sOperation, sMessage);
	}
	iAvailable = iLimit - iCurrent;
	if ( pSpan->Size > iAvailable ) {
		pSpan->Size = iAvailable;
	}
	return XTLS_OK;
}



/* 精确提交内部预留，避免跨过本次公开给编码器的硬上限。 */
static bool __xrtTlsSessionCommit(
	xnetbuf* pBuffer,
	size_t iLimit,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pBuffer->Reserved == NULL ) {
		(void)__xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			sOperation, "TLS session queue has no reservation"
		);
		return false;
	}
	if ( !__xrtTlsSessionFits(pBuffer, iLimit, iSize) ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			sOperation, "TLS session commit exceeds its queue limit", SIZE_MAX
		);
		return false;
	}
	if ( !xrtNetBufCommit(pBuffer, iSize) ) {
		(void)__xrtTlsSessionCause(sOperation, sMessage);
		return false;
	}
	return true;
}



/* 安全擦除队列即将消费的自有前缀，外部引用块保持只读。 */
static void __xrtTlsSessionWipePrefix(xnetbuf* pBuffer, size_t iSize)
{
	xnetblock* pBlock = pBuffer->Head;
	size_t iRemaining = iSize;

	while ( (pBlock != NULL) && (iRemaining != 0) ) {
		size_t iReadable = pBlock->End - pBlock->Begin;
		size_t iWipe = iReadable < iRemaining ? iReadable : iRemaining;

		if ( (iWipe != 0) && (pBlock->Class != XRT_NET_BLOCK_REF) ) {
			xrtSecureZero(pBlock->Data + pBlock->Begin, iWipe);
		}
		iRemaining -= iWipe;
		pBlock = pBlock->Next;
	}
}



/* 精确消费队列，调用方的完成字节数超过待处理数据时立即失败。 */
static bool __xrtTlsSessionConsume(
	xnetbuf* pBuffer,
	size_t iSize,
	bool bWipe,
	cstr sOperation,
	cstr sMessage
)
{
	if ( iSize > xrtNetBufSize(pBuffer) ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			sOperation, sMessage, SIZE_MAX
		);
		return false;
	}
	if ( bWipe ) {
		__xrtTlsSessionWipePrefix(pBuffer, iSize);
	}
	return xrtNetBufConsume(pBuffer, iSize) == iSize;
}



/* 擦除会话拥有的敏感块；外部引用块绝不写回调用方内存。 */
static void __xrtTlsSessionClearSensitive(xnetbuf* pBuffer)
{
	xnetblock* pBlock = pBuffer->Head;

	if ( (pBuffer->Reserved != NULL) && pBuffer->ReservedNew ) {
		xrtSecureZero(
			pBuffer->Reserved->Data,
			pBuffer->Reserved->Capacity
		);
	}
	while ( pBlock != NULL ) {
		if ( pBlock->Class != XRT_NET_BLOCK_REF ) {
			xrtSecureZero(pBlock->Data, pBlock->Capacity);
		}
		pBlock = pBlock->Next;
	}
	xrtNetBufClear(pBuffer);
}



/* 擦除并取消尚未提交的敏感预留空间。 */
static void __xrtTlsSessionCancelSensitive(xnetbuf* pBuffer)
{
	xnetblock* pReserved;
	size_t iBegin;

	if ( (pBuffer == NULL) || (pBuffer->Reserved == NULL) ) {
		return;
	}
	pReserved = pBuffer->Reserved;
	iBegin = pReserved->End;
	xrtSecureZero(
		pReserved->Data + iBegin,
		pReserved->Capacity - iBegin
	);
	(void)xrtNetBufCancel(pBuffer);
}



/* 以一次精确分配创建公共会话和可选角色尾部状态。 */
xtlssession* __xrtTlsSessionCreateSized(
	const xtlscontext* pContext,
	xnetbufpool* pPool,
	xtlsrole Role,
	size_t iRoleSize,
	xtlssessioncleanproc Clean
)
{
	xtlssession* pSession;
	xtlscontext* pRetained;
	size_t iSize;

	if ( pContext == NULL ) {
		(void)__xrtTlsSessionError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"create-tls-session", "TLS context is null"
		);
		return NULL;
	}
	if ( (Role != XTLS_CLIENT) && (Role != XTLS_SERVER) ) {
		(void)__xrtTlsSessionError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"create-tls-session", "TLS session role is invalid"
		);
		return NULL;
	}
	if ( iRoleSize > (SIZE_MAX - sizeof(*pSession)) ) {
		(void)__xrtTlsSessionError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"create-tls-session", "TLS session allocation size overflows"
		);
		return NULL;
	}
	iSize = sizeof(*pSession) + iRoleSize;
	pRetained = xrtTlsContextRetain(pContext);
	if ( pRetained == NULL ) {
		return NULL;
	}
	pSession = (xtlssession*)xrtCalloc(1, iSize);
	if ( pSession == NULL ) {
		xrtTlsContextRelease(pRetained);
		(void)__xrtTlsSessionCause(
			"create-tls-session", "TLS session allocation failed"
		);
		return NULL;
	}
	pSession->Context = pRetained;
	pSession->Role = Role;
	pSession->State = XTLS_STATE_NEW;
	pSession->AllocationSize = iSize;
	pSession->Clean = Clean;
	(void)xrtNetBufInit(&pSession->Feed, pPool);
	(void)xrtNetBufInit(&pSession->Send, pPool);
	(void)xrtNetBufInit(&pSession->Plain, pPool);
	(void)xrtNetBufInit(&pSession->Scratch, pPool);
	return pSession;
}



/* 为没有角色尾部状态的内部测试和底层组合创建会话。 */
xtlssession* __xrtTlsSessionCreate(
	const xtlscontext* pContext,
	xnetbufpool* pPool,
	xtlsrole Role
)
{
	return __xrtTlsSessionCreateSized(
		pContext, pPool, Role, 0, NULL
	);
}



/* 返回紧随公共会话本体之后的角色私有状态。 */
ptr __xrtTlsSessionRoleData(xtlssession* pSession)
{
	if ( (pSession == NULL) ||
		(pSession->AllocationSize <= sizeof(*pSession)) ) {
		return NULL;
	}
	return (ptr)(pSession + 1);
}



/* 销毁公共会话本体，明文归池前先执行安全擦除。 */
XRT_API void xrtTlsSessionDestroy(xtlssession* pSession)
{
	size_t iSize;

	if ( pSession == NULL ) {
		return;
	}
	iSize = pSession->AllocationSize;
	if ( pSession->Clean != NULL ) {
		pSession->Clean(pSession, __xrtTlsSessionRoleData(pSession));
	}
	xrtNetBufClear(&pSession->Feed);
	xrtNetBufClear(&pSession->Send);
	__xrtTlsSessionClearSensitive(&pSession->Plain);
	__xrtTlsSessionClearSensitive(&pSession->Scratch);
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	xrtTlsContextRelease(pSession->Context);
	xrtSecureZero(pSession, iSize);
	xrtFree(pSession);
}



/* 返回会话角色。 */
XRT_API xtlsrole xrtTlsSessionRole(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-role") ) {
		return (xtlsrole)0;
	}
	return pSession->Role;
}



/* 返回会话公开状态。 */
XRT_API xtlsstate xrtTlsSessionState(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-state") ) {
		return XTLS_STATE_FAILED;
	}
	return pSession->State;
}



/* 返回角色状态机已经提交的协商版本。 */
XRT_API xtlsversion xrtTlsSessionVersion(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-version") ) {
		return (xtlsversion)0;
	}
	return pSession->Version;
}



/* 返回角色状态机已经提交的协商密码套件。 */
XRT_API xtlscipher xrtTlsSessionCipher(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-cipher") ) {
		return (xtlscipher)0;
	}
	return pSession->Cipher;
}



/* 返回状态机当前等待原因。 */
XRT_API uint32 xrtTlsSessionWait(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-wait") ) {
		return XTLS_WAIT_NONE;
	}
	return pSession->Wait;
}



/* 借用会话拥有的上下文引用。 */
XRT_API const xtlscontext* xrtTlsSessionContext(
	const xtlssession* pSession
)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-context") ) {
		return NULL;
	}
	return pSession->Context;
}



/* 借用角色状态稳定持有的协商 ALPN 协议。 */
XRT_API bool xrtTlsSessionProtocol(
	const xtlssession* pSession,
	xbytesview* pProtocol
)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-protocol") ||
		(pProtocol == NULL) ) {
		if ( (pSession != NULL) && (pProtocol == NULL) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"get-tls-session-protocol",
				"TLS negotiated protocol output is null"
			);
		}
		return false;
	}
	if ( pSession->Protocol.Size == 0 ) {
		return false;
	}
	*pProtocol = pSession->Protocol;
	return true;
}



/* 查询最后一个经过严格解析的对端 Alert。 */
XRT_API bool xrtTlsSessionPeerAlert(
	const xtlssession* pSession,
	xtlsalertlevel* pLevel,
	xtlsalert* pAlert
)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-peer-alert") ||
		(pLevel == NULL) || (pAlert == NULL) ) {
		if ( (pSession != NULL) &&
			((pLevel == NULL) || (pAlert == NULL)) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"get-tls-session-peer-alert",
				"TLS peer alert output is null"
			);
		}
		return false;
	}
	if ( !pSession->PeerAlertSet ) {
		return false;
	}
	*pLevel = pSession->PeerAlertLevel;
	*pAlert = pSession->PeerAlert;
	return true;
}



/* 复制收到的密文并执行输入队列硬上限。 */
XRT_API xtlsresult xrtTlsSessionFeed(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionCopyValid(
		pSession, pData, iSize, "feed-tls-session"
	) ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(&pSession->Feed, pLimits->FeedLimit, iSize) ) {
		return XTLS_AGAIN;
	}
	return __xrtTlsSessionAppend(
		&pSession->Feed, pData, iSize,
		"feed-tls-session", "TLS session input allocation failed"
	);
}



/* 借用收到的密文并执行输入队列硬上限。 */
XRT_API xtlsresult xrtTlsSessionFeedBorrow(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionRefValid(
		pSession, pData, iSize, "borrow-tls-session-feed"
	) ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(&pSession->Feed, pLimits->FeedLimit, iSize) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufAppendBorrow(&pSession->Feed, pData, iSize) ) {
		return __xrtTlsSessionCause(
			"borrow-tls-session-feed",
			"TLS session borrowed input allocation failed"
		);
	}
	return XTLS_OK;
}



/* 接管收到的密文并执行输入队列硬上限。 */
XRT_API xtlsresult xrtTlsSessionFeedTake(
	xtlssession* pSession,
	ptr pData,
	size_t iSize
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionRefValid(
		pSession, pData, iSize, "take-tls-session-feed"
	) ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(&pSession->Feed, pLimits->FeedLimit, iSize) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufAppendTake(&pSession->Feed, pData, iSize) ) {
		return __xrtTlsSessionCause(
			"take-tls-session-feed",
			"TLS session owned input allocation failed"
		);
	}
	return XTLS_OK;
}



/* 接管带释放过程的密文并执行输入队列硬上限。 */
XRT_API xtlsresult xrtTlsSessionFeedRef(
	xtlssession* pSession,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionRefValid(
		pSession, pData, iSize, "reference-tls-session-feed"
	) ) {
		return XTLS_ERROR;
	}
	if ( pRelease == NULL ) {
		return __xrtTlsSessionError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"reference-tls-session-feed",
			"TLS session release procedure is null"
		);
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(&pSession->Feed, pLimits->FeedLimit, iSize) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufAppendRef(
		&pSession->Feed, pData, iSize, pRelease, pContext
	) ) {
		return __xrtTlsSessionCause(
			"reference-tls-session-feed",
			"TLS session referenced input allocation failed"
		);
	}
	return XTLS_OK;
}



/* 零复制接管收到的密文缓冲，并在移动前执行输入硬上限。 */
XRT_API xtlsresult xrtTlsSessionFeedBuffer(
	xtlssession* pSession,
	xnetbuf* pBuffer
)
{
	const xtlslimits* pLimits;
	size_t iSize;

	if ( !__xrtTlsSessionValid(
		pSession, "feed-tls-session-buffer"
	) || (pBuffer == NULL) ) {
		if ( (pSession != NULL) && (pBuffer == NULL) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT,
				XTLS_ERROR_ARGUMENT,
				"feed-tls-session-buffer",
				"TLS session input buffer is null"
			);
		}
		return XTLS_ERROR;
	}
	if ( pBuffer == &pSession->Feed ) {
		return __xrtTlsSessionError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"feed-tls-session-buffer",
			"TLS session feed buffer cannot feed itself"
		);
	}
	iSize = xrtNetBufSize(pBuffer);
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(
		&pSession->Feed,
		pLimits->FeedLimit,
		iSize
	) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufMove(&pSession->Feed, pBuffer) ) {
		return __xrtTlsSessionCause(
			"feed-tls-session-buffer",
			"TLS session input buffer transfer failed"
		);
	}
	return XTLS_OK;
}



/* 返回尚未消费的输入密文字节数。 */
XRT_API size_t xrtTlsSessionFeedSize(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-feed-size") ) {
		return 0;
	}
	return xrtNetBufSize(&pSession->Feed);
}



/* 返回待发送密文字节数。 */
XRT_API size_t xrtTlsSessionSendSize(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-send-size") ) {
		return 0;
	}
	return xrtNetBufSize(&pSession->Send);
}



/* 返回待发送密文 Span 数。 */
XRT_API size_t xrtTlsSessionSendSpanCount(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(
		pSession, "get-tls-session-send-span-count"
	) ) {
		return 0;
	}
	return xrtNetBufSpanCount(&pSession->Send);
}



/* 借用第一个待发送密文 Span。 */
XRT_API bool xrtTlsSessionSendFront(
	const xtlssession* pSession,
	xnetspan* pSpan
)
{
	if ( !__xrtTlsSessionValid(pSession, "front-tls-session-send") ||
		(pSpan == NULL) ) {
		if ( pSpan == NULL ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"front-tls-session-send", "TLS send span is null"
			);
		}
		return false;
	}
	return xrtNetBufFront(&pSession->Send, pSpan);
}



/* 借用待发送密文 Span 向量。 */
XRT_API size_t xrtTlsSessionSendSpans(
	const xtlssession* pSession,
	xnetspan* pSpans,
	size_t iCapacity
)
{
	if ( !__xrtTlsSessionValid(pSession, "span-tls-session-send") ||
		((pSpans == NULL) && (iCapacity != 0)) ) {
		if ( (pSpans == NULL) && (iCapacity != 0) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"span-tls-session-send", "TLS send spans are null"
			);
		}
		return 0;
	}
	return xrtNetBufSpans(&pSession->Send, pSpans, iCapacity);
}



/* 精确消费已经发送的密文。 */
XRT_API bool xrtTlsSessionSendConsume(
	xtlssession* pSession,
	size_t iSize
)
{
	bool bResult;

	if ( !__xrtTlsSessionValid(pSession, "consume-tls-session-send") ) {
		return false;
	}
	bResult = __xrtTlsSessionConsume(
		&pSession->Send, iSize, false, "consume-tls-session-send",
		"TLS send consumption exceeds pending ciphertext"
	);
	if ( bResult ) {
		__xrtTlsSessionCloseUpdate(pSession);
	}
	return bResult;
}



/* 返回等待应用读取的明文字节数。 */
XRT_API size_t xrtTlsSessionPlainSize(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-plain-size") ) {
		return 0;
	}
	return xrtNetBufSize(&pSession->Plain);
}



/* 返回等待应用读取的明文 Span 数。 */
XRT_API size_t xrtTlsSessionPlainSpanCount(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(
		pSession, "get-tls-session-plain-span-count"
	) ) {
		return 0;
	}
	return xrtNetBufSpanCount(&pSession->Plain);
}



/* 借用第一个等待应用读取的明文 Span。 */
XRT_API bool xrtTlsSessionPlainFront(
	const xtlssession* pSession,
	xnetspan* pSpan
)
{
	if ( !__xrtTlsSessionValid(pSession, "front-tls-session-plain") ||
		(pSpan == NULL) ) {
		if ( pSpan == NULL ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"front-tls-session-plain", "TLS plain span is null"
			);
		}
		return false;
	}
	return xrtNetBufFront(&pSession->Plain, pSpan);
}



/* 借用等待应用读取的明文 Span 向量。 */
XRT_API size_t xrtTlsSessionPlainSpans(
	const xtlssession* pSession,
	xnetspan* pSpans,
	size_t iCapacity
)
{
	if ( !__xrtTlsSessionValid(pSession, "span-tls-session-plain") ||
		((pSpans == NULL) && (iCapacity != 0)) ) {
		if ( (pSpans == NULL) && (iCapacity != 0) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"span-tls-session-plain", "TLS plain spans are null"
			);
		}
		return 0;
	}
	return xrtNetBufSpans(&pSession->Plain, pSpans, iCapacity);
}



/* 精确消费应用已经处理的明文。 */
XRT_API bool xrtTlsSessionPlainConsume(
	xtlssession* pSession,
	size_t iSize
)
{
	if ( !__xrtTlsSessionValid(pSession, "consume-tls-session-plain") ) {
		return false;
	}
	return __xrtTlsSessionConsume(
		&pSession->Plain, iSize, true, "consume-tls-session-plain",
		"TLS plain consumption exceeds pending plaintext"
	);
}



/* 复制并消费一段明文，控制结果不覆盖线程错误。 */
XRT_API xtlsresult xrtTlsSessionRead(
	xtlssession* pSession,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
)
{
	size_t iRead;

	if ( !__xrtTlsSessionValid(pSession, "read-tls-session") ||
		(pOutput == NULL) || (iCapacity == 0) || (pRead == NULL) ) {
		if ( (pSession != NULL) &&
			((pOutput == NULL) || (iCapacity == 0) || (pRead == NULL)) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"read-tls-session", "TLS session read arguments are invalid"
			);
		}
		return XTLS_ERROR;
	}
	*pRead = 0;
	if ( xrtNetBufEmpty(&pSession->Plain) ) {
		return ((pSession->State == XTLS_STATE_CLOSED) ||
			pSession->CloseReceived) ?
			XTLS_CLOSED : XTLS_AGAIN;
	}
	iRead = xrtNetBufPeek(&pSession->Plain, 0, pOutput, iCapacity);
	if ( iRead == 0 ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_INTERNAL,
			"read-tls-session", "TLS plaintext queue made no progress"
		);
	}
	if ( !__xrtTlsSessionConsume(
		&pSession->Plain, iRead, true, "read-tls-session",
		"TLS read exceeds pending plaintext"
	) ) {
		return XTLS_ERROR;
	}
	*pRead = iRead;
	return XTLS_OK;
}



/* 按公开生命周期图推进状态。 */
bool __xrtTlsSessionSetState(xtlssession* pSession, xtlsstate State)
{
	bool bAllowed = false;

	if ( !__xrtTlsSessionValid(pSession, "set-tls-session-state") ) {
		return false;
	}
	if ( State == pSession->State ) {
		return true;
	}
	if ( State == XTLS_STATE_FAILED ) {
		bAllowed = (pSession->State != XTLS_STATE_CLOSED) &&
			(pSession->State != XTLS_STATE_FAILED);
	} else if ( pSession->State == XTLS_STATE_NEW ) {
		bAllowed = State == XTLS_STATE_HANDSHAKE;
	} else if ( pSession->State == XTLS_STATE_HANDSHAKE ) {
		bAllowed = State == XTLS_STATE_READY;
	} else if ( pSession->State == XTLS_STATE_READY ) {
		bAllowed = State == XTLS_STATE_CLOSING;
	} else if ( pSession->State == XTLS_STATE_CLOSING ) {
		bAllowed = State == XTLS_STATE_CLOSED;
	}
	if ( !bAllowed ) {
		(void)__xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"set-tls-session-state", "TLS session state transition is invalid"
		);
		return false;
	}
	pSession->State = State;
	if ( (State == XTLS_STATE_CLOSED) || (State == XTLS_STATE_FAILED) ) {
		pSession->Wait = XTLS_WAIT_NONE;
	}
	return true;
}



/* 设置当前等待原因并拒绝未知位或终态等待。 */
bool __xrtTlsSessionSetWait(xtlssession* pSession, uint32 iWait)
{
	if ( !__xrtTlsSessionValid(pSession, "set-tls-session-wait") ) {
		return false;
	}
	if ( ((iWait & ~XTLS_WAIT_MASK) != 0) ||
		(((pSession->State == XTLS_STATE_CLOSED) ||
		  (pSession->State == XTLS_STATE_FAILED)) &&
		 (iWait != XTLS_WAIT_NONE)) ) {
		(void)__xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"set-tls-session-wait", "TLS session wait reasons are invalid"
		);
		return false;
	}
	pSession->Wait = iWait;
	return true;
}



/* 发布角色状态持有的非空协商 ALPN 协议。 */
bool __xrtTlsSessionSetProtocol(
	xtlssession* pSession,
	xbytesview Protocol
)
{
	if ( !__xrtTlsSessionValid(pSession, "set-tls-session-protocol") ||
		!__xrtTlsViewValid(Protocol) || (Protocol.Size == 0) ) {
		if ( pSession != NULL ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"set-tls-session-protocol",
				"TLS negotiated protocol is invalid"
			);
		}
		return false;
	}
	if ( pSession->Protocol.Size != 0 ) {
		(void)__xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"set-tls-session-protocol",
			"TLS negotiated protocol is already set"
		);
		return false;
	}
	pSession->Protocol = Protocol;
	return true;
}



/* 发布经过密码套件元数据交叉验证的最终协商结果。 */
bool __xrtTlsSessionNegotiated(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher
)
{
	const xtlscipherinfo* pCipher;

	if ( !__xrtTlsSessionValid(pSession, "set-tls-session-negotiated") ) {
		return false;
	}
	pCipher = xrtTlsCipherInfo(Cipher);
	if ( (pCipher == NULL) || (pCipher->Version != Version) ) {
		(void)__xrtTlsSessionError(
			XERR_ARGUMENT, XTLS_ERROR_CIPHER,
			"set-tls-session-negotiated",
			"TLS negotiated version and cipher are incompatible"
		);
		return false;
	}
	if ( (pSession->Version != 0) || (pSession->Cipher != 0) ) {
		(void)__xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"set-tls-session-negotiated",
			"TLS negotiated parameters are already set"
		);
		return false;
	}
	pSession->Version = Version;
	pSession->Cipher = Cipher;
	return true;
}



/* 借用协议状态机的输入密文队列。 */
const xnetbuf* __xrtTlsSessionFeedBuffer(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-feed") ) {
		return NULL;
	}
	return &pSession->Feed;
}



/* 借用待发密文块链给同线程传输适配器。 */
xnetbuf* __xrtTlsSessionSendBuffer(xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-send") ) {
		return NULL;
	}
	return &pSession->Send;
}



/* 借用待应用消费的明文块链。 */
const xnetbuf* __xrtTlsSessionPlainBuffer(const xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "get-tls-session-plain") ) {
		return NULL;
	}
	return &pSession->Plain;
}



/* 收敛应用即将解析的明文前缀，不改变待消费字节数。 */
xtlsresult __xrtTlsSessionPlainPullup(
	xtlssession* pSession,
	size_t iSize,
	xnetspan* pSpan
)
{
	if ( !__xrtTlsSessionValid(pSession, "pullup-tls-session-plain") ||
		(pSpan == NULL) || (iSize == 0) ) {
		if ( (pSession != NULL) && ((pSpan == NULL) || (iSize == 0)) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT,
				XTLS_ERROR_ARGUMENT,
				"pullup-tls-session-plain",
				"TLS plain pullup arguments are invalid"
			);
		}
		return XTLS_ERROR;
	}
	if ( iSize > xrtNetBufSize(&pSession->Plain) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufPullup(&pSession->Plain, iSize, pSpan) ) {
		return __xrtTlsSessionCause(
			"pullup-tls-session-plain",
			"TLS session plaintext coalescing failed"
		);
	}
	return XTLS_OK;
}



/* 把四个惰性队列的后续分配归属切换到当前 Worker。 */
bool __xrtTlsSessionPool(xtlssession* pSession, xnetbufpool* pPool)
{
	if ( !__xrtTlsSessionValid(pSession, "set-tls-session-buffer-pool") ) {
		return false;
	}
	if ( (pSession->Feed.Reserved != NULL) ||
		(pSession->Send.Reserved != NULL) ||
		(pSession->Plain.Reserved != NULL) ||
		(pSession->Scratch.Reserved != NULL) ) {
		(void)__xrtTlsSessionError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"set-tls-session-buffer-pool",
			"TLS session has an active buffer reservation"
		);
		return false;
	}
	pSession->Feed.Pool = pPool;
	pSession->Send.Pool = pPool;
	pSession->Plain.Pool = pPool;
	pSession->Scratch.Pool = pPool;
	return true;
}



/* 在传输接管全部密文后刷新双向认证关闭状态。 */
bool __xrtTlsSessionSendMoved(xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "move-tls-session-send") ) {
		return false;
	}
	if ( !xrtNetBufEmpty(&pSession->Send) ) {
		(void)__xrtTlsSessionError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"move-tls-session-send",
			"TLS session send queue is not empty after transfer"
		);
		return false;
	}
	__xrtTlsSessionCloseUpdate(pSession);
	return true;
}



/* 精确消费协议状态机已经处理的输入密文。 */
bool __xrtTlsSessionFeedConsume(xtlssession* pSession, size_t iSize)
{
	if ( !__xrtTlsSessionValid(pSession, "consume-tls-session-feed") ) {
		return false;
	}
	return __xrtTlsSessionConsume(
		&pSession->Feed, iSize, false, "consume-tls-session-feed",
		"TLS feed consumption exceeds pending ciphertext"
	);
}



/* 收敛协议状态机即将读取的输入前缀。 */
xtlsresult __xrtTlsSessionFeedPullup(
	xtlssession* pSession,
	size_t iSize,
	xnetspan* pSpan
)
{
	if ( !__xrtTlsSessionValid(pSession, "pullup-tls-session-feed") ||
		(pSpan == NULL) || (iSize == 0) ) {
		if ( (pSession != NULL) && ((pSpan == NULL) || (iSize == 0)) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"pullup-tls-session-feed",
				"TLS feed pullup arguments are invalid"
			);
		}
		return XTLS_ERROR;
	}
	if ( iSize > xrtNetBufSize(&pSession->Feed) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufPullup(&pSession->Feed, iSize, pSpan) ) {
		return __xrtTlsSessionCause(
			"pullup-tls-session-feed",
			"TLS session input coalescing failed"
		);
	}
	return XTLS_OK;
}



/* 复制追加待发送密文。 */
xtlsresult __xrtTlsSessionSend(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionCopyValid(
		pSession, pData, iSize, "queue-tls-session-send"
	) ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(&pSession->Send, pLimits->SendLimit, iSize) ) {
		return XTLS_AGAIN;
	}
	return __xrtTlsSessionAppend(
		&pSession->Send, pData, iSize,
		"queue-tls-session-send", "TLS session send allocation failed"
	);
}



/* 接管一块已经完整构造的密文输出，供角色状态机失败原子地提交航班。 */
xtlsresult __xrtTlsSessionSendTake(
	xtlssession* pSession,
	ptr pData,
	size_t iSize
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionRefValid(
		pSession, pData, iSize, "take-tls-session-send"
	) ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(&pSession->Send, pLimits->SendLimit, iSize) ) {
		return XTLS_AGAIN;
	}
	if ( !xrtNetBufAppendTake(&pSession->Send, pData, iSize) ) {
		return __xrtTlsSessionCause(
			"take-tls-session-send",
			"TLS session send ownership transfer failed"
		);
	}
	return XTLS_OK;
}



/* 预留待发送密文空间。 */
xtlsresult __xrtTlsSessionSendReserve(
	xtlssession* pSession,
	size_t iMinimum,
	xnetwspan* pSpan
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionValid(pSession, "reserve-tls-session-send") ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	return __xrtTlsSessionReserve(
		&pSession->Send, pLimits->SendLimit, iMinimum, pSpan,
		"reserve-tls-session-send", "TLS session send reservation failed"
	);
}



/* 提交待发送密文空间。 */
bool __xrtTlsSessionSendCommit(xtlssession* pSession, size_t iSize)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionValid(pSession, "commit-tls-session-send") ) {
		return false;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	return __xrtTlsSessionCommit(
		&pSession->Send, pLimits->SendLimit, iSize,
		"commit-tls-session-send", "TLS session send commit failed"
	);
}



/* 取消待发送密文预留。 */
bool __xrtTlsSessionSendCancel(xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "cancel-tls-session-send") ) {
		return false;
	}
	if ( pSession->Send.Reserved == NULL ) {
		(void)__xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"cancel-tls-session-send", "TLS session send has no reservation"
		);
		return false;
	}
	return xrtNetBufCancel(&pSession->Send);
}



/* 复制追加等待应用读取的明文。 */
xtlsresult __xrtTlsSessionPlain(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionCopyValid(
		pSession, pData, iSize, "queue-tls-session-plain"
	) ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( !__xrtTlsSessionFits(&pSession->Plain, pLimits->PlainLimit, iSize) ) {
		return XTLS_AGAIN;
	}
	return __xrtTlsSessionAppend(
		&pSession->Plain, pData, iSize,
		"queue-tls-session-plain", "TLS session plain allocation failed"
	);
}



/* 预留等待应用读取的明文空间。 */
xtlsresult __xrtTlsSessionPlainReserve(
	xtlssession* pSession,
	size_t iMinimum,
	xnetwspan* pSpan
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionValid(pSession, "reserve-tls-session-plain") ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	return __xrtTlsSessionReserve(
		&pSession->Plain, pLimits->PlainLimit, iMinimum, pSpan,
		"reserve-tls-session-plain", "TLS session plain reservation failed"
	);
}



/* 提交等待应用读取的明文空间。 */
bool __xrtTlsSessionPlainCommit(xtlssession* pSession, size_t iSize)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionValid(pSession, "commit-tls-session-plain") ) {
		return false;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	return __xrtTlsSessionCommit(
		&pSession->Plain, pLimits->PlainLimit, iSize,
		"commit-tls-session-plain", "TLS session plain commit failed"
	);
}



/* 取消等待应用读取的明文预留。 */
bool __xrtTlsSessionPlainCancel(xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "cancel-tls-session-plain") ) {
		return false;
	}
	if ( pSession->Plain.Reserved == NULL ) {
		(void)__xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"cancel-tls-session-plain", "TLS session plain has no reservation"
		);
		return false;
	}
	__xrtTlsSessionCancelSensitive(&pSession->Plain);
	return true;
}





/* 原子构造并替换一侧记录密钥。 */
static bool __xrtTlsSessionKey(
	xtlssession* pSession,
	xtlsrecordkey* pTarget,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview Key,
	xbytesview Iv,
	cstr sOperation
)
{
	xtlsrecordkey Next;

	if ( !__xrtTlsSessionValid(pSession, sOperation) ) {
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTlsRecordKeyInit(&Next, Version, Cipher, Key, Iv) ) {
		return false;
	}
	__xrtTlsRecordKeyClear(pTarget);
	*pTarget = Next;
	xrtSecureZero(&Next, sizeof(Next));
	return true;
}



/* 原子替换接收记录密钥。 */
bool __xrtTlsSessionReadKey(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview Key,
	xbytesview Iv
)
{
	return __xrtTlsSessionKey(
		pSession,
		pSession != NULL ? &pSession->ReadKey : NULL,
		Version, Cipher, Key, Iv, "set-tls-session-read-key"
	);
}



/* 原子替换发送记录密钥。 */
bool __xrtTlsSessionWriteKey(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview Key,
	xbytesview Iv
)
{
	return __xrtTlsSessionKey(
		pSession,
		pSession != NULL ? &pSession->WriteKey : NULL,
		Version, Cipher, Key, Iv, "set-tls-session-write-key"
	);
}



/* 验证两侧记录密钥后一次替换收发 epoch。 */
bool __xrtTlsSessionKeys(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview ReadKey,
	xbytesview ReadIv,
	xbytesview WriteKey,
	xbytesview WriteIv
)
{
	xtlsrecordkey NextRead;
	xtlsrecordkey NextWrite;

	if ( !__xrtTlsSessionValid(pSession, "set-tls-session-keys") ) {
		return false;
	}
	memset(&NextRead, 0, sizeof(NextRead));
	memset(&NextWrite, 0, sizeof(NextWrite));
	if ( !__xrtTlsRecordKeyInit(
		&NextRead, Version, Cipher, ReadKey, ReadIv
	) || !__xrtTlsRecordKeyInit(
		&NextWrite, Version, Cipher, WriteKey, WriteIv
	) ) {
		__xrtTlsRecordKeyClear(&NextRead);
		__xrtTlsRecordKeyClear(&NextWrite);
		return false;
	}
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->ReadKey = NextRead;
	pSession->WriteKey = NextWrite;
	xrtSecureZero(&NextRead, sizeof(NextRead));
	xrtSecureZero(&NextWrite, sizeof(NextWrite));
	return true;
}



/* 从 traffic secret 展开 key 和 iv，再原子初始化记录密钥。 */
bool __xrtTls13RecordKey(
	xtlscipher Cipher,
	xbytesview Traffic,
	xtlsrecordkey* pKey
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(Cipher);
	uint8 Key[XTLS_SESSION_KEY_MAX_SIZE];
	uint8 Iv[XTLS_SESSION_IV_MAX_SIZE];
	xbytesview Empty = { NULL, 0 };
	xcryptohash Hash;
	bool bResult = false;

	memset(Key, 0, sizeof(Key));
	memset(Iv, 0, sizeof(Iv));
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_13) ||
		(Traffic.Data == NULL) || (Traffic.Size != pCipher->HashSize) ||
		(pCipher->KeySize > sizeof(Key)) ||
		(pCipher->IvSize > sizeof(Iv)) || (pKey == NULL) ) {
		(void)__xrtTlsSessionError(
			XERR_ARGUMENT, XTLS_ERROR_KEY_DERIVATION,
			"derive-tls13-record-key",
			"TLS 1.3 traffic secret or cipher is invalid"
		);
		goto cleanup;
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	if ( (Hash == (xcryptohash)0) || !__xrtTls13ExpandLabel(
		Hash, Traffic, XRT_STR_LITERAL("key"), Empty,
		Key, pCipher->KeySize
	) || !__xrtTls13ExpandLabel(
		Hash, Traffic, XRT_STR_LITERAL("iv"), Empty,
		Iv, pCipher->IvSize
	) || !__xrtTlsRecordKeyInit(
		pKey, XTLS_VERSION_13, Cipher,
		(xbytesview) { Key, pCipher->KeySize },
		(xbytesview) { Iv, pCipher->IvSize }
	) ) {
		goto cleanup;
	}
	bResult = true;

cleanup:
	xrtSecureZero(Iv, sizeof(Iv));
	xrtSecureZero(Key, sizeof(Key));
	return bResult;
}



/* 检查一条受保护记录是否能在不修改队列的前提下完整入队。 */
xtlsresult __xrtTlsSessionRecordWritable(
	xtlssession* pSession,
	size_t iPlaintext,
	size_t iPadding,
	cstr sOperation
)
{
	const xtlslimits* pLimits;
	size_t iRecord;
	size_t iQueued;

	if ( (sOperation == NULL) ||
		!__xrtTlsSessionValid(pSession, sOperation) ) {
		return XTLS_ERROR;
	}
	pLimits = xrtTlsContextLimits(pSession->Context);
	iRecord = __xrtTlsRecordSealSize(
		&pSession->WriteKey, iPlaintext, iPadding
	);
	iQueued = xrtTlsSessionSendSize(pSession);
	if ( (pLimits == NULL) || (iPlaintext == 0) || (iRecord == 0) ) {
		return __xrtTlsSessionError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL, sOperation,
			"TLS protected record output state is invalid"
		);
	}
	if ( iRecord > pLimits->SendLimit ) {
		return __xrtTlsSessionError(
			XERR_RANGE, XTLS_ERROR_LIMIT, sOperation,
			"TLS send limit cannot hold the protected record"
		);
	}
	if ( (iQueued > pLimits->SendLimit) ||
		(iRecord > (pLimits->SendLimit - iQueued)) ) {
		return XTLS_AGAIN;
	}
	return XTLS_OK;
}



/* 检查 KeyUpdate 的完整受保护记录是否位于发送硬上限内。 */
xtlsresult __xrtTlsSessionKeyUpdateWritable(
	xtlssession* pSession,
	cstr sOperation
)
{
	size_t iMessage = xrtTlsHandshakeSize(1u);

	if ( iMessage == 0 ) {
		return XTLS_ERROR;
	}
	return __xrtTlsSessionRecordWritable(
		pSession, iMessage, 0, sOperation
	);
}



/* 从当前应用 traffic secret 派生下一代 secret 和记录密钥。 */
static bool __xrtTls13TrafficUpdate(
	xtlscipher Cipher,
	xbytesview Current,
	void* pTraffic,
	size_t iTrafficSize,
	xtlsrecordkey* pKey,
	cstr sOperation
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(Cipher);
	xbytesview Empty = { NULL, 0 };
	xcryptohash Hash;

	if ( (pCipher == NULL) || (Current.Data == NULL) ||
		(Current.Size != pCipher->HashSize) || (pTraffic == NULL) ||
		(iTrafficSize != pCipher->HashSize) || (pKey == NULL) ||
		(sOperation == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS KeyUpdate traffic state is inconsistent", SIZE_MAX
		);
		return false;
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	return __xrtTls13ExpandLabel(
		Hash, Current, XRT_STR_LITERAL("traffic upd"), Empty,
		pTraffic, iTrafficSize
	) && __xrtTls13RecordKey(
		Cipher, (xbytesview) { pTraffic, iTrafficSize }, pKey
	);
}



/* 把请求值编码为一条完整且独占记录的 KeyUpdate 握手消息。 */
static bool __xrtTls13KeyUpdateMessage(
	xtlskeyupdate Request,
	xtlssessionupdate* pNext,
	cstr sOperation
)
{
	uint8 Body = (uint8)Request;

	if ( (Request != XTLS_KEY_UPDATE_NOT_REQUESTED) &&
		(Request != XTLS_KEY_UPDATE_REQUESTED) ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_HANDSHAKE, sOperation,
			"TLS KeyUpdate request value is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_KEY_UPDATE,
		(xbytesview) { &Body, sizeof(Body) },
		pNext->Message, sizeof(pNext->Message)
	) ) {
		return false;
	}
	pNext->MessageSize = xrtTlsHandshakeSize(sizeof(Body));
	return pNext->MessageSize != 0;
}



/* 准备被动 KeyUpdate；失败时不留下任何派生密钥或秘密。 */
bool __xrtTls13KeyUpdateReceive(
	xtlscipher Cipher,
	xbytesview ReadTraffic,
	xbytesview WriteTraffic,
	xtlskeyupdate Request,
	xtlssessionupdate* pNext,
	cstr sOperation
)
{
	bool bResult = false;

	if ( (pNext == NULL) || (sOperation == NULL) ||
		((Request != XTLS_KEY_UPDATE_NOT_REQUESTED) &&
		 (Request != XTLS_KEY_UPDATE_REQUESTED)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS KeyUpdate receive arguments are invalid", SIZE_MAX
		);
		return false;
	}
	memset(pNext, 0, sizeof(*pNext));
	if ( !__xrtTls13TrafficUpdate(
		Cipher, ReadTraffic, pNext->ReadTraffic,
		ReadTraffic.Size, &pNext->ReadKey, sOperation
	) ) {
		goto cleanup;
	}
	if ( Request == XTLS_KEY_UPDATE_REQUESTED ) {
		if ( !__xrtTls13TrafficUpdate(
			Cipher, WriteTraffic, pNext->WriteTraffic,
			WriteTraffic.Size, &pNext->WriteKey, sOperation
		) || !__xrtTls13KeyUpdateMessage(
			XTLS_KEY_UPDATE_NOT_REQUESTED, pNext, sOperation
		) ) {
			goto cleanup;
		}
	}
	bResult = true;

cleanup:
	if ( !bResult ) {
		__xrtTlsRecordKeyClear(&pNext->WriteKey);
		__xrtTlsRecordKeyClear(&pNext->ReadKey);
		xrtSecureZero(pNext, sizeof(*pNext));
	}
	return bResult;
}



/* 准备主动 KeyUpdate；消息仍须使用旧发送 epoch 加密后才能提交。 */
bool __xrtTls13KeyUpdateSend(
	xtlscipher Cipher,
	xbytesview WriteTraffic,
	xtlskeyupdate Request,
	xtlssessionupdate* pNext,
	cstr sOperation
)
{
	bool bResult;

	if ( (pNext == NULL) || (sOperation == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS KeyUpdate send arguments are invalid", SIZE_MAX
		);
		return false;
	}
	memset(pNext, 0, sizeof(*pNext));
	bResult = __xrtTls13TrafficUpdate(
		Cipher, WriteTraffic, pNext->WriteTraffic,
		WriteTraffic.Size, &pNext->WriteKey, sOperation
	) && __xrtTls13KeyUpdateMessage(Request, pNext, sOperation);
	if ( !bResult ) {
		__xrtTlsRecordKeyClear(&pNext->WriteKey);
		xrtSecureZero(pNext, sizeof(*pNext));
	}
	return bResult;
}



/* 清除接收记录密钥。 */
void __xrtTlsSessionReadKeyClear(xtlssession* pSession)
{
	if ( pSession != NULL ) {
		__xrtTlsRecordKeyClear(&pSession->ReadKey);
	}
}



/* 清除发送记录密钥。 */
void __xrtTlsSessionWriteKeyClear(xtlssession* pSession)
{
	if ( pSession != NULL ) {
		__xrtTlsRecordKeyClear(&pSession->WriteKey);
	}
}



/* 提交已经写入发送队列预留区的完整记录。 */
static xtlsresult __xrtTlsSessionRecordCommit(
	xtlssession* pSession,
	size_t iSize,
	cstr sOperation
)
{
	if ( __xrtTlsSessionSendCommit(pSession, iSize) ) {
		return XTLS_OK;
	}
	__xrtTlsSessionCancelSensitive(&pSession->Send);
	return __xrtTlsSessionCause(
		sOperation, "TLS session record commit failed"
	);
}



/* 排队一条初始明文记录或兼容 CCS 记录。 */
xtlsresult __xrtTlsSessionRecordPlain(
	xtlssession* pSession,
	xtlsrecordtype Type,
	uint16 iLegacyVersion,
	xbytesview Data
)
{
	xnetwspan Span;
	size_t iRequired;
	xtlsresult Result;

	if ( !__xrtTlsSessionValid(pSession, "queue-plain-tls-record") ||
		((Data.Data == NULL) && (Data.Size != 0)) ) {
		if ( (pSession != NULL) && (Data.Data == NULL) &&
			(Data.Size != 0) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"queue-plain-tls-record",
				"TLS plaintext record data is invalid"
			);
		}
		return XTLS_ERROR;
	}
	iRequired = xrtTlsRecordSize(Data.Size);
	if ( iRequired == 0 ) {
		return XTLS_ERROR;
	}
	Result = __xrtTlsSessionSendReserve(pSession, iRequired, &Span);
	if ( Result != XTLS_OK ) {
		return Result;
	}
	if ( !xrtTlsRecordEncode(
		Type, iLegacyVersion, Data, Span.Data, Span.Size
	) ) {
		__xrtTlsSessionCancelSensitive(&pSession->Send);
		return XTLS_ERROR;
	}
	return __xrtTlsSessionRecordCommit(
		pSession, iRequired, "queue-plain-tls-record"
	);
}



/* 使用当前发送 epoch 排队一条受保护记录。 */
xtlsresult __xrtTlsSessionRecordProtect(
	xtlssession* pSession,
	xtlsrecordtype Type,
	xbytesview Data,
	size_t iPadding
)
{
	xnetwspan Span;
	size_t iRequired;
	size_t iWritten = 0;
	uint64 iSequence;
	xtlsresult Result;

	if ( !__xrtTlsSessionValid(pSession, "queue-protected-tls-record") ||
		((Data.Data == NULL) && (Data.Size != 0)) ) {
		if ( (pSession != NULL) && (Data.Data == NULL) &&
			(Data.Size != 0) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"queue-protected-tls-record",
				"TLS protected record data is invalid"
			);
		}
		return XTLS_ERROR;
	}
	if ( !pSession->WriteKey.Ready ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"queue-protected-tls-record",
			"TLS session write key is not ready"
		);
	}
	iRequired = __xrtTlsRecordSealSize(
		&pSession->WriteKey, Data.Size, iPadding
	);
	if ( iRequired == 0 ) {
		return __xrtTlsSessionError(
			XERR_RANGE, XTLS_ERROR_RECORD_SIZE,
			"queue-protected-tls-record",
			"TLS protected record exceeds its version limit"
		);
	}
	/* 为 KeyUpdate 保留旧 epoch 的最后一个记录号，应用数据使用新 epoch。
	   仅角色会话自动轮换；TLS 1.2 和底层记录 API 仍在硬上限处拒绝。 */
	if ( (Type == XTLS_RECORD_APPLICATION_DATA) &&
		(pSession->State == XTLS_STATE_READY) &&
		(pSession->Version == XTLS_VERSION_13) &&
		(pSession->KeyUpdate != NULL) &&
		(pSession->WriteKey.Sequence >=
		 (__xrtTlsRecordKeyLimit(&pSession->WriteKey) - 1u)) ) {
		Result = pSession->KeyUpdate(
			pSession, XTLS_KEY_UPDATE_NOT_REQUESTED
		);
		if ( Result != XTLS_OK ) {
			return Result;
		}
	}
	Result = __xrtTlsSessionSendReserve(pSession, iRequired, &Span);
	if ( Result != XTLS_OK ) {
		return Result;
	}
	iSequence = pSession->WriteKey.Sequence;
	if ( !__xrtTlsRecordSeal(
		&pSession->WriteKey, Type, Data, iPadding,
		Span.Data, Span.Size, &iWritten
	) ) {
		__xrtTlsSessionCancelSensitive(&pSession->Send);
		return XTLS_ERROR;
	}
	if ( iWritten != iRequired ) {
		pSession->WriteKey.Sequence = iSequence;
		__xrtTlsSessionCancelSensitive(&pSession->Send);
		return __xrtTlsSessionError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"queue-protected-tls-record",
			"TLS record encoder returned an inconsistent size"
		);
	}
	Result = __xrtTlsSessionRecordCommit(
		pSession, iWritten, "queue-protected-tls-record"
	);
	if ( Result != XTLS_OK ) {
		pSession->WriteKey.Sequence = iSequence;
	}
	return Result;
}



/* 发布已经挂起的记录视图。 */
static xtlsresult __xrtTlsSessionRecordPublish(
	const xtlssession* pSession,
	xtlssessionrecord* pRecord
)
{
	*pRecord = pSession->Record;
	return XTLS_OK;
}



/* 从 Feed 前缀解析一条完整线路记录。 */
static xtlsresult __xrtTlsSessionRecordParse(
	xtlssession* pSession,
	xtlsrecord* pRecord
)
{
	xnetspan Span;
	size_t iRequired = XTLS_RECORD_HEADER_SIZE;
	xtlsresult Result;

	if ( xrtNetBufSize(&pSession->Feed) < XTLS_RECORD_HEADER_SIZE ) {
		return XTLS_AGAIN;
	}
	Result = __xrtTlsSessionFeedPullup(
		pSession, XTLS_RECORD_HEADER_SIZE, &Span
	);
	if ( Result != XTLS_OK ) {
		return Result;
	}
	Result = xrtTlsRecordParse(
		(xbytesview) { Span.Data, Span.Size }, pRecord, &iRequired
	);
	if ( Result == XTLS_ERROR ) {
		return XTLS_ERROR;
	}
	if ( Result == XTLS_OK ) {
		return XTLS_OK;
	}
	if ( xrtNetBufSize(&pSession->Feed) < iRequired ) {
		return XTLS_AGAIN;
	}
	Result = __xrtTlsSessionFeedPullup(pSession, iRequired, &Span);
	if ( Result != XTLS_OK ) {
		return Result;
	}
	return xrtTlsRecordParse(
		(xbytesview) { Span.Data, iRequired }, pRecord, NULL
	);
}



/* 打开受保护记录并把敏感明文保留在惰性 Scratch 块中。 */
static xtlsresult __xrtTlsSessionRecordOpen(
	xtlssession* pSession,
	const xtlsrecord* pRecord
)
{
	xnetwspan Span;
	xnetspan Plain;
	uint64 iSequence = pSession->ReadKey.Sequence;
	size_t iMinimum = pRecord->Payload.Size != 0 ?
		pRecord->Payload.Size : 1u;
	size_t iWritten = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;

	if ( !xrtNetBufReserve(&pSession->Scratch, iMinimum, &Span) ) {
		return __xrtTlsSessionCause(
			"read-protected-tls-record",
			"TLS record scratch allocation failed"
		);
	}
	if ( !__xrtTlsRecordOpen(
		&pSession->ReadKey, pRecord, Span.Data, Span.Size,
		&Type, &iWritten
	) ) {
		__xrtTlsSessionCancelSensitive(&pSession->Scratch);
		return XTLS_ERROR;
	}
	if ( !xrtNetBufCommit(&pSession->Scratch, iWritten) ) {
		pSession->ReadKey.Sequence = iSequence;
		__xrtTlsSessionCancelSensitive(&pSession->Scratch);
		return __xrtTlsSessionCause(
			"read-protected-tls-record",
			"TLS record scratch commit failed"
		);
	}
	pSession->Record.Type = Type;
	pSession->Record.Protected = true;
	if ( iWritten == 0 ) {
		pSession->Record.Data = (xbytesview) { NULL, 0 };
	} else if ( !xrtNetBufFront(&pSession->Scratch, &Plain) ) {
		pSession->ReadKey.Sequence = iSequence;
		__xrtTlsSessionClearSensitive(&pSession->Scratch);
		return __xrtTlsSessionError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"read-protected-tls-record",
			"TLS record scratch did not publish plaintext"
		);
	} else {
		pSession->Record.Data = (xbytesview) {
			Plain.Data, iWritten
		};
	}
	return XTLS_OK;
}



/* 读取但不消费下一条记录，并在背压期间稳定返回同一视图。 */
xtlsresult __xrtTlsSessionRecordNext(
	xtlssession* pSession,
	xtlssessionrecord* pRecord
)
{
	xtlsrecord Wire;
	xtlsresult Result;

	if ( !__xrtTlsSessionValid(pSession, "read-tls-session-record") ||
		(pRecord == NULL) ) {
		if ( (pSession != NULL) && (pRecord == NULL) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"read-tls-session-record", "TLS record output is null"
			);
		}
		return XTLS_ERROR;
	}
	if ( pSession->RecordPending ) {
		return __xrtTlsSessionRecordPublish(pSession, pRecord);
	}
	if ( !xrtNetBufEmpty(&pSession->Scratch) ||
		(pSession->Scratch.Reserved != NULL) ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_INTERNAL,
			"read-tls-session-record",
			"TLS record scratch is not empty"
		);
	}
	Result = __xrtTlsSessionRecordParse(pSession, &Wire);
	if ( Result != XTLS_OK ) {
		return Result;
	}
	memset(&pSession->Record, 0, sizeof(pSession->Record));
	if ( pSession->ReadKey.Ready &&
		(Wire.Type != XTLS_RECORD_CHANGE_CIPHER_SPEC) ) {
		Result = __xrtTlsSessionRecordOpen(pSession, &Wire);
		if ( Result != XTLS_OK ) {
			return Result;
		}
	} else {
		pSession->Record.Type = Wire.Type;
		pSession->Record.Data = Wire.Payload;
		pSession->Record.Protected = false;
	}
	pSession->RecordSize = Wire.EncodedSize;
	pSession->RecordPending = true;
	return __xrtTlsSessionRecordPublish(pSession, pRecord);
}



/* 清除挂起记录的元数据。 */
static void __xrtTlsSessionRecordReset(xtlssession* pSession)
{
	memset(&pSession->Record, 0, sizeof(pSession->Record));
	pSession->RecordSize = 0;
	pSession->RecordPending = false;
}



/* 完成当前记录，并按需把应用明文块移入公开读取队列。 */
xtlsresult __xrtTlsSessionRecordFinish(
	xtlssession* pSession,
	bool bApplication
)
{
	const xtlslimits* pLimits;

	if ( !__xrtTlsSessionValid(pSession, "finish-tls-session-record") ) {
		return XTLS_ERROR;
	}
	if ( !pSession->RecordPending ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"finish-tls-session-record",
			"TLS session has no pending record"
		);
	}
	if ( bApplication ) {
		if ( (pSession->Record.Type != XTLS_RECORD_APPLICATION_DATA) ||
			!pSession->Record.Protected ) {
			return __xrtTlsSessionError(
				XERR_PROTOCOL, XTLS_ERROR_RECORD_TYPE,
				"finish-tls-session-record",
				"TLS application record is not protected"
			);
		}
		pLimits = xrtTlsContextLimits(pSession->Context);
		if ( !__xrtTlsSessionFits(
			&pSession->Plain, pLimits->PlainLimit,
			pSession->Record.Data.Size
		) ) {
			return XTLS_AGAIN;
		}
		if ( !xrtNetBufMove(&pSession->Plain, &pSession->Scratch) ) {
			return __xrtTlsSessionCause(
				"finish-tls-session-record",
				"TLS application plaintext move failed"
			);
		}
	} else {
		__xrtTlsSessionClearSensitive(&pSession->Scratch);
	}
	if ( !__xrtTlsSessionFeedConsume(
		pSession, pSession->RecordSize
	) ) {
		return XTLS_ERROR;
	}
	__xrtTlsSessionRecordReset(pSession);
	return XTLS_OK;
}





/* 使用当前发送 epoch 排队一条 Alert。 */
static xtlsresult __xrtTlsSessionAlertSend(
	xtlssession* pSession,
	xtlsalertlevel Level,
	xtlsalert Alert
)
{
	uint8 Payload[2];

	if ( !xrtTlsAlertEncode(
		Level, Alert, Payload, sizeof(Payload)
	) ) {
		return XTLS_ERROR;
	}
	if ( pSession->WriteKey.Ready ) {
		return __xrtTlsSessionRecordProtect(
			pSession, XTLS_RECORD_ALERT,
			(xbytesview) { Payload, sizeof(Payload) }, 0
		);
	}
	return __xrtTlsSessionRecordPlain(
		pSession, XTLS_RECORD_ALERT, UINT16_C(0x0303),
		(xbytesview) { Payload, sizeof(Payload) }
	);
}



/* 把 TLS 根错误稳定映射到应发送给对端的 fatal Alert。 */
static xtlsalert __xrtTlsSessionFailureAlert(const xerror* pError)
{
	if ( pError == NULL ) {
		return XTLS_ALERT_INTERNAL_ERROR;
	}
	if ( (xrtErrorKind(pError) != XERR_PROTOCOL) ||
		(strcmp(xrtErrorDomain(pError), "xrt.tls") != 0) ) {
		return XTLS_ALERT_INTERNAL_ERROR;
	}
	switch ( (xtlserror)xrtErrorCode(pError) ) {
		case XTLS_ERROR_VERSION:
		case XTLS_ERROR_RECORD_VERSION:
			return XTLS_ALERT_PROTOCOL_VERSION;

		case XTLS_ERROR_RECORD_SIZE:
			return XTLS_ALERT_RECORD_OVERFLOW;

		case XTLS_ERROR_RECORD_TYPE:
		case XTLS_ERROR_STATE:
		case XTLS_ERROR_HANDSHAKE:
		case XTLS_ERROR_CLOSED:
			return XTLS_ALERT_UNEXPECTED_MESSAGE;

		case XTLS_ERROR_NEGOTIATION:
		case XTLS_ERROR_KEY_EXCHANGE:
		case XTLS_ERROR_CIPHER:
			return XTLS_ALERT_HANDSHAKE_FAILURE;

		case XTLS_ERROR_CERTIFICATE:
			return XTLS_ALERT_BAD_CERTIFICATE;

		case XTLS_ERROR_VERIFY:
		case XTLS_ERROR_RESUME:
			return XTLS_ALERT_DECRYPT_ERROR;

		case XTLS_ERROR_ALERT:
		case XTLS_ERROR_EXTENSION:
			return XTLS_ALERT_DECODE_ERROR;

		default:
			return XTLS_ALERT_INTERNAL_ERROR;
	}
}



/* 保留首个根错误，fatal Alert 编码或排队失败不得覆盖它。 */
xtlsresult __xrtTlsSessionFail(xtlssession* pSession)
{
	xerror* pRoot = xrtTakeError();
	xtlsalert Alert = __xrtTlsSessionFailureAlert(pRoot);

	if ( (pRoot == NULL) && (pSession != NULL) &&
		(pSession->State != XTLS_STATE_FAILED) &&
		(pSession->State != XTLS_STATE_CLOSED) ) {
		(void)__xrtTlsSessionError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"fail-tls-session", "TLS session failed without a root error"
		);
		pRoot = xrtTakeError();
		Alert = XTLS_ALERT_INTERNAL_ERROR;
	}
	if ( (pSession != NULL) && (pSession->State != XTLS_STATE_FAILED) &&
		(pSession->State != XTLS_STATE_CLOSED) && !pSession->FatalSent &&
		((pRoot == NULL) ||
		 (strcmp(xrtErrorDomain(pRoot), "xrt.tls") != 0) ||
		 (xrtErrorCode(pRoot) != (int32)XTLS_ERROR_TRUNCATED)) ) {
		if ( __xrtTlsSessionAlertSend(
			pSession, XTLS_ALERT_FATAL, Alert
		) == XTLS_OK ) {
			pSession->FatalSent = true;
		}
		xrtErrorFree(xrtTakeError());
	}
	if ( (pSession != NULL) && (pSession->State != XTLS_STATE_FAILED) &&
		(pSession->State != XTLS_STATE_CLOSED) ) {
		(void)__xrtTlsSessionSetState(pSession, XTLS_STATE_FAILED);
		xrtErrorFree(xrtTakeError());
	}
	if ( pRoot != NULL ) {
		__xrtErrorSetOwned(pRoot);
	}
	return XTLS_ERROR;
}



/* 处理对端 Alert，并确保 close_notify 只自动应答一次。 */
xtlsresult __xrtTlsSessionRecordAlert(
	xtlssession* pSession,
	const xtlssessionrecord* pRecord
)
{
	xtlsalertlevel Level;
	xtlsalert Alert;
	xtlsresult Result;

	if ( !__xrtTlsSessionValid(pSession, "handle-tls-session-alert") ||
		(pRecord == NULL) ) {
		if ( (pSession != NULL) && (pRecord == NULL) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"handle-tls-session-alert", "TLS alert record is null"
			);
		}
		return XTLS_ERROR;
	}
	if ( !pSession->RecordPending ||
		(pRecord->Type != XTLS_RECORD_ALERT) ||
		(pRecord->Data.Data != pSession->Record.Data.Data) ||
		(pRecord->Data.Size != pSession->Record.Data.Size) ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"handle-tls-session-alert",
			"TLS alert is not the current pending record"
		);
	}
	if ( !xrtTlsAlertParse(pRecord->Data, &Level, &Alert) ) {
		return XTLS_ERROR;
	}
	if ( (Alert == XTLS_ALERT_CLOSE_NOTIFY) &&
		(Level == XTLS_ALERT_WARNING) && !pSession->CloseSent ) {
		Result = __xrtTlsSessionAlertSend(
			pSession, XTLS_ALERT_WARNING, XTLS_ALERT_CLOSE_NOTIFY
		);
		if ( Result != XTLS_OK ) {
			return Result;
		}
		pSession->CloseSent = true;
	}
	if ( __xrtTlsSessionRecordFinish(pSession, false) != XTLS_OK ) {
		return XTLS_ERROR;
	}
	pSession->PeerAlertLevel = Level;
	pSession->PeerAlert = Alert;
	pSession->PeerAlertSet = true;
	if ( (Alert == XTLS_ALERT_CLOSE_NOTIFY) &&
		(Level == XTLS_ALERT_WARNING) ) {
		pSession->CloseReceived = true;
		__xrtTlsSessionCloseUpdate(pSession);
		return pSession->State == XTLS_STATE_CLOSED ?
			XTLS_CLOSED : XTLS_OK;
	}
	if ( (Alert == XTLS_ALERT_USER_CANCELED) &&
		(Level == XTLS_ALERT_WARNING) ) {
		return XTLS_OK;
	}
	(void)__xrtTlsSessionError(
		XERR_PROTOCOL, XTLS_ERROR_ALERT,
		"handle-tls-session-alert",
		Level == XTLS_ALERT_FATAL ?
			"TLS peer sent a fatal alert" :
			"TLS peer sent an unsupported warning alert"
	);
	(void)__xrtTlsSessionSetState(pSession, XTLS_STATE_FAILED);
	return XTLS_ERROR;
}



/* 把应用明文按协议上限切分为受保护记录并允许成功短写。 */
XRT_API xtlsresult xrtTlsSessionWrite(
	xtlssession* pSession,
	const void* pData,
	size_t iSize,
	size_t* pWritten
)
{
	cbytes pInput = (cbytes)pData;
	size_t iOffset = 0;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( !__xrtTlsSessionValid(pSession, "write-tls-session") ||
		(pWritten == NULL) || ((pData == NULL) && (iSize != 0)) ) {
		if ( (pSession != NULL) && ((pWritten == NULL) ||
			((pData == NULL) && (iSize != 0))) ) {
			(void)__xrtTlsSessionError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"write-tls-session",
				"TLS session write arguments are invalid"
			);
		}
		return XTLS_ERROR;
	}
	if ( (pSession->State == XTLS_STATE_CLOSED) ||
		pSession->CloseSent ) {
		return XTLS_CLOSED;
	}
	if ( pSession->State != XTLS_STATE_READY ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"write-tls-session", "TLS session is not ready for writing"
		);
	}
	if ( !pSession->WriteKey.Ready ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"write-tls-session", "TLS application write key is not ready"
		);
	}
	while ( iOffset < iSize ) {
		size_t iChunk = iSize - iOffset;
		xtlsresult Result;

		if ( iChunk > XTLS_RECORD_PLAINTEXT_MAX ) {
			iChunk = XTLS_RECORD_PLAINTEXT_MAX;
		}
		Result = __xrtTlsSessionRecordProtect(
			pSession, XTLS_RECORD_APPLICATION_DATA,
			(xbytesview) { pInput + iOffset, iChunk }, 0
		);
		if ( Result == XTLS_AGAIN ) {
			*pWritten = iOffset;
			(void)__xrtTlsSessionSetWait(
				pSession, XTLS_WAIT_OUTPUT
			);
			return iOffset != 0 ? XTLS_OK : XTLS_AGAIN;
		}
		if ( Result != XTLS_OK ) {
			*pWritten = iOffset;
			(void)__xrtTlsSessionSetState(
				pSession, XTLS_STATE_FAILED
			);
			return XTLS_ERROR;
		}
		iOffset += iChunk;
	}
	*pWritten = iOffset;
	(void)__xrtTlsSessionSetWait(pSession, XTLS_WAIT_NONE);
	return XTLS_OK;
}



/* 排队本端 close_notify，并发布需要继续读写的方向。 */
XRT_API xtlsresult xrtTlsSessionClose(xtlssession* pSession)
{
	xtlsresult Result;
	bool bQueued = false;

	if ( !__xrtTlsSessionValid(pSession, "close-tls-session") ) {
		return XTLS_ERROR;
	}
	if ( pSession->State == XTLS_STATE_CLOSED ) {
		return XTLS_CLOSED;
	}
	if ( pSession->State == XTLS_STATE_FAILED ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"close-tls-session", "TLS session has failed"
		);
	}
	if ( (pSession->State != XTLS_STATE_READY) &&
		(pSession->State != XTLS_STATE_CLOSING) ) {
		return __xrtTlsSessionError(
			XERR_STATE, XTLS_ERROR_STATE,
			"close-tls-session", "TLS session is not ready to close"
		);
	}
	if ( !pSession->CloseSent ) {
		Result = __xrtTlsSessionAlertSend(
			pSession, XTLS_ALERT_WARNING, XTLS_ALERT_CLOSE_NOTIFY
		);
		if ( Result != XTLS_OK ) {
			return Result;
		}
		pSession->CloseSent = true;
		bQueued = true;
	}
	__xrtTlsSessionCloseUpdate(pSession);
	if ( pSession->State == XTLS_STATE_CLOSED ) {
		return XTLS_CLOSED;
	}
	return bQueued ? XTLS_OK : XTLS_AGAIN;
}



/* 把传输 EOF 映射为认证关闭或明确的截断错误。 */
XRT_API xtlsresult xrtTlsSessionEof(xtlssession* pSession)
{
	if ( !__xrtTlsSessionValid(pSession, "end-tls-session-input") ) {
		return XTLS_ERROR;
	}
	if ( pSession->State == XTLS_STATE_CLOSED ) {
		return XTLS_CLOSED;
	}
	if ( pSession->State == XTLS_STATE_FAILED ) {
		return XTLS_ERROR;
	}
	pSession->TransportEof = true;
	if ( pSession->CloseReceived ) {
		__xrtTlsSessionCloseUpdate(pSession);
		return pSession->State == XTLS_STATE_CLOSED ?
			XTLS_CLOSED : XTLS_OK;
	}
	(void)__xrtTlsSessionError(
		XERR_PROTOCOL, XTLS_ERROR_TRUNCATED,
		"end-tls-session-input",
		"TLS transport ended without close_notify"
	);
	(void)__xrtTlsSessionSetState(pSession, XTLS_STATE_FAILED);
	return XTLS_ERROR;
}

#endif
