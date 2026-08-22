#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>
#include <xrt/memory.h>



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 会话状态只在所属会话锁内推进计数，其余字段创建后保持不变。 */
typedef struct xrt_http_digest_session_state {
	volatile int32 RefCount;
	size_t AllocationSize;
	uint32 Flags;
	uint32 NextCount;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xstrview Username;
	xstrview UsernameLanguage;
	xstrview Secret;
	xstrview Cnonce;
	uint8 Data[];
} xrt_http_digest_session_state;



/* 会话用一把短临界区互斥锁保护当前状态指针和 nonce-count。 */
struct xhttpdigestsession {
	volatile int32 RefCount;
	xmutex Lock;
	xrt_http_digest_session_state* State;
};



/* Exchange 拥有本次请求目标、实体摘要和生成的凭据字节。 */
struct xhttpdigestexchange {
	volatile int32 RefCount;
	size_t AllocationSize;
	xhttpdigestsession* Session;
	xrt_http_digest_session_state* State;
	xhttpdigestauth Auth;
	xhttpdigestproof Proof;
	uint8 Data[];
};



/* 校验写出区不会破坏会话、Exchange 及其保留的认证状态。 */
bool __xrtHttpDigestSessionOutputValid(
	const xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const void* pOutput,
	size_t iSize
)
{
	const xrt_http_digest_session_state* pState;

	if ( !__xrtRangeValid(pSession, sizeof(*pSession)) ||
		!__xrtRangeValid(pOutput, iSize) ||
		__xrtRangesOverlap(
			pSession, sizeof(*pSession), pOutput, iSize
		) ) {
		return false;
	}
	if ( pExchange == NULL ) {
		return true;
	}
	if ( !__xrtRangeValid(pExchange, sizeof(*pExchange)) ||
		(pExchange->Session != pSession) ||
		(pExchange->AllocationSize < sizeof(*pExchange)) ||
		!__xrtRangeValid(
			pExchange, pExchange->AllocationSize
		) ) {
		return false;
	}
	pState = pExchange->State;
	if ( !__xrtRangeValid(pState, sizeof(*pState)) ||
		(pState->AllocationSize < sizeof(*pState)) ||
		!__xrtRangeValid(pState, pState->AllocationSize) ) {
		return false;
	}
	return !__xrtRangesOverlap(
		pExchange,
		pExchange->AllocationSize,
		pOutput,
		iSize
	) && !__xrtRangesOverlap(
		pState,
		pState->AllocationSize,
		pOutput,
		iSize
	);
}



/* 增加内部状态引用，避免会话更新破坏正在处理的请求。 */
static bool __xrtHttpDigestSessionStateRetain(
	xrt_http_digest_session_state* pState
)
{
	if ( xrtRefRetain(&pState->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 释放状态最后一个引用，并可靠清除其中的长期 Secret。 */
static void __xrtHttpDigestSessionStateRelease(
	xrt_http_digest_session_state* pState
)
{
	size_t iAllocation;

	if ( (pState == NULL) ||
		(xrtRefRelease(&pState->RefCount) != 0) ) {
		return;
	}
	iAllocation = pState->AllocationSize;
	xrtSecureZero(pState, iAllocation);
	xrtFree(pState);
}



/* 把一个可选视图复制到对象尾部，并返回新的自有视图。 */
static xstrview __xrtHttpDigestSessionCopy(
	bytes* ppOutput,
	xstrview Value
)
{
	xstrview Result = { NULL, 0 };

	if ( Value.Size == 0 ) {
		return Result;
	}
	memcpy(*ppOutput, Value.Data, Value.Size);
	Result = (xstrview){ (cstr)*ppOutput, Value.Size };
	*ppOutput += Value.Size;
	return Result;
}



/* 复制未对齐配置和它引用的两个公开协议描述符。 */
static bool __xrtHttpDigestSessionConfigPrepare(
	const xhttpdigestsessionconfig* pInput,
	xhttpdigestsessionconfig* pConfig,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pConfig, pInput, sizeof(*pConfig));
	if ( !__xrtRangeValid(
		pConfig->Challenge, sizeof(*pChallenge)
	) || !__xrtRangeValid(
		pConfig->Choice, sizeof(*pChoice)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pChallenge, pConfig->Challenge, sizeof(*pChallenge));
	memcpy(pChoice, pConfig->Choice, sizeof(*pChoice));
	pConfig->Challenge = pChallenge;
	pConfig->Choice = pChoice;
	return true;
}



/*
	先复用无状态构建器完成全部语义校验，再按实际字段长度创建状态。
	这样状态层不会维护第二套 Digest 协议规则。
*/
static xrt_http_digest_session_state* __xrtHttpDigestSessionStateCreate(
	const xhttpdigestsessionconfig* pInput
)
{
	xhttpdigestsessionconfig Config;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xhttpdigestclientauth Client;
	xhttpdigestauth Auth;
	xrt_http_digest_session_state* pState;
	xstrview Views[9];
	char EntityHash[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	bytes pNext;
	size_t iAllocation = sizeof(*pState);
	size_t iDigest;
	size_t iSize;

	memset(&Config, 0, sizeof(Config));
	memset(&Challenge, 0, sizeof(Challenge));
	memset(&Choice, 0, sizeof(Choice));
	memset(&Auth, 0, sizeof(Auth));
	memset(EntityHash, '0', sizeof(EntityHash));
	if ( !__xrtHttpDigestSessionConfigPrepare(
		pInput, &Config, &Challenge, &Choice
	) ) {
		return NULL;
	}
	iDigest = xrtHttpDigestSize(Choice.Algorithm) * 2u;
	Client = (xhttpdigestclientauth){
		Config.Flags,
		&Challenge,
		&Choice,
		Config.Username,
		Config.UsernameLanguage,
		Config.Secret,
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/"),
		Config.Cnonce,
		Choice.Qop == XHTTP_DIGEST_QOP_AUTH_INT ?
			(xstrview){ EntityHash, iDigest } :
			(xstrview){ NULL, 0 },
		1u
	};
	if ( !xrtHttpDigestClientAuth(
		&Client, NULL, 0, &iSize, &Auth
	) ) {
		xrtSecureZero(EntityHash, sizeof(EntityHash));
		return NULL;
	}
	Views[0] = Challenge.Realm;
	Views[1] = Challenge.Domain;
	Views[2] = Challenge.Nonce;
	Views[3] = Challenge.Opaque;
	Views[4] = Challenge.AlgorithmName;
	Views[5] = Config.Username;
	Views[6] = Config.UsernameLanguage;
	Views[7] = Config.Secret;
	Views[8] = Config.Cnonce;
	for ( size_t i = 0;
		i < (sizeof(Views) / sizeof(Views[0]));
		i++ ) {
		if ( !__xrtHttpSizeAdd(&iAllocation, Views[i].Size) ) {
			xrtSecureZero(EntityHash, sizeof(EntityHash));
			return NULL;
		}
	}
	pState = (xrt_http_digest_session_state*)xrtMalloc(iAllocation);
	if ( pState == NULL ) {
		xrtSecureZero(EntityHash, sizeof(EntityHash));
		return NULL;
	}
	memset(pState, 0, sizeof(*pState));
	pState->RefCount = 1;
	pState->AllocationSize = iAllocation;
	pState->Flags = Config.Flags;
	pState->NextCount = 1u;
	pState->Challenge = Challenge;
	pState->Choice = Choice;
	pNext = pState->Data;
	pState->Challenge.Realm = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Realm
	);
	pState->Challenge.Domain = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Domain
	);
	pState->Challenge.Nonce = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Nonce
	);
	pState->Challenge.Opaque = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Opaque
	);
	pState->Challenge.AlgorithmName = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.AlgorithmName
	);
	pState->Username = __xrtHttpDigestSessionCopy(
		&pNext, Config.Username
	);
	pState->UsernameLanguage = __xrtHttpDigestSessionCopy(
		&pNext, Config.UsernameLanguage
	);
	pState->Secret = __xrtHttpDigestSessionCopy(
		&pNext, Config.Secret
	);
	pState->Cnonce = __xrtHttpDigestSessionCopy(
		&pNext, Config.Cnonce
	);
	xrtSecureZero(EntityHash, sizeof(EntityHash));
	return pState;
}



/* 从已验证响应的 nextnonce 派生新状态，并把计数重置为 1。 */
static xrt_http_digest_session_state* __xrtHttpDigestSessionStateNext(
	const xrt_http_digest_session_state* pState,
	xstrview NextNonce,
	xstrview NextCnonce
)
{
	xhttpdigestchallenge Challenge = pState->Challenge;
	xhttpdigestsessionconfig Config;

	Challenge.Nonce = NextNonce;
	Config = (xhttpdigestsessionconfig){
		pState->Flags,
		&Challenge,
		&pState->Choice,
		pState->Username,
		pState->UsernameLanguage,
		pState->Secret,
		NextCnonce.Size != 0 ? NextCnonce : pState->Cnonce
	};
	return __xrtHttpDigestSessionStateCreate(&Config);
}



/* 创建拥有独立状态副本的 Digest 客户端会话。 */
XRT_API xhttpdigestsession* xrtHttpDigestSessionCreate(
	const xhttpdigestsessionconfig* pConfig
)
{
	xrt_http_digest_session_state* pState;
	xhttpdigestsession* pSession;

	pState = __xrtHttpDigestSessionStateCreate(pConfig);
	if ( pState == NULL ) {
		return NULL;
	}
	pSession = (xhttpdigestsession*)xrtMalloc(sizeof(*pSession));
	if ( pSession == NULL ) {
		__xrtHttpDigestSessionStateRelease(pState);
		return NULL;
	}
	memset(pSession, 0, sizeof(*pSession));
	pSession->RefCount = 1;
	pSession->State = pState;
	if ( !xrtMutexInit(&pSession->Lock) ) {
		__xrtHttpDigestSessionStateRelease(pState);
		xrtFree(pSession);
		return NULL;
	}
	return pSession;
}



/* 增加会话引用并返回原对象。 */
XRT_API xhttpdigestsession* xrtHttpDigestSessionRetain(
	xhttpdigestsession* pSession
)
{
	if ( !__xrtRangeValid(pSession, sizeof(*pSession)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pSession->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pSession;
}



/* 释放会话最后一个引用和当前状态。 */
XRT_API void xrtHttpDigestSessionRelease(
	xhttpdigestsession* pSession
)
{
	xrt_http_digest_session_state* pState;

	if ( (pSession == NULL) ||
		(xrtRefRelease(&pSession->RefCount) != 0) ) {
		return;
	}
	pState = pSession->State;
	(void)xrtMutexUnit(&pSession->Lock);
	memset(pSession, 0, sizeof(*pSession));
	xrtFree(pSession);
	__xrtHttpDigestSessionStateRelease(pState);
}



/* 先完整构造新状态，再用短临界区提交，失败不改变原状态。 */
XRT_API bool xrtHttpDigestSessionUpdate(
	xhttpdigestsession* pSession,
	const xhttpdigestsessionconfig* pConfig
)
{
	xrt_http_digest_session_state* pState;
	xrt_http_digest_session_state* pOld;

	if ( !__xrtRangeValid(pSession, sizeof(*pSession)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pState = __xrtHttpDigestSessionStateCreate(pConfig);
	if ( pState == NULL ) {
		return false;
	}
	if ( !xrtMutexLock(&pSession->Lock) ) {
		__xrtHttpDigestSessionStateRelease(pState);
		return false;
	}
	pOld = pSession->State;
	pSession->State = pState;
	(void)xrtMutexUnlock(&pSession->Lock);
	__xrtHttpDigestSessionStateRelease(pOld);
	return true;
}



/* 释放尚未发布或已经发布的 Exchange。 */
static void __xrtHttpDigestExchangeFree(
	xhttpdigestexchange* pExchange
)
{
	xhttpdigestsession* pSession = pExchange->Session;
	xrt_http_digest_session_state* pState = pExchange->State;
	size_t iAllocation = pExchange->AllocationSize;

	xrtSecureZero(pExchange, iAllocation);
	xrtFree(pExchange);
	__xrtHttpDigestSessionStateRelease(pState);
	xrtHttpDigestSessionRelease(pSession);
}



/* 为请求分配精确大小的 Exchange，并在线性化点保留唯一 nonce-count。 */
XRT_API xhttpdigestexchange* xrtHttpDigestSessionAuthorize(
	xhttpdigestsession* pSession,
	xstrview Method,
	xstrview RequestTarget,
	xstrview EntityHash
)
{
	for ( ;; ) {
		xrt_http_digest_session_state* pState;
		xhttpdigestexchange* pExchange;
		xhttpdigestclientauth Client;
		xhttpdigestauth Auth;
		bytes pNext;
		size_t iAllocation = sizeof(*pExchange);
		size_t iOutput;
		size_t iWritten;
		uint32 iNonceCount;

		if ( !__xrtRangeValid(pSession, sizeof(*pSession)) ||
			!__xrtHttpViewValid(Method) ||
			!__xrtHttpViewValid(RequestTarget) ||
			!__xrtHttpViewValid(EntityHash) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		if ( !xrtMutexLock(&pSession->Lock) ) {
			return NULL;
		}
		pState = pSession->State;
		if ( !__xrtHttpDigestSessionStateRetain(pState) ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			return NULL;
		}
		(void)xrtMutexUnlock(&pSession->Lock);
		Client = (xhttpdigestclientauth){
			pState->Flags,
			&pState->Challenge,
			&pState->Choice,
			pState->Username,
			pState->UsernameLanguage,
			pState->Secret,
			Method,
			RequestTarget,
			pState->Cnonce,
			EntityHash,
			1u
		};
		memset(&Auth, 0, sizeof(Auth));
		if ( !xrtHttpDigestClientAuth(
			&Client, NULL, 0, &iOutput, &Auth
		) || !__xrtHttpSizeAdd(
			&iAllocation, RequestTarget.Size
		) || !__xrtHttpSizeAdd(
			&iAllocation, EntityHash.Size
		) || !__xrtHttpSizeAdd(
			&iAllocation, iOutput
		) ) {
			__xrtHttpDigestSessionStateRelease(pState);
			return NULL;
		}
		pExchange = (xhttpdigestexchange*)xrtMalloc(iAllocation);
		if ( pExchange == NULL ) {
			__xrtHttpDigestSessionStateRelease(pState);
			return NULL;
		}
		memset(pExchange, 0, sizeof(*pExchange));
		pExchange->RefCount = 1;
		pExchange->AllocationSize = iAllocation;
		pExchange->State = pState;
		pNext = pExchange->Data;
		Client.RequestTarget = __xrtHttpDigestSessionCopy(
			&pNext, RequestTarget
		);
		Client.EntityHash = __xrtHttpDigestSessionCopy(
			&pNext, EntityHash
		);
		if ( !xrtMutexLock(&pSession->Lock) ) {
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		if ( pSession->State != pState ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			__xrtHttpDigestExchangeFree(pExchange);
			continue;
		}
		if ( pState->NextCount == 0 ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			__xrtErrorSetRange();
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		iNonceCount = pState->NextCount;
		pState->NextCount = (iNonceCount == UINT32_MAX) ?
			0u : (iNonceCount + 1u);
		if ( xrtRefRetain(&pSession->RefCount) < 0 ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			__xrtErrorSetInvalidState();
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		pExchange->Session = pSession;
		(void)xrtMutexUnlock(&pSession->Lock);
		Client.NonceCount = iNonceCount;
		if ( !xrtHttpDigestClientAuth(
			&Client, pNext, iOutput, &iWritten, &Auth
		) ) {
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		pExchange->Auth = Auth;
		pExchange->Proof = (xhttpdigestproof){
			pState->Choice.Algorithm,
			pState->Choice.Qop,
			iNonceCount,
			pState->Secret,
			pState->Challenge.Nonce,
			pState->Cnonce,
			Client.RequestTarget,
			Client.EntityHash
		};
		return pExchange;
	}
}



/* 增加 Exchange 引用并返回原对象。 */
XRT_API xhttpdigestexchange* xrtHttpDigestExchangeRetain(
	xhttpdigestexchange* pExchange
)
{
	if ( !__xrtRangeValid(pExchange, sizeof(*pExchange)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pExchange->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pExchange;
}



/* 释放 Exchange 最后一个引用和它持有的状态快照。 */
XRT_API void xrtHttpDigestExchangeRelease(
	xhttpdigestexchange* pExchange
)
{
	if ( (pExchange == NULL) ||
		(xrtRefRelease(&pExchange->RefCount) != 0) ) {
		return;
	}
	__xrtHttpDigestExchangeFree(pExchange);
}



/* 取得不可变 Authorization 协议对象。 */
XRT_API const xhttpdigestauth* xrtHttpDigestExchangeAuth(
	const xhttpdigestexchange* pExchange
)
{
	if ( !__xrtRangeValid(pExchange, sizeof(*pExchange)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return &pExchange->Auth;
}



/* 取得与本次请求严格绑定的响应验证 proof。 */
XRT_API const xhttpdigestproof* xrtHttpDigestExchangeProof(
	const xhttpdigestexchange* pExchange
)
{
	if ( !__xrtRangeValid(pExchange, sizeof(*pExchange)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return &pExchange->Proof;
}



/* 验证服务器证明，并只允许当前状态对应的 Exchange 推进 nextnonce。 */
XRT_API xhttpdigestsessioncheck xrtHttpDigestSessionAccept(
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const xhttpdigestinfo* pInput,
	xstrview ResponseEntityHash,
	xstrview NextCnonce
)
{
	xhttpdigestinfo Info;
	xhttpdigestinfoverification Verification;
	xrt_http_digest_session_state* pNext;
	xrt_http_digest_session_state* pOld;
	xhttpdigestinfocheck Check;

	if ( !__xrtRangeValid(pSession, sizeof(*pSession)) ||
		!__xrtRangeValid(pExchange, sizeof(*pExchange)) ||
		!__xrtRangeValid(pInput, sizeof(*pInput)) ||
		!__xrtHttpViewValid(ResponseEntityHash) ||
		!__xrtHttpViewValid(NextCnonce) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( pExchange->Session != pSession ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	memcpy(&Info, pInput, sizeof(Info));
	Verification = (xhttpdigestinfoverification){
		&Info,
		&pExchange->Proof,
		ResponseEntityHash
	};
	Check = xrtHttpDigestInfoVerify(&Verification);
	if ( Check == XHTTP_DIGEST_INFO_ERROR ) {
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( Check == XHTTP_DIGEST_INFO_INVALID ) {
		return XHTTP_DIGEST_SESSION_INVALID;
	}
	if ( (Info.Flags & XHTTP_DIGEST_INFO_HAS_NEXT_NONCE) == 0 ) {
		if ( NextCnonce.Size != 0 ) {
			__xrtErrorSetValue();
			return XHTTP_DIGEST_SESSION_ERROR;
		}
		return XHTTP_DIGEST_SESSION_VALID;
	}
	pNext = __xrtHttpDigestSessionStateNext(
		pExchange->State, Info.NextNonce, NextCnonce
	);
	if ( pNext == NULL ) {
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( !xrtMutexLock(&pSession->Lock) ) {
		__xrtHttpDigestSessionStateRelease(pNext);
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( pSession->State != pExchange->State ) {
		(void)xrtMutexUnlock(&pSession->Lock);
		__xrtHttpDigestSessionStateRelease(pNext);
		return XHTTP_DIGEST_SESSION_SUPERSEDED;
	}
	pOld = pSession->State;
	pSession->State = pNext;
	(void)xrtMutexUnlock(&pSession->Lock);
	__xrtHttpDigestSessionStateRelease(pOld);
	return XHTTP_DIGEST_SESSION_UPDATED;
}

#endif
