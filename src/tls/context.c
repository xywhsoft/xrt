#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_CONTEXT)

struct xtlscontext {
	volatile int32 RefCount;
	xtlspolicy Policy;
	xtlslimits Limits;
};



/* 设置 TLS 上下文错误并返回 false。 */
static bool __xrtTlsContextError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(Kind, Code, sOperation, sMessage, SIZE_MAX);
	return false;
}



/* 安全累加策略快照的单次分配大小。 */
static bool __xrtTlsContextAddSize(
	size_t* pTotal,
	size_t iCount,
	size_t iItemSize
)
{
	if ( iCount > ((SIZE_MAX - *pTotal) / iItemSize) ) {
		return __xrtTlsContextError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-context",
			"TLS context policy snapshot size overflows"
		);
	}
	*pTotal += iCount * iItemSize;
	return true;
}



/* 计算包含四个有序数组的紧凑上下文大小。 */
static bool __xrtTlsContextSize(
	const xtlspolicy* pPolicy,
	size_t* pSize
)
{
	size_t iSize = sizeof(xtlscontext);

	if ( !__xrtTlsContextAddSize(
		&iSize, pPolicy->VersionCount, sizeof(xtlsversion)
	) || !__xrtTlsContextAddSize(
		&iSize, pPolicy->CipherCount, sizeof(xtlscipher)
	) || !__xrtTlsContextAddSize(
		&iSize, pPolicy->SignatureCount, sizeof(xtlssignature)
	) || !__xrtTlsContextAddSize(
		&iSize, pPolicy->GroupCount, sizeof(uint16)
	) ) {
		return false;
	}
	*pSize = iSize;
	return true;
}



/* 把调用方策略复制到上下文尾部的单次分配存储。 */
static void __xrtTlsContextCopyPolicy(
	xtlscontext* pContext,
	const xtlspolicy* pPolicy
)
{
	bytes pStorage = (bytes)(pContext + 1);

	pContext->Policy = *pPolicy;
	pContext->Policy.Versions = (const xtlsversion*)pStorage;
	memcpy(
		pStorage, pPolicy->Versions,
		pPolicy->VersionCount * sizeof(xtlsversion)
	);
	pStorage += pPolicy->VersionCount * sizeof(xtlsversion);

	pContext->Policy.Ciphers = (const xtlscipher*)pStorage;
	memcpy(
		pStorage, pPolicy->Ciphers,
		pPolicy->CipherCount * sizeof(xtlscipher)
	);
	pStorage += pPolicy->CipherCount * sizeof(xtlscipher);

	if ( pPolicy->SignatureCount != 0 ) {
		pContext->Policy.Signatures = (const xtlssignature*)pStorage;
		memcpy(
			pStorage, pPolicy->Signatures,
			pPolicy->SignatureCount * sizeof(xtlssignature)
		);
		pStorage += pPolicy->SignatureCount * sizeof(xtlssignature);
	} else {
		pContext->Policy.Signatures = NULL;
	}

	if ( pPolicy->GroupCount != 0 ) {
		pContext->Policy.Groups = (const uint16*)pStorage;
		memcpy(
			pStorage, pPolicy->Groups,
			pPolicy->GroupCount * sizeof(uint16)
		);
	} else {
		pContext->Policy.Groups = NULL;
	}
}



/* 初始化不会造成每连接预分配的安全默认限制。 */
XRT_API void xrtTlsLimitsInit(xtlslimits* pLimits)
{
	if ( pLimits == NULL ) {
		__xrtTlsContextError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"init-tls-limits", "TLS limits are null"
		);
		return;
	}
	pLimits->FeedLimit = XTLS_FEED_LIMIT_DEFAULT;
	pLimits->SendLimit = XTLS_SEND_LIMIT_DEFAULT;
	pLimits->PlainLimit = XTLS_PLAIN_LIMIT_DEFAULT;
	pLimits->HandshakeLimit = XTLS_HANDSHAKE_LIMIT_DEFAULT;
	pLimits->RecordBudget = XTLS_DRIVE_RECORD_BUDGET_DEFAULT;
	pLimits->HandshakeBudget = XTLS_DRIVE_HANDSHAKE_BUDGET_DEFAULT;
}



/* 验证合法对端记录不会让会话因限制过小而永久停滞。 */
XRT_API bool xrtTlsLimitsValid(const xtlslimits* pLimits)
{
	size_t iRecordLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX;

	if ( pLimits == NULL ) {
		return __xrtTlsContextError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"validate-tls-limits", "TLS limits are null"
		);
	}
	if ( (pLimits->FeedLimit < iRecordLimit) ||
		(pLimits->SendLimit < iRecordLimit) ||
		(pLimits->PlainLimit < XTLS_RECORD_PLAINTEXT_MAX) ) {
		return __xrtTlsContextError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "validate-tls-limits",
			"TLS queue limit cannot hold one maximum record"
		);
	}
	if ( (pLimits->HandshakeLimit < XTLS_HANDSHAKE_HEADER_SIZE) ||
		(pLimits->HandshakeLimit >
		 (XTLS_HANDSHAKE_HEADER_SIZE + XTLS_HANDSHAKE_BODY_MAX)) ) {
		return __xrtTlsContextError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "validate-tls-limits",
			"TLS handshake limit is outside the wire range"
		);
	}
	if ( (pLimits->RecordBudget == 0) ||
		(pLimits->HandshakeBudget == 0) ) {
		return __xrtTlsContextError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "validate-tls-limits",
			"TLS drive budget cannot be zero"
		);
	}
	return true;
}



/* 初始化默认策略和限制的借用式创建配置。 */
XRT_API void xrtTlsContextConfigInit(xtlscontextconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtTlsContextError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"init-tls-context-config", "TLS context config is null"
		);
		return;
	}
	pConfig->Policy = NULL;
	xrtTlsLimitsInit(&pConfig->Limits);
}



/* 创建一份策略数组和限制都不再借用调用方内存的共享快照。 */
XRT_API xtlscontext* xrtTlsContextCreate(
	const xtlscontextconfig* pConfig
)
{
	xtlscontextconfig Config;
	xtlspolicy DefaultPolicy;
	const xtlspolicy* pPolicy;
	xtlscontext* pContext;
	size_t iSize;

	if ( pConfig == NULL ) {
		xrtTlsContextConfigInit(&Config);
		pConfig = &Config;
	}
	pPolicy = pConfig->Policy;
	if ( pPolicy == NULL ) {
		xrtTlsPolicyInit(&DefaultPolicy);
		pPolicy = &DefaultPolicy;
	}
	if ( !xrtTlsLimitsValid(&pConfig->Limits) ||
		!xrtTlsPolicyValid(pPolicy) ||
		!__xrtTlsContextSize(pPolicy, &iSize) ) {
		return NULL;
	}
	pContext = (xtlscontext*)xrtMalloc(iSize);
	if ( pContext == NULL ) {
		const xerror* pCause = xrtGetError();

		__xrtTlsErrorCause(
			XERR_MEMORY, XTLS_ERROR_INTERNAL, "create-tls-context",
			"TLS context allocation failed", SIZE_MAX, pCause
		);
		return NULL;
	}
	memset(pContext, 0, sizeof(*pContext));
	pContext->RefCount = 1;
	pContext->Limits = pConfig->Limits;
	__xrtTlsContextCopyPolicy(pContext, pPolicy);
	return pContext;
}



/* 增加共享上下文引用并拒绝无效或耗尽的引用计数。 */
XRT_API xtlscontext* xrtTlsContextRetain(
	const xtlscontext* pContext
)
{
	if ( pContext == NULL ) {
		__xrtTlsContextError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"retain-tls-context", "TLS context is null"
		);
		return NULL;
	}
	if ( xrtRefRetain((volatile int32*)&pContext->RefCount) < 0 ) {
		__xrtTlsContextError(
			XERR_STATE, XTLS_ERROR_STATE,
			"retain-tls-context", "TLS context reference is invalid"
		);
		return NULL;
	}
	return (xtlscontext*)pContext;
}



/* 释放最后一个共享上下文引用和唯一一块快照内存。 */
XRT_API void xrtTlsContextRelease(xtlscontext* pContext)
{
	if ( (pContext == NULL) ||
		(xrtRefRelease(&pContext->RefCount) != 0) ) {
		return;
	}
	xrtFree(pContext);
}



/* 发布由上下文拥有的只读策略快照。 */
XRT_API const xtlspolicy* xrtTlsContextPolicy(
	const xtlscontext* pContext
)
{
	if ( pContext == NULL ) {
		__xrtTlsContextError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"get-tls-context-policy", "TLS context is null"
		);
		return NULL;
	}
	return &pContext->Policy;
}



/* 发布由上下文拥有的只读限制快照。 */
XRT_API const xtlslimits* xrtTlsContextLimits(
	const xtlscontext* pContext
)
{
	if ( pContext == NULL ) {
		__xrtTlsContextError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"get-tls-context-limits", "TLS context is null"
		);
		return NULL;
	}
	return &pContext->Limits;
}

#endif
