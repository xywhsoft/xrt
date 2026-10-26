#include "../internal/xrt_tls.h"

#include <xrt/tls_verify.h>



#if defined(XRT_FEATURE_TLS_VERIFY)

struct xtlsverifier {
	volatile int32 RefCount;
	xx509store* Store;
	xtlsverifyproc Verify;
	xtlsverifypolicyproc Policy;
	xtlsverifytimeproc Time;
	xtlsverifyreleaseproc Release;
	ptr Context;
	bool AllowSha1;
	bool Limited;
	xtlsverifylimits Limits;
	const xtlsverifiercontextownershipv1* OwnershipPolicy;
	xatomicptr OwnershipClaim;
	xatomic32 OwnershipActive;
	bool OwnershipCleared;
	bool OwnershipFinishing;
	bool OwnershipFinished;
	xerror* OwnershipFinishError;
};



/* 设置 TLS 验证错误并返回 false。 */
static bool __xrtTlsVerifyError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(
		Kind, XTLS_ERROR_VERIFY, sOperation, sMessage, SIZE_MAX
	);
	return false;
}



/* 使用当前错误作为原因设置 TLS 验证错误。 */
static bool __xrtTlsVerifyCause(cstr sOperation, cstr sMessage)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_PROTOCOL;

	__xrtTlsErrorCause(
		Kind, XTLS_ERROR_VERIFY, sOperation,
		sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 深复制信任库，使验证器不再借用可变锚和 DER 视图。 */
static xx509store* __xrtTlsVerifierStore(const xx509store* pSource)
{
	/* The canonical clone rebuilds the index without a quadratic dedup scan. */
	return pSource != NULL ? xrtX509StoreClone(pSource) : NULL;
}


static bool __xrtTlsVerifyLimit(
	size_t iActual, size_t iLimit, cstr sMessage
)
{
	return (iLimit == 0) || (iActual <= iLimit) ||
		__xrtTlsVerifyError(XERR_RANGE, "limit-tls-verifier", sMessage);
}


/* Evaluate trust budgets before creating a second owned store. */
static bool __xrtTlsTrustLimits(
	const xx509store* pStore, const xtlsverifylimits* pLimits
)
{
	size_t iBytes = 0;
	size_t iCount;

	if ( (pStore == NULL) || (pLimits == NULL) ) {
		return true;
	}
	iCount = xrtX509StoreCount(pStore);
	if ( !__xrtTlsVerifyLimit(iCount, pLimits->MaxTrustCertificates,
		"TLS trust certificate count exceeds its budget") ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xx509cert* pCertificate = xrtX509StoreCertificate(pStore, i);

		if ( pCertificate == NULL ) {
			return __xrtTlsVerifyCause("limit-tls-verifier", "TLS trust certificate is unavailable");
		}
		if ( pCertificate->Raw.Size > SIZE_MAX - iBytes ) {
			return __xrtTlsVerifyError(XERR_RANGE, "limit-tls-verifier", "TLS trust byte count overflows");
		}
		iBytes += pCertificate->Raw.Size;
		if ( !__xrtTlsVerifyLimit(iBytes, pLimits->MaxTrustBytes,
			"TLS trust bytes exceed their budget") ) {
			return false;
		}
	}
	return true;
}



/* 验证公开对端视图的指针、角色和证书数量。 */
static bool __xrtTlsPeerValid(
	const xtlspeer* pPeer, const xtlsverifylimits* pLimits
)
{
	size_t iBytes = 0;

	if ( (pPeer == NULL) ||
		((pPeer->Role != XTLS_CLIENT) && (pPeer->Role != XTLS_SERVER)) ||
		((pPeer->Name.Data == NULL) && (pPeer->Name.Size != 0)) ||
		(pPeer->Certificates == NULL) ||
		(pPeer->CertificateCount == 0) ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "verify-tls-peer",
			"TLS peer verification input is invalid"
		);
	}
	if ( (pLimits != NULL) &&
		!__xrtTlsVerifyLimit(pPeer->CertificateCount, pLimits->MaxPeerCertificates,
			"TLS peer certificate count exceeds its budget") ) {
		return false;
	}
	for ( size_t i = 0; i < pPeer->CertificateCount; i++ ) {
		if ( (pPeer->Certificates[i].Raw.Data == NULL) ||
			(pPeer->Certificates[i].Raw.Size == 0) ) {
			return __xrtTlsVerifyError(
				XERR_ARGUMENT, "verify-tls-peer",
				"TLS peer contains an invalid certificate view"
			);
		}
		if ( pLimits != NULL ) {
			if ( pPeer->Certificates[i].Raw.Size > SIZE_MAX - iBytes ) {
				return __xrtTlsVerifyError(XERR_RANGE, "limit-tls-verifier", "TLS peer byte count overflows");
			}
			iBytes += pPeer->Certificates[i].Raw.Size;
			if ( !__xrtTlsVerifyLimit(iBytes, pLimits->MaxPeerBytes,
				"TLS peer bytes exceed their budget") ) {
				return false;
			}
		}
	}
	if ( (pPeer->Role == XTLS_SERVER) && (pPeer->Name.Size == 0) ) {
		return __xrtTlsVerifyError(
			XERR_VALUE, "verify-tls-peer",
			"TLS server verification requires an identity name"
		);
	}
	return true;
}



/* 为路径构建分配发行者和输出指针工作区。 */
static const xx509cert** __xrtTlsPeerWorkspace(size_t iCount)
{
	if ( iCount > (SIZE_MAX / (2u * sizeof(const xx509cert*))) ) {
		(void)__xrtTlsVerifyError(
			XERR_RANGE, "verify-tls-peer",
			"TLS peer certificate count overflows path workspace"
		);
		return NULL;
	}
	return (const xx509cert**)xrtMalloc(
		2u * iCount * sizeof(const xx509cert*)
	);
}



/* 隔离调用线程错误槽并执行默认路径后的附加验证策略。 */
static bool __xrtTlsVerifyPolicy(
	const xtlsverifiedpeer* pPeer,
	xtlsverifypolicyproc pPolicy,
	ptr pContext
)
{
	xerror* pBefore = xrtTakeError();
	bool bAccepted = pPolicy(pPeer, pContext);
	xerror* pAfter = xrtTakeError();

	if ( bAccepted ) {
		xrtErrorFree(pAfter);
		if ( pBefore != NULL ) {
			__xrtErrorSetOwned(pBefore);
		}
		return true;
	}
	xrtErrorFree(pBefore);
	if ( pAfter == NULL ) {
		return __xrtTlsVerifyError(
			XERR_PERMISSION, "verify-tls-policy",
			"TLS verification policy rejected the peer"
		);
	}
	__xrtErrorSetOwned(pAfter);
	return __xrtTlsVerifyCause(
		"verify-tls-policy", "TLS verification policy failed"
	);
}



/* 使用显式时间、证书和借用信任库执行默认路径与身份验证。 */
static bool __xrtTlsPeerVerify(
	const xtlspeer* pPeer,
	const xx509store* pStore,
	xtlsverifypolicyproc pPolicy,
	ptr pContext,
	bool bAllowSha1,
	const xtlsverifylimits* pLimits
)
{
	static const uint8 ServerPurpose[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};
	static const uint8 ClientPurpose[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02
	};
	const xx509cert** ppWorkspace;
	const xx509cert** ppPath;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	xx509pathdecision Decision;
	xtlsverifiedpeer Verified;
	bool bVerified;
	size_t iPathCapacity;

	if ( !__xrtTlsPeerValid(pPeer, pLimits) || (pStore == NULL) ) {
		if ( pStore == NULL ) {
			(void)__xrtTlsVerifyError(
				XERR_ARGUMENT, "verify-tls-peer",
				"TLS peer trust store is null"
			);
		}
		return false;
	}
	if ( !__xrtTlsTrustLimits(pStore, pLimits) ) {
		return false;
	}
	iPathCapacity = pPeer->CertificateCount;
	if ( (pLimits != NULL) && (pLimits->MaxPathCertificates != 0) &&
		(iPathCapacity > pLimits->MaxPathCertificates) ) {
		iPathCapacity = pLimits->MaxPathCertificates;
	}
	ppWorkspace = __xrtTlsPeerWorkspace(pPeer->CertificateCount);
	if ( ppWorkspace == NULL ) {
		return __xrtTlsVerifyCause(
			"verify-tls-peer", "TLS peer path workspace allocation failed"
		);
	}
	for ( size_t i = 1; i < pPeer->CertificateCount; i++ ) {
		ppWorkspace[i - 1u] = &pPeer->Certificates[i];
	}
	ppPath = ppWorkspace + pPeer->CertificateCount;
	xrtX509PathConfigInit(&Config);
	memset(&Result, 0, sizeof(Result));
	Config.Time = pPeer->Time;
	if ( bAllowSha1 ) {
		Config.Flags |= X509_PATH_ALLOW_SHA1;
	}
	Config.KeyUsage = X509_USAGE_DIGITAL_SIGNATURE;
	if ( pPeer->Role == XTLS_SERVER ) {
		Config.Purpose = (xbytesview) {
			ServerPurpose, sizeof(ServerPurpose)
		};
	} else {
		Config.Purpose = (xbytesview) {
			ClientPurpose, sizeof(ClientPurpose)
		};
	}
	bVerified = xrtX509StoreSource(
		pStore, ppWorkspace, pPeer->CertificateCount - 1u, &Source
	);
	if ( bVerified && (pLimits == NULL) ) {
		/* Keep the legacy bool entry's exact error translation. */
		bVerified = xrtX509PathBuild(&pPeer->Certificates[0], &Source,
			&Config, ppPath, iPathCapacity, &Result);
	} else if ( bVerified ) {
		xx509check Check = xrtX509PathBuildCheckLimit(
			&pPeer->Certificates[0], &Source, &Config, ppPath,
			iPathCapacity, pLimits->MaxSearchSteps, &Result, &Decision
		);
		if ( Check == X509_CHECK_REJECTED ) {
			/* A completed policy rejection is not an unrelated stale error. */
			xrtFree(ppWorkspace);
			return __xrtTlsVerifyError(XERR_PROTOCOL, "verify-tls-peer",
				Decision.Message != NULL ? Decision.Message :
				"TLS peer certification path is invalid");
		}
		bVerified = Check == X509_CHECK_VALID;
	}
	if ( !bVerified ) {
		xrtFree(ppWorkspace);
		return __xrtTlsVerifyCause(
			"verify-tls-peer",
			"TLS peer certification path is invalid"
		);
	}
	if ( pPeer->Role == XTLS_SERVER ) {
		xx509result Match = xrtX509MatchHost(
			&pPeer->Certificates[0], pPeer->Name, NULL
		);

		if ( Match == X509_DONE ) {
			xrtFree(ppWorkspace);
			return __xrtTlsVerifyError(
				XERR_PROTOCOL, "verify-tls-peer",
				"TLS peer certificate does not match the requested name"
			);
		}
		bVerified = Match == X509_VALUE;
	}
	if ( !bVerified ) {
		xrtFree(ppWorkspace);
		return __xrtTlsVerifyCause(
			"verify-tls-peer",
			"TLS peer identity is invalid"
		);
	}
	if ( pPolicy != NULL ) {
		Verified.Peer = pPeer;
		Verified.Path = ppPath;
		Verified.PathCount = Result.Count;
		Verified.Anchor = Result.Anchor;
		bVerified = __xrtTlsVerifyPolicy(
			&Verified, pPolicy, pContext
		);
	}
	xrtFree(ppWorkspace);
	return bVerified;
}



/* 使用默认现代算法策略验证路径、用途和对端身份。 */
XRT_API bool xrtTlsPeerVerify(
	const xtlspeer* pPeer,
	const xx509store* pStore,
	xtlsverifypolicyproc pPolicy,
	ptr pContext
)
{
	return __xrtTlsPeerVerify(
		pPeer, pStore, pPolicy, pContext, false, NULL
	);
}


XRT_API bool xrtTlsPeerVerifyLimited(
	const xtlspeer* pPeer,
	const xx509store* pStore,
	const xtlsverifylimits* pLimits,
	xtlsverifypolicyproc pPolicy,
	ptr pContext
)
{
	if ( pLimits == NULL ) {
		return __xrtTlsVerifyError(XERR_ARGUMENT, "verify-tls-peer", "TLS verification limits are null");
	}
	return __xrtTlsPeerVerify(pPeer, pStore, pPolicy, pContext, false, pLimits);
}


XRT_API void xrtTlsVerifyLimitsInit(xtlsverifylimits* pLimits)
{
	if ( pLimits == NULL ) {
		(void)__xrtTlsVerifyError(XERR_ARGUMENT, "init-tls-verify-limits", "TLS verification limits are null");
		return;
	}
	*pLimits = (xtlsverifylimits) {
		64u, 16u * 1024u * 1024u, 64u, 100000u,
		4096u, 64u * 1024u * 1024u
	};
}



/* 初始化尚未绑定信任库、回调或自定义时钟的验证器配置。 */
XRT_API void xrtTlsVerifierConfigInit(xtlsverifierconfig* pConfig)
{
	if ( pConfig == NULL ) {
		(void)__xrtTlsVerifyError(
			XERR_ARGUMENT, "init-tls-verifier-config",
			"TLS verifier config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
}



/* 创建深复制信任库并接管自定义上下文的共享验证器。 */
static xtlsverifier* __xrtTlsVerifierCreate(
	const xtlsverifierconfig* pConfig,
	const xtlsverifylimits* pLimits
)
{
	xtlsverifier* pVerifier;
	xx509store* pStore;

	if ( (pConfig == NULL) ||
		((pConfig->Store == NULL) && (pConfig->Verify == NULL)) ) {
		(void)__xrtTlsVerifyError(
			XERR_ARGUMENT, "create-tls-verifier",
			"TLS verifier requires a trust store or custom verifier"
		);
		return NULL;
	}
	if ( (pConfig->Verify == NULL) &&
		(xrtX509StoreCount(pConfig->Store) == 0) ) {
		(void)__xrtTlsVerifyError(
			XERR_VALUE, "create-tls-verifier",
			"TLS verifier trust store is empty"
		);
		return NULL;
	}
	if ( !__xrtTlsTrustLimits(pConfig->Store, pLimits) ) {
		return NULL;
	}
	pStore = __xrtTlsVerifierStore(pConfig->Store);
	if ( (pConfig->Store != NULL) && (pStore == NULL) ) {
		(void)__xrtTlsVerifyCause(
			"create-tls-verifier", "TLS trust store snapshot failed"
		);
		return NULL;
	}
	pVerifier = (xtlsverifier*)xrtMalloc(sizeof(*pVerifier));
	if ( pVerifier == NULL ) {
		xrtX509StoreFree(pStore);
		(void)__xrtTlsVerifyCause(
			"create-tls-verifier", "TLS verifier allocation failed"
		);
		return NULL;
	}
	memset(pVerifier, 0, sizeof(*pVerifier));
	pVerifier->RefCount = 1;
	pVerifier->Store = pStore;
	pVerifier->Verify = pConfig->Verify;
	pVerifier->Policy = pConfig->Policy;
	pVerifier->Time = pConfig->Time;
	pVerifier->Release = pConfig->Release;
	pVerifier->Context = pConfig->Context;
	pVerifier->AllowSha1 = pConfig->AllowSha1;
	pVerifier->Limited = pLimits != NULL;
	if ( pLimits != NULL ) {
		pVerifier->Limits = *pLimits;
	}
	return pVerifier;
}


XRT_API xtlsverifier* xrtTlsVerifierCreate(const xtlsverifierconfig* pConfig)
{
	return __xrtTlsVerifierCreate(pConfig, NULL);
}


XRT_API xtlsverifier* xrtTlsVerifierCreateLimited(
	const xtlsverifierconfig* pConfig,
	const xtlsverifylimits* pLimits
)
{
	if ( pLimits == NULL ) {
		(void)__xrtTlsVerifyError(XERR_ARGUMENT, "create-tls-verifier", "TLS verification limits are null");
		return NULL;
	}
	return __xrtTlsVerifierCreate(pConfig, pLimits);
}

XRT_API xtlsverifier* xrtTlsVerifierCreateOwnedV1(
	const xtlsverifierconfig* pConfig,
	const xtlsverifylimits* pLimits,
	const xtlsverifiercontextownershipv1* pPolicy
)
{
	if (!pConfig || !pLimits || !pPolicy || pPolicy->size != sizeof(*pPolicy) ||
		!pConfig->Context || !pPolicy->Drop || !pPolicy->Ops ||
		!pPolicy->Ops->Count || !pPolicy->Ops->Trace ||
		pConfig->Verify != pPolicy->Verify || pConfig->Policy != pPolicy->Policy ||
		pConfig->Time != pPolicy->Time || pConfig->Release != pPolicy->Drop) {
		(void)__xrtTlsVerifyError(XERR_ARGUMENT, "create-owned-tls-verifier",
			"TLS verifier needs an exact resident context ownership policy");
		return NULL;
	}
	xtlsverifier* pVerifier = __xrtTlsVerifierCreate(pConfig, pLimits);
	if (pVerifier) {
		pVerifier->OwnershipPolicy = pPolicy;
		xrtAtomicPtrInit(&pVerifier->OwnershipClaim, NULL);
		xrtAtomic32Init(&pVerifier->OwnershipActive, 0);
	}
	return pVerifier;
}



/* 增加不可变验证器引用。 */
XRT_API xtlsverifier* xrtTlsVerifierRetain(
	const xtlsverifier* pVerifier
)
{
	if ( pVerifier == NULL ) {
		(void)__xrtTlsVerifyError(
			XERR_ARGUMENT, "retain-tls-verifier",
			"TLS verifier is null"
		);
		return NULL;
	}
	xrtownershipscope Mutation = {0};
	if (pVerifier->OwnershipPolicy && !xrtOwnershipMutationBegin(&Mutation)) return NULL;
	bool bHeld = !pVerifier->OwnershipCleared &&
		xrtRefRetain((volatile int32*)&pVerifier->RefCount) >= 0;
	if (pVerifier->OwnershipPolicy && !xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( !bHeld ) {
		(void)__xrtTlsVerifyError(
			XERR_STATE, "retain-tls-verifier",
			"TLS verifier reference is invalid"
		);
		return NULL;
	}
	return (xtlsverifier*)pVerifier;
}



/* 释放最后一个验证器引用、信任快照和自定义上下文。 */
XRT_API void xrtTlsVerifierRelease(xtlsverifier* pVerifier)
{
	if (!pVerifier) return;
	xrtownershipscope Mutation = {0};
	if (pVerifier->OwnershipPolicy && !xrtOwnershipMutationBegin(&Mutation)) abort();
	bool bLast = xrtRefRelease(&pVerifier->RefCount) == 0;
	if (bLast && pVerifier->OwnershipPolicy &&
		(xrtAtomic32Load(&pVerifier->OwnershipActive, XMEMORY_ACQUIRE) ||
		pVerifier->OwnershipFinishing)) abort();
	if (bLast && pVerifier->OwnershipPolicy) pVerifier->OwnershipFinishing = true;
	if (pVerifier->OwnershipPolicy && !xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bLast) return;
	if ( pVerifier->Release != NULL ) {
		pVerifier->Release(pVerifier->Context);
	}
	xrtX509StoreFree(pVerifier->Store);
	xrtErrorFree(pVerifier->OwnershipFinishError);
	memset(pVerifier, 0, sizeof(*pVerifier));
	xrtFree(pVerifier);
}



/* 执行自定义或默认信任决策。 */
static bool __xrtTlsVerifierVerify(
	const xtlsverifier* pVerifier,
	xtlsrole Role,
	xstrview Name,
	const xx509cert* pCertificates,
	size_t iCertificateCount
)
{
	xtlspeer Peer;
	xtlsverifydecision Decision = XTLS_VERIFY_DEFAULT;
	xerror* pBefore = NULL;
	xerror* pAfter = NULL;

	if ( pVerifier == NULL ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "run-tls-verifier", "TLS verifier is null"
		);
	}
	Peer.Role = Role;
	Peer.Name = Name;
	Peer.Time = pVerifier->Time != NULL ?
		pVerifier->Time(pVerifier->Context) : xrtNow();
	Peer.Certificates = pCertificates;
	Peer.CertificateCount = iCertificateCount;
	if ( !__xrtTlsPeerValid(&Peer, pVerifier->Limited ? &pVerifier->Limits : NULL) ) {
		return false;
	}
	if ( pVerifier->Verify != NULL ) {
		pBefore = xrtTakeError();
		Decision = pVerifier->Verify(&Peer, pVerifier->Context);
		pAfter = xrtTakeError();
		if ( Decision == XTLS_VERIFY_ERROR ) {
			xrtErrorFree(pBefore);
			if ( pAfter == NULL ) {
				return __xrtTlsVerifyError(
					XERR_INTERNAL, "run-tls-verifier",
					"TLS custom verifier failed without setting an error"
				);
			}
			__xrtErrorSetOwned(pAfter);
			return __xrtTlsVerifyCause(
				"run-tls-verifier", "TLS custom verifier failed"
			);
		}
		xrtErrorFree(pAfter);
		if ( pBefore != NULL ) {
			__xrtErrorSetOwned(pBefore);
		}
	}
	if ( Decision == XTLS_VERIFY_ACCEPT ) {
		return true;
	}
	if ( Decision == XTLS_VERIFY_DEFAULT ) {
		if ( pVerifier->Store == NULL ) {
			return __xrtTlsVerifyError(
				XERR_STATE, "run-tls-verifier",
				"TLS custom verifier deferred without a trust store"
			);
		}
		return __xrtTlsPeerVerify(
			&Peer, pVerifier->Store,
			pVerifier->Policy, pVerifier->Context,
			pVerifier->AllowSha1,
			pVerifier->Limited ? &pVerifier->Limits : NULL
		);
	}
	if ( Decision == XTLS_VERIFY_REJECT ) {
		return __xrtTlsVerifyError(
			XERR_PERMISSION, "run-tls-verifier",
			"TLS custom verifier rejected the peer"
		);
	}
	return __xrtTlsVerifyError(
		XERR_VALUE, "run-tls-verifier",
		"TLS custom verifier returned an invalid decision"
	);
}

/* Hold an actual verifier reference across ALL caller code, not just Policy.
 * A callback may release its original public reference; it cannot destroy the
 * currently executing verifier or make its Context disappear mid-call. */
XRT_API bool xrtTlsVerifierVerify(
	const xtlsverifier* pVerifier, xtlsrole Role, xstrview Name,
	const xx509cert* pCertificates, size_t iCertificateCount
)
{
	if (!pVerifier || !pVerifier->OwnershipPolicy)
		return __xrtTlsVerifierVerify(pVerifier, Role, Name, pCertificates, iCertificateCount);
	xtlsverifier* pMutable = (xtlsverifier*)pVerifier;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	uint32 iActive = xrtAtomic32Load(&pVerifier->OwnershipActive, XMEMORY_ACQUIRE);
	bool bEntered = false;
	if (!pVerifier->OwnershipCleared &&
		!xrtAtomicPtrLoad(&pVerifier->OwnershipClaim, XMEMORY_ACQUIRE)) {
		while (iActive < UINT32_MAX) {
			uint32 iExpected = iActive;
			if (xrtAtomic32CompareExchange(&pMutable->OwnershipActive, &iExpected,
				iActive + 1, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
				bEntered = xrtRefRetain(&pMutable->RefCount) >= 0;
				if (!bEntered) xrtAtomic32FetchSub(&pMutable->OwnershipActive, 1, XMEMORY_ACQ_REL);
				break;
			}
			iActive = iExpected;
		}
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bEntered) return __xrtTlsVerifyError(XERR_STATE, "run-tls-verifier",
		"TLS verifier is claimed, cleared or has exhausted active references");
	bool bResult = __xrtTlsVerifierVerify(pVerifier, Role, Name, pCertificates, iCertificateCount);
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (!xrtAtomic32FetchSub(&pMutable->OwnershipActive, 1, XMEMORY_ACQ_REL)) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	xrtTlsVerifierRelease(pMutable);
	return bResult;
}

static bool __xrtTlsVerifierOwnershipBusy(const void* pData)
{
	const xtlsverifier* pVerifier = pData;
	return pVerifier && (xrtAtomic32Load(&pVerifier->OwnershipActive, XMEMORY_ACQUIRE) ||
		pVerifier->OwnershipFinishing);
}
static bool __xrtTlsVerifierOwnershipCount(const void* pData, size_t* pCount)
{
	const xtlsverifier* pVerifier = pData;
	if (!pVerifier || !pCount || !pVerifier->OwnershipPolicy ||
		!pVerifier->Context || pVerifier->OwnershipCleared ||
		__xrtTlsVerifierOwnershipBusy(pVerifier)) return false;
	int32 iRefs = __xrtAtomicRefLoad(&pVerifier->RefCount);
	if (iRefs <= 0) return false;
	*pCount = (size_t)iRefs;
	return true;
}
static bool __xrtTlsVerifierOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xtlsverifier* pVerifier = pData; size_t iCount;
	return pVisit && __xrtTlsVerifierOwnershipCount(pVerifier, &iCount) &&
		pVisit((xrtownershipref){pVerifier->Context, pVerifier->OwnershipPolicy->Ops}, pContext);
}
static const xrtownershipops __xrtTlsVerifierOwnershipOps = {
	__xrtTlsVerifierOwnershipCount, __xrtTlsVerifierOwnershipTrace
};
XRT_API xrtownershipref xrtTlsVerifierOwnership(const xtlsverifier* pVerifier)
{ return (xrtownershipref){pVerifier, pVerifier ? &__xrtTlsVerifierOwnershipOps : NULL}; }
XRT_API const xrtownershipquiescencev1* xrtTlsVerifierOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {
		sizeof(Policy), &__xrtTlsVerifierOwnershipOps, __xrtTlsVerifierOwnershipBusy
	};
	return &Policy;
}
static bool __xrtTlsVerifierOwnershipHold(const void* pData)
{ return xrtTlsVerifierRetain(pData) != NULL; }
static void __xrtTlsVerifierOwnershipDrop(const void* pData)
{ xrtTlsVerifierRelease((xtlsverifier*)pData); }
static bool __xrtTlsVerifierOwnershipClaim(const void* pData, const void* pToken)
{
	xtlsverifier* pVerifier = (xtlsverifier*)pData; size_t iCount;
	ptr pOld = xrtAtomicPtrLoad(&pVerifier->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!pToken || (pOld && pOld != pToken) ||
		!__xrtTlsVerifierOwnershipCount(pVerifier, &iCount)) return false;
	xrtAtomicPtrStore(&pVerifier->OwnershipClaim, (ptr)pToken, XMEMORY_RELEASE);
	return true;
}
static void __xrtTlsVerifierOwnershipRestore(const void* pData, const void* pToken)
{
	xtlsverifier* pVerifier = (xtlsverifier*)pData;
	if (!pToken || pVerifier->OwnershipCleared ||
		xrtAtomicPtrLoad(&pVerifier->OwnershipClaim, XMEMORY_ACQUIRE) != pToken) abort();
	xrtAtomicPtrStore(&pVerifier->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
static bool __xrtTlsVerifierOwnershipReady(const void* pData)
{
	size_t iCount;
	return __xrtTlsVerifierOwnershipCount(pData, &iCount);
}
static xrtownershipprepareresult __xrtTlsVerifierOwnershipPrepare(const void* pData, const void* pToken)
{
	const xtlsverifier* pVerifier = pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	bool bReady = pVerifier && pToken &&
		xrtAtomicPtrLoad(&pVerifier->OwnershipClaim, XMEMORY_ACQUIRE) == pToken &&
		__xrtTlsVerifierOwnershipReady(pVerifier);
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTlsVerifierOwnershipClear(const void* pData, const void* pToken)
{
	xtlsverifier* pVerifier = (xtlsverifier*)pData;
	if (!pToken || xrtAtomicPtrLoad(&pVerifier->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		!__xrtTlsVerifierOwnershipReady(pVerifier)) abort();
	pVerifier->OwnershipCleared = true;
}
static bool __xrtTlsVerifierOwnershipFinish(const void* pData, const void* pToken)
{
	xtlsverifier* pVerifier = (xtlsverifier*)pData; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	if (!pToken || xrtAtomicPtrLoad(&pVerifier->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		!pVerifier->OwnershipCleared) abort();
	if (pVerifier->OwnershipFinishing) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return false;
	}
	if (pVerifier->OwnershipFinished) {
		xerror* pFailure = pVerifier->OwnershipFinishError;
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		if (pFailure && !xrtGetError()) xrtSetError(pFailure);
		return pFailure == NULL;
	}
	pVerifier->OwnershipFinishing = true;
	ptr pContext = pVerifier->Context; pVerifier->Context = NULL;
	xtlsverifyreleaseproc pDrop = pVerifier->Release; pVerifier->Release = NULL;
	xx509store* pStore = pVerifier->Store; pVerifier->Store = NULL;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	xerror* pBefore = xrtTakeError();
	pDrop(pContext);
	xrtX509StoreFree(pStore);
	xerror* pFailure = xrtTakeError();
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	pVerifier->OwnershipFinishError = pFailure;
	pVerifier->OwnershipFinishing = false; pVerifier->OwnershipFinished = true;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (pBefore) xrtSetErrorTake(pBefore);
	else if (pFailure) xrtSetError(pFailure);
	return pFailure == NULL;
}
XRT_API const xrtownershipadapterv1* xrtTlsVerifierOwnershipAdapterV1(
	xrtownershipref Reference, const xtlsverifiercontextownershipv1* const* pPolicies,
	size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation
)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtTlsVerifierOwnershipHold,
		__xrtTlsVerifierOwnershipDrop, __xrtTlsVerifierOwnershipClaim, __xrtTlsVerifierOwnershipRestore,
		NULL, __xrtTlsVerifierOwnershipClear, __xrtTlsVerifierOwnershipFinish};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtTlsVerifierOwnershipReady, __xrtTlsVerifierOwnershipPrepare};
	if (!ppPreparation || Reference.Ops != &__xrtTlsVerifierOwnershipOps || !Reference.Data ||
		(iPolicyCount && !pPolicies) || __xrtTlsVerifierOwnershipBusy(Reference.Data)) return NULL;
	const xtlsverifier* pVerifier = Reference.Data; bool bKnown = false;
	for (size_t i = 0; i < iPolicyCount; ++i) {
		if (pPolicies[i] && pPolicies[i] == pVerifier->OwnershipPolicy) { bKnown = true; break; }
	}
	if (!bKnown) return NULL;
	const xtlsverifiercontextownershipv1* pPolicy = pVerifier->OwnershipPolicy;
	if (pPolicy->size != sizeof(*pPolicy) || !pPolicy->Drop || !pPolicy->Ops ||
		!pPolicy->Ops->Count || !pPolicy->Ops->Trace || !__xrtTlsVerifierOwnershipReady(pVerifier)) return NULL;
	*ppPreparation = &Preparation;
	return &Adapter;
}



/* 把 TLS 线路签名方案转换为通用 X.509 密码验签描述。 */
static bool __xrtTlsVerifyScheme(
	xtlsversion Version,
	xtlssignature Signature,
	const xx509pubkey* pPublicKey,
	xx509signature* pScheme,
	cstr sOperation
)
{
	memset(pScheme, 0, sizeof(*pScheme));
	switch ( Signature ) {
		case XTLS_SIGNATURE_RSA_PKCS1_SHA256:
		case XTLS_SIGNATURE_RSA_PKCS1_SHA384:
		case XTLS_SIGNATURE_RSA_PKCS1_SHA512:
			if ( (Version != XTLS_VERSION_12) ||
				(pPublicKey->Type != X509_KEY_RSA) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_RSA_PKCS1;
			break;
		case XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256:
			if ( (pPublicKey->Type != X509_KEY_EC) ||
				((Version == XTLS_VERSION_13) &&
				 (pPublicKey->Curve != X509_CURVE_P256)) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ECDSA;
			pScheme->Hash = X509_HASH_SHA256;
			return true;
		case XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384:
			if ( (pPublicKey->Type != X509_KEY_EC) ||
				((Version == XTLS_VERSION_13) &&
				 (pPublicKey->Curve != X509_CURVE_P384)) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ECDSA;
			pScheme->Hash = X509_HASH_SHA384;
			return true;
		case XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512:
			if ( (Version != XTLS_VERSION_12) ||
				(pPublicKey->Type != X509_KEY_EC) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ECDSA;
			pScheme->Hash = X509_HASH_SHA512;
			return true;
		case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256:
		case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384:
		case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512:
			if ( pPublicKey->Type != X509_KEY_RSA ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_RSA_PSS;
			break;
		case XTLS_SIGNATURE_RSA_PSS_PSS_SHA256:
		case XTLS_SIGNATURE_RSA_PSS_PSS_SHA384:
		case XTLS_SIGNATURE_RSA_PSS_PSS_SHA512:
			if ( pPublicKey->Type != X509_KEY_RSA_PSS ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_RSA_PSS;
			break;
		case XTLS_SIGNATURE_ED25519:
			if ( pPublicKey->Type != X509_KEY_ED25519 ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ED25519;
			return true;
		default:
			break;
	}
	if ( pScheme->Type == X509_SIGNATURE_RSA_PKCS1 ) {
		pScheme->Hash = X509_HASH_SHA256;
		if ( Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA384 ) {
			pScheme->Hash = X509_HASH_SHA384;
		} else if ( Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA512 ) {
			pScheme->Hash = X509_HASH_SHA512;
		}
		return true;
	}
	if ( pScheme->Type == X509_SIGNATURE_RSA_PSS ) {
		pScheme->MaskHash = X509_HASH_SHA256;
		pScheme->SaltSize = 32u;
		if ( (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384) ||
			(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA384) ) {
			pScheme->MaskHash = X509_HASH_SHA384;
			pScheme->SaltSize = 48u;
		} else if ( (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512) ||
			(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA512) ) {
			pScheme->MaskHash = X509_HASH_SHA512;
			pScheme->SaltSize = 64u;
		}
		pScheme->Hash = pScheme->MaskHash;
		pScheme->Trailer = 1u;
		return true;
	}
	return __xrtTlsVerifyError(
		XERR_PROTOCOL, sOperation,
		"TLS signature scheme and certificate key are incompatible"
	);
}



/* 验证 TLS 1.2 ECDHE 参数签名，不让调用方重复拼接线路签名内容。 */
XRT_API bool xrtTls12ServerKeyExchangeVerify(
	xtlssignature SignatureScheme,
	xbytesview ClientRandom,
	xbytesview ServerRandom,
	xbytesview Parameters,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	uint8 Content[(XTLS_RANDOM_SIZE * 2u) + 3u + 1u + UINT8_MAX];
	size_t iContentSize;
	xx509signature Scheme;

	if ( (ClientRandom.Data == NULL) ||
		(ClientRandom.Size != XTLS_RANDOM_SIZE) ||
		(ServerRandom.Data == NULL) ||
		(ServerRandom.Size != XTLS_RANDOM_SIZE) ||
		(Parameters.Data == NULL) || (Parameters.Size < 4u) ||
		(Parameters.Size > (3u + 1u + UINT8_MAX)) ||
		(Signature.Data == NULL) || (Signature.Size == 0) ||
		(pPublicKey == NULL) ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "verify-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange signature input is invalid"
		);
	}
	if ( !__xrtTlsVerifyScheme(
		XTLS_VERSION_12, SignatureScheme, pPublicKey, &Scheme,
		"verify-tls12-server-key-exchange"
	) ) {
		return false;
	}
	memcpy(Content, ClientRandom.Data, XTLS_RANDOM_SIZE);
	memcpy(
		Content + XTLS_RANDOM_SIZE,
		ServerRandom.Data, XTLS_RANDOM_SIZE
	);
	memcpy(
		Content + (XTLS_RANDOM_SIZE * 2u),
		Parameters.Data, Parameters.Size
	);
	iContentSize = (XTLS_RANDOM_SIZE * 2u) + Parameters.Size;
	if ( !xrtX509SignatureVerify(
		&Scheme, (xbytesview) { Content, iContentSize },
		Signature, pPublicKey
	) ) {
		xrtSecureZero(Content, sizeof(Content));
		return __xrtTlsVerifyCause(
			"verify-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange signature is invalid"
		);
	}
	xrtSecureZero(Content, sizeof(Content));
	return true;
}



/* 验证 TLS 1.3 对端 CertificateVerify。 */
XRT_API bool xrtTls13CertificateVerifySignature(
	xtlsrole Signer,
	xtlssignature SignatureScheme,
	xbytesview TranscriptHash,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	uint8 Content[64u + 34u + 1u + 48u];
	size_t iContentSize;
	xx509signature Scheme;

	if ( ((Signer != XTLS_CLIENT) && (Signer != XTLS_SERVER)) ||
		((TranscriptHash.Size != 32u) && (TranscriptHash.Size != 48u)) ||
		(TranscriptHash.Data == NULL) || (Signature.Data == NULL) ||
		(Signature.Size == 0) || (pPublicKey == NULL) ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "verify-tls13-certificate-signature",
			"TLS 1.3 CertificateVerify input is invalid"
		);
	}
	if ( !__xrtTlsVerifyScheme(
		XTLS_VERSION_13, SignatureScheme, pPublicKey, &Scheme,
		"verify-tls13-certificate-signature"
	) ) {
		return false;
	}
	iContentSize = xrtTls13CertificateVerifyContentSize(
		Signer, TranscriptHash.Size
	);
	if ( (iContentSize == 0) || !xrtTls13CertificateVerifyContentEncode(
		Signer, TranscriptHash, Content, sizeof(Content)
	) ) {
		xrtSecureZero(Content, sizeof(Content));
		return false;
	}
	if ( !xrtX509SignatureVerify(
		&Scheme, (xbytesview) { Content, iContentSize },
		Signature, pPublicKey
	) ) {
		xrtSecureZero(Content, sizeof(Content));
		return __xrtTlsVerifyCause(
			"verify-tls13-certificate-signature",
			"TLS 1.3 CertificateVerify signature is invalid"
		);
	}
	xrtSecureZero(Content, sizeof(Content));
	return true;
}

#endif
